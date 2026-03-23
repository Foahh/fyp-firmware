/**
 ******************************************************************************
 * @file    app_lcd.c
 * @author  Long Liangmao
 * @brief   LTDC display pipeline implementation for STM32N6570-DK
 *          Layer 0: DCMIPP preview (RGB565)
 *          Layer 1: UI (ARGB8888)
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

#include "app_error.h"
#include "app_lcd.h"
#include "cam.h"
#include "stm32_lcd.h"
#include "stm32n6570_discovery_lcd.h"
#include "stm32n6xx_hal.h"
#include "ui.h"
#include <string.h>

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void LCD_ConfigLayer(uint32_t layer, uint16_t x0, uint16_t y0,
                            uint16_t x1, uint16_t y1,
                            uint32_t format, void *buffer);
static void LCD_SetLayerVisible(uint32_t layer, uint8_t enable);

/* ============================================================================
 * Global State Variables
 * ============================================================================ */

static uint8_t lcd_initialized = 0;

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief  Configure an LCD layer
 * @param  layer: Layer index
 * @param  x0: Top-left X coordinate
 * @param  y0: Top-left Y coordinate
 * @param  x1: Bottom-right X coordinate
 * @param  y1: Bottom-right Y coordinate
 * @param  format: Pixel format
 * @param  buffer: Frame buffer address
 */
static void LCD_ConfigLayer(uint32_t layer, uint16_t x0, uint16_t y0,
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

  APP_REQUIRE(BSP_LCD_ConfigLayer(0, layer, &config) == BSP_ERROR_NONE);
}

/**
 * @brief  Set layer visibility
 * @param  layer: Layer index
 * @param  enable: 1 to enable, 0 to disable
 */
static void LCD_SetLayerVisible(uint32_t layer, uint8_t enable) {
  APP_REQUIRE(lcd_initialized);
  APP_REQUIRE(BSP_LCD_SetLayerVisible(0, layer, enable ? ENABLE : DISABLE) == BSP_ERROR_NONE);
  APP_REQUIRE(BSP_LCD_Reload(0, BSP_LCD_RELOAD_VERTICAL_BLANKING) == BSP_ERROR_NONE);
}

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

/**
 * @brief  Initialize LTDC with dual-layer configuration (HAL/BSP only)
 */
void LCD_Init(void) {
  uint8_t *camera_buf, *ui_buf;

  if (lcd_initialized) {
    return;
  }

  /* Clear display buffers and invalidate cache */
  memset(camera_display_buffers, 0, sizeof(camera_display_buffers));
  SCB_CleanInvalidateDCache_by_Addr((void *)camera_display_buffers, sizeof(camera_display_buffers));
  memset(ui_display_buffers, 0, sizeof(ui_display_buffers));
  SCB_CleanInvalidateDCache_by_Addr((void *)ui_display_buffers, sizeof(ui_display_buffers));

  APP_REQUIRE(BSP_LCD_InitEx(0, LCD_ORIENTATION_LANDSCAPE,
                             LCD_PIXEL_FORMAT_RGB565,
                             LCD_WIDTH, LCD_HEIGHT) == BSP_ERROR_NONE);

  /* Configure Layer 0: DCMIPP preview */
  camera_buf = Buffer_GetCameraDisplayBuffer(0);
  APP_REQUIRE(camera_buf != NULL);
  LCD_ConfigLayer(LCD_LAYER_0_CAMERA,
                  DISPLAY_LETTERBOX_X0, 0,
                  DISPLAY_LETTERBOX_X1, LCD_HEIGHT,
                  LCD_PIXEL_FORMAT_RGB565, camera_buf);

  /* Configure Layer 1: UI overlay */
  ui_buf = Buffer_GetUIFrontBuffer();
  APP_REQUIRE(ui_buf != NULL);
  LCD_ConfigLayer(LCD_LAYER_1_UI,
                  0, 0,
                  LCD_WIDTH, LCD_HEIGHT,
                  LCD_PIXEL_FORMAT_ARGB8888, ui_buf);

  /* Enable layers, set UI transparent initially */
  BSP_LCD_SetLayerVisible(0, LCD_LAYER_0_CAMERA, ENABLE);
  BSP_LCD_SetLayerVisible(0, LCD_LAYER_1_UI, ENABLE);
  BSP_LCD_SetTransparency(0, LCD_LAYER_1_UI, 0);
  BSP_LCD_Reload(0, BSP_LCD_RELOAD_IMMEDIATE);
  BSP_LCD_DisplayOn(0);

  UTIL_LCD_SetFuncDriver(&LCD_Driver);

  lcd_initialized = 1;
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
 * @brief  Reload Layer 0 with buffer address (buffering)
 *         Called from frame event callback
 * @param  frame_buffer: Pointer to the next display buffer
 */
void LCD_ReloadCameraLayer(uint8_t *frame_buffer) {
  HAL_StatusTypeDef status;

  APP_REQUIRE(lcd_initialized);
  APP_REQUIRE(frame_buffer != NULL);

  SCB_CleanDCache_by_Addr((void *)frame_buffer,
                          DISPLAY_LETTERBOX_WIDTH * DISPLAY_LETTERBOX_HEIGHT * DISPLAY_BPP);

  HAL_NVIC_DisableIRQ(DCMIPP_IRQn);
  status = HAL_LTDC_SetAddress_NoReload(&hlcd_ltdc, (uint32_t)frame_buffer,
                                        LCD_LAYER_0_CAMERA);
  if (status == HAL_OK) {
    status = HAL_LTDC_ReloadLayer(&hlcd_ltdc, LTDC_RELOAD_VERTICAL_BLANKING,
                                  LCD_LAYER_0_CAMERA);
  }
  HAL_NVIC_EnableIRQ(DCMIPP_IRQn);
  APP_REQUIRE(status == HAL_OK);
}

/**
 * @brief  Set UI layer buffer address for drawing (without reloading display)
 *         Only updates the HAL struct so BSP draw functions target the correct
 *         buffer.  Does NOT touch the LTDC shadow register — that is deferred
 *         to LCD_ReloadUILayer() to avoid a race where an early shadow-register
 *         write causes VBlank reload to display a partially-drawn back buffer.
 * @param  frame_buffer: Pointer to the buffer to draw to
 */
void LCD_SetUILayerAddress(uint8_t *frame_buffer) {
  APP_REQUIRE(lcd_initialized);
  APP_REQUIRE(frame_buffer != NULL);

  hlcd_ltdc.LayerCfg[LCD_LAYER_1_UI].FBStartAdress =
      (uint32_t)frame_buffer;
}

/**
 * @brief  Reload Layer 1 (UI) with buffer address (double buffering)
 *         Called after UI rendering is complete
 * @param  frame_buffer: Pointer to the next UI display buffer
 */
void LCD_ReloadUILayer(uint8_t *frame_buffer) {
  HAL_StatusTypeDef status;

  APP_REQUIRE(lcd_initialized);
  APP_REQUIRE(frame_buffer != NULL);

  SCB_CleanDCache_by_Addr((void *)frame_buffer, LCD_WIDTH * LCD_HEIGHT * 4);

  HAL_NVIC_DisableIRQ(DCMIPP_IRQn);
  status = HAL_LTDC_SetAddress_NoReload(&hlcd_ltdc, (uint32_t)frame_buffer,
                                        LCD_LAYER_1_UI);
  if (status == HAL_OK) {
    status = HAL_LTDC_ReloadLayer(&hlcd_ltdc, LTDC_RELOAD_VERTICAL_BLANKING,
                                  LCD_LAYER_1_UI);
  }
  HAL_NVIC_EnableIRQ(DCMIPP_IRQn);
  APP_REQUIRE(status == HAL_OK);
}

/**
 * @brief  Set Layer 1 (UI) transparency/alpha
 * @param  alpha: Alpha value (0-255, 0 = fully transparent, 255 = fully opaque)
 */
void LCD_SetUIAlpha(uint8_t alpha) {
  APP_REQUIRE(lcd_initialized);
  APP_REQUIRE(BSP_LCD_SetTransparency(0, LCD_LAYER_1_UI, alpha) == BSP_ERROR_NONE);
  APP_REQUIRE(BSP_LCD_Reload(0, BSP_LCD_RELOAD_VERTICAL_BLANKING) == BSP_ERROR_NONE);
}

/**
 * @brief  Enable or disable Layer 1 (UI)
 * @param  enable: 1 to enable, 0 to disable
 */
void LCD_SetUILayerVisible(uint8_t enable) {
  LCD_SetLayerVisible(LCD_LAYER_1_UI, enable);
}

/**
 * @brief  Enable or disable Layer 0 (Camera)
 * @param  enable: 1 to enable, 0 to disable
 */
void LCD_SetCameraLayerVisible(uint8_t enable) {
  LCD_SetLayerVisible(LCD_LAYER_0_CAMERA, enable);
}

/**
 * @brief  Handle LTDC reload (call after frame buffer updates)
 * @param  reload_type: BSP_LCD_RELOAD_IMMEDIATE or BSP_LCD_RELOAD_VERTICAL_BLANKING
 */
void LCD_Reload(uint32_t reload_type) {
  APP_REQUIRE(lcd_initialized);
  APP_REQUIRE(BSP_LCD_Reload(0, reload_type) == BSP_ERROR_NONE);
}
