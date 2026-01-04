/**
 ******************************************************************************
 * @file    app_buffers.c
 * @author  Long Liangmao
 * @brief   Centralized buffer management for display and camera pipelines
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

#include "app_buffers.h"
#include "stm32n6xx_hal.h"
#include "utils.h"
#include <string.h>

/* Camera display buffers (RGB565) - buffering */
uint8_t camera_display_buffers[DISPLAY_BUFFER_NB][DISPLAY_LETTERBOX_WIDTH * DISPLAY_LETTERBOX_HEIGHT * DISPLAY_BPP] ALIGN_32 IN_PSRAM;

/* Buffer state - accessed from ISR context */
volatile int camera_display_idx = 1;
volatile int camera_capture_idx = 0;

/* UI foreground buffer (ARGB8888) */
uint8_t ui_buffer[LCD_WIDTH * LCD_HEIGHT * 4] ALIGN_32 IN_PSRAM;

/**
 * @brief  Initialize all buffers and cache
 */
void Buffer_Init(void) {
  memset(camera_display_buffers, 0, sizeof(camera_display_buffers));
  SCB_CleanInvalidateDCache_by_Addr((void *)camera_display_buffers, sizeof(camera_display_buffers));

  memset(ui_buffer, 0, sizeof(ui_buffer));
  SCB_CleanInvalidateDCache_by_Addr((void *)ui_buffer, sizeof(ui_buffer));

  camera_display_idx = 1;
  camera_capture_idx = 0;
}
