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
#include "stm32n6xx_hal.h"
#include "app_error.h"

/**
 * @brief  Check if currently in interrupt context
 * @retval 1 if in ISR, 0 otherwise
 */
static inline int is_in_isr(void) {
  return (__get_IPSR() != 0);
}

int bqueue_init(bqueue_t *bq, int buffer_nb, uint8_t *buffers[]) {
  UINT status;

  if (bq == NULL || buffers == NULL) {
    return -1;
  }

  if (buffer_nb <= 0 || buffer_nb > BQUEUE_MAX_BUFFERS) {
    return -1;
  }

  /* Create free semaphore - initially all buffers are free */
  status = tx_semaphore_create(&bq->free_sem, "bq_free", (ULONG)buffer_nb);
  if (status != TX_SUCCESS) {
    return -1;
  }

  /* Create ready semaphore - initially no buffers are ready */
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

uint8_t *bqueue_get_free(bqueue_t *bq, int is_blocking) {
  uint8_t *result;
  UINT status;
  ULONG wait_option = is_blocking ? TX_WAIT_FOREVER : TX_NO_WAIT;

  TX_INTERRUPT_SAVE_AREA

  status = tx_semaphore_get(&bq->free_sem, wait_option);
  if (status != TX_SUCCESS) {
    return NULL;
  }

  /* Protect index update with critical section */
  TX_DISABLE
  result = bq->buffers[bq->free_idx];
  bq->free_idx = (bq->free_idx + 1) % bq->buffer_nb;
  TX_RESTORE

  return result;
}

void bqueue_put_free(bqueue_t *bq) {
  UINT status;

  status = tx_semaphore_put(&bq->free_sem);
  APP_REQUIRE(status == TX_SUCCESS);
  (void)status; /* Suppress unused warning in release builds */
}

uint8_t *bqueue_get_ready(bqueue_t *bq) {
  uint8_t *result;
  UINT status;

  TX_INTERRUPT_SAVE_AREA

  status = tx_semaphore_get(&bq->ready_sem, TX_WAIT_FOREVER);
  APP_REQUIRE(status == TX_SUCCESS);
  (void)status;

  /* Protect index update with critical section */
  TX_DISABLE
  result = bq->buffers[bq->ready_idx];
  bq->ready_idx = (bq->ready_idx + 1) % bq->buffer_nb;
  TX_RESTORE

  return result;
}

void bqueue_put_ready(bqueue_t *bq) {
  UINT status;

  if (is_in_isr()) {
    /* Use ceiling put for ISR context - increments count without scheduling */
    status = tx_semaphore_ceiling_put(&bq->ready_sem, (ULONG)bq->buffer_nb);
  } else {
    status = tx_semaphore_put(&bq->ready_sem);
  }
  APP_REQUIRE(status == TX_SUCCESS);
  (void)status;
}

