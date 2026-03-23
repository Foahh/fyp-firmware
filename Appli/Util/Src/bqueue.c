/**
 ******************************************************************************
 * @file    bqueue.c
 * @author  Long Liangmao
 * @brief   ThreadX-based single producer, single consumer (SPSC) buffer queue implementation
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

#include "bqueue.h"
#include "app_error.h"
#include "stm32n6xx_hal.h"
#include "utils.h"

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

/**
 * @brief  Initialize a buffer queue
 * @param  bq: Pointer to buffer queue structure
 * @param  buffer_nb: Number of buffers (1 to BQUE_MAX_BUFFERS)
 * @param  buffers: Array of buffer pointers
 * @retval 0 on success, -1 on error
 */
int BQUE_Init(bqueue_t *bq, uint8_t buffer_nb, uint8_t *buffers[]) {
  UINT status;

  if (bq == NULL || buffers == NULL) {
    return -1;
  }

  if (buffer_nb <= 0 || buffer_nb > BQUE_MAX_BUFFERS) {
    return -1;
  }

  status = tx_semaphore_create(&bq->free_sem, "bq_free", (ULONG)buffer_nb);
  if (status != TX_SUCCESS) {
    return -1;
  }

  status = tx_semaphore_create(&bq->ready_sem, "bq_ready", 0);
  if (status != TX_SUCCESS) {
    tx_semaphore_delete(&bq->free_sem);
    return -1;
  }

  bq->buffer_nb = buffer_nb;
  for (int i = 0; i < buffer_nb; i++) {
    APP_REQUIRE(buffers[i] != NULL);
    bq->buffers[i] = buffers[i];
  }

  bq->free_idx = 0;
  bq->ready_idx = 0;

  return 0;
}

/**
 * @brief  Get a free buffer from the queue (producer side)
 * @param  bq: Pointer to buffer queue
 * @param  is_blocking: true to wait forever, false to return immediately
 * @retval Pointer to free buffer, or NULL if none available (non-blocking)
 */
uint8_t *BQUE_GetFree(bqueue_t *bq, bool is_blocking) {
  uint8_t *result;
  UINT status;
  ULONG wait_option = is_blocking ? TX_WAIT_FOREVER : TX_NO_WAIT;

  status = tx_semaphore_get(&bq->free_sem, wait_option);
  if (status != TX_SUCCESS) {
    return NULL;
  }

  /* SPSC: Producer is the only writer to free_idx, so no lock needed */
  result = bq->buffers[bq->free_idx];
  MEMORY_BARRIER(); /* Ensure buffer read completes before index update */
  bq->free_idx = (bq->free_idx + 1) % bq->buffer_nb;
  MEMORY_BARRIER(); /* Ensure index update is visible to consumer */

  return result;
}

/**
 * @brief  Release a buffer back to free pool (consumer side, after processing)
 * @param  bq: Pointer to buffer queue
 */
void BQUE_PutFree(bqueue_t *bq) {
  UINT status;

  /* Ensure consumer has finished reading buffer before releasing it */
  MEMORY_BARRIER();

  status = tx_semaphore_put(&bq->free_sem);
  APP_REQUIRE(status == TX_SUCCESS);
}

/**
 * @brief  Get a ready buffer from the queue (consumer side)
 * @param  bq: Pointer to buffer queue
 * @retval Pointer to ready buffer (blocking call)
 */
uint8_t *BQUE_GetReady(bqueue_t *bq) {
  uint8_t *result;
  UINT status;

  status = tx_semaphore_get(&bq->ready_sem, TX_WAIT_FOREVER);
  APP_REQUIRE(status == TX_SUCCESS);
  (void)status;

  /* SPSC: Consumer is the only writer to ready_idx, so no lock needed */
  result = bq->buffers[bq->ready_idx];
  MEMORY_BARRIER(); /* Ensure buffer read completes before index update */
  bq->ready_idx = (bq->ready_idx + 1) % bq->buffer_nb;
  MEMORY_BARRIER(); /* Ensure index update is visible to producer */

  return result;
}

/**
 * @brief  Get the latest ready buffer, discarding older ones (consumer side)
 *         Blocks until at least one buffer is ready, then drains any additional
 *         ready buffers and returns the newest. Older buffers are returned to
 *         the free pool.
 * @param  bq: Pointer to buffer queue
 * @param  skipped: If non-NULL, receives the number of frames discarded
 * @retval Pointer to the most recent ready buffer (blocking call)
 */
uint8_t *BQUE_GetReadyLatest(bqueue_t *bq, uint32_t *skipped) {
  UINT status;
  uint32_t skip_count = 0;

  /* Block until at least one buffer is ready */
  uint8_t *latest = BQUE_GetReady(bq);

  /* Drain any additional ready buffers, keeping only the newest */
  while (1) {
    status = tx_semaphore_get(&bq->ready_sem, TX_NO_WAIT);
    if (status != TX_SUCCESS) {
      break;
    }

    /* Return the older buffer to the free pool */
    MEMORY_BARRIER();
    tx_semaphore_put(&bq->free_sem);
    skip_count++;

    /* Advance to the next ready buffer */
    latest = bq->buffers[bq->ready_idx];
    MEMORY_BARRIER();
    bq->ready_idx = (bq->ready_idx + 1) % bq->buffer_nb;
    MEMORY_BARRIER();
  }

  if (skipped != NULL) {
    *skipped = skip_count;
  }

  return latest;
}

/**
 * @brief  Mark a buffer as ready (producer side, after filling)
 * @param  bq: Pointer to buffer queue
 * @note   ISR-safe: can be called from interrupt context
 */
void BQUE_PutReady(bqueue_t *bq) {
  UINT status;

  /* Ensure all buffer writes are complete before signaling consumer */
  MEMORY_BARRIER();

  if (IS_IRQ_MODE()) {
    status = tx_semaphore_ceiling_put(&bq->ready_sem, (ULONG)bq->buffer_nb);
  } else {
    status = tx_semaphore_put(&bq->ready_sem);
  }
  APP_REQUIRE(status == TX_SUCCESS);
  (void)status;
}
