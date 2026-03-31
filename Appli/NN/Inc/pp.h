/**
 ******************************************************************************
 * @file    app_pp.h
 * @author  Long Liangmao
 * @brief   Postprocessing thread and detection state management
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
#ifndef PP_H
#define PP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "app_config.h"

#include "arm_math.h"

#include "od_pp_output_if.h"
#include "tx_api.h"
#include <stdint.h>

/* Maximum detections to display */
#define DETECTION_MAX_BOXES MDL_PP_MAX_BOXES

typedef struct {
  float x_center;
  float y_center;
  float width;
  float height;
  uint32_t id;
} tracked_box_t;

/**
 * @brief  Detection info shared between postprocess and display threads
 */
typedef struct {
  uint32_t timestamp_ms;                          /**< HAL_GetTick() at postprocess completion */
  int32_t nb_detect;                              /**< Number of valid detections */
  od_pp_outBuffer_t detects[DETECTION_MAX_BOXES]; /**< Detection results */
  uint32_t nn_period_us;                          /**< NN inference period */
  uint32_t inference_us;                          /**< Inference time */
  uint32_t postprocess_us;                        /**< Library OD postprocess (NMS, decode) */
  uint32_t tracker_us;                            /**< Tracker update (det → tracks) */
  uint32_t frame_drops;                           /**< Cumulative frame drop count */
  int32_t nb_tracked;                             /**< Number of active tracked boxes */
  tracked_box_t tracked[DETECTION_MAX_BOXES];     /**< Kalman-smoothed tracked detections */
} detection_info_t;

/**
 * @brief  Initialize postprocessing module (thread, sync primitives)
 * @param  memory_ptr: ThreadX memory pool (unused, static allocation)
 */
void PP_ThreadStart(void);

/**
 * @brief  Signal that new detection results are available
 *         Called by postprocess after updating detection state
 */
void PP_SignalUpdate(void);

/**
 * @brief  Get pointer to detection info structure (lock-free, read-only)
 * @retval Pointer to detection_info_t (read-only, no lock needed)
 */
detection_info_t *PP_GetInfo(void);

/**
 * @brief  Get pointer to update event flags group (for overlay thread)
 * @retval Pointer to event flags group
 */
TX_EVENT_FLAGS_GROUP *PP_GetUpdateEventFlags(void);

/**
 * @brief  Suspend postprocessing thread for power measurement
 */
void PP_ThreadSuspend(void);

/**
 * @brief  Resume postprocessing thread
 */
void PP_ThreadResume(void);

#ifdef __cplusplus
}
#endif

#endif /* PP_H */
