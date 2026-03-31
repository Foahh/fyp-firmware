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

#include "comm_log.h"
#include "comm_tx.h"
#include "cpu_load.h"
#include "error.h"
#include "messages.pb.h"
#include "pp.h"
#include "stm32n6xx_hal.h"
#include "tof.h"
#include "tx_api.h"

/* ============================================================================
 * Configuration
 * ============================================================================ */

#define COMM_LOG_THREAD_STACK_SIZE 2048U
#define COMM_LOG_THREAD_PRIORITY   8

/* ============================================================================
 * Static resources
 * ============================================================================ */

/* Thread resources */
static struct {
  TX_THREAD thread;
  UCHAR stack[COMM_LOG_THREAD_STACK_SIZE];
} comm_log_ctx;

/* ============================================================================
 * Internal helpers
 * ============================================================================ */

static void comm_send_detection_result(const detection_info_t *info) {
  static DeviceMessage msg;
  msg = (DeviceMessage)DeviceMessage_init_zero;
  msg.which_payload = DeviceMessage_detection_result_tag;

  DetectionResult *df = &msg.payload.detection_result;
  df->timestamp_ms = HAL_GetTick();
  df->detection_timestamp_ms = info->timestamp_ms;

  df->has_timing = true;
  df->timing.inference_us = info->inference_us;
  df->timing.postprocess_us = info->postprocess_us;
  df->timing.tracker_us = info->tracker_us;
  df->timing.nn_period_us = info->nn_period_us;
  df->timing.frame_drop_count = info->frame_drops;

  df->cpu_usage_percent = CPU_LoadGetUsageRatio() * 100.0f;

  int n = info->nb_detect;
  if (n > DETECTION_MAX_BOXES) {
    n = DETECTION_MAX_BOXES;
  }
  df->detections_count = (pb_size_t)n;
  for (int i = 0; i < n; i++) {
    const od_pp_outBuffer_t *d = &info->detects[i];
    df->detections[i].x_center_norm = d->x_center;
    df->detections[i].y_center_norm = d->y_center;
    df->detections[i].width_norm = d->width;
    df->detections[i].height_norm = d->height;
    df->detections[i].confidence_ratio = d->conf;
    df->detections[i].class_index = d->class_index;
  }

  int nt = info->nb_tracked;
  if (nt > DETECTION_MAX_BOXES) {
    nt = DETECTION_MAX_BOXES;
  }
  df->tracked_boxes_count = (pb_size_t)nt;
  for (int i = 0; i < nt; i++) {
    const tracked_box_t *tb = &info->tracked[i];
    df->tracked_boxes[i].x_center_norm = tb->x_center;
    df->tracked_boxes[i].y_center_norm = tb->y_center;
    df->tracked_boxes[i].width_norm = tb->width;
    df->tracked_boxes[i].height_norm = tb->height;
    df->tracked_boxes[i].track_id = tb->id;
  }

  const tof_alert_t *alert = TOF_GetAlert();
  const tof_depth_grid_t *grid = TOF_GetDepthGrid();
  df->has_tof = true;
  if (alert != NULL) {
    df->tof.hand_distance_mm = alert->hand_distance_mm;
    df->tof.hazard_distance_mm = alert->hazard_distance_mm;
    df->tof.alert = alert->alert;
    df->tof.distance_3d_mm = alert->distance_3d_mm;
    df->tof.stale = alert->stale;
  }
  if (grid != NULL && grid->valid) {
    df->tof.depth_grid_mm_count = TOF_GRID_SIZE * TOF_GRID_SIZE;
    for (int r = 0; r < TOF_GRID_SIZE; r++) {
      for (int c = 0; c < TOF_GRID_SIZE; c++) {
        df->tof.depth_grid_mm[r * TOF_GRID_SIZE + c] = grid->distance_mm[r][c];
      }
    }
  }

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

void COM_Log_ThreadStart(void) {
  UINT status;

  status =
      tx_thread_create(&comm_log_ctx.thread, "comm_log", comm_log_thread_entry, 0,
                       comm_log_ctx.stack, COMM_LOG_THREAD_STACK_SIZE,
                       COMM_LOG_THREAD_PRIORITY, COMM_LOG_THREAD_PRIORITY,
                       TX_NO_TIME_SLICE, TX_AUTO_START);
  APP_REQUIRE(status == TX_SUCCESS);
}
