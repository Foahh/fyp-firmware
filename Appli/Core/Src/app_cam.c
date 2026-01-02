/**
 ******************************************************************************
 * @file    app_cam.c
 * @author  Long Liangmao
 * @brief   Camera application implementation for STM32N6570-DK
 *          Single DCMIPP pipe configuration for display output
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

#include <assert.h>
#include "app_cam.h"
#include "stm32n6xx_hal.h"
#include "cmw_camera.h"
#include "utils.h"

/* Stored sensor resolution */
static uint32_t sensor_width = 0;
static uint32_t sensor_height = 0;

/**
 * @brief  Configure crop area to maintain aspect ratio
 * @param  roi: Pointer to ROI configuration structure
 * @param  s_width: Sensor width
 * @param  s_height: Sensor height
 * @retval None
 */
static void CAM_InitCropConfig(CMW_Manual_roi_area_t *roi, int s_width, int s_height)
{
  const float ratiox = (float)s_width / LCD_BG_WIDTH;
  const float ratioy = (float)s_height / LCD_BG_HEIGHT;
  const float ratio = MIN(ratiox, ratioy);

  assert(ratio >= 1);
  assert(ratio < 64);

  roi->width = (uint32_t)MIN(LCD_BG_WIDTH * ratio, s_width);
  roi->height = (uint32_t)MIN(LCD_BG_HEIGHT * ratio, s_height);
  roi->offset_x = (s_width - roi->width + 1) / 2;
  roi->offset_y = (s_height - roi->height + 1) / 2;
}

/**
 * @brief  Initialize DCMIPP Pipe1 for display output
 * @param  s_width: Sensor width
 * @param  s_height: Sensor height
 * @retval 0 on success, negative on failure
 */
static int DCMIPP_PipeInitDisplay(int s_width, int s_height)
{
  CMW_DCMIPP_Conf_t dcmipp_conf;
  uint32_t hw_pitch;
  int ret;

  assert(LCD_BG_WIDTH >= LCD_BG_HEIGHT);

  dcmipp_conf.output_width = LCD_BG_WIDTH;
  dcmipp_conf.output_height = LCD_BG_HEIGHT;
  dcmipp_conf.output_format = DISPLAY_FORMAT;
  dcmipp_conf.output_bpp = DISPLAY_BPP;
  dcmipp_conf.mode = CMW_Aspect_ratio_manual_roi;
  dcmipp_conf.enable_swap = 0;
  dcmipp_conf.enable_gamma_conversion = 0;

  CAM_InitCropConfig(&dcmipp_conf.manual_conf, s_width, s_height);

  ret = CMW_CAMERA_SetPipeConfig(DCMIPP_PIPE1, &dcmipp_conf, &hw_pitch);
  if (ret != HAL_OK)
  {
    return -1;
  }

  assert(hw_pitch == dcmipp_conf.output_width * dcmipp_conf.output_bpp);
  return 0;
}

/**
 * @brief  Initialize the camera module
 * @retval 0 on success, negative error code on failure
 */
int CAM_Init(void)
{
  CMW_CameraInit_t cam_conf;
  int ret;

  /* Let sensor driver choose which width/height to use */
  cam_conf.width = 0;
  cam_conf.height = 0;
  cam_conf.fps = CAMERA_FPS;
  cam_conf.pixel_format = 0; /* Default; Not implemented yet */
  cam_conf.anti_flicker = 0;
  cam_conf.mirror_flip = CAMERA_FLIP;

  ret = CMW_CAMERA_Init(&cam_conf, NULL);
  if (ret != CMW_ERROR_NONE)
  {
    return -1;
  }

  /* Store the resolution chosen by sensor driver */
  sensor_width = cam_conf.width;
  sensor_height = cam_conf.height;

  /* Configure DCMIPP Pipe1 for display */
  ret = DCMIPP_PipeInitDisplay(cam_conf.width, cam_conf.height);
  if (ret != 0)
  {
    return -2;
  }

  return 0;
}

/**
 * @brief  Start the display pipe capture
 * @param  display_pipe_dst: Pointer to the display buffer
 * @param  cam_mode: Camera mode (CMW_MODE_CONTINUOUS or CMW_MODE_SNAPSHOT)
 * @retval None
 */
void CAM_DisplayPipe_Start(uint8_t *display_pipe_dst, uint32_t cam_mode)
{
  int ret;

  ret = CMW_CAMERA_Start(DCMIPP_PIPE1, display_pipe_dst, cam_mode);
  assert(ret == CMW_ERROR_NONE);
}

/**
 * @brief  Update ISP parameters (call periodically for auto exposure/white balance)
 * @retval None
 */
void CAM_IspUpdate(void)
{
  int ret;

  ret = CMW_CAMERA_Run();
  assert(ret == CMW_ERROR_NONE);
}

/**
 * @brief  Get sensor resolution after initialization
 * @param  width: Pointer to store width
 * @param  height: Pointer to store height
 * @retval None
 */
void CAM_GetResolution(uint32_t *width, uint32_t *height)
{
  if (width != NULL)
  {
    *width = sensor_width;
  }
  if (height != NULL)
  {
    *height = sensor_height;
  }
}

/**
 * @brief  Frame event callback from Camera Middleware (called from ISR context)
 * @param  pipe: Pipe that triggered the event
 * @retval HAL_OK
 */
int CMW_CAMERA_PIPE_FrameEventCallback(uint32_t pipe)
{
  /* User can implement frame handling here */
  /* For example: notify a thread that a new frame is available */
  (void)pipe;
  return HAL_OK;
}

/**
 * @brief  Vsync event callback from Camera Middleware (called from ISR context)
 * @param  pipe: Pipe that triggered the event
 * @retval HAL_OK
 */
int CMW_CAMERA_PIPE_VsyncEventCallback(uint32_t pipe)
{
  /* User can implement vsync handling here */
  /* For example: trigger ISP update */
  (void)pipe;
  return HAL_OK;
}

