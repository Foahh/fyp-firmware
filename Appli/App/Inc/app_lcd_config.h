/**
 ******************************************************************************
 * @file    app_lcd_config.h
 * @author  Long Liangmao
 * @brief   Display/LCD configuration for STM32N6570-DK
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
#ifndef APP_LCD_CONFIG_H
#define APP_LCD_CONFIG_H

/* Define display size */
#define LCD_WIDTH  800
#define LCD_HEIGHT 480

/* Letterboxed display size (maintains camera 4:3 aspect ratio) */
/* Camera sensor: 2592x1944 (4:3), LCD: 800x480 (5:3) */
/* Letterboxed: 640x480 (4:3) positioned on right side */
#define DISPLAY_LETTERBOX_WIDTH  640
#define DISPLAY_LETTERBOX_HEIGHT 480
#define DISPLAY_LETTERBOX_X0     (LCD_WIDTH - DISPLAY_LETTERBOX_WIDTH) /* 160 - left margin for black bars */
#define DISPLAY_LETTERBOX_X1     LCD_WIDTH                             /* 800 - right edge */

/* Display format and bits per pixel */
#define DISPLAY_FORMAT DCMIPP_PIXEL_PACKER_FORMAT_RGB565_1
#define DISPLAY_BPP    2

#endif /* APP_LCD_CONFIG_H */
