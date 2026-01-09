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

#ifndef APP_UI_H
#define APP_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief  Initialize the UI
 * @note   Must be called after LCD_Init() and Postprocess_Thread_Init()
 */
void UI_Init(void);

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
 * @brief  Update UI
 */
void UI_Update(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_UI_H */
