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
#ifndef NN_H
#define NN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bqueue.h"
#include "model_config.h"
#include "stai.h"
#include "tx_api.h"
#include "utils.h"
#include <stdint.h>

/**
 * @brief  NN timing statistics
 */
typedef struct {
  uint32_t nn_period_us; /**< End-to-end period between inference completions */
  uint32_t inference_us; /**< Inference duration */
  uint32_t frame_drops;  /**< Cumulative count of dropped/skipped frames */
} nn_timing_t;

enum {
  NN_OUTPUT_FRAME_SIZE = MDL_NN_OUT_BUFFER_SIZE + sizeof(nn_timing_t),
  NN_OUTPUT_FRAME_PADDING_SIZE =
      ALIGN_VALUE(NN_OUTPUT_FRAME_SIZE, 32U) - NN_OUTPUT_FRAME_SIZE,
};

/**
 * @brief  One published NN result frame: raw outputs plus timing metadata
 */
typedef struct {
  uint8_t data[MDL_NN_OUT_BUFFER_SIZE];
  nn_timing_t timing;
  uint8_t padding[NN_OUTPUT_FRAME_PADDING_SIZE];
} nn_output_frame_t ALIGN_32;

#ifdef SNAPSHOT_MODE

/**
 * @brief  Signal that a snapshot frame is ready (ISR-safe)
 */
void NN_SignalSnapshotReady(void);

/**
 * @brief  Get the single NN snapshot input buffer
 * @retval Pointer to the NN input buffer
 */
uint8_t *NN_GetSnapshotBuffer(void);

#else

/**
 * @brief  Get pointer to NN input buffer queue
 * @retval Pointer to input queue
 */
bqueue_t *NN_GetInputQueue(void);

#endif /* SNAPSHOT_MODE */

/**
 * @brief  Get pointer to NN output buffer queue
 * @retval Pointer to output queue carrying nn_output_frame_t buffers
 */
bqueue_t *NN_GetOutputQueue(void);

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
 * @note   Must be called after NN_ThreadStart
 */
stai_network_info *NN_GetNetworkInfo(void);

/**
 * @brief  Initialize NN thread and resources
 * @param  memory_ptr: ThreadX memory pool (unused, static allocation)
 */
void NN_ThreadStart(void);

/**
 * @brief  Suspend NN inference thread for power measurement
 */
void NN_ThreadSuspend(void);

/**
 * @brief  Resume NN inference thread
 */
void NN_ThreadResume(void);

#ifdef __cplusplus
}
#endif

#endif /* NN_H */
