/**
 ******************************************************************************
 * @file    app_cpuload.c
 * @author  Long Liangmao
 * @brief   CPU load measurement implementation using DWT cycle counter
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

#include "cpuload.h"
#include "stm32n6xx_hal.h"
#include <string.h>

/* ============================================================================
 * Configuration Constants
 * ============================================================================ */

/* EMA smoothing factor (0.0 - 1.0)
 * Lower values = more smoothing, higher values = more responsive
 */
#define CPU_LOAD_EMA_ALPHA 0.2f

/* ============================================================================
 * Global State Variables
 * ============================================================================ */

/* Idle time accumulator
 * Using volatile for basic thread-safety
 * For diagnostic display, occasional glitches are acceptable
 */
static volatile uint32_t g_idle_cycles_total = 0;
static volatile uint32_t g_idle_enter_cycle = 0;
static volatile uint8_t g_in_idle = 0;

/* ============================================================================
 * DWT Cycle Counter Functions
 * ============================================================================ */

/* Internal inline version for hot paths */
#define GET_CYCLE_COUNT() (DWT->CYCCNT)

/**
 * @brief  Initialize DWT cycle counter for CPU load measurement
 */
void CPULoad_InitCounter(void) {
  /* Enable DWT and cycle counter */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/**
 * @brief  Get current cycle count
 * @retval Current DWT cycle count
 */
uint32_t CPULoad_GetCycleCount(void) {
  return DWT->CYCCNT;
}

/* ============================================================================
 * Idle Time Measurement Functions
 * ============================================================================ */

/**
 * @brief  Enter idle state (call from idle thread)
 */
void CPULoad_IdleEnter(void) {
  if (!g_in_idle) {
    g_idle_enter_cycle = GET_CYCLE_COUNT();
    g_in_idle = 1;
  }
}

/**
 * @brief  Exit idle state (call from idle thread)
 */
void CPULoad_IdleExit(void) {
  if (g_in_idle) {
    uint32_t exit_cycle = GET_CYCLE_COUNT();
    uint32_t elapsed = exit_cycle - g_idle_enter_cycle;
    g_idle_cycles_total += elapsed;
    g_in_idle = 0;
  }
}

/* ============================================================================
 * CPU Load Measurement Functions
 * ============================================================================ */

/**
 * @brief  Clamp CPU load value to valid range [0.0, 100.0]
 */
static float CPULoad_Clamp(float load) {
  if (load < 0.0f) {
    return 0.0f;
  }
  if (load > 100.0f) {
    return 100.0f;
  }
  return load;
}

/**
 * @brief  Initialize CPU load tracking structure
 * @param  cpu_load: Pointer to CPU load info structure
 */
void CPULoad_Init(cpuload_info_t *cpu_load) {
  memset(cpu_load, 0, sizeof(cpuload_info_t));
  g_idle_cycles_total = 0;
  cpu_load->cpu_load_ema = 0.0f;
  cpu_load->initialized = 0;
}

/**
 * @brief  Update CPU load measurement (call periodically)
 * @param  cpu_load: Pointer to CPU load info structure
 */
void CPULoad_Update(cpuload_info_t *cpu_load) {
  uint32_t current_tick = HAL_GetTick();
  uint32_t current_total = GET_CYCLE_COUNT();
  uint32_t current_idle = g_idle_cycles_total;
  float instant_load = 0.0f;

  if (!cpu_load->initialized) {
    /* First sample: initialize values */
    cpu_load->last_total = current_total;
    cpu_load->last_idle = current_idle;
    cpu_load->last_tick = current_tick;
    cpu_load->cpu_load_ema = 0.0f;
    cpu_load->initialized = 1;
    return;
  }

  /* Calculate instant CPU load from delta */
  uint32_t total_delta = current_total - cpu_load->last_total;
  if (total_delta > 0) {
    uint32_t idle_delta = current_idle - cpu_load->last_idle;
    instant_load = 100.0f * (1.0f - (float)idle_delta / (float)total_delta);
    instant_load = CPULoad_Clamp(instant_load);
  }

  /* Update EMA: new_ema = alpha * instant + (1 - alpha) * old_ema */
  cpu_load->cpu_load_ema = CPU_LOAD_EMA_ALPHA * instant_load +
                           (1.0f - CPU_LOAD_EMA_ALPHA) * cpu_load->cpu_load_ema;

  /* Update last values for next sample */
  cpu_load->last_total = current_total;
  cpu_load->last_idle = current_idle;
  cpu_load->last_tick = current_tick;
}

/**
 * @brief  Get smoothed CPU load percentage
 * @param  cpu_load: Pointer to CPU load info structure
 * @retval CPU load percentage (0.0 - 100.0)
 */
float CPULoad_GetSmoothed(const cpuload_info_t *cpu_load) {
  if (!cpu_load->initialized) {
    return 0.0f;
  }
  return cpu_load->cpu_load_ema;
}
