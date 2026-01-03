/**
 ******************************************************************************
 * @file    app_config.h
 * @author  Long Liangmao
 * @brief   Application configuration header for STM32N6570-DK
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 Long Liangmao.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
#ifndef APP_CONFIG
#define APP_CONFIG

/* Camera FPS configuration */
#define CAMERA_FPS 30

/* Define sensor orientation */
#define CAMERA_FLIP CMW_MIRRORFLIP_MIRROR

/* Define display size */
#define LCD_WIDTH 800
#define LCD_HEIGHT 480

/* Delay display by DISPLAY_DELAY frame number */
#define DISPLAY_DELAY 1

#define DISPLAY_FORMAT DCMIPP_PIXEL_PACKER_FORMAT_RGB565_1
#define DISPLAY_BPP 2

/* Machine Learning pipeline configuration for AI inference */
#define ML_WIDTH 480
#define ML_HEIGHT 480
#define ML_FORMAT DCMIPP_PIXEL_PACKER_FORMAT_RGB888_YUV444_1
#define ML_BPP 3

#endif
