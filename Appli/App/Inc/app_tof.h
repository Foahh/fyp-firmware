/**
 ******************************************************************************
 * @file    app_tof.h
 * @author  Long Liangmao
 * @brief   VL53L5CX Time-of-Flight sensor integration and hazard proximity
 *          alert (hand-near-hazard detection)
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

#ifndef APP_TOF_H
#define APP_TOF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define TOF_GRID_SIZE 8

/** Default alert threshold: hand-to-hazard distance in mm */
#define TOF_DEFAULT_ALERT_THRESHOLD_MM 150

/** Maximum detections per category for the hazard detection stub */
#define TOF_MAX_DETECTIONS 4

/** Maximum allowed NN-to-ToF timestamp delta for fusion (ms) */
#define FUSION_MAX_DT_MS 60

/* ============================================================================
 * Camera intrinsics proxy (placeholder calibration constants)
 *
 * These approximate a pinhole model for mapping NN normalised [0,1]
 * coordinates to metric XY at a given depth Z.  Replace with real values
 * from camera calibration.  Units: pixels at the NN input resolution.
 * ============================================================================ */

/** Focal lengths in NN-normalised units (fx_norm = fx_px / img_w) */
#define TOF_CAM_FX_NORM 1.0f /* TODO: calibrate */
#define TOF_CAM_FY_NORM 1.0f /* TODO: calibrate */

/** Principal point in NN-normalised units */
#define TOF_CAM_CX_NORM 0.5f
#define TOF_CAM_CY_NORM 0.5f

/** Camera-to-ToF alignment offset in mm (added to ToF XY) */
#define TOF_ALIGN_DX_MM 0.0f /* TODO: calibrate */
#define TOF_ALIGN_DY_MM 0.0f /* TODO: calibrate */

/* ============================================================================
 * 3D point and fusion mode types
 * ============================================================================ */

typedef enum {
  TOF_MODE_NONE = 0, /**< No valid fusion data */
  TOF_MODE_3D,       /**< 3D Euclidean distance */
} tof_fusion_mode_t;

typedef struct {
  float x_mm;
  float y_mm;
  float z_mm;
} tof_point3d_t;

/* ============================================================================
 * Bounding box type (normalized NN coordinates [0,1])
 * ============================================================================ */

/**
 * @brief  Axis-aligned bounding box in normalized [0,1] NN coordinates.
 *         Used by the hazard detection stub; will match the future model output.
 */
typedef struct {
  float x_center;
  float y_center;
  float width;
  float height;
  float conf;
} tof_bbox_t;

/* ============================================================================
 * Hazard detection result (STUB — no model trained yet)
 * ============================================================================ */

/**
 * @brief  Result from the hazard detection model.
 *         Currently a stub that always returns zero detections.
 *         Replace the implementation when a hand/hazard model is available.
 */
typedef struct {
  int32_t nb_hands;
  tof_bbox_t hands[TOF_MAX_DETECTIONS];
  int32_t nb_hazards;
  tof_bbox_t hazards[TOF_MAX_DETECTIONS];
} hazard_detection_t;

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
 * @brief  Hazard proximity alert state (fused depth of hand vs hazard)
 */
typedef struct {
  uint32_t hand_distance_mm;   /**< Closest hand depth in mm (0 = no data) */
  uint32_t hazard_distance_mm; /**< Closest hazard depth in mm (0 = no data) */
  uint8_t has_hand_depth;      /**< 1 if depth data overlapped with a hand bbox */
  uint8_t has_hazard_depth;    /**< 1 if depth data overlapped with a hazard bbox */
  uint8_t alert;               /**< 1 if hand and hazard are within threshold */
  tof_fusion_mode_t mode;      /**< Fusion mode used for this cycle */
  tof_point3d_t hand_xyz_mm;   /**< Hand representative 3D point */
  tof_point3d_t hazard_xyz_mm; /**< Hazard representative 3D point */
  float distance_3d_mm;        /**< Euclidean 3D distance (0 if Z-only mode) */
  uint8_t stale;               /**< 1 if suppressed due to timestamp mismatch */
} tof_alert_t;

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief  Create and start the ToF ranging thread.
 */
void TOF_Thread_Start(void);

/**
 * @brief  Get pointer to latest depth grid (read-only, double-buffered).
 */
const tof_depth_grid_t *TOF_GetDepthGrid(void);

/**
 * @brief  Get pointer to latest alert state (read-only, double-buffered).
 */
const tof_alert_t *TOF_GetAlert(void);

/**
 * @brief  Change the hand-to-hazard proximity alert threshold at runtime.
 * @param  threshold_mm: Alert if distance < threshold_mm (3D or Z-only)
 */
void TOF_SetAlertThreshold(uint32_t threshold_mm);

/**
 * @brief  STUB — Get current hazard detections.
 *         Returns zero detections until a hand/hazard model is trained.
 *         Replace this implementation when the model is ready.
 * @retval Pointer to static hazard_detection_t (always valid, may be empty)
 */
const hazard_detection_t *TOF_GetHazardDetections(void);

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

#endif /* APP_TOF_H */
