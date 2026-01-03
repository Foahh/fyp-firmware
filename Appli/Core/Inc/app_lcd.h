/**
 ******************************************************************************
 * @file    app_lcd.h
 * @author  Long Liangmao
 * @brief   LCD/LTDC dual-layer display pipeline for STM32N6570-DK
 *          Layer 0: Live DCMI/camera preview
 *          Layer 1: UI (alpha blended)
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

#ifndef APP_LCD_H
#define APP_LCD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32n6xx_hal.h"
#include "stm32n6570_discovery_lcd.h"
#include <stdint.h>
#include "app_config.h"

/* Layer definitions */
#define LCD_LAYER_0_CAMERA    0U  /* Camera preview layer */
#define LCD_LAYER_1_UI  1U  /* UI layer */

/**
 * @brief  Initialize LTDC with dual-layer configuration
 * @param  None
 * @retval 0 on success, negative error code on failure
 */
int LCD_Init(void);

/**
 * @brief  Deinitialize LTDC
 * @param  None
 * @retval None
 */
void LCD_DeInit(void);

/**
 * @brief  Reload Layer 0 with buffer address (triple buffering)
 *         Called from frame event callback
 * @param  frame_buffer: Pointer to the next display buffer
 * @retval 0 on success, negative error code on failure
 */
int LCD_ReloadCameraLayer(uint8_t *frame_buffer);

/**
 * @brief  Set Layer 1 (UI) transparency/alpha
 * @param  alpha: Alpha value (0-255, 0 = fully transparent, 255 = fully opaque)
 * @retval 0 on success, negative error code on failure
 */
int LCD_SetUIAlpha(uint8_t alpha);

/**
 * @brief  Enable or disable Layer 1 (UI)
 * @param  enable: 1 to enable, 0 to disable
 * @retval 0 on success, negative error code on failure
 */
int LCD_SetUILayerVisible(uint8_t enable);

/**
 * @brief  Enable or disable Layer 0 (Camera)
 * @param  enable: 1 to enable, 0 to disable
 * @retval 0 on success, negative error code on failure
 */
int LCD_SetCameraLayerVisible(uint8_t enable);

/**
 * @brief  Handle LTDC reload (call after frame buffer updates)
 * @param  reload_type: BSP_LCD_RELOAD_IMMEDIATE or BSP_LCD_RELOAD_VERTICAL_BLANKING
 * @retval 0 on success, negative error code on failure
 */
int LCD_Reload(uint32_t reload_type);

#ifdef __cplusplus
}
#endif

#endif /* APP_LCD_H */

