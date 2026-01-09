/**
 ******************************************************************************
 * @file    app_config.h
 * @author  Long Liangmao
 * @brief   Application configuration header for STM32N6570-DK
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
#ifndef APP_CONFIG
#define APP_CONFIG

/* CMSIS DSP types (for float32_t, etc.) */
#include "arm_math.h"

/* Camera FPS configuration */
#define CAMERA_FPS 30

/* Define sensor orientation */
#define CAMERA_FLIP CMW_MIRRORFLIP_MIRROR

/* Define display size */
#define LCD_WIDTH 800
#define LCD_HEIGHT 480

/* Letterboxed display size (maintains camera 4:3 aspect ratio) */
/* Camera sensor: 2592x1944 (4:3), LCD: 800x480 (5:3) */
/* Letterboxed: 640x480 (4:3) positioned on right side */
#define DISPLAY_LETTERBOX_WIDTH 640
#define DISPLAY_LETTERBOX_HEIGHT 480
#define DISPLAY_LETTERBOX_X0 (LCD_WIDTH - DISPLAY_LETTERBOX_WIDTH) /* 160 - left margin for black bars */
#define DISPLAY_LETTERBOX_X1 LCD_WIDTH                             /* 800 - right edge */

/* Display format and bits per pixel */
#define DISPLAY_FORMAT DCMIPP_PIXEL_PACKER_FORMAT_RGB565_1
#define DISPLAY_BPP 2

/* Machine Learning pipeline configuration for AI inference */
#define ML_WIDTH 480
#define ML_HEIGHT 480
#define ML_FORMAT DCMIPP_PIXEL_PACKER_FORMAT_RGB888_YUV444_1
#define ML_BPP 3

/* ===========================================================================
 * Post-processing configuration for ST YOLO-X person detection model
 * Model: st_yolo_x_nano_480_1.0_0.25_3_int8.tflite
 * ===========================================================================
 */

/* Post-processing type selection */
#define POSTPROCESS_TYPE POSTPROCESS_OD_ST_YOLOX_UF

/* ST YOLO-X specific configuration */
#define AI_OD_ST_YOLOX_PP_NB_CLASSES 1 /* Single class: person */
#define AI_OD_ST_YOLOX_PP_NB_ANCHORS 1 /* Anchor-free detection */

/* Grid sizes for multi-scale detection (480x480 input) */
/* Large scale: 480/8 = 60 */
#define AI_OD_ST_YOLOX_PP_L_GRID_WIDTH 60
#define AI_OD_ST_YOLOX_PP_L_GRID_HEIGHT 60
/* Medium scale: 480/16 = 30 */
#define AI_OD_ST_YOLOX_PP_M_GRID_WIDTH 30
#define AI_OD_ST_YOLOX_PP_M_GRID_HEIGHT 30
/* Small scale: 480/32 = 15 */
#define AI_OD_ST_YOLOX_PP_S_GRID_WIDTH 15
#define AI_OD_ST_YOLOX_PP_S_GRID_HEIGHT 15

/* Anchor values (anchor-free model uses 1.0). */
static const float32_t AI_OD_ST_YOLOX_PP_L_ANCHORS[2 * AI_OD_ST_YOLOX_PP_NB_ANCHORS] = {1.0f, 1.0f};
static const float32_t AI_OD_ST_YOLOX_PP_M_ANCHORS[2 * AI_OD_ST_YOLOX_PP_NB_ANCHORS] = {1.0f, 1.0f};
static const float32_t AI_OD_ST_YOLOX_PP_S_ANCHORS[2 * AI_OD_ST_YOLOX_PP_NB_ANCHORS] = {1.0f, 1.0f};

/* Detection thresholds */
#define AI_OD_ST_YOLOX_PP_CONF_THRESHOLD 0.5f /* Confidence threshold */
#define AI_OD_ST_YOLOX_PP_IOU_THRESHOLD 0.4f  /* NMS IoU threshold */
#define AI_OD_ST_YOLOX_PP_MAX_BOXES_LIMIT 10  /* Maximum detections */

/* Generic max boxes limit (maps to model-specific limit) */
#define AI_OD_PP_MAX_BOXES_LIMIT AI_OD_ST_YOLOX_PP_MAX_BOXES_LIMIT

#endif
