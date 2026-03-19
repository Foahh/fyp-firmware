/**
 ******************************************************************************
 * @file    app_comm_log.c
 * @author  Long Liangmao
 * @brief   Periodic device-to-host reporting
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

#include "app_comm_log.h"
#include "app_comm_tx.h"
#include "app_error.h"
#include "app_pp.h"
#include "messages.pb.h"
#include "stm32n6xx_hal.h"
#include "tx_api.h"

/* ============================================================================
 * Configuration
 * ============================================================================ */

#define COMM_LOG_THREAD_STACK_SIZE 2048U
#define COMM_LOG_THREAD_PRIORITY   8

/* ============================================================================
 * Static resources
 * ============================================================================ */

static TX_THREAD comm_log_thread;
static UCHAR comm_log_thread_stack[COMM_LOG_THREAD_STACK_SIZE];

/* ============================================================================
 * Internal helpers
 * ============================================================================ */

static void comm_send_detection_result(const detection_info_t *info) {
  DeviceMessage msg = DeviceMessage_init_zero;
  msg.which_payload = DeviceMessage_detection_result_tag;

  DetectionResult *df = &msg.payload.detection_result;
  df->timestamp = HAL_GetTick();

  df->has_timing = true;
  df->timing.inference_ms = info->inference_ms;
  df->timing.postprocess_ms = info->postprocess_ms;
  df->timing.nn_period_ms = info->nn_period_ms;
  df->timing.frame_drops = info->frame_drops;

  int n = info->nb_detect;
  if (n > DETECTION_MAX_BOXES) {
    n = DETECTION_MAX_BOXES;
  }
  df->detections_count = (pb_size_t)n;
  for (int i = 0; i < n; i++) {
    const od_pp_outBuffer_t *d = &info->detects[i];
    df->detections[i].x_center = d->x_center;
    df->detections[i].y_center = d->y_center;
    df->detections[i].width = d->width;
    df->detections[i].height = d->height;
    df->detections[i].conf = d->conf;
    df->detections[i].class_index = d->class_index;
  }

  df->host_image_id = info->host_image_id;

  COM_TX_Send(&msg);
}

/* ============================================================================
 * Log thread entry
 * ============================================================================ */

static void comm_log_thread_entry(ULONG arg) {
  UNUSED(arg);

  TX_EVENT_FLAGS_GROUP *event_flags = PP_GetUpdateEventFlags();
  ULONG actual_flags;

  while (1) {
    UINT status = tx_event_flags_get(event_flags, 0x01, TX_OR_CLEAR,
                                     &actual_flags, TX_WAIT_FOREVER);
    if (status != TX_SUCCESS) {
      continue;
    }

    detection_info_t *info = PP_GetInfo();
    if (info != NULL) {
      comm_send_detection_result(info);
    }
  }
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void COM_Log_Thread_Start(void) {
  UINT status;

  status =
      tx_thread_create(&comm_log_thread, "comm_log", comm_log_thread_entry, 0,
                       comm_log_thread_stack, COMM_LOG_THREAD_STACK_SIZE,
                       COMM_LOG_THREAD_PRIORITY, COMM_LOG_THREAD_PRIORITY,
                       TX_NO_TIME_SLICE, TX_AUTO_START);
  APP_REQUIRE(status == TX_SUCCESS);
}
