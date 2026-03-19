/**
 ******************************************************************************
 * @file    app_nn.h
 * @author  Long Liangmao
 * @brief   Neural network thread and ATON runtime wrapper
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
#ifndef APP_NN_H
#define APP_NN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "app_bqueue.h"
#include "stai.h"
#include "tx_api.h"
#include <stdint.h>

/**
 * @brief  NN timing statistics
 */
typedef struct {
  uint32_t nn_period_ms; /**< Period between inference starts */
  uint32_t inference_ms; /**< Inference duration */
} nn_timing_t;

/**
 * @brief  Get pointer to NN input buffer queue
 * @retval Pointer to input queue
 */
bqueue_t *NN_GetInputQueue(void);

/**
 * @brief  Get pointer to NN output buffer queue
 * @retval Pointer to output queue
 */
bqueue_t *NN_GetOutputQueue(void);

/**
 * @brief  Get current NN timing statistics
 * @param  timing: Pointer to timing structure to fill
 */
void NN_GetTiming(nn_timing_t *timing);

/**
 * @brief  Get number of NN outputs
 * @retval Number of output buffers
 */
int NN_GetOutputCount(void);

/**
 * @brief  Get NN output buffer sizes
 * @retval Pointer to array of output sizes
 */
const uint32_t *NN_GetOutputSizes(void);

/**
 * @brief  Get network info for postprocessing
 * @retval Pointer to network info structure
 * @note   Must be called after NN_Thread_Start
 */
stai_network_info *NN_GetNetworkInfo(void);

/**
 * @brief  Set the host image ID for the next inference result
 * @param  image_id: non-zero ID from host, 0 means camera frame
 */
void NN_SetHostImageId(uint32_t image_id);

/**
 * @brief  Read and consume the host image ID
 * @retval image_id if the current inference was triggered by a host image, 0 otherwise
 */
uint32_t NN_ConsumeHostImageId(void);

/**
 * @brief  Initialize NN thread and resources
 * @param  memory_ptr: ThreadX memory pool (unused, static allocation)
 */
void NN_Thread_Start(VOID *memory_ptr);

#ifdef __cplusplus
}
#endif

#endif /* APP_NN_H */
