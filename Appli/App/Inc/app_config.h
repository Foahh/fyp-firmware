/**
 ******************************************************************************
 * @file    app_postprocess_config.h
 * @author  Long Liangmao
 * @brief   Post-processing configuration for object detection model
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
#ifndef APP_POSTPROCESS_CONFIG_H
#define APP_POSTPROCESS_CONFIG_H

/* CMSIS DSP types (for float32_t, etc.) */
#include "arm_math_types.h"

/* Post-processing type selection */
#define POSTPROCESS_TYPE POSTPROCESS_OD_ST_YOLOX_UF

/* ST YOLO-X specific configuration */
#define AI_OD_ST_YOLOX_PP_NB_CLASSES 1 /* Single class: person */
#define AI_OD_ST_YOLOX_PP_NB_ANCHORS 3 /* Number of anchors */

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

/* Anchor values for 480x480 input model */
static const float32_t AI_OD_ST_YOLOX_PP_L_ANCHORS[2 * AI_OD_ST_YOLOX_PP_NB_ANCHORS] = {30.0f, 30.0f, 4.2f, 15.0f, 13.8f, 42.0f};
static const float32_t AI_OD_ST_YOLOX_PP_M_ANCHORS[2 * AI_OD_ST_YOLOX_PP_NB_ANCHORS] = {15.0f, 15.0f, 2.1f, 7.5f, 6.9f, 21.0f};
static const float32_t AI_OD_ST_YOLOX_PP_S_ANCHORS[2 * AI_OD_ST_YOLOX_PP_NB_ANCHORS] = {7.5f, 7.5f, 1.05f, 3.75f, 3.45f, 10.5f};

/* Detection thresholds */
#define AI_OD_ST_YOLOX_PP_CONF_THRESHOLD 0.6f /* Confidence threshold */
#define AI_OD_ST_YOLOX_PP_IOU_THRESHOLD 0.5f  /* NMS IoU threshold */
#define AI_OD_ST_YOLOX_PP_MAX_BOXES_LIMIT 10  /* Maximum detections */

#endif /* APP_POSTPROCESS_CONFIG_H */
