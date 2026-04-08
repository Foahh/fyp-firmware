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
#include "rcu_buffer.h"

#include "arm_math.h"

#include "od_pp_output_if.h"
#include "tx_api.h"
#include <stdbool.h>
#include <stdint.h>

/* Maximum detections carried in detection_info_t and protobuf reports. */
#define DETECTION_MAX_BOXES PROTO_DETECTION_RESULT_MAX_DETECTIONS

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

typedef struct {
  float pp_conf_threshold;
  float pp_iou_threshold;
  float track_thresh;
  float det_thresh;
  float sim1_thresh;
  float sim2_thresh;
  uint32_t tlost_cnt;
} pp_runtime_config_t;

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
 * @brief  Acquire the latest detection snapshot for zero-copy read access.
 * @param  token: Reader token released with PP_ReleaseInfo()
 * @retval Pointer to read-only detection state, or NULL on invalid args
 */
const detection_info_t *PP_AcquireInfo(rcu_read_token_t *token);

/**
 * @brief  Release a detection snapshot previously acquired with PP_AcquireInfo().
 * @param  token: Reader token to release
 */
void PP_ReleaseInfo(rcu_read_token_t *token);

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

/**
 * @brief  Request runtime update of postprocess and tracker thresholds
 * @param  cfg: Requested runtime configuration
 * @retval true on accepted request, false on invalid input
 */
bool PP_RequestRuntimeConfig(const pp_runtime_config_t *cfg);

/**
 * @brief  Get currently active runtime postprocess/tracker configuration
 * @param  out_cfg: Output structure for active configuration
 * @retval true if configuration has been initialized and copied
 */
bool PP_GetRuntimeConfig(pp_runtime_config_t *out_cfg);

#ifdef __cplusplus
}
#endif

#endif /* PP_H */
