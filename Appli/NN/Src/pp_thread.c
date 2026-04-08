/**
 ******************************************************************************
 * @file    pp_thread.c
 * @author  Long Liangmao
 * @brief   Postprocessing thread and detection state management implementation
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 Long Liangmao.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#include "bqueue.h"
#include "cam.h"
#include "error.h"
#include "model_config.h"
#include "nn.h"
#include "nn_config.h"
#include "pp.h"
#include "stm32n6xx_hal.h"
#include "thread_config.h"
#include "timebase.h"
#include "tof_fusion.h"
#include "tracker.h"
#include "utils.h"
#include <stdbool.h>
#include <string.h>

/* Include library headers */
#include "app_postprocess.h"
#include "stai.h"

#if MDL_PP_TYPE == POSTPROCESS_OD_ST_YOLOX_UI
#include "od_st_yolox_pp_if.h"
#elif MDL_PP_TYPE == POSTPROCESS_OD_YOLO_V8_UI
#include "od_yolov8_pp_if.h"
#elif MDL_PP_TYPE == POSTPROCESS_OD_ST_YOLOD_UI
#include "od_yolo_d_pp_if.h"
#endif

/* Forward declarations for library functions */
int32_t app_postprocess_init(void *params_postprocess, stai_network_info *NN_Info);
int32_t app_postprocess_run(void *pInput[], int nb_input, void *pOutput, void *pInput_param);

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void pp_thread_entry(ULONG arg);

/* ============================================================================
 * Global State Variables
 * ============================================================================ */

/* Thread resources */
static struct {
  TX_THREAD thread;
  UCHAR stack[PP_THREAD_STACK_SIZE];
} pp_ctx;

/* Synchronization primitives */
static TX_EVENT_FLAGS_GROUP update_event_flags;

/* Double-buffered detection state */
static detection_info_t detection_info_buffers[2];
static volatile detection_info_t *detection_info_read_ptr = &detection_info_buffers[0];
static int write_buffer_idx = 1;

/* Postprocess parameters (type depends on selected model) */
#if MDL_PP_TYPE == POSTPROCESS_OD_ST_YOLOX_UI
static od_st_yolox_pp_static_param_t pp_params;
#elif MDL_PP_TYPE == POSTPROCESS_OD_YOLO_V8_UI
static od_yolov8_pp_static_param_t pp_params;
#elif MDL_PP_TYPE == POSTPROCESS_OD_ST_YOLOD_UI
static od_yolo_d_pp_static_param_t pp_params;
#endif

static trk_ctx_t trk_ctx;
static trk_tbox_t trk_tboxes[2 * DETECTION_MAX_BOXES];
static trk_dbox_t trk_dboxes[DETECTION_MAX_BOXES];
static volatile uint32_t runtime_cfg_req_seq;
static pp_runtime_config_t runtime_cfg_pending;
static volatile uint32_t runtime_cfg_active_seq;
static pp_runtime_config_t runtime_cfg_active;
static volatile bool runtime_cfg_ready;

static bool pp_runtime_config_is_valid(const pp_runtime_config_t *cfg) {
  if (cfg == NULL) {
    return false;
  }
  if (cfg->pp_conf_threshold < 0.0f || cfg->pp_conf_threshold > 1.0f) {
    return false;
  }
  if (cfg->pp_iou_threshold < 0.0f || cfg->pp_iou_threshold > 1.0f) {
    return false;
  }
  if (cfg->track_thresh < 0.0f || cfg->track_thresh > 1.0f) {
    return false;
  }
  if (cfg->det_thresh < 0.0f || cfg->det_thresh > 1.0f) {
    return false;
  }
  if (cfg->sim1_thresh < 0.0f || cfg->sim1_thresh > 1.0f) {
    return false;
  }
  if (cfg->sim2_thresh < 0.0f || cfg->sim2_thresh > 1.0f) {
    return false;
  }
  if (cfg->tlost_cnt == 0U || cfg->tlost_cnt > 1000U) {
    return false;
  }
  return true;
}

static void pp_apply_runtime_config(const pp_runtime_config_t *cfg) {
  APP_REQUIRE(cfg != NULL);
  pp_params.conf_threshold = cfg->pp_conf_threshold;
  pp_params.iou_threshold = cfg->pp_iou_threshold;
  trk_ctx.cfg.track_thresh = cfg->track_thresh;
  trk_ctx.cfg.det_thresh = cfg->det_thresh;
  trk_ctx.cfg.sim1_thresh = cfg->sim1_thresh;
  trk_ctx.cfg.sim2_thresh = cfg->sim2_thresh;
  trk_ctx.cfg.tlost_cnt = cfg->tlost_cnt;

  runtime_cfg_active_seq++;
  __DMB();
  runtime_cfg_active = *cfg;
  __DMB();
  runtime_cfg_active_seq++;
  runtime_cfg_ready = true;
}

/* ============================================================================
 * Thread Entry Points
 * ============================================================================ */

/**
 * @brief  Postprocess thread entry function
 *         Processes NN outputs and updates detection state
 * @param  arg: Thread argument (unused)
 */
static void pp_thread_entry(ULONG arg) {
  UNUSED(arg);

  bqueue_t *output_queue = NN_GetOutputQueue();
  int out_count = NN_GetOutputCount();
#if NN_OUT_NB > 1
  const uint32_t *out_sizes = NN_GetOutputSizes();
#endif
  od_pp_out_t pp_output;
  uint8_t *pp_input[NN_OUT_NB];
  uint32_t pp_start_cycles;
  uint32_t pp_end_cycles;
  uint32_t trk_start_cycles;
  uint32_t trk_end_cycles;
  nn_timing_t nn_timing;
  int ret;
  uint32_t applied_runtime_cfg_req_seq = 0;

  /* Initialize postprocess with network info (object detection model) */
  stai_network_info *nn_info = NN_GetNetworkInfo();
  APP_REQUIRE(nn_info != NULL);
  ret = app_postprocess_init(&pp_params, nn_info);
  APP_REQUIRE(ret == 0);

  const trk_conf_t trk_cfg = {
      .track_thresh = 0.25,
      .det_thresh = 0.8,
      .sim1_thresh = 0.8,
      .sim2_thresh = 0.5,
      .tlost_cnt = 30,
  };
  trk_init(&trk_ctx, (trk_conf_t *)&trk_cfg,
           2 * DETECTION_MAX_BOXES, trk_tboxes);

  const pp_runtime_config_t default_runtime_cfg = {
      .pp_conf_threshold = pp_params.conf_threshold,
      .pp_iou_threshold = pp_params.iou_threshold,
      .track_thresh = trk_cfg.track_thresh,
      .det_thresh = trk_cfg.det_thresh,
      .sim1_thresh = trk_cfg.sim1_thresh,
      .sim2_thresh = trk_cfg.sim2_thresh,
      .tlost_cnt = trk_cfg.tlost_cnt,
  };
  pp_apply_runtime_config(&default_runtime_cfg);

  while (1) {
    uint8_t *output_buffer;
    uint32_t seq_start;
    uint32_t seq_end;
    pp_runtime_config_t requested_runtime_cfg;
    tof_person_detection_t person_detections = {0};

    /* Wait for NN output */
    output_buffer = BQUE_GetReady(output_queue);
    APP_REQUIRE(output_buffer != NULL);

    /* Apply host-updated thresholds at a frame boundary. */
    do {
      seq_start = runtime_cfg_req_seq;
      if (seq_start & 1U) {
        continue;
      }
      __DMB();
      requested_runtime_cfg = runtime_cfg_pending;
      __DMB();
      seq_end = runtime_cfg_req_seq;
    } while ((seq_start & 1U) || (seq_start != seq_end));

    if (seq_end != 0U && seq_end != applied_runtime_cfg_req_seq) {
      if (pp_runtime_config_is_valid(&requested_runtime_cfg)) {
        pp_apply_runtime_config(&requested_runtime_cfg);
      }
      applied_runtime_cfg_req_seq = seq_end;
    }

    /* Calculate output buffer pointers */
    pp_input[0] = output_buffer;
#if NN_OUT_NB > 1
    for (int i = 1; i < out_count; i++) {
      pp_input[i] = pp_input[i - 1] + ALIGN_VALUE(out_sizes[i - 1], 32);
    }
#endif

    pp_output.pOutBuff = NULL;

    /* Run postprocessing */
    pp_start_cycles = DWT->CYCCNT;
    ret = app_postprocess_run((void **)pp_input, out_count, &pp_output, &pp_params);
    APP_REQUIRE(ret == 0);
    pp_end_cycles = DWT->CYCCNT;

    trk_start_cycles = DWT->CYCCNT;
    for (int i = 0; i < pp_output.nb_detect; i++) {
      trk_dboxes[i].cx = pp_output.pOutBuff[i].x_center;
      trk_dboxes[i].cy = pp_output.pOutBuff[i].y_center;
      trk_dboxes[i].w = pp_output.pOutBuff[i].width;
      trk_dboxes[i].h = pp_output.pOutBuff[i].height;
      trk_dboxes[i].conf = pp_output.pOutBuff[i].conf;
    }
    trk_update(&trk_ctx, pp_output.nb_detect, trk_dboxes);
    trk_end_cycles = DWT->CYCCNT;

    /* Get NN timing */
    NN_GetTiming(&nn_timing);

    /* Write to the inactive buffer */
    detection_info_t *write_buf = &detection_info_buffers[write_buffer_idx];
    write_buf->nb_detect = pp_output.nb_detect;
    person_detections.nb_persons = 0;
    person_detections.timestamp_ms = HAL_GetTick();
    for (int i = 0; i < pp_output.nb_detect && i < DETECTION_MAX_BOXES; i++) {
      write_buf->detects[i] = pp_output.pOutBuff[i];
      if (pp_output.pOutBuff[i].class_index == 0 &&
          person_detections.nb_persons < TOF_MAX_DETECTIONS) {
        tof_bbox_t *bbox = &person_detections.persons[person_detections.nb_persons++];
        bbox->x_center = pp_output.pOutBuff[i].x_center;
        bbox->y_center = pp_output.pOutBuff[i].y_center;
        bbox->width = pp_output.pOutBuff[i].width;
        bbox->height = pp_output.pOutBuff[i].height;
        bbox->conf = pp_output.pOutBuff[i].conf;
      }
    }
    write_buf->nn_period_us = nn_timing.nn_period_us;
    write_buf->inference_us = nn_timing.inference_us;
    write_buf->postprocess_us = CYCLES_TO_US(pp_end_cycles - pp_start_cycles);
    write_buf->tracker_us = CYCLES_TO_US(trk_end_cycles - trk_start_cycles);
    write_buf->frame_drops = nn_timing.frame_drops;
    write_buf->timestamp_ms = person_detections.timestamp_ms;

    write_buf->nb_tracked = 0;
    for (int i = 0; i < 2 * DETECTION_MAX_BOXES; i++) {
      if (!trk_tboxes[i].is_tracking || trk_tboxes[i].tlost_cnt) {
        continue;
      }
      tracked_box_t *tb = &write_buf->tracked[write_buf->nb_tracked];
      tb->x_center = (float)trk_tboxes[i].cx;
      tb->y_center = (float)trk_tboxes[i].cy;
      tb->width = (float)trk_tboxes[i].w;
      tb->height = (float)trk_tboxes[i].h;
      tb->id = trk_tboxes[i].id;
      write_buf->nb_tracked++;
      if (write_buf->nb_tracked >= DETECTION_MAX_BOXES) {
        break;
      }
    }

    /* Publish: ensure all writes to write_buf are visible before index swap. */
    __DMB();
    TOF_SetPersonDetections(&person_detections);
    detection_info_read_ptr = write_buf;
    __DMB();

    /* Switch to other buffer for next write */
    write_buffer_idx = write_buffer_idx ^ 1;

    /* Release output buffer */
    BQUE_PutFree(output_queue);

    /* Signal consumers (comm thread, overlay thread) */
    PP_SignalUpdate();
  }
}

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

/**
 * @brief  Signal that new detection results are available
 *         Called by postprocess after updating detection state
 */
void PP_SignalUpdate(void) {
  tx_event_flags_set(&update_event_flags, 0x01, TX_OR);
}

/**
 * @brief  Get pointer to detection info structure (lock-free, read-only)
 * @retval Pointer to detection_info_t (read-only, no lock needed)
 * @note   Uses double buffering for lock-free access
 */
detection_info_t *PP_GetInfo(void) {
  detection_info_t *ptr = (detection_info_t *)detection_info_read_ptr;
  __DMB();
  return ptr;
}

/**
 * @brief  Get pointer to update event flags group (for overlay thread)
 * @retval Pointer to event flags group
 */
TX_EVENT_FLAGS_GROUP *PP_GetUpdateEventFlags(void) {
  return &update_event_flags;
}

void PP_ThreadSuspend(void) {
  tx_thread_suspend(&pp_ctx.thread);
}

void PP_ThreadResume(void) {
  tx_thread_resume(&pp_ctx.thread);
}

bool PP_RequestRuntimeConfig(const pp_runtime_config_t *cfg) {
  if (!pp_runtime_config_is_valid(cfg)) {
    return false;
  }
  runtime_cfg_req_seq++;
  __DMB();
  runtime_cfg_pending = *cfg;
  __DMB();
  runtime_cfg_req_seq++;
  return true;
}

bool PP_GetRuntimeConfig(pp_runtime_config_t *out_cfg) {
  uint32_t seq_start;
  uint32_t seq_end;

  if (out_cfg == NULL || !runtime_cfg_ready) {
    return false;
  }

  do {
    seq_start = runtime_cfg_active_seq;
    if (seq_start & 1U) {
      continue;
    }
    __DMB();
    *out_cfg = runtime_cfg_active;
    __DMB();
    seq_end = runtime_cfg_active_seq;
  } while ((seq_start & 1U) || (seq_start != seq_end));
  return true;
}

/**
 * @brief  Initialize postprocessing module (thread, sync primitives)
 * @param  memory_ptr: ThreadX memory pool (unused, static allocation)
 */
void PP_ThreadStart(void) {
  UINT status;

  /* Initialize detection state buffers */
  memset(detection_info_buffers, 0, sizeof(detection_info_buffers));

  /* Initialize read pointer to first buffer */
  detection_info_read_ptr = &detection_info_buffers[0];
  write_buffer_idx = 1;
  runtime_cfg_req_seq = 0U;
  runtime_cfg_active_seq = 0U;
  runtime_cfg_ready = false;
  memset(&runtime_cfg_pending, 0, sizeof(runtime_cfg_pending));
  memset(&runtime_cfg_active, 0, sizeof(runtime_cfg_active));

  /* Create synchronization primitives */
  status = tx_event_flags_create(&update_event_flags, "detection_update");
  APP_REQUIRE(status == TX_SUCCESS);

  /* Create postprocess thread */
  status = tx_thread_create(&pp_ctx.thread, "postprocess",
                            pp_thread_entry, 0,
                            pp_ctx.stack, PP_THREAD_STACK_SIZE,
                            PP_THREAD_PRIORITY, PP_THREAD_PRIORITY,
                            TX_NO_TIME_SLICE, TX_AUTO_START);
  APP_REQUIRE(status == TX_SUCCESS);
}
