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
#include "timebase.h"
#include "tracker.h"
#include "utils.h"
#include <stdbool.h>
#include <string.h>

/* Include library headers */
#include "stai.h"

#if MDL_PP_TYPE == POSTPROCESS_OD_ST_YOLOX_UI
#include "od_st_yolox_pp_if.h"
#elif MDL_PP_TYPE == POSTPROCESS_OD_YOLO_V8_UI
#include "od_yolov8_pp_if.h"
#endif

/* Forward declarations for library functions */
int32_t app_postprocess_init(void *params_postprocess, stai_network_info *NN_Info);
int32_t app_postprocess_run(void *pInput[], int nb_input, void *pOutput, void *pInput_param);

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void pp_thread_entry(ULONG arg);

/* ============================================================================
 * Configuration Constants
 * ============================================================================ */

/* Thread configurations */
#define PP_THREAD_STACK_SIZE 4096
#define PP_THREAD_PRIORITY   4

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
#endif

static trk_ctx_t trk_ctx;
static trk_tbox_t trk_tboxes[2 * DETECTION_MAX_BOXES];
static trk_dbox_t trk_dboxes[DETECTION_MAX_BOXES];

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
  const uint32_t *out_sizes = NN_GetOutputSizes();
  int out_count = NN_GetOutputCount();
  od_pp_out_t pp_output;
  uint8_t *pp_input[NN_OUT_NB];
  uint32_t pp_start_cycles;
  uint32_t pp_end_cycles;
  uint32_t trk_start_cycles;
  uint32_t trk_end_cycles;
  nn_timing_t nn_timing;
  int ret;

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

  while (1) {
    uint8_t *output_buffer;

    /* Wait for NN output */
    output_buffer = BQUE_GetReady(output_queue);
    APP_REQUIRE(output_buffer != NULL);

    /* Calculate output buffer pointers */
    pp_input[0] = output_buffer;
    for (int i = 1; i < out_count; i++) {
      pp_input[i] = pp_input[i - 1] + ALIGN_VALUE(out_sizes[i - 1], 32);
    }

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
    for (int i = 0; i < pp_output.nb_detect && i < DETECTION_MAX_BOXES; i++) {
      write_buf->detects[i] = pp_output.pOutBuff[i];
    }
    write_buf->nn_period_us = nn_timing.nn_period_us;
    write_buf->inference_us = nn_timing.inference_us;
    write_buf->postprocess_us = CYCLES_TO_US(pp_end_cycles - pp_start_cycles);
    write_buf->tracker_us = CYCLES_TO_US(trk_end_cycles - trk_start_cycles);
    write_buf->frame_drops = nn_timing.frame_drops;
    write_buf->timestamp_ms = HAL_GetTick();

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
