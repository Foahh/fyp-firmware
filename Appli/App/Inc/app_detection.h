/**
 ******************************************************************************
 * @file    app_detection.h
 * @author  Long Liangmao
 * @brief   Object detection state management and overlay rendering
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
#ifndef APP_DETECTION_H
#define APP_DETECTION_H

#ifdef __cplusplus
extern "C" {
#endif

#include "app_config.h"
#include "od_pp_output_if.h"
#include "tx_api.h"
#include <stdint.h>

/* Maximum detections to display */
#define DETECTION_MAX_BOXES AI_OD_PP_MAX_BOXES_LIMIT

/**
 * @brief  Detection info shared between postprocess and display threads
 */
typedef struct {
  int32_t nb_detect;                              /**< Number of valid detections */
  od_pp_outBuffer_t detects[DETECTION_MAX_BOXES]; /**< Detection results */
  uint32_t nn_period_ms;                          /**< NN inference period */
  uint32_t inference_ms;                          /**< Inference time */
  uint32_t postprocess_ms;                        /**< Postprocess time */
} detection_info_t;

/**
 * @brief  Initialize detection module (threads, sync primitives)
 * @param  memory_ptr: ThreadX memory pool (unused, static allocation)
 */
void Detection_Thread_Init(VOID *memory_ptr);

/**
 * @brief  Signal that new detection results are available
 *         Called by postprocess after updating detection state
 */
void Detection_SignalUpdate(void);

/**
 * @brief  Lock detection state mutex for reading/writing
 */
void Detection_Lock(void);

/**
 * @brief  Unlock detection state mutex
 */
void Detection_Unlock(void);

/**
 * @brief  Get pointer to detection info structure
 * @retval Pointer to detection_info_t (lock before accessing)
 */
detection_info_t *Detection_GetInfo(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_DETECTION_H */
