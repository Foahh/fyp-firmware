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
#include "thread_config.h"
#include "tof.h"
#include "tx_api.h"

/* ToFAlert.flags (protobuf); keep in sync with messages.proto */
#define TOF_PB_FLAG_ALERT (1u << 0)
#define TOF_PB_FLAG_STALE (1u << 1)

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
  df->sent_timestamp_ms = HAL_GetTick();
  df->frame_timestamp_ms = info->timestamp_ms;

  df->inference_us = info->inference_us;
  df->postprocess_us = info->postprocess_us;
  df->tracker_us = info->tracker_us;
  df->nn_period_us = info->nn_period_us;
  df->frame_drop_count = info->frame_drops;

  df->cpu_usage_percent = CPU_LoadGetUsageRatio() * 100.0f;

  int n = info->nb_detect;
  int max_det = (int)(sizeof(df->detections) / sizeof(df->detections[0]));
  if (n > max_det) {
    n = max_det;
  }
  df->detections_count = (pb_size_t)n;
  for (int i = 0; i < n; i++) {
    const od_pp_outBuffer_t *d = &info->detects[i];
    df->detections[i].x = d->x_center;
    df->detections[i].y = d->y_center;
    df->detections[i].w = d->width;
    df->detections[i].h = d->height;
    df->detections[i].score = d->conf;
    df->detections[i].class_id = (uint32_t)d->class_index;
  }

  int nt = info->nb_tracked;
  int max_trk = (int)(sizeof(df->tracks) / sizeof(df->tracks[0]));
  if (nt > max_trk) {
    nt = max_trk;
  }
  df->tracks_count = (pb_size_t)nt;
  for (int i = 0; i < nt; i++) {
    const tracked_box_t *tb = &info->tracked[i];
    df->tracks[i].x = tb->x_center;
    df->tracks[i].y = tb->y_center;
    df->tracks[i].w = tb->width;
    df->tracks[i].h = tb->height;
    df->tracks[i].track_id = tb->id;
  }

  const tof_alert_t *alert = TOF_GetAlert();
  const tof_depth_grid_t *grid = TOF_GetDepthGrid();
  df->has_tof = true;
  uint32_t tof_flags = 0;
  if (alert != NULL) {
    df->tof.person_mm_count = alert->nb_person_depths;
    for (int i = 0; i < alert->nb_person_depths; i++) {
      df->tof.person_mm[i] = alert->person_distances_mm[i];
    }
    if (alert->alert) {
      tof_flags |= TOF_PB_FLAG_ALERT;
    }
    if (alert->stale) {
      tof_flags |= TOF_PB_FLAG_STALE;
    }
  }
  df->tof.flags = tof_flags;
  if (grid != NULL && grid->valid) {
    df->tof.depth_mm_count = TOF_GRID_SIZE * TOF_GRID_SIZE;
    for (int r = 0; r < TOF_GRID_SIZE; r++) {
      for (int c = 0; c < TOF_GRID_SIZE; c++) {
        df->tof.depth_mm[r * TOF_GRID_SIZE + c] = (int32_t)grid->distance_mm[r][c];
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
