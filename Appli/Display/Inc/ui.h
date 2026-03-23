/**
 ******************************************************************************
 * @file    app_ui.h
 * @author  Long Liangmao
 * @brief   UI for STM32N6570-DK
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

#ifndef UI_H
#define UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lcd_config.h"
#include <stdint.h>

/* UI display double-buffer storage */
extern uint8_t ui_display_buffers[2][LCD_WIDTH * LCD_HEIGHT * 4];
extern volatile int ui_display_idx;

#define Buffer_GetUIDisplayIndex()     (ui_display_idx)
#define Buffer_GetNextUIDisplayIndex() ((ui_display_idx) ^ 1)

#define Buffer_GetUIBuffer(idx)                              \
  ({                                                         \
    int _idx = (idx);                                        \
    ((unsigned)_idx >= 2) ? NULL : ui_display_buffers[_idx]; \
  })

#define Buffer_GetUIFrontBuffer() (ui_display_buffers[ui_display_idx])
#define Buffer_GetUIBackBuffer()  (ui_display_buffers[ui_display_idx ^ 1])

#define Buffer_SetUIDisplayIndex(idx)          \
  ({                                           \
    int _idx = (idx);                          \
    int _ret = ((unsigned)_idx >= 2) ? -1 : 0; \
    if (_ret == 0) {                           \
      ui_display_idx = _idx;                   \
    }                                          \
    _ret;                                      \
  })

/**
 * @brief  Initialize the UI
 * @note   Must be called after DISPLAY_Init() and Postprocess_Thread_Start()
 */
void UI_ThreadStart(void);

/**
 * @brief  Show/hide the UI
 * @param  visible: 1 to show, 0 to hide
 */
void UI_SetVisible(uint8_t visible);

/**
 * @brief  Check if UI is visible
 * @retval 1 if visible, 0 if hidden
 */
uint8_t UI_IsVisible(void);

/**
 * @brief  Toggle ToF alert overlay (depth grid, proximity section, alert banner)
 */
void UI_ToggleTOFOverlay(void);

/**
 * @brief  Suspend UI and idle measurement threads for power measurement
 */
void UI_ThreadSuspend(void);

/**
 * @brief  Resume UI and idle measurement threads
 */
void UI_ThreadResume(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_H */
