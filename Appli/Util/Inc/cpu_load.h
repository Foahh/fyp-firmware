#ifndef CPU_LOAD_H
#define CPU_LOAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief  Create and start the CPU monitor thread.
 *         This thread is the only owner of sampling and publishes
 *         the latest usage ratio for lock-free readers.
 */
void CPU_LoadThreadStart(void);

/**
 * @brief  Sample one measurement window and refresh usage ratio.
 * @note   Internal sampling primitive, typically used by CPU monitor thread only.
 * @retval true when a valid sample was produced.
 */
bool CPU_LoadSample(void);

/**
 * @brief  Latest published CPU usage ratio in [0,1].
 */
float CPU_LoadGetUsageRatio(void);

/**
 * @brief  Convenience helper returning current usage in percent.
 * @retval true on success.
 */
bool CPU_LoadGetPercent(float *usage_percent);

#ifdef __cplusplus
}
#endif

#endif /* CPU_LOAD_H */
