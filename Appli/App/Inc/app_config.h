/**
 ******************************************************************************
 * @file    app_config.h
 * @author  Long Liangmao
 * @brief   Post-processing configuration for object detection model
 ******************************************************************************
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "arm_math.h"
#include "model_config.h"

#define POSTPROCESS_TYPE MDL_PP_TYPE

#if MDL_PP_TYPE == POSTPROCESS_OD_ST_YOLOX_UI

#define AI_OD_ST_YOLOX_PP_NB_CLASSES       MDL_PP_NB_CLASSES
#define AI_OD_ST_YOLOX_PP_NB_ANCHORS       MDL_PP_NB_ANCHORS
#define AI_OD_ST_YOLOX_PP_L_GRID_WIDTH     MDL_PP_L_GRID_W
#define AI_OD_ST_YOLOX_PP_L_GRID_HEIGHT    MDL_PP_L_GRID_H
#define AI_OD_ST_YOLOX_PP_L_NB_INPUT_BOXES (MDL_PP_L_GRID_W * MDL_PP_L_GRID_H)
#define AI_OD_ST_YOLOX_PP_M_GRID_WIDTH     MDL_PP_M_GRID_W
#define AI_OD_ST_YOLOX_PP_M_GRID_HEIGHT    MDL_PP_M_GRID_H
#define AI_OD_ST_YOLOX_PP_M_NB_INPUT_BOXES (MDL_PP_M_GRID_W * MDL_PP_M_GRID_H)
#define AI_OD_ST_YOLOX_PP_S_GRID_WIDTH     MDL_PP_S_GRID_W
#define AI_OD_ST_YOLOX_PP_S_GRID_HEIGHT    MDL_PP_S_GRID_H
#define AI_OD_ST_YOLOX_PP_S_NB_INPUT_BOXES (MDL_PP_S_GRID_W * MDL_PP_S_GRID_H)
#define AI_OD_ST_YOLOX_PP_CONF_THRESHOLD   MDL_PP_CONF_THRESHOLD
#define AI_OD_ST_YOLOX_PP_IOU_THRESHOLD    MDL_PP_IOU_THRESHOLD
#define AI_OD_ST_YOLOX_PP_MAX_BOXES_LIMIT  MDL_PP_MAX_BOXES

#elif MDL_PP_TYPE == POSTPROCESS_OD_YOLO_V8_UI

#define AI_OD_YOLOV8_PP_NB_CLASSES      MDL_PP_NB_CLASSES
#define AI_OD_YOLOV8_PP_TOTAL_BOXES     MDL_PP_TOTAL_BOXES
#define AI_OD_YOLOV8_PP_MAX_BOXES_LIMIT MDL_PP_MAX_BOXES
#define AI_OD_YOLOV8_PP_CONF_THRESHOLD  MDL_PP_CONF_THRESHOLD
#define AI_OD_YOLOV8_PP_IOU_THRESHOLD   MDL_PP_IOU_THRESHOLD

#endif

#endif /* APP_CONFIG_H */
