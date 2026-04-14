/**
 ******************************************************************************
 * @file    tof_fusion.h
 * @author  Long Liangmao
 * @brief   ToF / NN person-detection fusion: alerts, LED
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

#ifndef TOF_FUSION_H
#define TOF_FUSION_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tof.h"
#include "tx_api.h"
#include <stdint.h>

/** Default alert threshold: nearest person distance in mm */
#define TOF_DEFAULT_ALERT_THRESHOLD_MM 1000

/** Maximum detections per category */
#define TOF_MAX_DETECTIONS PROTO_TOF_ALERT_MAX_PERSON_MM

/** Maximum allowed NN-to-ToF timestamp delta for fusion (ms) */
#define FUSION_MAX_DT_MS 100

/* ============================================================================
 * Bounding box type (normalized NN coordinates [0,1])
 * ============================================================================ */

/**
 * @brief  Axis-aligned bounding box in normalized [0,1] NN coordinates.
 */
typedef struct {
  float x_center;
  float y_center;
  float width;
  float height;
  float conf;
  uint32_t track_id; /**< SORT track ID (0 = not associated with a track) */
} tof_bbox_t;

/* ============================================================================
 * Person detection result
 * ============================================================================ */

/**
 * @brief  NN detections filtered to person class boxes.
 */
typedef struct {
  int32_t nb_persons;
  uint32_t timestamp_ms;
  tof_bbox_t persons[TOF_MAX_DETECTIONS];
} tof_person_detection_t;

/* ============================================================================
 * Alert type
 * ============================================================================ */

/**
 * @brief  Person-distance alert state derived from fused NN + ToF data
 */
typedef struct {
  uint8_t nb_person_depths; /**< Number of person boxes with sampled depth */
  uint32_t person_distances_mm[TOF_MAX_DETECTIONS];
  uint8_t person_depth_valid[TOF_MAX_DETECTIONS];
  uint32_t person_track_ids[TOF_MAX_DETECTIONS];
  uint32_t person_distance_mm; /**< Closest person depth in mm (0 = no data) */
  uint32_t fusion_period_us;   /**< Period between consecutive fusion publishes */
  uint8_t has_person_depth;    /**< 1 if depth data overlapped with a person bbox */
  uint8_t alert;               /**< 1 if person is within threshold */
  uint8_t stale;               /**< 1 if suppressed due to timestamp mismatch */
} tof_alert_t;

/* ============================================================================
 * Lifecycle (called from tof.c)
 * ============================================================================ */

void TOF_FUSION_ThreadStart(void);
void TOF_FUSION_NotifyDepthReady(void);
void TOF_FUSION_Stop(void);
void TOF_FUSION_Resume(void);

/* ============================================================================
 * Public fusion API
 * ============================================================================ */

/**
 * @brief  Acquire the latest alert state for zero-copy read access.
 * @param  token: Reader token released with TOF_ReleaseAlert()
 */
const tof_alert_t *TOF_AcquireAlert(rcu_read_token_t *token);

/**
 * @brief  Release an alert snapshot previously acquired with TOF_AcquireAlert().
 * @param  token: Reader token to release
 */
void TOF_ReleaseAlert(rcu_read_token_t *token);

/**
 * @brief  Get pointer to ToF alert update event flags group.
 */
TX_EVENT_FLAGS_GROUP *TOF_GetAlertUpdateEventFlags(void);

/**
 * @brief  Change the person-distance alert threshold at runtime.
 * @param  threshold_mm: Alert if nearest person distance < threshold_mm
 */
void TOF_SetAlertThreshold(uint32_t threshold_mm);

/**
 * @brief  Publish latest person detections for ToF fusion.
 * @param  detections: Pointer to cached person detections
 */
void TOF_SetPersonDetections(const tof_person_detection_t *detections);

/**
 * @brief  Acquire the latest cached person detections for zero-copy read access.
 * @param  token: Reader token released with TOF_ReleasePersonDetections()
 * @retval Pointer to latest cached person detections
 */
const tof_person_detection_t *TOF_AcquirePersonDetections(rcu_read_token_t *token);

/**
 * @brief  Release person detections previously acquired with TOF_AcquirePersonDetections().
 * @param  token: Reader token to release
 */
void TOF_ReleasePersonDetections(rcu_read_token_t *token);

#ifdef __cplusplus
}
#endif

#endif /* TOF_FUSION_H */
