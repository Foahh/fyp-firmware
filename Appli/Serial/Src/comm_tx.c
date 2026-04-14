/**
 ******************************************************************************
 * @file    app_comm_tx.c
 * @author  Long Liangmao
 * @brief   Communication TX encoding and send infrastructure
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

#include "comm_tx.h"
#include "build_timestamp.h"
#include "cam_config.h"
#include "cpu_load.h"
#include "error.h"
#include "init_clock.h"
#include "lcd_config.h"
#include "model_config.h"
#include "nn_config.h"
#include "pb_encode.h"
#include "power_mode.h"
#include "pp.h"
#include "stm32n6570_discovery.h"
#include "stm32n6xx_hal.h"
#include "thread_config.h"
#include "tof_fusion.h"
#include "tx_api.h"
#include <string.h>

_Static_assert(sizeof(MDL_DISPLAY_NAME) <= PROTO_DEVICE_INFO_MODEL_NAME_CAPACITY,
               "MDL_DISPLAY_NAME exceeds messages.proto DeviceInfo.model_name max_size");
_Static_assert(MDL_PP_CLASS_LABEL_COUNT <= PROTO_DEVICE_INFO_CLASS_LABEL_COUNT_CAPACITY,
               "MDL_PP_CLASS_LABELS exceeds messages.proto DeviceInfo.class_labels max_count");
_Static_assert(sizeof(MDL_PP_CLASS_LABEL_0) <= PROTO_DEVICE_INFO_CLASS_LABEL_CAPACITY,
               "MDL_PP_CLASS_LABELS entry exceeds messages.proto DeviceInfo.class_labels max_size");
_Static_assert(sizeof(BUILD_TIMESTAMP) <= PROTO_DEVICE_INFO_BUILD_TIMESTAMP_CAPACITY,
               "BUILD_TIMESTAMP exceeds messages.proto DeviceInfo.build_timestamp max_size");

/* TofAlert.flags (protobuf); keep in sync with messages.proto */
#define TOF_PB_FLAG_ALERT (1u << 0)
#define TOF_PB_FLAG_STALE (1u << 1)

/* ============================================================================
 * Static resources
 * ============================================================================ */

/* TX frame queue */
#define TX_FRAME_BUF_SIZE  (4 + DeviceMessage_size)
#define COM_TX_QUEUE_DEPTH 8U

static __NON_CACHEABLE uint8_t tx_frame_bufs[COM_TX_QUEUE_DEPTH][TX_FRAME_BUF_SIZE];
static uint16_t tx_frame_lens[COM_TX_QUEUE_DEPTH];
static uint8_t tx_enqueue_idx = 0;
static uint8_t tx_dequeue_idx = 0;

static struct {
  TX_THREAD thread;
  UCHAR stack[COMM_TX_THREAD_STACK_SIZE];
} comm_tx_ctx;

/* TX mutex for shared send helper */
static TX_MUTEX tx_mutex;

/* TX complete semaphore (signalled from HAL_UART_TxCpltCallback) */
static TX_SEMAPHORE tx_done_sem;
static TX_SEMAPHORE tx_free_sem;
static TX_SEMAPHORE tx_ready_sem;

/* Bounded waits keep transient UART faults from wedging the host link forever. */
#define COM_TX_DMA_START_RETRIES  3U
#define COM_TX_DMA_RETRY_TICKS    1U
#define COM_TX_DONE_TIMEOUT_TICKS 50U

/* ============================================================================
 * Public API
 * ============================================================================ */

static void comm_tx_thread_entry(ULONG arg);

void COM_TX_ThreadStart(void) {
  UINT status;

  status = tx_mutex_create(&tx_mutex, "comm_tx_mutex", TX_NO_INHERIT);
  APP_REQUIRE(status == TX_SUCCESS);

  status = tx_semaphore_create(&tx_done_sem, "comm_tx_done", 0);
  APP_REQUIRE(status == TX_SUCCESS);

  status = tx_semaphore_create(&tx_free_sem, "comm_tx_free", COM_TX_QUEUE_DEPTH);
  APP_REQUIRE(status == TX_SUCCESS);

  status = tx_semaphore_create(&tx_ready_sem, "comm_tx_ready", 0);
  APP_REQUIRE(status == TX_SUCCESS);

  tx_enqueue_idx = 0;
  tx_dequeue_idx = 0;
  memset(tx_frame_lens, 0, sizeof(tx_frame_lens));

  status =
      tx_thread_create(&comm_tx_ctx.thread, "comm_tx", comm_tx_thread_entry, 0,
                       comm_tx_ctx.stack, COMM_TX_THREAD_STACK_SIZE,
                       COMM_TX_THREAD_PRIORITY, COMM_TX_THREAD_PRIORITY,
                       TX_NO_TIME_SLICE, TX_AUTO_START);
  APP_REQUIRE(status == TX_SUCCESS);
}

void COM_TX_Send(const DeviceMessage *msg) {
  if (tx_semaphore_get(&tx_free_sem, TX_NO_WAIT) != TX_SUCCESS) {
    // BSP_LED_Toggle(LED_GREEN);
    return;
  }

  UINT status = tx_mutex_get(&tx_mutex, TX_WAIT_FOREVER);
  APP_REQUIRE(status == TX_SUCCESS);

  uint8_t slot_idx = tx_enqueue_idx;
  uint8_t *buf = tx_frame_bufs[slot_idx];

  pb_ostream_t stream = pb_ostream_from_buffer(buf + 4, TX_FRAME_BUF_SIZE - 4);
  bool ok = pb_encode(&stream, DeviceMessage_fields, msg);
  if (!ok) {
    tx_mutex_put(&tx_mutex);
    tx_semaphore_put(&tx_free_sem);
    APP_REQUIRE(ok);
  }

  uint32_t len = (uint32_t)stream.bytes_written;
  buf[0] = (uint8_t)(len);
  buf[1] = (uint8_t)(len >> 8);
  buf[2] = (uint8_t)(len >> 16);
  buf[3] = (uint8_t)(len >> 24);

  if (4 + len > UINT16_MAX) {
    tx_mutex_put(&tx_mutex);
    tx_semaphore_put(&tx_free_sem);
    APP_REQUIRE(4 + len <= UINT16_MAX);
  }

  tx_frame_lens[slot_idx] = (uint16_t)(4U + len);
  tx_enqueue_idx = (uint8_t)((tx_enqueue_idx + 1U) % COM_TX_QUEUE_DEPTH);
  tx_mutex_put(&tx_mutex);
  tx_semaphore_put(&tx_ready_sem);
}

void COM_Send_DeviceInfo(void) {
  DeviceMessage msg = DeviceMessage_init_zero;
  pp_runtime_config_t pp_cfg;
  msg.which_payload = DeviceMessage_device_info_tag;

  DeviceInfo *di = &msg.payload.device_info;
  di->display_width_px = LCD_WIDTH;
  di->display_height_px = LCD_HEIGHT;
  di->letterbox_width_px = DISPLAY_LETTERBOX_WIDTH;
  di->letterbox_height_px = DISPLAY_LETTERBOX_HEIGHT;
  di->nn_width_px = NN_WIDTH;
  di->nn_height_px = NN_HEIGHT;
  di->nn_input_size_bytes = (uint32_t)(NN_WIDTH * NN_HEIGHT * NN_BPP);

  di->power_mode = POWER_MODE;
  di->mcu_freq_mhz = AppClock_GetCpuFreqMHz();
  di->npu_freq_mhz = AppClock_GetNpuFreqMHz();

#ifdef SNAPSHOT_MODE
  di->camera_fps = 0;
#else
  di->camera_fps = CAMERA_FPS;
#endif

  /* Model name */
  strncpy(di->model_name, MDL_DISPLAY_NAME, sizeof(di->model_name) - 1);
  di->model_name[sizeof(di->model_name) - 1] = '\0';

  /* Class labels */
  int n_labels = (int)(sizeof(MDL_PP_CLASS_LABELS) / sizeof(MDL_PP_CLASS_LABELS[0]));
  int max_class_labels = (int)(sizeof(di->class_labels) / sizeof(di->class_labels[0]));
  if (n_labels > max_class_labels) {
    n_labels = max_class_labels;
  }
  di->class_labels_count = (pb_size_t)n_labels;
  for (int i = 0; i < n_labels; i++) {
    strncpy(di->class_labels[i], MDL_PP_CLASS_LABELS[i], sizeof(di->class_labels[i]) - 1);
    di->class_labels[i][sizeof(di->class_labels[i]) - 1] = '\0';
  }

  /* Build timestamp */
  strncpy(di->build_timestamp, BUILD_TIMESTAMP, sizeof(di->build_timestamp) - 1);
  di->build_timestamp[sizeof(di->build_timestamp) - 1] = '\0';

  if (PP_GetRuntimeConfig(&pp_cfg)) {
    di->pp_conf_threshold = pp_cfg.pp_conf_threshold;
    di->pp_iou_threshold = pp_cfg.pp_iou_threshold;
    di->track_thresh = pp_cfg.track_thresh;
    di->det_thresh = pp_cfg.det_thresh;
    di->sim1_thresh = pp_cfg.sim1_thresh;
    di->sim2_thresh = pp_cfg.sim2_thresh;
    di->tlost_cnt = pp_cfg.tlost_cnt;
  } else {
    di->pp_conf_threshold = MDL_PP_CONF_THRESHOLD;
    di->pp_iou_threshold = MDL_PP_IOU_THRESHOLD;
    di->track_thresh = 0.25f;
    di->det_thresh = 0.8f;
    di->sim1_thresh = 0.8f;
    di->sim2_thresh = 0.5f;
    di->tlost_cnt = 30U;
  }
  di->alert_threshold_mm = TOF_GetAlertThreshold();

  di->timestamp_ms = HAL_GetTick();

  COM_TX_Send(&msg);
}

void COM_Send_Ack(bool success) {
  DeviceMessage msg = DeviceMessage_init_zero;
  msg.which_payload = DeviceMessage_ack_tag;
  msg.payload.ack.success = success;
  msg.payload.ack.timestamp_ms = HAL_GetTick();

  COM_TX_Send(&msg);
}

void COM_Send_TraceXChunk(uint32_t offset, uint32_t total_size, const uint8_t *data, uint32_t data_len, bool done) {
  DeviceMessage msg = DeviceMessage_init_zero;
  TraceXChunk *chunk;

  if (data == NULL || data_len == 0U || data_len > sizeof(msg.payload.tracex_chunk.data.bytes)) {
    return;
  }

  msg.which_payload = DeviceMessage_tracex_chunk_tag;
  chunk = &msg.payload.tracex_chunk;
  chunk->offset_bytes = offset;
  chunk->total_size_bytes = total_size;
  chunk->data.size = (pb_size_t)data_len;
  memcpy(chunk->data.bytes, data, data_len);
  chunk->done = done;
  chunk->timestamp_ms = HAL_GetTick();

  COM_TX_Send(&msg);
}

void COM_Send_TofResult(void) {
  DeviceMessage msg = DeviceMessage_init_zero;
  TofResult *tof_result;
  const tof_depth_grid_t *grid;
  rcu_read_token_t grid_token = {0};

  msg.which_payload = DeviceMessage_tof_result_tag;
  tof_result = &msg.payload.tof_result;
  tof_result->timestamp_ms = 0U;

  grid = TOF_AcquireDepthGrid(&grid_token);

  if (grid == NULL || !grid->valid) {
    TOF_ReleaseDepthGrid(&grid_token);
    return;
  }

  tof_result->timestamp_ms = grid->timestamp_ms;
  tof_result->tof_period_us = grid->tof_period_us;
  tof_result->depth_mm_count = PROTO_TOF_RESULT_MAX_DEPTH_MM;
  tof_result->range_sigma_mm_count = PROTO_TOF_RESULT_MAX_RANGE_SIGMA_MM;
  tof_result->signal_per_spad_count = PROTO_TOF_RESULT_MAX_SIGNAL_PER_SPAD;
  for (int r = 0; r < TOF_GRID_SIZE; r++) {
    for (int c = 0; c < TOF_GRID_SIZE; c++) {
      tof_result->depth_mm[r * TOF_GRID_SIZE + c] =
          (int32_t)grid->distance_mm[r][c];
      tof_result->range_sigma_mm[r * TOF_GRID_SIZE + c] =
          (uint32_t)grid->range_sigma_mm[r][c];
      tof_result->signal_per_spad[r * TOF_GRID_SIZE + c] =
          grid->signal_per_spad[r][c];
    }
  }

  TOF_ReleaseDepthGrid(&grid_token);
  COM_TX_Send(&msg);
}

void COM_Send_CpuUsageSample(void) {
  DeviceMessage msg = DeviceMessage_init_zero;
  CpuUsageSample *cpu_usage_sample;
  cpu_load_sample_t sample;

  if (!CPU_LoadGetLatestSample(&sample)) {
    return;
  }

  msg.which_payload = DeviceMessage_cpu_usage_sample_tag;
  cpu_usage_sample = &msg.payload.cpu_usage_sample;
  cpu_usage_sample->timestamp_ms = sample.timestamp_ms;
  cpu_usage_sample->cpu_usage_percent = sample.usage_ratio * 100.0f;

  COM_TX_Send(&msg);
}

void COM_Send_TofAlertResult(void) {
  DeviceMessage msg = DeviceMessage_init_zero;
  TofAlertResult *tof_alert_result;
  const tof_alert_t *alert;
  uint32_t tof_flags = 0;
  rcu_read_token_t alert_token = {0};

  msg.which_payload = DeviceMessage_tof_alert_result_tag;
  tof_alert_result = &msg.payload.tof_alert_result;

  alert = TOF_AcquireAlert(&alert_token);
  if (alert == NULL) {
    TOF_ReleaseAlert(&alert_token);
    return;
  }

  tof_alert_result->timestamp_ms = HAL_GetTick();
  tof_alert_result->fusion_period_us = alert->fusion_period_us;

  {
    uint8_t person_depth_count = 0U;
    for (uint8_t i = 0; i < TOF_MAX_DETECTIONS; i++) {
      if (!alert->person_depth_valid[i]) {
        continue;
      }
      if (person_depth_count >= PROTO_TOF_ALERT_MAX_PERSON_MM) {
        break;
      }

      tof_alert_result->person_distances[person_depth_count].track_id =
          alert->person_track_ids[i];
      tof_alert_result->person_distances[person_depth_count].distance_mm =
          alert->person_distances_mm[i];
      person_depth_count++;
    }
    tof_alert_result->person_distances_count = person_depth_count;
  }

  if (alert->alert) {
    tof_flags |= TOF_PB_FLAG_ALERT;
  }
  if (alert->stale) {
    tof_flags |= TOF_PB_FLAG_STALE;
  }
  tof_alert_result->flags = tof_flags;

  TOF_ReleaseAlert(&alert_token);
  COM_TX_Send(&msg);
}

/* ============================================================================
 * HAL UART TX complete callback (ISR context)
 * ============================================================================ */

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == hcom_uart[COM1].Instance) {
    tx_semaphore_put(&tx_done_sem);
  }
}

static void comm_tx_thread_entry(ULONG arg) {
  UNUSED(arg);

  while (1) {
    HAL_StatusTypeDef hal_status = HAL_ERROR;
    uint8_t slot_idx;
    uint8_t *buf;
    uint16_t len;

    tx_semaphore_get(&tx_ready_sem, TX_WAIT_FOREVER);

    slot_idx = tx_dequeue_idx;
    buf = tx_frame_bufs[slot_idx];
    len = tx_frame_lens[slot_idx];

    /* Drop any stale completion signal before starting a new DMA transfer. */
    while (tx_semaphore_get(&tx_done_sem, TX_NO_WAIT) == TX_SUCCESS) {
    }

    for (uint32_t attempt = 0; attempt < COM_TX_DMA_START_RETRIES; ++attempt) {
      hal_status = HAL_UART_Transmit_DMA(&hcom_uart[COM1], buf, len);
      if (hal_status == HAL_OK) {
        break;
      }
      tx_thread_sleep(COM_TX_DMA_RETRY_TICKS);
    }

    if (hal_status == HAL_OK) {
      UINT status = tx_semaphore_get(&tx_done_sem, COM_TX_DONE_TIMEOUT_TICKS);
      if (status != TX_SUCCESS) {
        HAL_UART_AbortTransmit(&hcom_uart[COM1]);
      }
    }

    tx_frame_lens[slot_idx] = 0U;
    tx_dequeue_idx = (uint8_t)((tx_dequeue_idx + 1U) % COM_TX_QUEUE_DEPTH);
    tx_semaphore_put(&tx_free_sem);
  }
}
