/**
 ******************************************************************************
 * @file    app_cam.c
 * @author  Long Liangmao
 * @brief   Camera application implementation for STM32N6570-DK
 *          Dual DCMIPP pipe configuration: Pipe1 for display, Pipe2 for ML
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
#include "app_buffers.h"
#include "app_config.h"
#include "app_lcd.h"
#include "cmw_camera.h"
#include "main.h"
#include "stm32n6xx_hal.h"
#include "utils.h"
#include <assert.h>

/* ISP update thread configuration */
#define ISP_THREAD_STACK_SIZE 2048
#define ISP_THREAD_PRIORITY 5

/* ISP thread resources */
static struct {
  TX_SEMAPHORE vsync_sem;
  TX_THREAD thread;
  UCHAR stack[ISP_THREAD_STACK_SIZE];
} isp_ctx;

/**
 * @brief  Calculate centered crop ROI maintaining aspect ratio
 * @param  roi: Output ROI configuration
 * @param  sensor_w: Sensor width
 * @param  sensor_h: Sensor height
 * @param  output_w: Target output width
 * @param  output_h: Target output height
 */
static void CAM_CalcCropRoi(CMW_Manual_roi_area_t *roi,
                            uint32_t sensor_w, uint32_t sensor_h,
                            uint32_t output_w, uint32_t output_h) {
  /* Scale factor = sensor / output (fixed point with 10-bit fraction) */
  const uint32_t scale_x = (sensor_w << 10) / output_w;
  const uint32_t scale_y = (sensor_h << 10) / output_h;
  const uint32_t scale = MIN(scale_x, scale_y);

  assert(scale >= (1 << 10)); /* Scale must be >= 1.0 */

  roi->width = MIN((output_w * scale) >> 10, sensor_w);
  roi->height = MIN((output_h * scale) >> 10, sensor_h);
  roi->offset_x = (sensor_w - roi->width) / 2;
  roi->offset_y = (sensor_h - roi->height) / 2;
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
 * @retval 0 on success, negative on failure
 */
static int CAM_ConfigPipe(uint32_t pipe,
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
  int ret;

  CAM_CalcCropRoi(&conf.manual_conf, sensor_w, sensor_h, out_w, out_h);

  ret = CMW_CAMERA_SetPipeConfig(pipe, &conf, &hw_pitch);
  if (ret != HAL_OK) {
    return -1;
  }

  assert(hw_pitch == out_w * bpp);
  return 0;
}

/**
 * @brief  DCMIPP clock configuration callback
 */
HAL_StatusTypeDef MX_DCMIPP_ClockConfig(DCMIPP_HandleTypeDef *hdcmipp) {
  UNUSED(hdcmipp);
  RCC_PeriphCLKInitTypeDef clk = {0};

  /* Configure DCMIPP clock from PLL1 / 4 */
  clk.PeriphClockSelection = RCC_PERIPHCLK_DCMIPP;
  clk.DcmippClockSelection = RCC_DCMIPPCLKSOURCE_IC17;
  clk.ICSelection[RCC_IC17].ClockSelection = RCC_ICCLKSOURCE_PLL1;
  clk.ICSelection[RCC_IC17].ClockDivider = 4;
  if (HAL_RCCEx_PeriphCLKConfig(&clk) != HAL_OK) {
    return HAL_ERROR;
  }

  /* Configure CSI clock from PLL1 / 60 */
  clk.PeriphClockSelection = RCC_PERIPHCLK_CSI;
  clk.ICSelection[RCC_IC18].ClockSelection = RCC_ICCLKSOURCE_PLL1;
  clk.ICSelection[RCC_IC18].ClockDivider = 60;
  if (HAL_RCCEx_PeriphCLKConfig(&clk) != HAL_OK) {
    return HAL_ERROR;
  }

  return HAL_OK;
}

/**
 * @brief  Initialize the camera module
 * @retval 0 on success, negative error code on failure
 */
int CAM_Init(void) {
  CMW_CameraInit_t cam_conf = {
      .width = 0, /* Let sensor driver choose */
      .height = 0,
      .fps = CAMERA_FPS,
      .pixel_format = 0,
      .anti_flicker = 0,
      .mirror_flip = CAMERA_FLIP,
  };
  int ret;

  ret = CMW_CAMERA_Init(&cam_conf, NULL);
  if (ret != CMW_ERROR_NONE) {
    return -1;
  }

  /* Configure display pipe (Pipe1) */
  ret = CAM_ConfigPipe(DCMIPP_PIPE1,
                       cam_conf.width, cam_conf.height,
                       DISPLAY_LETTERBOX_WIDTH, DISPLAY_LETTERBOX_HEIGHT,
                       DISPLAY_FORMAT, DISPLAY_BPP, 0);
  if (ret != 0) {
    return -2;
  }

  /* Configure ML pipe (Pipe2) */
  ret = CAM_ConfigPipe(DCMIPP_PIPE2,
                       cam_conf.width, cam_conf.height,
                       ML_WIDTH, ML_HEIGHT,
                       ML_FORMAT, ML_BPP, 1);
  if (ret != 0) {
    return -3;
  }

  return 0;
}

/**
 * @brief  Start the display pipe capture
 * @param  cam_mode: CMW_MODE_CONTINUOUS or CMW_MODE_SNAPSHOT
 */
void CAM_DisplayPipe_Start(uint32_t cam_mode) {
  uint8_t *buffer;
  int ret;

  buffer = Buffer_GetCameraDisplayBuffer(Buffer_GetCameraCaptureIndex());
  assert(buffer != NULL);

  ret = CMW_CAMERA_Start(DCMIPP_PIPE1, buffer, cam_mode);
  assert(ret == CMW_ERROR_NONE);
}

/**
 * @brief  Start the ML pipe capture
 * @param  ml_buffer: Pointer to the ML output buffer
 * @param  cam_mode: CMW_MODE_CONTINUOUS or CMW_MODE_SNAPSHOT
 */
void CAM_MLPipe_Start(uint8_t *ml_buffer, uint32_t cam_mode) {
  int ret;

  ret = CMW_CAMERA_Start(DCMIPP_PIPE2, ml_buffer, cam_mode);
  assert(ret == CMW_ERROR_NONE);
}

/**
 * @brief  Update ISP parameters (auto exposure, white balance)
 */
void CAM_IspUpdate(void) {
  int ret;

  ret = CMW_CAMERA_Run();
  assert(ret == CMW_ERROR_NONE);
}

/**
 * @brief  Frame event callback (ISR context) - handles triple buffering
 * @param  pipe: Pipe that triggered the event
 * @retval HAL_OK
 */
int CMW_CAMERA_PIPE_FrameEventCallback(uint32_t pipe) {
  if (pipe != DCMIPP_PIPE1) {
    return HAL_OK;
  }

  DCMIPP_HandleTypeDef *hdcmipp = CMW_CAMERA_GetDCMIPPHandle();
  int next_disp = Buffer_GetNextCameraDisplayIndex();
  int next_capt = Buffer_GetNextCameraCaptureIndex();

  /* Update DCMIPP to capture into next buffer */
  if (hdcmipp != NULL) {
    uint8_t *next_capt_buf = Buffer_GetCameraDisplayBuffer(next_capt);
    HAL_DCMIPP_PIPE_SetMemoryAddress(hdcmipp, DCMIPP_PIPE1,
                                     DCMIPP_MEMORY_ADDRESS_0,
                                     (uint32_t)next_capt_buf);
  }

  /* Update LCD to display completed buffer */
  LCD_ReloadCameraLayer(Buffer_GetCameraDisplayBuffer(next_disp));

  /* Advance buffer indices */
  Buffer_SetCameraDisplayIndex(next_disp);
  Buffer_SetCameraCaptureIndex(next_capt);

  return HAL_OK;
}

/**
 * @brief  Vsync event callback (ISR context) - triggers ISP update
 * @param  pipe: Pipe that triggered the event
 * @retval HAL_OK
 */
int CMW_CAMERA_PIPE_VsyncEventCallback(uint32_t pipe) {
  if (pipe == DCMIPP_PIPE1) {
    tx_semaphore_put(&isp_ctx.vsync_sem);
  }
  return HAL_OK;
}

/**
 * @brief  ISP update thread entry
 */
static void isp_thread_entry(ULONG arg) {
  UNUSED(arg);

  while (1) {
    tx_semaphore_get(&isp_ctx.vsync_sem, TX_WAIT_FOREVER);
    CAM_IspUpdate();
  }
}

/**
 * @brief  Initialize ISP semaphore
 * @retval TX_SUCCESS on success
 */
UINT CAM_InitIspSemaphore(void) {
  return tx_semaphore_create(&isp_ctx.vsync_sem, "isp_vsync", 0);
}

/**
 * @brief  Initialize and start the ISP update thread
 * @param  memory_ptr: Unused (static allocation)
 * @retval TX_SUCCESS on success
 */
UINT Thread_IspUpdate_Init(VOID *memory_ptr) {
  UNUSED(memory_ptr);

  return tx_thread_create(&isp_ctx.thread, "isp_update",
                          isp_thread_entry, 0,
                          isp_ctx.stack, ISP_THREAD_STACK_SIZE,
                          ISP_THREAD_PRIORITY, ISP_THREAD_PRIORITY,
                          TX_NO_TIME_SLICE, TX_AUTO_START);
}
