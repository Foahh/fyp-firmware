/**
 ******************************************************************************
 * @file    app_tof.h
 * @author  Long Liangmao
 * @brief   VL53L5CX Time-of-Flight sensor integration and person-distance
 *          alerting
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

#ifndef TOF_H
#define TOF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "messages_limits.h"
#include "tx_api.h"
#include <stdint.h>

#define TOF_GRID_SIZE 8

/** Default alert threshold: nearest person distance in mm */
#define TOF_DEFAULT_ALERT_THRESHOLD_MM 1000

/** Maximum detections per category */
#define TOF_MAX_DETECTIONS PROTO_TOF_ALERT_MAX_PERSON_MM

/** Maximum allowed NN-to-ToF timestamp delta for fusion (ms) */
#define FUSION_MAX_DT_MS 60

_Static_assert(TOF_GRID_SIZE * TOF_GRID_SIZE == PROTO_TOF_ALERT_MAX_DEPTH_MM,
               "TOF_GRID_SIZE * TOF_GRID_SIZE must match messages.proto TofResult.depth_mm max_count");

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
 * Depth grid & alert types
 * ============================================================================ */

/**
 * @brief  8x8 depth grid snapshot from ToF sensor.
 *
 *         Row/column indexing matches the camera / NN image after the same
 *         mirror/flip as CAMERA_FLIP in app_cam_config.h (e.g. horizontal
 *         mirror when CAMERA_FLIP is CMW_MIRRORFLIP_MIRROR), so depth lines up
 *         with bounding boxes and overlays.
 */
typedef struct {
  int16_t distance_mm[TOF_GRID_SIZE][TOF_GRID_SIZE];
  uint8_t status[TOF_GRID_SIZE][TOF_GRID_SIZE]; /**< 5 or 9 = valid */
  uint32_t timestamp_ms;
  uint8_t valid;
} tof_depth_grid_t;

/**
 * @brief  Person-distance alert state derived from fused NN + ToF data
 */
typedef struct {
  uint8_t nb_person_depths;     /**< Number of person boxes with sampled depth */
  uint32_t person_distances_mm[TOF_MAX_DETECTIONS];
  uint8_t person_depth_valid[TOF_MAX_DETECTIONS];
  uint32_t person_distance_mm; /**< Closest person depth in mm (0 = no data) */
  uint8_t has_person_depth;    /**< 1 if depth data overlapped with a person bbox */
  uint8_t alert;               /**< 1 if person is within threshold */
  uint8_t stale;               /**< 1 if suppressed due to timestamp mismatch */
} tof_alert_t;

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief  Initialize ToF hardware (GPIO, EXTI, power-on, firmware upload,
 *         sensor configuration). Requires BSP_I2C1_Init() first. ~1-2 s.
 * @note   Call from main() before ThreadX starts.
 */
void TOF_Init(void);

/**
 * @brief  Create and start the ToF ranging thread.
 */
void TOF_ThreadStart(void);

/**
 * @brief  Get pointer to latest depth grid (read-only, double-buffered).
 */
const tof_depth_grid_t *TOF_GetDepthGrid(void);

/**
 * @brief  Get pointer to latest alert state (read-only, double-buffered).
 */
const tof_alert_t *TOF_GetAlert(void);

/**
 * @brief  Get pointer to ToF depth-result update event flags group.
 */
TX_EVENT_FLAGS_GROUP *TOF_GetResultUpdateEventFlags(void);

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
 * @brief  Get current person detections from the NN model.
 * @retval Pointer to latest cached person detections
 */
const tof_person_detection_t *TOF_GetPersonDetections(void);

/**
 * @brief  Stop ToF ranging, power off sensor, and suspend thread
 */
void TOF_Stop(void);

/**
 * @brief  Resume ToF thread (re-powers sensor, restarts ranging)
 */
void TOF_Resume(void);

#ifdef __cplusplus
}
#endif

#endif /* TOF_H */
