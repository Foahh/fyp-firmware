/**
 ******************************************************************************
 * @file    app_cpuload.h
 * @author  Long Liangmao
 * @brief   CPU load measurement utilities using DWT cycle counter
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
#ifndef CPULOAD_H
#define CPULOAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief  CPU load measurement structure
 */
typedef struct {
  uint32_t last_total; /* Last total cycle count */
  uint32_t last_idle;  /* Last idle cycle count */
  uint32_t last_tick;  /* Last update timestamp (ms) */
  float cpu_load_ema;  /* Exponential moving average of CPU load (%) */
  uint8_t initialized; /* Flag to indicate first sample */
} cpuload_info_t;

/**
 * @brief  Initialize DWT cycle counter for CPU load measurement
 */
void CPULoad_InitCounter(void);

/**
 * @brief  Get current cycle count
 * @retval Current DWT cycle count
 */
uint32_t CPULoad_GetCycleCount(void);

/**
 * @brief  Initialize CPU load tracking structure
 * @param  cpu_load: Pointer to CPU load info structure
 */
void CPULoad_Init(cpuload_info_t *cpu_load);

/**
 * @brief  Update CPU load measurement (call periodically)
 * @param  cpu_load: Pointer to CPU load info structure
 */
void CPULoad_Update(cpuload_info_t *cpu_load);

/**
 * @brief  Get smoothed CPU load percentage
 * @param  cpu_load: Pointer to CPU load info structure
 * @retval CPU load percentage (0.0 - 100.0)
 */
float CPULoad_GetSmoothed(const cpuload_info_t *cpu_load);

/**
 * @brief  Enter idle state (call from idle thread)
 */
void CPULoad_IdleEnter(void);

/**
 * @brief  Exit idle state (call from idle thread)
 */
void CPULoad_IdleExit(void);

#ifdef __cplusplus
}
#endif

#endif /* CPULOAD_H */
