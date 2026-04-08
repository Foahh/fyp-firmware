/**
 ******************************************************************************
 * @file    tof_fusion.c
 * @author  Long Liangmao
 * @brief   Person-distance fusion thread: NN + ToF depth grid, alert, LED.
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

#include "tof_fusion.h"
#include "error.h"
#include "stm32n6570_discovery.h"
#include "stm32n6xx_hal.h"
#include "thread_config.h"
#include "timebase.h"
#include "tx_api.h"

#include <string.h>

/* ============================================================================
 * Configuration
 * ============================================================================ */

/* Depth data staleness threshold */
#define TOF_STALENESS_MS 500

/* FOV calibration: ToF FOV mapped to NN normalised [0,1] coordinates.
 * These are approximate and should be refined with physical calibration. */
#define TOF_NN_X0 0.0f
#define TOF_NN_Y0 -0.10f
#define TOF_NN_X1 1.0f
#define TOF_NN_Y1 0.90f

/* ============================================================================
 * Thread resources
 * ============================================================================ */

static struct {
  TX_THREAD thread;
  UCHAR stack[TOF_FUSION_THREAD_STACK_SIZE];
} tof_fusion_ctx;

#define TOF_FUSION_EVT_DEPTH_READY  0x1U
#define TOF_FUSION_EVT_PERSON_READY 0x2U

static TX_EVENT_FLAGS_GROUP tof_fusion_events;
static TX_EVENT_FLAGS_GROUP tof_alert_update_event_flags;

/* ============================================================================
 * Fusion state
 * ============================================================================ */

/* RCU-published alert state */
static tof_alert_t alerts[RCU_BUFFER_SLOT_COUNT];
static rcu_buffer_t alert_rcu;

/* Configurable alert threshold (nearest person distance) */
static uint32_t alert_threshold_mm = TOF_DEFAULT_ALERT_THRESHOLD_MM;

/* Cached person detections published by postprocess */
static tof_person_detection_t person_detection_buffers[RCU_BUFFER_SLOT_COUNT];
static rcu_buffer_t person_detection_rcu;

/* ============================================================================
 * Depth extraction helper
 * ============================================================================ */

/**
 * @brief  Extract the minimum valid depth (mm) from the ToF grid cells that
 *         overlap a normalised bounding box.
 * @param  grid       Current depth grid
 * @param  bbox       Bounding box in NN normalised [0,1] coords
 * @param  out_mm     Output: minimum depth in mm (only written when valid)
 * @retval 1 if at least one valid depth sample was found, 0 otherwise
 */
static uint8_t tof_extract_depth(const tof_depth_grid_t *grid,
                                 const tof_bbox_t *bbox, uint32_t *out_mm) {
  float hw = bbox->width * 0.5f;
  float hh = bbox->height * 0.5f;

  /* Bbox edges in NN coords */
  float bx0 = bbox->x_center - hw;
  float by0 = bbox->y_center - hh;
  float bx1 = bbox->x_center + hw;
  float by1 = bbox->y_center + hh;

  /* Map NN coords → ToF grid indices [0..7] */
  float sx = (float)TOF_GRID_SIZE / (TOF_NN_X1 - TOF_NN_X0);
  float sy = (float)TOF_GRID_SIZE / (TOF_NN_Y1 - TOF_NN_Y0);

  int gx0 = (int)((bx0 - TOF_NN_X0) * sx);
  int gy0 = (int)((by0 - TOF_NN_Y0) * sy);
  int gx1 = (int)((bx1 - TOF_NN_X0) * sx);
  int gy1 = (int)((by1 - TOF_NN_Y0) * sy);

  /* Clamp to grid bounds */
  if (gx0 < 0) {
    gx0 = 0;
  }
  if (gy0 < 0) {
    gy0 = 0;
  }
  if (gx1 >= TOF_GRID_SIZE) {
    gx1 = TOF_GRID_SIZE - 1;
  }
  if (gy1 >= TOF_GRID_SIZE) {
    gy1 = TOF_GRID_SIZE - 1;
  }
  if (gx0 > gx1 || gy0 > gy1) {
    return 0;
  }

  uint32_t min_d = UINT32_MAX;
  uint8_t found = 0;

  for (int gy = gy0; gy <= gy1; gy++) {
    for (int gx = gx0; gx <= gx1; gx++) {
      uint8_t st = grid->status[gy][gx];
      if (st != 5 && st != 9) {
        continue;
      }
      int16_t d = grid->distance_mm[gy][gx];
      if (d <= 0) {
        continue;
      }
      found = 1;
      if ((uint32_t)d < min_d) {
        min_d = (uint32_t)d;
      }
    }
  }

  if (found) {
    *out_mm = min_d;
  }
  return found;
}

/* ============================================================================
 * Fusion logic
 * ============================================================================ */

/**
 * @brief  Run person-distance fusion with temporal pairing.
 *
 *         Pairs the latest NN detection with the depth grid, checking the
 *         timestamp delta is within FUSION_MAX_DT_MS. Depth is sampled for
 *         each detected person bbox; the alert is asserted when the nearest
 *         sampled person is within the configured threshold.
 *
 * @param  grid  Current depth grid snapshot
 * @param  out   Alert state to populate
 */
static void tof_run_fusion(const tof_depth_grid_t *grid,
                           const tof_person_detection_t *det, tof_alert_t *out) {

  memset(out, 0, sizeof(*out));

  /* Check depth grid staleness */
  uint32_t now = HAL_GetTick();
  if (!grid->valid || (now - grid->timestamp_ms) > TOF_STALENESS_MS) {
    out->stale = 1;
    return;
  }

  if (det == NULL || det->timestamp_ms == 0U) {
    out->stale = 1;
    return;
  }

  /* Temporal pairing: check PP detection timestamp vs depth grid */
  {
    uint32_t dt = (grid->timestamp_ms > det->timestamp_ms)
                      ? (grid->timestamp_ms - det->timestamp_ms)
                      : (det->timestamp_ms - grid->timestamp_ms);
    if (dt > FUSION_MAX_DT_MS) {
      out->stale = 1;
      return;
    }
  }

  uint32_t person_min = UINT32_MAX;
  for (int i = 0; i < det->nb_persons; i++) {
    uint32_t d;
    if (tof_extract_depth(grid, &det->persons[i], &d)) {
      out->person_distances_mm[i] = d;
      out->person_depth_valid[i] = 1U;
      out->nb_person_depths++;
      out->has_person_depth = 1;
      if (d < person_min) {
        person_min = d;
      }
    }
  }
  if (out->has_person_depth) {
    out->person_distance_mm = person_min;
    out->alert = (person_min < alert_threshold_mm) ? 1U : 0U;
  }
}

/* ============================================================================
 * Thread entry
 * ============================================================================ */

static void tof_fusion_thread_entry(ULONG arg) {
  UNUSED(arg);

  while (1) {
    ULONG actual_flags;
    uint8_t alert_write_idx = 0U;
    bool have_alert_slot;
    rcu_read_token_t grid_token = {0};
    rcu_read_token_t det_token = {0};
    const tof_depth_grid_t *grid;
    const tof_person_detection_t *det;
    tof_alert_t local_alert;
    tof_alert_t *alert_out = &local_alert;

    tx_event_flags_get(&tof_fusion_events,
                       TOF_FUSION_EVT_DEPTH_READY | TOF_FUSION_EVT_PERSON_READY,
                       TX_OR_CLEAR, &actual_flags, TX_WAIT_FOREVER);

    grid = TOF_AcquireDepthGrid(&grid_token);
    det = TOF_AcquirePersonDetections(&det_token);

    have_alert_slot = RCU_WriteReserve(&alert_rcu, &alert_write_idx);
    if (have_alert_slot) {
      alert_out = &alerts[alert_write_idx];
    }

    tof_run_fusion(grid, det, alert_out);
    TOF_ReleasePersonDetections(&det_token);
    TOF_ReleaseDepthGrid(&grid_token);

    uint8_t fired = alert_out->alert;
    if (have_alert_slot) {
      RCU_WritePublish(&alert_rcu, alert_write_idx);
      tx_event_flags_set(&tof_alert_update_event_flags, 0x01, TX_OR);
    }

    if (fired) {
      BSP_LED_On(LED_RED);
    } else {
      BSP_LED_Off(LED_RED);
    }
  }
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void TOF_FUSION_ThreadStart(void) {
  UINT status = tx_event_flags_create(&tof_fusion_events, "tof_fusion");
  APP_REQUIRE(status == TX_SUCCESS);

  status =
      tx_event_flags_create(&tof_alert_update_event_flags, "tof_alert_update");
  APP_REQUIRE(status == TX_SUCCESS);

  memset(alerts, 0, sizeof(alerts));
  memset(person_detection_buffers, 0, sizeof(person_detection_buffers));
  RCU_BufferInit(&alert_rcu);
  RCU_BufferInit(&person_detection_rcu);

  status = tx_thread_create(&tof_fusion_ctx.thread, "tof_fusion",
                            tof_fusion_thread_entry, 0, tof_fusion_ctx.stack,
                            TOF_FUSION_THREAD_STACK_SIZE,
                            TOF_FUSION_THREAD_PRIORITY,
                            TOF_FUSION_THREAD_PRIORITY, TX_NO_TIME_SLICE,
                            TX_AUTO_START);
  APP_REQUIRE(status == TX_SUCCESS);
}

void TOF_FUSION_NotifyDepthReady(void) {
  tx_event_flags_set(&tof_fusion_events, TOF_FUSION_EVT_DEPTH_READY, TX_OR);
}

void TOF_FUSION_Stop(void) {
  tx_thread_suspend(&tof_fusion_ctx.thread);
}

void TOF_FUSION_Resume(void) {
  tx_thread_resume(&tof_fusion_ctx.thread);
}

const tof_alert_t *TOF_AcquireAlert(rcu_read_token_t *token) {
  uint8_t slot_idx;

  if (!RCU_ReadAcquire(&alert_rcu, token, &slot_idx)) {
    return NULL;
  }

  return &alerts[slot_idx];
}

void TOF_ReleaseAlert(rcu_read_token_t *token) {
  RCU_ReadRelease(&alert_rcu, token);
}

TX_EVENT_FLAGS_GROUP *TOF_GetAlertUpdateEventFlags(void) {
  return &tof_alert_update_event_flags;
}

void TOF_SetAlertThreshold(uint32_t threshold_mm) {
  alert_threshold_mm = threshold_mm;
}

void TOF_SetPersonDetections(const tof_person_detection_t *detections) {
  uint8_t write_idx;

  APP_REQUIRE(detections != NULL);

  if (!RCU_WriteReserve(&person_detection_rcu, &write_idx)) {
    return;
  }

  person_detection_buffers[write_idx] = *detections;
  RCU_WritePublish(&person_detection_rcu, write_idx);
  tx_event_flags_set(&tof_fusion_events, TOF_FUSION_EVT_PERSON_READY, TX_OR);
}

const tof_person_detection_t *TOF_AcquirePersonDetections(rcu_read_token_t *token) {
  uint8_t slot_idx;

  if (!RCU_ReadAcquire(&person_detection_rcu, token, &slot_idx)) {
    return NULL;
  }

  return &person_detection_buffers[slot_idx];
}

void TOF_ReleasePersonDetections(rcu_read_token_t *token) {
  RCU_ReadRelease(&person_detection_rcu, token);
}
