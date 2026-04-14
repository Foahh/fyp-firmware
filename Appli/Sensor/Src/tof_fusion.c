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

/* FOV calibration: ToF FOV mapped to NN normalised [0,1] coordinates.
 * These are approximate and should be refined with physical calibration. */
#define TOF_NN_X0 0.0f
#define TOF_NN_Y0 -0.10f
#define TOF_NN_X1 1.0f
#define TOF_NN_Y1 0.90f

/* Torso ROI: shrink bbox to center 50% of width and middle 50% of height
 * (25%-75% of full box height). Avoids floor cells at the feet and background
 * cells near the top/sides. */
#define TOF_TORSO_HW_FACTOR 0.25f /* half-width = 25% of full bbox width */
#define TOF_TORSO_HH_FACTOR 0.25f /* half-height = 25% of full bbox height */

/* Cell quality gate: reject only when BOTH sigma is high AND signal is weak.
 * High sigma + strong signal → real low-reflectance target (dark clothing); keep.
 * High sigma + weak signal  → likely multipath or noise; reject.
 * sigma == 0 means field unavailable; those cells are kept unconditionally. */
#define TOF_SIGMA_REJECT_MM  25U /* mm  — threshold above which sigma is "high" */
#define TOF_SIGNAL_KEEP_KCPS 30U /* kcps/spad — min signal to trust a high-sigma cell */

/* EMA smoothing per tracked person.  alpha=0.3 weights raw:filtered ≈ 30:70. */
#define TOF_EMA_ALPHA      0.3f
#define TOF_EMA_MAX_MISSED 5 /* evict a track after this many consecutive misses */

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

/* Per-track EMA state.  track_id == 0 means the slot is free. */
typedef struct {
  uint32_t track_id;
  float filtered_mm;
  uint8_t missed;
} tof_track_ema_t;

static tof_track_ema_t ema_table[TOF_MAX_DETECTIONS];
static volatile uint8_t tof_fusion_timing_reset_pending = 1U;

/* ============================================================================
 * Depth extraction helper
 * ============================================================================ */

/**
 * @brief  Extract a robust depth estimate (mm) from the ToF grid cells that
 *         overlap a torso-centred inner ROI of the bounding box.
 *
 *         Uses the centre 50% of the bbox in both axes (25%-75% height,
 *         centre ±25% width) to avoid floor and background cells.  Valid cell
 *         depths are insertion-sorted and the 25th-percentile value is
 *         returned — more stable than min() while still biased toward the
 *         near side of the person.
 *
 * @param  grid    Current depth grid
 * @param  bbox    Bounding box in NN normalised [0,1] coords
 * @param  out_mm  Output: 25th-percentile depth in mm (written only when valid)
 * @retval 1 if at least one valid depth sample was found, 0 otherwise
 */
static uint8_t tof_extract_depth(const tof_depth_grid_t *grid,
                                 const tof_bbox_t *bbox, uint32_t *out_mm) {
  /* Torso ROI: centre 50% width × centre 50% height */
  float hw = bbox->width * TOF_TORSO_HW_FACTOR;
  float hh = bbox->height * TOF_TORSO_HH_FACTOR;

  float bx0 = bbox->x_center - hw;
  float bx1 = bbox->x_center + hw;
  float by0 = bbox->y_center - hh;
  float by1 = bbox->y_center + hh;

  /* Map NN coords → ToF grid indices [0..TOF_GRID_SIZE-1] */
  float sx = (float)TOF_GRID_SIZE / (TOF_NN_X1 - TOF_NN_X0);
  float sy = (float)TOF_GRID_SIZE / (TOF_NN_Y1 - TOF_NN_Y0);

  int gx0 = (int)((bx0 - TOF_NN_X0) * sx);
  int gy0 = (int)((by0 - TOF_NN_Y0) * sy);
  int gx1 = (int)((bx1 - TOF_NN_X0) * sx);
  int gy1 = (int)((by1 - TOF_NN_Y0) * sy);

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

  /* Collect valid depths, kept sorted via insertion sort (max 64 cells). */
  uint32_t depths[TOF_GRID_SIZE * TOF_GRID_SIZE];
  uint8_t n = 0;

  for (int gy = gy0; gy <= gy1; gy++) {
    for (int gx = gx0; gx <= gx1; gx++) {
      uint8_t st = grid->status[gy][gx];
      if (st != 5 && st != 9) {
        continue;
      }
      uint16_t sigma = grid->range_sigma_mm[gy][gx];
      if (sigma > 0U && sigma >= TOF_SIGMA_REJECT_MM &&
          grid->signal_per_spad[gy][gx] < TOF_SIGNAL_KEEP_KCPS) {
        continue;
      }
      int16_t d = grid->distance_mm[gy][gx];
      if (d <= 0) {
        continue;
      }
      uint32_t val = (uint32_t)d;
      uint8_t j = n;
      while (j > 0 && depths[j - 1] > val) {
        depths[j] = depths[j - 1];
        j--;
      }
      depths[j] = val;
      n++;
    }
  }

  if (n == 0) {
    return 0;
  }

  /* 25th-percentile: index n/4 into the sorted array. */
  *out_mm = depths[n / 4];
  return 1;
}

/* ============================================================================
 * Fusion logic
 * ============================================================================ */

/**
 * @brief  Run person-distance fusion with temporal pairing and per-track EMA.
 *
 *         Pairs the latest NN detection with the depth grid, checking the
 *         timestamp delta is within FUSION_MAX_DT_MS.  For each person bbox
 *         a torso-centred ROI is projected onto the 8x8 grid and the 25th-
 *         percentile valid cell depth is taken as the raw estimate.  The raw
 *         estimate is then smoothed via per-track EMA (alpha=TOF_EMA_ALPHA) to
 *         suppress frame-to-frame jitter.  The alert fires when the nearest
 *         smoothed person is within the configured threshold.
 *
 * @param  grid  Current depth grid snapshot
 * @param  det   Latest person detections from the NN/tracker
 * @param  out   Alert state to populate
 */
static void tof_run_fusion(const tof_depth_grid_t *grid,
                           const tof_person_detection_t *det, tof_alert_t *out) {

  memset(out, 0, sizeof(*out));

  /* Depth grid must be valid; */
  if (!grid->valid) {
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
      // BSP_LED_Toggle(LED_GREEN);
      return;
    }
  }

  /* Track which EMA slots were updated this frame so we can age the rest. */
  uint8_t ema_touched[TOF_MAX_DETECTIONS];
  memset(ema_touched, 0, sizeof(ema_touched));

  uint32_t person_min = UINT32_MAX;
  for (int i = 0; i < det->nb_persons; i++) {
    uint32_t raw_d;
    if (!tof_extract_depth(grid, &det->persons[i], &raw_d)) {
      continue;
    }

    /* Look up EMA slot by track ID; fall back to raw if untracked. */
    uint32_t tid = det->persons[i].track_id;
    uint32_t smoothed_d = raw_d;

    if (tid != 0U) {
      int slot = -1;
      int free_slot = -1;
      for (int j = 0; j < TOF_MAX_DETECTIONS; j++) {
        if (ema_table[j].track_id == tid) {
          slot = j;
          break;
        }
        if (ema_table[j].track_id == 0U && free_slot < 0) {
          free_slot = j;
        }
      }

      if (slot >= 0) {
        ema_table[slot].filtered_mm = TOF_EMA_ALPHA * (float)raw_d +
                                      (1.0f - TOF_EMA_ALPHA) *
                                          ema_table[slot].filtered_mm;
        ema_table[slot].missed = 0;
        ema_touched[slot] = 1;
        smoothed_d = (uint32_t)ema_table[slot].filtered_mm;
      } else if (free_slot >= 0) {
        ema_table[free_slot].track_id = tid;
        ema_table[free_slot].filtered_mm = (float)raw_d;
        ema_table[free_slot].missed = 0;
        ema_touched[free_slot] = 1;
        smoothed_d = raw_d;
      }
    }

    out->person_distances_mm[i] = smoothed_d;
    out->person_depth_valid[i] = 1U;
    out->person_track_ids[i] = tid;
    out->nb_person_depths++;
    out->has_person_depth = 1;
    if (smoothed_d < person_min) {
      person_min = smoothed_d;
    }
  }

  /* Age EMA entries that were not seen this frame; evict stale ones. */
  for (int j = 0; j < TOF_MAX_DETECTIONS; j++) {
    if (ema_table[j].track_id == 0U || ema_touched[j]) {
      continue;
    }
    if (++ema_table[j].missed >= TOF_EMA_MAX_MISSED) {
      ema_table[j].track_id = 0U;
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
  uint32_t last_publish_cycles = 0U;

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

    tx_event_flags_get(&tof_fusion_events, TOF_FUSION_EVT_PERSON_READY,
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
      uint32_t publish_cycles = DWT->CYCCNT;
      if (tof_fusion_timing_reset_pending || last_publish_cycles == 0U) {
        alert_out->fusion_period_us = 0U;
        tof_fusion_timing_reset_pending = 0U;
      } else {
        alert_out->fusion_period_us =
            CYCLES_TO_US(publish_cycles - last_publish_cycles);
      }
      last_publish_cycles = publish_cycles;
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
  memset(ema_table, 0, sizeof(ema_table));
  RCU_BufferInit(&alert_rcu);
  RCU_BufferInit(&person_detection_rcu);
  tof_fusion_timing_reset_pending = 1U;

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
  tof_fusion_timing_reset_pending = 1U;
  tx_thread_suspend(&tof_fusion_ctx.thread);
}

void TOF_FUSION_Resume(void) {
  tof_fusion_timing_reset_pending = 1U;
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

uint32_t TOF_GetAlertThreshold(void) {
  return alert_threshold_mm;
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
