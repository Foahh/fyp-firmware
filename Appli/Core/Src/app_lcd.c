/**
 ******************************************************************************
 * @file    app_lcd.c
 * @author  Long Liangmao
 * @brief   LTDC display pipeline implementation for STM32N6570-DK
 *          Layer 0: Live DCMI/camera preview (RGB565)
 *          Layer 1: UI (ARGB8888 with alpha blending)
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

#include "app_lcd.h"
#include "app_buffers.h"
#include "stm32n6570_discovery_lcd.h"

static uint8_t lcd_initialized = 0;

/**
 * @brief  Configure an LCD layer
 * @param  layer: Layer index
 * @param  x0, y0: Top-left corner
 * @param  x1, y1: Bottom-right corner
 * @param  format: Pixel format
 * @param  buffer: Frame buffer address
 * @retval 0 on success, -1 on failure
 */
static int LCD_ConfigLayer(uint32_t layer, uint16_t x0, uint16_t y0,
                           uint16_t x1, uint16_t y1,
                           uint32_t format, void *buffer) {
  BSP_LCD_LayerConfig_t config = {
      .X0 = x0,
      .Y0 = y0,
      .X1 = x1,
      .Y1 = y1,
      .PixelFormat = format,
      .Address = (uint32_t)buffer,
  };

  return (BSP_LCD_ConfigLayer(0, layer, &config) == BSP_ERROR_NONE) ? 0 : -1;
}

/**
 * @brief  Initialize LTDC with dual-layer configuration
 * @retval 0 on success, negative error code on failure
 */
int LCD_Init(void) {
  uint8_t *camera_buf, *ui_buf;
  int ret;

  if (lcd_initialized) {
    return 0;
  }

  ret = BSP_LCD_InitEx(0, LCD_ORIENTATION_LANDSCAPE,
                       LCD_PIXEL_FORMAT_RGB565,
                       LCD_WIDTH, LCD_HEIGHT);
  if (ret != BSP_ERROR_NONE) {
    return -1;
  }

  /* Configure Layer 0: Camera preview (letterboxed) */
  camera_buf = Buffer_GetCameraDisplayBuffer(0);
  if (camera_buf == NULL) {
    BSP_LCD_DeInit(0);
    return -2;
  }
  ret = LCD_ConfigLayer(LCD_LAYER_0_CAMERA,
                        DISPLAY_LETTERBOX_X0, 0,
                        DISPLAY_LETTERBOX_X1, LCD_HEIGHT,
                        LCD_PIXEL_FORMAT_RGB565, camera_buf);
  if (ret != 0) {
    BSP_LCD_DeInit(0);
    return -2;
  }

  /* Configure Layer 1: UI overlay (top half) */
  ui_buf = Buffer_GetUIBuffer();
  if (ui_buf == NULL) {
    BSP_LCD_DeInit(0);
    return -3;
  }
  ret = LCD_ConfigLayer(LCD_LAYER_1_UI,
                        0, 0,
                        LCD_WIDTH, LCD_HEIGHT / 2,
                        LCD_PIXEL_FORMAT_ARGB8888, ui_buf);
  if (ret != 0) {
    BSP_LCD_DeInit(0);
    return -3;
  }

  /* Enable layers, set UI transparent initially */
  BSP_LCD_SetLayerVisible(0, LCD_LAYER_0_CAMERA, ENABLE);
  BSP_LCD_SetLayerVisible(0, LCD_LAYER_1_UI, ENABLE);
  BSP_LCD_SetTransparency(0, LCD_LAYER_1_UI, 0);
  BSP_LCD_Reload(0, BSP_LCD_RELOAD_IMMEDIATE);
  BSP_LCD_DisplayOn(0);

  lcd_initialized = 1;
  return 0;
}

/**
 * @brief  Deinitialize LTDC
 */
void LCD_DeInit(void) {
  if (lcd_initialized) {
    BSP_LCD_DisplayOff(0);
    BSP_LCD_DeInit(0);
    lcd_initialized = 0;
  }
}

/**
 * @brief  Reload Layer 0 with new buffer address (triple buffering)
 * @param  frame_buffer: Pointer to the next display buffer
 * @retval 0 on success, negative error code on failure
 */
int LCD_ReloadCameraLayer(uint8_t *frame_buffer) {
  if (!lcd_initialized || frame_buffer == NULL) {
    return -1;
  }

  if (HAL_LTDC_SetAddress_NoReload(&hlcd_ltdc, (uint32_t)frame_buffer,
                                   LCD_LAYER_0_CAMERA) != HAL_OK) {
    return -2;
  }

  if (HAL_LTDC_ReloadLayer(&hlcd_ltdc, LTDC_RELOAD_VERTICAL_BLANKING,
                           LCD_LAYER_0_CAMERA) != HAL_OK) {
    return -3;
  }

  return 0;
}

/**
 * @brief  Set Layer 1 (UI) transparency
 * @param  alpha: 0 = transparent, 255 = opaque
 * @retval 0 on success, negative on failure
 */
int LCD_SetUIAlpha(uint8_t alpha) {
  if (!lcd_initialized) {
    return -1;
  }

  if (BSP_LCD_SetTransparency(0, LCD_LAYER_1_UI, alpha) != BSP_ERROR_NONE) {
    return -2;
  }

  return (BSP_LCD_Reload(0, BSP_LCD_RELOAD_VERTICAL_BLANKING) == BSP_ERROR_NONE) ? 0 : -3;
}

/**
 * @brief  Set layer visibility
 * @param  layer: Layer index
 * @param  enable: 1 to enable, 0 to disable
 * @retval 0 on success, negative on failure
 */
static int LCD_SetLayerVisible(uint32_t layer, uint8_t enable) {
  if (!lcd_initialized) {
    return -1;
  }

  if (BSP_LCD_SetLayerVisible(0, layer, enable ? ENABLE : DISABLE) != BSP_ERROR_NONE) {
    return -2;
  }

  return (BSP_LCD_Reload(0, BSP_LCD_RELOAD_VERTICAL_BLANKING) == BSP_ERROR_NONE) ? 0 : -3;
}

/**
 * @brief  Enable or disable Layer 1 (UI)
 * @param  enable: 1 to enable, 0 to disable
 * @retval 0 on success, negative on failure
 */
int LCD_SetUILayerVisible(uint8_t enable) {
  return LCD_SetLayerVisible(LCD_LAYER_1_UI, enable);
}

/**
 * @brief  Enable or disable Layer 0 (Camera)
 * @param  enable: 1 to enable, 0 to disable
 * @retval 0 on success, negative on failure
 */
int LCD_SetCameraLayerVisible(uint8_t enable) {
  return LCD_SetLayerVisible(LCD_LAYER_0_CAMERA, enable);
}

/**
 * @brief  Trigger LTDC reload
 * @param  reload_type: BSP_LCD_RELOAD_IMMEDIATE or BSP_LCD_RELOAD_VERTICAL_BLANKING
 * @retval 0 on success, negative on failure
 */
int LCD_Reload(uint32_t reload_type) {
  if (!lcd_initialized) {
    return -1;
  }

  return (BSP_LCD_Reload(0, reload_type) == BSP_ERROR_NONE) ? 0 : -2;
}
