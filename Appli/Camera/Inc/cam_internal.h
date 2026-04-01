/**
 ******************************************************************************
 * @file    cam_internal.h
 * @author  Long Liangmao
 * @brief   Internal shared state and declarations for camera module
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

#ifndef CAM_INTERNAL_H
#define CAM_INTERNAL_H

#include "thread_config.h"
#include "bqueue.h"
#include "cam.h"
#include "cmw_camera.h"
#include "nn.h"
#include "stm32n6xx_hal.h"

/* ============================================================================
 * Shared State (extern declarations)
 * ============================================================================ */

/* NN pipe running state (defined in cam_pipe.c) */
extern volatile uint8_t nn_pipe_running;

/* NN crop ROI in sensor coordinates (defined in cam_init.c) */
typedef struct {
  uint32_t sensor_w;
  uint32_t sensor_h;
  CMW_Manual_roi_area_t nn_roi;
} nn_crop_info_t;

extern nn_crop_info_t nn_crop_info;
extern nn_crop_info_display_t roi_info_d;

/* ISP thread resources (defined in cam.c) */
typedef struct {
  TX_EVENT_FLAGS_GROUP vsync_flags;
  TX_THREAD thread;
  UCHAR stack[ISP_THREAD_STACK_SIZE];
} isp_ctx_t;

extern isp_ctx_t isp_ctx;

/* LCD reload thread resources (defined in cam.c) */
typedef struct {
  TX_EVENT_FLAGS_GROUP reload_flags;
  TX_THREAD thread;
  UCHAR stack[LCD_RELOAD_THREAD_STACK_SIZE];
  volatile uint8_t *pending_buffer;
} lcd_reload_ctx_t;

extern lcd_reload_ctx_t lcd_reload_ctx;

/* ============================================================================
 * Internal Function Declarations
 * ============================================================================ */

void CAM_CalcCropRoi(CMW_Manual_roi_area_t *roi,
                     uint32_t sensor_w, uint32_t sensor_h,
                     uint32_t output_w, uint32_t output_h);

void CAM_UpdateDisplayROI(CMW_Manual_roi_area_t *roi,
                          uint32_t sensor_w, uint32_t sensor_h);

void CAM_ConfigPipe(uint32_t pipe,
                    uint32_t sensor_w, uint32_t sensor_h,
                    uint32_t out_w, uint32_t out_h,
                    uint32_t format, uint32_t bpp,
                    int swap_enabled);

void CAM_IspUpdate(void);

#endif /* CAM_INTERNAL_H */
