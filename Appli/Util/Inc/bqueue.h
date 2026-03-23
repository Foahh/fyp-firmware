/**
 ******************************************************************************
 * @file    bqueue.h
 * @author  Long Liangmao
 * @brief   ThreadX-based buffer queue for pipeline communication
 *          Port of https://github.com/STMicroelectronics/x-cube-n6-ai-people-detection-tracking bqueue_t pattern to ThreadX RTOS
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
#ifndef BQUE_H
#define BQUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tx_api.h"
#include <stdbool.h>
#include <stdint.h>

/* Maximum number of buffers in a queue */
#define BQUE_MAX_BUFFERS 4

/**
 * @brief  Buffer queue structure for single producer, single consumer (SPSC) pattern
 *         Uses counting semaphores for blocking synchronization and lock-free index updates
 */
typedef struct {
  TX_SEMAPHORE free_sem;              /**< Counts available free buffers */
  TX_SEMAPHORE ready_sem;             /**< Counts buffers ready for consumption */
  uint8_t buffer_nb;                  /**< Number of buffers in the queue */
  uint8_t *buffers[BQUE_MAX_BUFFERS]; /**< Array of buffer pointers */
  volatile uint8_t free_idx;          /**< Next free buffer index (producer-only) */
  volatile uint8_t ready_idx;         /**< Next ready buffer index (consumer-only) */
} bqueue_t;

/**
 * @brief  Initialize a buffer queue
 * @param  bq: Pointer to buffer queue structure
 * @param  buffer_nb: Number of buffers (1 to BQUE_MAX_BUFFERS)
 * @param  buffers: Array of buffer pointers
 * @retval 0 on success, -1 on error
 */
int BQUE_Init(bqueue_t *bq, uint8_t buffer_nb, uint8_t *buffers[]);

/**
 * @brief  Get a free buffer from the queue (producer side)
 * @param  bq: Pointer to buffer queue
 * @param  is_blocking: 1 to wait forever, 0 to return immediately
 * @retval Pointer to free buffer, or NULL if none available (non-blocking)
 */
uint8_t *BQUE_GetFree(bqueue_t *bq, bool is_blocking);

/**
 * @brief  Release a buffer back to free pool (consumer side, after processing)
 * @param  bq: Pointer to buffer queue
 */
void BQUE_PutFree(bqueue_t *bq);

/**
 * @brief  Get a ready buffer from the queue (consumer side)
 * @param  bq: Pointer to buffer queue
 * @retval Pointer to ready buffer (blocking call)
 */
uint8_t *BQUE_GetReady(bqueue_t *bq);

/**
 * @brief  Get the latest ready buffer, discarding older ones (consumer side)
 *         Blocks until at least one buffer is ready, then drains any additional
 *         ready buffers and returns the newest. Older buffers are returned to
 *         the free pool.
 * @param  bq: Pointer to buffer queue
 * @param  skipped: If non-NULL, receives the number of frames discarded
 * @retval Pointer to the most recent ready buffer (blocking call)
 */
uint8_t *BQUE_GetReadyLatest(bqueue_t *bq, uint32_t *skipped);

/**
 * @brief  Mark a buffer as ready (producer side, after filling)
 * @param  bq: Pointer to buffer queue
 * @note   ISR-safe: can be called from interrupt context
 */
void BQUE_PutReady(bqueue_t *bq);

#ifdef __cplusplus
}
#endif

#endif /* BQUE_H */
