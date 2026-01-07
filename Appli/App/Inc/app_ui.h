/**
 ******************************************************************************
 * @file    app_ui.h
 * @author  Long Liangmao
 * @brief   Diagnostic UI overlay for STM32N6570-DK
 *          Displays CPU load, FPS, and other runtime metrics on LCD Layer 1
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

#ifndef APP_UI_H
#define APP_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tx_api.h"
#include <stdint.h>

/* EMA smoothing factor (0.0 - 1.0)
 * Lower values = more smoothing, higher values = more responsive
 * 0.2 provides good balance for 10Hz display refresh rate
 */
#define CPU_LOAD_EMA_ALPHA 0.2f

/**
 * @brief  CPU load measurement structure (EMA-based)
 */
typedef struct {
  uint32_t last_total;  /* Last total cycle count */
  uint32_t last_idle;   /* Last idle cycle count */
  uint32_t last_tick;   /* Last update timestamp (ms) */
  float cpu_load_ema;   /* Exponential moving average of CPU load (%) */
  uint8_t initialized;  /* Flag to indicate first sample */
} cpuload_info_t;

/**
 * @brief  Initialize CPU load tracking
 * @param  cpu_load: Pointer to CPU load info structure
 */
void UI_CPULoad_Init(cpuload_info_t *cpu_load);

/**
 * @brief  Update CPU load measurement (call periodically from main loop/thread)
 * @param  cpu_load: Pointer to CPU load info structure
 */
void UI_CPULoad_Update(cpuload_info_t *cpu_load);

/**
 * @brief  Get smoothed CPU load percentage (EMA)
 * @param  cpu_load: Pointer to CPU load info structure
 * @retval Smoothed CPU load percentage (0.0 - 100.0)
 */
float UI_CPULoad_GetSmoothed(const cpuload_info_t *cpu_load);

/**
 * @brief  Initialize the UI diagnostic display
 * @note   Must be called after LCD_Init()
 */
void UI_Init(void);

/**
 * @brief  Update and render diagnostic display
 * @note   Call this periodically from the display thread
 */
void UI_Update(void);

/**
 * @brief  Show/hide the diagnostic overlay
 * @param  visible: 1 to show, 0 to hide
 */
void UI_SetVisible(uint8_t visible);

/**
 * @brief  Check if diagnostic overlay is visible
 * @retval 1 if visible, 0 if hidden
 */
uint8_t UI_IsVisible(void);

/**
 * @brief  Idle thread entry hook - call at start of idle loop iteration
 * @note   Call this from tx_application_define or idle thread entry
 */
void UI_IdleThread_Enter(void);

/**
 * @brief  Idle thread exit hook - call at end of idle loop iteration
 * @note   Call this from idle thread before yielding
 */
void UI_IdleThread_Exit(void);

/**
 * @brief  Get current DWT cycle counter value
 * @retval Current cycle count
 */
uint32_t UI_GetCycleCount(void);

/**
 * @brief  Initialize DWT cycle counter for profiling
 */
void UI_InitCycleCounter(void);

/**
 * @brief  Process pending UI events (call from UI thread)
 * @param  timeout: Timeout in ThreadX ticks (TX_NO_WAIT, TX_WAIT_FOREVER, or ticks)
 * @retval TX_SUCCESS if event processed, TX_QUEUE_EMPTY if no events, error otherwise
 * @note   This function should be called periodically from the UI thread
 */
UINT UI_ProcessEvents(ULONG timeout);

/**
 * @brief  Request UI update via event bus
 * @note   This function can be called from any context to trigger UI refresh
 */
void UI_RequestUpdate(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_UI_H */
