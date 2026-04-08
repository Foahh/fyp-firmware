#ifndef RCU_BUFFER_H
#define RCU_BUFFER_H

#include "stm32n6xx.h"
#include "tx_api.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Minimal single-writer RCU slot pool for zero-copy readers.
 * Readers pin the currently published slot with a short-lived token.
 * Writer publishes into any non-published slot whose reader count is zero.
 */
#define RCU_BUFFER_SLOT_COUNT 3U

typedef struct {
  uint8_t slot_idx;
  uint8_t active;
} rcu_read_token_t;

typedef struct {
  volatile uint8_t published_idx;
  volatile uint16_t reader_counts[RCU_BUFFER_SLOT_COUNT];
} rcu_buffer_t;

static inline void RCU_BufferInit(rcu_buffer_t *buffer) {
  if (buffer == NULL) {
    return;
  }

  buffer->published_idx = 0U;
  for (uint32_t i = 0; i < RCU_BUFFER_SLOT_COUNT; i++) {
    buffer->reader_counts[i] = 0U;
  }
  __DMB();
}

static inline bool RCU_ReadAcquire(rcu_buffer_t *buffer, rcu_read_token_t *token,
                                   uint8_t *slot_idx) {
  TX_INTERRUPT_SAVE_AREA

  if (buffer == NULL || token == NULL || slot_idx == NULL) {
    return false;
  }

  token->active = 0U;

  while (true) {
    uint8_t idx;

    TX_DISABLE
    idx = buffer->published_idx;
    buffer->reader_counts[idx]++;
    TX_RESTORE

    __DMB();
    if (buffer->published_idx == idx) {
      token->slot_idx = idx;
      token->active = 1U;
      *slot_idx = idx;
      return true;
    }

    __DMB();
    TX_DISABLE
    buffer->reader_counts[idx]--;
    TX_RESTORE
  }
}

static inline void RCU_ReadRelease(rcu_buffer_t *buffer, rcu_read_token_t *token) {
  TX_INTERRUPT_SAVE_AREA

  if (buffer == NULL || token == NULL || token->active == 0U) {
    return;
  }

  /* Complete all data reads before making this slot reusable. */
  __DMB();

  TX_DISABLE
  buffer->reader_counts[token->slot_idx]--;
  TX_RESTORE

  token->active = 0U;
}

static inline bool RCU_WriteReserve(rcu_buffer_t *buffer, uint8_t *slot_idx) {
  if (buffer == NULL || slot_idx == NULL) {
    return false;
  }

  uint8_t published_idx = buffer->published_idx;
  __DMB();

  for (uint8_t idx = 0; idx < RCU_BUFFER_SLOT_COUNT; idx++) {
    if (idx == published_idx) {
      continue;
    }
    if (buffer->reader_counts[idx] == 0U) {
      __DMB();
      *slot_idx = idx;
      return true;
    }
  }

  return false;
}

static inline void RCU_WritePublish(rcu_buffer_t *buffer, uint8_t slot_idx) {
  if (buffer == NULL || slot_idx >= RCU_BUFFER_SLOT_COUNT) {
    return;
  }

  /* Publish only after the new slot contents are globally visible. */
  __DMB();
  buffer->published_idx = slot_idx;
  __DMB();
}

#ifdef __cplusplus
}
#endif

#endif /* RCU_BUFFER_H */
