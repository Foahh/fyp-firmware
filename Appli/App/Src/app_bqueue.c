/**
 ******************************************************************************
 * @file    app_bqueue.c
 * @author  Long Liangmao
 * @brief   ThreadX-based buffer queue implementation
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

#include "app_bqueue.h"
#include "app_error.h"
#include "stm32n6xx_hal.h"
#include "utils.h"

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

/**
 * @brief  Initialize a buffer queue
 * @param  bq: Pointer to buffer queue structure
 * @param  buffer_nb: Number of buffers (1 to BQUEUE_MAX_BUFFERS)
 * @param  buffers: Array of buffer pointers
 * @retval 0 on success, -1 on error
 */
int bqueue_init(bqueue_t *bq, int buffer_nb, uint8_t *buffers[]) {
  UINT status;

  if (bq == NULL || buffers == NULL) {
    return -1;
  }

  if (buffer_nb <= 0 || buffer_nb > BQUEUE_MAX_BUFFERS) {
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
 * @param  is_blocking: 1 to wait forever, 0 to return immediately
 * @retval Pointer to free buffer, or NULL if none available (non-blocking)
 */
uint8_t *bqueue_get_free(bqueue_t *bq, int is_blocking) {
  uint8_t *result;
  UINT status;
  ULONG wait_option = is_blocking ? TX_WAIT_FOREVER : TX_NO_WAIT;

  TX_INTERRUPT_SAVE_AREA

  status = tx_semaphore_get(&bq->free_sem, wait_option);
  if (status != TX_SUCCESS) {
    return NULL;
  }

  TX_DISABLE
  result = bq->buffers[bq->free_idx];
  bq->free_idx = (bq->free_idx + 1) % bq->buffer_nb;
  TX_RESTORE

  return result;
}

/**
 * @brief  Release a buffer back to free pool (consumer side, after processing)
 * @param  bq: Pointer to buffer queue
 */
void bqueue_put_free(bqueue_t *bq) {
  UINT status;
  status = tx_semaphore_put(&bq->free_sem);
  APP_REQUIRE(status == TX_SUCCESS);
}

/**
 * @brief  Get a ready buffer from the queue (consumer side)
 * @param  bq: Pointer to buffer queue
 * @retval Pointer to ready buffer (blocking call)
 */
uint8_t *bqueue_get_ready(bqueue_t *bq) {
  uint8_t *result;
  UINT status;

  TX_INTERRUPT_SAVE_AREA

  status = tx_semaphore_get(&bq->ready_sem, TX_WAIT_FOREVER);
  APP_REQUIRE(status == TX_SUCCESS);
  (void)status;

  TX_DISABLE
  result = bq->buffers[bq->ready_idx];
  bq->ready_idx = (bq->ready_idx + 1) % bq->buffer_nb;
  TX_RESTORE

  return result;
}

/**
 * @brief  Mark a buffer as ready (producer side, after filling)
 * @param  bq: Pointer to buffer queue
 * @note   ISR-safe: can be called from interrupt context
 */
void bqueue_put_ready(bqueue_t *bq) {
  UINT status;

  if (IS_IRQ_MODE()) {
    status = tx_semaphore_ceiling_put(&bq->ready_sem, (ULONG)bq->buffer_nb);
  } else {
    status = tx_semaphore_put(&bq->ready_sem);
  }
  APP_REQUIRE(status == TX_SUCCESS);
  (void)status;
}
