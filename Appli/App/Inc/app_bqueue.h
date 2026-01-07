/**
 ******************************************************************************
 * @file    app_bqueue.h
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
#ifndef APP_BQUEUE_H
#define APP_BQUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tx_api.h"
#include <stdint.h>

/* Maximum number of buffers in a queue */
#define BQUEUE_MAX_BUFFERS 2

/**
 * @brief  Buffer queue structure for producer-consumer pattern
 *         Uses counting semaphores for free/ready synchronization
 */
typedef struct {
  TX_SEMAPHORE free_sem;              /**< Counts available free buffers */
  TX_SEMAPHORE ready_sem;             /**< Counts buffers ready for consumption */
  int buffer_nb;                      /**< Number of buffers in the queue */
  uint8_t *buffers[BQUEUE_MAX_BUFFERS]; /**< Array of buffer pointers */
  volatile int free_idx;              /**< Next free buffer index */
  volatile int ready_idx;             /**< Next ready buffer index */
} bqueue_t;

/**
 * @brief  Initialize a buffer queue
 * @param  bq: Pointer to buffer queue structure
 * @param  buffer_nb: Number of buffers (1 to BQUEUE_MAX_BUFFERS)
 * @param  buffers: Array of buffer pointers
 * @retval 0 on success, -1 on error
 */
int bqueue_init(bqueue_t *bq, int buffer_nb, uint8_t *buffers[]);

/**
 * @brief  Get a free buffer from the queue (producer side)
 * @param  bq: Pointer to buffer queue
 * @param  is_blocking: 1 to wait forever, 0 to return immediately
 * @retval Pointer to free buffer, or NULL if none available (non-blocking)
 */
uint8_t *bqueue_get_free(bqueue_t *bq, int is_blocking);

/**
 * @brief  Release a buffer back to free pool (consumer side, after processing)
 * @param  bq: Pointer to buffer queue
 */
void bqueue_put_free(bqueue_t *bq);

/**
 * @brief  Get a ready buffer from the queue (consumer side)
 * @param  bq: Pointer to buffer queue
 * @retval Pointer to ready buffer (blocking call)
 */
uint8_t *bqueue_get_ready(bqueue_t *bq);

/**
 * @brief  Mark a buffer as ready (producer side, after filling)
 * @param  bq: Pointer to buffer queue
 * @note   ISR-safe: can be called from interrupt context
 */
void bqueue_put_ready(bqueue_t *bq);

#ifdef __cplusplus
}
#endif

#endif /* APP_BQUEUE_H */

