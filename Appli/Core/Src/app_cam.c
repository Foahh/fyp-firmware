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
static void CAM_InitCropConfig(CMW_Manual_roi_area_t *roi, int s_width,
                               int s_height) {
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
  cam_conf.pixel_format = 0; /* Default; Not implemented yet */
  cam_conf.anti_flicker = 0;
  cam_conf.mirror_flip = CAMERA_FLIP;

  ret = CMW_CAMERA_Init(&cam_conf, NULL);
  if (ret != CMW_ERROR_NONE) {
    return -1;
  }

  /* Store the resolution chosen by sensor driver */
  sensor_width = cam_conf.width;
  sensor_height = cam_conf.height;

  /* Configure DCMIPP Pipe1 for display */
  ret = DCMIPP_PipeInitDisplay(cam_conf.width, cam_conf.height);
  if (ret != 0) {
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
void CAM_DisplayPipe_Start(uint8_t *display_pipe_dst, uint32_t cam_mode) {
  int ret;

  ret = CMW_CAMERA_Start(DCMIPP_PIPE1, display_pipe_dst, cam_mode);
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
 * @brief  Get sensor resolution after initialization
 * @param  width: Pointer to store width
 * @param  height: Pointer to store height
 * @retval None
 */
void CAM_GetResolution(uint32_t *width, uint32_t *height) {
  if (width != NULL) {
    *width = sensor_width;
  }
  if (height != NULL) {
    *height = sensor_height;
  }
}

/**
 * @brief  Frame event callback from Camera Middleware (called from ISR context)
 * @param  pipe: Pipe that triggered the event
 * @retval HAL_OK
 */
int CMW_CAMERA_PIPE_FrameEventCallback(uint32_t pipe) {
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
int CMW_CAMERA_PIPE_VsyncEventCallback(uint32_t pipe) {
  /* User can implement vsync handling here */
  /* For example: trigger ISP update */
  (void)pipe;
  return HAL_OK;
}

/* Thread configuration */
#define CAMERA_THREAD_STACK_SIZE 2048
#define CAMERA_THREAD_PRIORITY 5

/* Display buffer - placed in PSRAM for large buffer */
/* Buffer size: 800 x 480 x 2 (RGB565) = 768000 bytes */
#define DISPLAY_BUFFER_SIZE (LCD_BG_WIDTH * LCD_BG_HEIGHT * DISPLAY_BPP)

/* Thread control block and stack */
static TX_THREAD camera_thread;
static UCHAR camera_thread_stack[CAMERA_THREAD_STACK_SIZE];

/* Display frame buffer - should be in external RAM for real application */
/* For now using internal RAM, but you may need to place this in PSRAM */
__attribute__((aligned(32))) static uint8_t display_buffer[DISPLAY_BUFFER_SIZE];

/* Thread entry function */
static VOID camera_thread_entry(ULONG thread_input);

/**
 * @brief  Configure camera peripherals (called before thread starts)
 * @retval 0 on success, negative error code on failure
 */
int CAM_ConfigurePeripherals(void) {
  int ret;
  uint32_t sensor_width, sensor_height;

  /* Initialize camera */
  ret = CAM_Init();
  if (ret != 0) {
    return ret;
  }

  /* Get sensor resolution for debug */
  CAM_GetResolution(&sensor_width, &sensor_height);

  /* Clear display buffer */
  memset(display_buffer, 0, DISPLAY_BUFFER_SIZE);

  /* Start camera capture in continuous mode */
  CAM_DisplayPipe_Start(display_buffer, CMW_MODE_CONTINUOUS);

  return 0;
}

/**
 * @brief  Initialize and create the camera thread
 * @param  memory_ptr: Memory pointer (unused, thread uses static allocation)
 * @retval TX_SUCCESS if successful, error code otherwise
 */
UINT Thread_Camera_Init(VOID *memory_ptr) {
  UINT ret = TX_SUCCESS;
  UNUSED(memory_ptr);

  ret = tx_thread_create(&camera_thread, "camera_thread", camera_thread_entry,
                         0, camera_thread_stack, CAMERA_THREAD_STACK_SIZE,
                         CAMERA_THREAD_PRIORITY, CAMERA_THREAD_PRIORITY,
                         TX_NO_TIME_SLICE, TX_AUTO_START);

  return ret;
}

/**
 * @brief  Camera thread entry function
 * @param  thread_input: Thread input parameter (unused)
 * @retval None
 */
static VOID camera_thread_entry(ULONG thread_input) {
  UNUSED(thread_input);

  while (1) {
    CAM_IspUpdate();
    tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND / 30);
  }
}
