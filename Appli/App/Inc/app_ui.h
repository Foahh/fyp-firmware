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

#include <stdint.h>

/* CPU load history depth for averaging */
#define CPU_LOAD_HISTORY_DEPTH 8

/**
 * @brief  CPU load measurement structure
 */
typedef struct {
  struct {
    uint32_t total; /* Total execution cycles */
    uint32_t idle;  /* Idle thread cycles */
    uint32_t tick;  /* Timestamp (ms) */
  } history[CPU_LOAD_HISTORY_DEPTH];
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
 * @brief  Get CPU load percentages
 * @param  cpu_load: Pointer to CPU load info structure
 * @param  cpu_load_last: Output for last sample CPU load (can be NULL)
 * @param  cpu_load_last_second: Output for 1-second average (can be NULL)
 * @param  cpu_load_last_five_seconds: Output for 5-second average (can be NULL)
 */
void UI_CPULoad_GetInfo(cpuload_info_t *cpu_load,
                        float *cpu_load_last,
                        float *cpu_load_last_second,
                        float *cpu_load_last_five_seconds);

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

#ifdef __cplusplus
}
#endif

#endif /* APP_UI_H */
