/**
 ******************************************************************************
 * @file    app_cam.c
 * @author  Long Liangmao
 * @brief   Camera application implementation for STM32N6570-DK
 *          Dual DCMIPP pipe configuration: Pipe1 for display, Pipe2 for machine learning
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

#include "app_cam.h"
#include "cmw_camera.h"
#include "imx335.h"
#include "main.h"
#include "stm32n6570_discovery_camera.h"
#include "stm32n6xx_hal.h"
#include "utils.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app_config.h"

/**
 * @brief  Configure crop area to maintain aspect ratio
 * @param  roi: Pointer to ROI configuration structure
 * @param  s_width: Sensor width
 * @param  s_height: Sensor height
 * @retval None
 */
static void CAM_InitCropConfig(CMW_Manual_roi_area_t *roi, int s_width,
                               int s_height) {
  const float ratiox = (float)s_width / LCD_WIDTH;
  const float ratioy = (float)s_height / LCD_HEIGHT;
  const float ratio = MIN(ratiox, ratioy);

  assert(ratio >= 1);
  assert(ratio < 64);

  roi->width = (uint32_t)MIN(LCD_WIDTH * ratio, s_width);
  roi->height = (uint32_t)MIN(LCD_HEIGHT * ratio, s_height);
  roi->offset_x = (s_width - roi->width + 1) / 2;
  roi->offset_y = (s_height - roi->height + 1) / 2;
}

HAL_StatusTypeDef MX_DCMIPP_ClockConfig(DCMIPP_HandleTypeDef *hdcmipp) {
  UNUSED(hdcmipp);
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_DCMIPP;
  PeriphClkInitStruct.DcmippClockSelection = RCC_DCMIPPCLKSOURCE_IC17;
  PeriphClkInitStruct.ICSelection[RCC_IC17].ClockSelection = RCC_ICCLKSOURCE_PLL1;
  PeriphClkInitStruct.ICSelection[RCC_IC17].ClockDivider = 4;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
    return HAL_ERROR;
  }

  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_CSI;
  PeriphClkInitStruct.ICSelection[RCC_IC18].ClockSelection = RCC_ICCLKSOURCE_PLL1;
  PeriphClkInitStruct.ICSelection[RCC_IC18].ClockDivider = 60;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
    return HAL_ERROR;
  }

  return HAL_OK;
}

/**
 * @brief  Initialize DCMIPP Pipe1 for display output
 * @param  s_width: Sensor width
 * @param  s_height: Sensor height
 * @retval 0 on success, negative on failure
 */
static int DCMIPP_PipeInitDisplay(int s_width, int s_height) {
  CMW_DCMIPP_Conf_t dcmipp_conf;
  uint32_t hw_pitch;
  int ret;

  assert(LCD_WIDTH >= LCD_HEIGHT);

  dcmipp_conf.output_width = LCD_WIDTH;
  dcmipp_conf.output_height = LCD_HEIGHT;
  dcmipp_conf.output_format = DISPLAY_FORMAT;
  dcmipp_conf.output_bpp = DISPLAY_BPP;
  dcmipp_conf.mode = CMW_Aspect_ratio_manual_roi;
  dcmipp_conf.enable_swap = 0;
  dcmipp_conf.enable_gamma_conversion = 0;

  CAM_InitCropConfig(&dcmipp_conf.manual_conf, s_width, s_height);

  ret = CMW_CAMERA_SetPipeConfig(DCMIPP_PIPE1, &dcmipp_conf, &hw_pitch);
  if (ret != HAL_OK) {
    return -1;
  }

  assert(hw_pitch == dcmipp_conf.output_width * dcmipp_conf.output_bpp);
  return 0;
}

/**
 * @brief  Initialize DCMIPP Pipe2 for neural network/AI inference
 * @param  s_width: Sensor width
 * @param  s_height: Sensor height
 * @retval 0 on success, negative on failure
 */
static int DCMIPP_PipeInitML(int s_width, int s_height) {
  CMW_DCMIPP_Conf_t dcmipp_conf;
  uint32_t hw_pitch;
  int ret;

  dcmipp_conf.output_width = ML_WIDTH;
  dcmipp_conf.output_height = ML_HEIGHT;
  dcmipp_conf.output_format = ML_FORMAT;
  dcmipp_conf.output_bpp = ML_BPP;
  dcmipp_conf.mode = CMW_Aspect_ratio_manual_roi;
  dcmipp_conf.enable_swap = 1;
  dcmipp_conf.enable_gamma_conversion = 0;

  CAM_InitCropConfig(&dcmipp_conf.manual_conf, s_width, s_height);

  ret = CMW_CAMERA_SetPipeConfig(DCMIPP_PIPE2, &dcmipp_conf, &hw_pitch);
  if (ret != HAL_OK) {
    return -1;
  }

  assert(hw_pitch == dcmipp_conf.output_width * dcmipp_conf.output_bpp);
  return 0;
}

/**
 * @brief  Initialize the camera module
 * @retval 0 on success, negative error code on failure
 */
int CAM_Init(void) {
  CMW_CameraInit_t cam_conf;
  int ret;

  /* Let sensor driver choose which width/height to use */
  cam_conf.width = 0;
  cam_conf.height = 0;
  cam_conf.fps = CAMERA_FPS;
  cam_conf.pixel_format = 0;
  cam_conf.anti_flicker = 0;
  cam_conf.mirror_flip = CAMERA_FLIP;

  ret = CMW_CAMERA_Init(&cam_conf, NULL);
  if (ret != CMW_ERROR_NONE) {
    return -1;
  }

  /* cam_conf.width / cam_conf.height now contains chosen resolution */
  ret = DCMIPP_PipeInitDisplay(cam_conf.width, cam_conf.height);
  if (ret != 0) {
    return -2;
  }

  ret = DCMIPP_PipeInitML(cam_conf.width, cam_conf.height);
  if (ret != 0) {
    return -3;
  }

  return 0;
}

/**
 * @brief  Start the display pipe capture
 * @param  display_pipe_dst: Pointer to the display buffer
 * @param  cam_mode: Camera mode (CMW_MODE_CONTINUOUS or CMW_MODE_SNAPSHOT)
 * @retval None
 */
void CAM_DisplayPipe_Start(uint8_t *display_pipe_dst, uint32_t cam_mode) {
  int ret;

  ret = CMW_CAMERA_Start(DCMIPP_PIPE1, display_pipe_dst, cam_mode);
  assert(ret == CMW_ERROR_NONE);
}

/**
 * @brief  Start the neural network pipe capture
 * @param  ml_pipe_dst: Pointer to the NN buffer
 * @param  cam_mode: Camera mode (CMW_MODE_CONTINUOUS or CMW_MODE_SNAPSHOT)
 * @retval None
 */
void CAM_MLPipe_Start(uint8_t *ml_pipe_dst, uint32_t cam_mode) {
  int ret;

  ret = CMW_CAMERA_Start(DCMIPP_PIPE2, ml_pipe_dst, cam_mode);
  assert(ret == CMW_ERROR_NONE);
}

/**
 * @brief  Update ISP parameters (call periodically for auto exposure/white
 * balance)
 * @retval None
 */
void CAM_IspUpdate(void) {
  int ret;

  ret = CMW_CAMERA_Run();
  assert(ret == CMW_ERROR_NONE);
}

/**
 * @brief  Frame event callback from Camera Middleware (called from ISR context)
 * @param  pipe: Pipe that triggered the event
 * @retval HAL_OK
 */
int CMW_CAMERA_PIPE_FrameEventCallback(uint32_t pipe) {
  (void)pipe;
  return HAL_OK;
}

/**
 * @brief  Vsync event callback from Camera Middleware (called from ISR context)
 * @param  pipe: Pipe that triggered the event
 * @retval HAL_OK
 */
int CMW_CAMERA_PIPE_VsyncEventCallback(uint32_t pipe) {
  (void)pipe;
  return HAL_OK;
}

/* Thread configuration */
#define ISP_UPDATE_THREAD_STACK_SIZE 2048
#define ISP_UPDATE_THREAD_PRIORITY 5

/* Thread control block and stack */
static TX_THREAD isp_update_thread;
static UCHAR isp_update_thread_stack[ISP_UPDATE_THREAD_STACK_SIZE];

/* Thread entry function */
static VOID isp_update_thread_entry(ULONG thread_input);

/**
 * @brief  Initialize and create the ISP update thread
 * @param  memory_ptr: Memory pointer (unused, thread uses static allocation)
 * @retval TX_SUCCESS if successful, error code otherwise
 */
UINT Thread_IspUpdate_Init(VOID *memory_ptr) {
  UINT ret = TX_SUCCESS;
  UNUSED(memory_ptr);

  ret = tx_thread_create(&isp_update_thread, "isp_update_thread", isp_update_thread_entry,
                         0, isp_update_thread_stack, ISP_UPDATE_THREAD_STACK_SIZE,
                         ISP_UPDATE_THREAD_PRIORITY, ISP_UPDATE_THREAD_PRIORITY,
                         TX_NO_TIME_SLICE, TX_AUTO_START);

  return ret;
}

/**
 * @brief  ISP update thread entry function
 * @param  thread_input: Thread input parameter (unused)
 * @retval None
 */
static VOID isp_update_thread_entry(ULONG thread_input) {
  UNUSED(thread_input);

  while (1) {
    CAM_IspUpdate();
    tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND / 30);
  }
}
