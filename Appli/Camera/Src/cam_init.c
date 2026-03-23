/**
 ******************************************************************************
 * @file    cam_init.c
 * @author  Long Liangmao
 * @brief   Camera initialization, configuration, and crop ROI calculation
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
#include "cam_config.h"
#include "cam_internal.h"
#include "lcd_config.h"
#include "nn_config.h"
#include "utils.h"

/* ============================================================================
 * Global State Definitions
 * ============================================================================ */

/* NN crop ROI in sensor coordinates (stored for overlay visualization) */
nn_crop_info_t nn_crop_info = {0};

nn_crop_info_display_t roi_info_d = {0};

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief  Calculate centered crop ROI maintaining aspect ratio
 * @param  roi: Output ROI configuration
 * @param  sensor_w: Sensor width
 * @param  sensor_h: Sensor height
 * @param  output_w: Target output width
 * @param  output_h: Target output height
 */
void CAM_CalcCropRoi(CMW_Manual_roi_area_t *roi,
                     uint32_t sensor_w, uint32_t sensor_h,
                     uint32_t output_w, uint32_t output_h) {
  /* Scale factor = sensor / output (fixed point with 10-bit fraction) */
  const uint32_t scale_x = (sensor_w << 10) / output_w;
  const uint32_t scale_y = (sensor_h << 10) / output_h;
  const uint32_t scale = MIN(scale_x, scale_y);

  APP_REQUIRE(scale >= (1 << 10)); /* Scale must be >= 1.0 */

  roi->width = MIN((output_w * scale) >> 10, sensor_w);
  roi->height = MIN((output_h * scale) >> 10, sensor_h);
  roi->offset_x = (sensor_w - roi->width) / 2;
  roi->offset_y = (sensor_h - roi->height) / 2;
}

void CAM_UpdateDisplayROI(CMW_Manual_roi_area_t *roi,
                          uint32_t sensor_w, uint32_t sensor_h) {
  /* Map sensor coordinates to display letterbox coordinates */
  float scale_x = (float)DISPLAY_LETTERBOX_WIDTH / (float)sensor_w;
  float scale_y = (float)DISPLAY_LETTERBOX_HEIGHT / (float)sensor_h;

  roi_info_d.roi_x0 = (int)(roi->offset_x * scale_x + 0.5f);
  roi_info_d.roi_y0 = (int)(roi->offset_y * scale_y + 0.5f);
  roi_info_d.roi_x1 = (int)((roi->offset_x + roi->width) * scale_x + 0.5f);
  roi_info_d.roi_y1 = (int)((roi->offset_y + roi->height) * scale_y + 0.5f);
  roi_info_d.roi_w = roi_info_d.roi_x1 - roi_info_d.roi_x0;
  roi_info_d.roi_h = roi_info_d.roi_y1 - roi_info_d.roi_y0;
}

/**
 * @brief  Configure DCMIPP pipe with common settings
 * @param  pipe: DCMIPP pipe number (DCMIPP_PIPE1 or DCMIPP_PIPE2)
 * @param  sensor_w: Sensor width
 * @param  sensor_h: Sensor height
 * @param  out_w: Output width
 * @param  out_h: Output height
 * @param  format: Output pixel format
 * @param  bpp: Bytes per pixel
 * @param  swap_enabled: Enable byte swap (for RGB888)
 */
void CAM_ConfigPipe(uint32_t pipe,
                    uint32_t sensor_w, uint32_t sensor_h,
                    uint32_t out_w, uint32_t out_h,
                    uint32_t format, uint32_t bpp,
                    int swap_enabled) {
  CMW_DCMIPP_Conf_t conf = {
      .output_width = out_w,
      .output_height = out_h,
      .output_format = format,
      .output_bpp = bpp,
      .mode = CMW_Aspect_ratio_manual_roi,
      .enable_swap = swap_enabled,
      .enable_gamma_conversion = 0,
  };
  uint32_t hw_pitch;

  CAM_CalcCropRoi(&conf.manual_conf, sensor_w, sensor_h, out_w, out_h);

  APP_REQUIRE(CMW_CAMERA_SetPipeConfig(pipe, &conf, &hw_pitch) == HAL_OK);

  APP_REQUIRE(hw_pitch == out_w * bpp);
}

/* ============================================================================
 * HAL Callback Functions
 * ============================================================================ */

/**
 * @brief  DCMIPP clock configuration callback
 * @param  hdcmipp: DCMIPP handle
 * @retval HAL_OK on success, HAL_ERROR on failure
 */
HAL_StatusTypeDef MX_DCMIPP_ClockConfig(DCMIPP_HandleTypeDef *hdcmipp) {
  UNUSED(hdcmipp);
  RCC_PeriphCLKInitTypeDef clk = {0};

  clk.PeriphClockSelection = RCC_PERIPHCLK_DCMIPP;
  clk.DcmippClockSelection = RCC_DCMIPPCLKSOURCE_IC17;
  clk.ICSelection[RCC_IC17].ClockSelection = RCC_ICCLKSOURCE_PLL2;
  clk.ICSelection[RCC_IC17].ClockDivider = 3;
  if (HAL_RCCEx_PeriphCLKConfig(&clk) != HAL_OK) {
    return HAL_ERROR;
  }

  clk.PeriphClockSelection = RCC_PERIPHCLK_CSI;
  clk.ICSelection[RCC_IC18].ClockSelection = RCC_ICCLKSOURCE_PLL1;
  clk.ICSelection[RCC_IC18].ClockDivider = 40;
  if (HAL_RCCEx_PeriphCLKConfig(&clk) != HAL_OK) {
    return HAL_ERROR;
  }

  return HAL_OK;
}

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

/**
 * @brief  Initialize the camera module (HAL/BSP only)
 *         Configures camera sensor and both DCMIPP pipes
 * @note   This function only performs HAL/BSP initialization and does NOT create ThreadX resources
 */
void CAM_Init(void) {
  CMW_CameraInit_t cam_conf = {
      .width = 0, /* Let sensor driver choose */
      .height = 0,
      .fps = CAMERA_FPS,
      .mirror_flip = CAMERA_FLIP,
  };

  APP_REQUIRE(CMW_CAMERA_Init(&cam_conf, NULL) == CMW_ERROR_NONE);

  /* Configure display pipe (Pipe1) */
  CAM_ConfigPipe(DCMIPP_PIPE1,
                 cam_conf.width, cam_conf.height,
                 DISPLAY_LETTERBOX_WIDTH, DISPLAY_LETTERBOX_HEIGHT,
                 DISPLAY_FORMAT, DISPLAY_BPP, 0);

  /* Configure NN pipe (Pipe2) */
  CAM_ConfigPipe(DCMIPP_PIPE2,
                 cam_conf.width, cam_conf.height,
                 NN_WIDTH, NN_HEIGHT,
                 NN_FORMAT, NN_BPP, 1);

  /* Store NN crop ROI info for overlay visualization */
  nn_crop_info.sensor_w = cam_conf.width;
  nn_crop_info.sensor_h = cam_conf.height;
  CAM_CalcCropRoi(&nn_crop_info.nn_roi,
                  cam_conf.width, cam_conf.height,
                  NN_WIDTH, NN_HEIGHT);

  CAM_UpdateDisplayROI(&nn_crop_info.nn_roi, cam_conf.width, cam_conf.height);
}

/**
 * @brief  Full camera deinit (DCMIPP, CSI, sensor power down)
 */
void CAM_DeInit(void) {
  nn_pipe_running = 0;
  CMW_CAMERA_DeInit();
}

/**
 * @brief  Get NN crop ROI in display coordinates
 * @retval Pointer to nn_crop_info_display_t struct if initialized, NULL otherwise
 */
nn_crop_info_display_t *CAM_GetDisplayROI(void) {
  if (nn_crop_info.sensor_w == 0 || nn_crop_info.sensor_h == 0) {
    return NULL;
  }
  return &roi_info_d;
}
