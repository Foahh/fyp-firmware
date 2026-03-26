#ifndef TIMEBASE_H
#define TIMEBASE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tx_user.h"
#include <stdint.h>

/* Convert milliseconds to ThreadX ticks (compile-time safe) */
#define MS_TO_TICKS(ms) (TX_TIMER_TICKS_PER_SECOND * (ms) / 1000U)

/* Convert FPS to ThreadX tick interval (ceiling division) */
#define FPS_TO_TICKS(fps) ((TX_TIMER_TICKS_PER_SECOND + (fps) - 1) / (fps))

/* Convert DWT cycle count to milliseconds (rounded) */
#define CYCLES_TO_MS(cycles)                                                        \
  ((uint32_t)(((uint64_t)(cycles) * 1000ULL + ((uint64_t)SystemCoreClock / 2ULL)) / \
              (uint64_t)SystemCoreClock))

/* Convert DWT cycle count to microseconds (rounded) */
#define CYCLES_TO_US(cycles)                                                           \
  ((uint32_t)(((uint64_t)(cycles) * 1000000ULL + ((uint64_t)SystemCoreClock / 2ULL)) / \
              (uint64_t)SystemCoreClock))

#ifdef __cplusplus
}
#endif

#endif /* TIMEBASE_H */
