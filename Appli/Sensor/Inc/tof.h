/**
 ******************************************************************************
 * @file    tof.h
 * @author  Long Liangmao
 * @brief   VL53L5CX Time-of-Flight ranging and depth grid publishing
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
#include "rcu_buffer.h"
#include "tx_api.h"
#include <stdint.h>

#define TOF_GRID_SIZE 8

_Static_assert(TOF_GRID_SIZE *TOF_GRID_SIZE == PROTO_TOF_ALERT_MAX_DEPTH_MM,
               "TOF_GRID_SIZE * TOF_GRID_SIZE must match messages.proto "
               "TofResult.depth_mm max_count");

/* ============================================================================
 * Depth grid
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

/* ============================================================================
 * Public API — ranging and depth
 * ============================================================================ */

/**
 * @brief  Initialize ToF hardware (GPIO, EXTI, power-on, firmware upload,
 *         sensor configuration). Requires BSP_I2C1_Init() first. ~1-2 s.
 * @note   Call from main() before ThreadX starts.
 */
void TOF_Init(void);

/**
 * @brief  Create and start the ToF ranging thread (and fusion thread).
 */
void TOF_ThreadStart(void);

/**
 * @brief  Acquire the latest depth grid for zero-copy read access.
 * @param  token: Reader token released with TOF_ReleaseDepthGrid()
 */
const tof_depth_grid_t *TOF_AcquireDepthGrid(rcu_read_token_t *token);

/**
 * @brief  Release a depth-grid snapshot previously acquired with TOF_AcquireDepthGrid().
 * @param  token: Reader token to release
 */
void TOF_ReleaseDepthGrid(rcu_read_token_t *token);

/**
 * @brief  Get pointer to ToF depth-result update event flags group.
 */
TX_EVENT_FLAGS_GROUP *TOF_GetResultUpdateEventFlags(void);

/**
 * @brief  Stop ToF ranging, power off sensor, and suspend threads
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
