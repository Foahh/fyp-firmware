/**
 ******************************************************************************
 * @file    app_cam.c
 * @author  Long Liangmao
 * @brief   Camera application implementation for STM32N6570-DK
 *          Dual DCMIPP pipe configuration: Pipe1 for display, Pipe2 for NN
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

#include "app_cam.h"
#include "app_bqueue.h"
#include "app_buffers.h"
#include "app_cam_config.h"
#include "app_error.h"
#include "app_lcd.h"
#include "app_nn.h"
#include "app_nn_config.h"
#include "cmw_camera.h"
#include "main.h"
#include "stm32n6xx_hal.h"
#include "utils.h"

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void CAM_CalcCropRoi(CMW_Manual_roi_area_t *roi,
                            uint32_t sensor_w, uint32_t sensor_h,
                            uint32_t output_w, uint32_t output_h);
static void CAM_ConfigPipe(uint32_t pipe,
                           uint32_t sensor_w, uint32_t sensor_h,
                           uint32_t out_w, uint32_t out_h,
                           uint32_t format, uint32_t bpp,
                           int swap_enabled);
static void cam_display_pipe_frame_event(void);
static void cam_nn_pipe_frame_event(void);
static void isp_thread_entry(ULONG arg);
static void lcd_reload_thread_entry(ULONG arg);

/* ============================================================================
 * Configuration Constants
 * ============================================================================ */

/* ISP update thread configuration */
#define ISP_THREAD_STACK_SIZE 2048
#define ISP_THREAD_PRIORITY   5

/* LCD reload thread configuration (high priority to minimize display latency) */
#define LCD_RELOAD_THREAD_STACK_SIZE 1024
#define LCD_RELOAD_THREAD_PRIORITY   4

/* ============================================================================
 * Global State Variables
 * ============================================================================ */

/* ISP thread resources */
static struct {
  TX_EVENT_FLAGS_GROUP vsync_flags;
  TX_THREAD thread;
  UCHAR stack[ISP_THREAD_STACK_SIZE];
} isp_ctx;

/* LCD reload thread resources */
static struct {
  TX_EVENT_FLAGS_GROUP reload_flags;
  TX_THREAD thread;
  UCHAR stack[LCD_RELOAD_THREAD_STACK_SIZE];
  volatile uint8_t *pending_buffer;
} lcd_reload_ctx;

/* NN pipe running state */
static volatile uint8_t nn_pipe_running = 0;

/* NN frame drop counter */
static volatile uint32_t nn_frame_drop_count = 0;

/* NN crop ROI in sensor coordinates (stored for overlay visualization) */
static struct {
  uint32_t sensor_w;
  uint32_t sensor_h;
  CMW_Manual_roi_area_t nn_roi;
} nn_crop_info = {0};

static nn_crop_info_display_t roi_info_d = {0};

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
static void CAM_CalcCropRoi(CMW_Manual_roi_area_t *roi,
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

static void CAM_UpdateDisplayROI(CMW_Manual_roi_area_t *roi,
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
static void CAM_ConfigPipe(uint32_t pipe,
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
 * @brief  Start the display pipe capture
 * @param  cam_mode: Camera mode (CMW_MODE_CONTINUOUS or CMW_MODE_SNAPSHOT)
 */
void CAM_DisplayPipe_Start(uint32_t cam_mode) {
  uint8_t *buffer;

  buffer = Buffer_GetCameraDisplayBuffer(Buffer_GetCameraCaptureIndex());
  APP_REQUIRE(buffer != NULL);

  APP_REQUIRE(CMW_CAMERA_Start(DCMIPP_PIPE1, buffer, cam_mode) == CMW_ERROR_NONE);
}

/**
 * @brief  Start the neural network pipe capture
 * @param  nn_buffer: Pointer to the NN buffer
 * @param  cam_mode: Camera mode (CMW_MODE_CONTINUOUS or CMW_MODE_SNAPSHOT)
 */
void CAM_NNPipe_Start(uint8_t *nn_buffer, uint32_t cam_mode) {
  APP_REQUIRE(nn_buffer != NULL);
  APP_REQUIRE(CMW_CAMERA_Start(DCMIPP_PIPE2, nn_buffer, cam_mode) == CMW_ERROR_NONE);
  nn_pipe_running = 1;
}

/**
 * @brief  Stop the neural network pipe capture
 */
void CAM_NNPipe_Stop(void) {
  nn_pipe_running = 0;
  CMW_CAMERA_Suspend(DCMIPP_PIPE2);
}

#ifdef CAMERA_NN_SNAPSHOT_MODE
/**
 * @brief  Request a single snapshot from the NN pipe
 * @param  nn_buffer: Pointer to the NN buffer to capture into
 */
void CAM_NNPipe_RequestSnapshot(uint8_t *nn_buffer) {
  APP_REQUIRE(nn_buffer != NULL);
  APP_REQUIRE(CMW_CAMERA_Start(DCMIPP_PIPE2, nn_buffer, CMW_MODE_SNAPSHOT) == CMW_ERROR_NONE);
}
#endif

/**
 * @brief  Get NN crop ROI in display coordinates
 * @retval Pointer to nn_crop_info_display_t struct if initialized, NULL otherwise
 */
nn_crop_info_display_t *CAM_GetNNCropROI_Display(void) {
  if (nn_crop_info.sensor_w == 0 || nn_crop_info.sensor_h == 0) {
    return NULL;
  }
  return &roi_info_d;
}

/**
 * @brief  Get cumulative NN frame drop count
 * @retval Number of frames dropped since boot
 */
uint32_t CAM_GetFrameDropCount(void) {
  return nn_frame_drop_count;
}

/**
 * @brief  Update ISP parameters (call periodically for auto exposure/white balance)
 */
void CAM_IspUpdate(void) {
  APP_REQUIRE(CMW_CAMERA_Run() == CMW_ERROR_NONE);
}

/**
 * @brief  Initialize and create the ISP update thread
 * @param  memory_ptr: Memory pointer (unused, thread uses static allocation)
 */
void CAM_ISP_Thread_Start(VOID *memory_ptr) {
  UNUSED(memory_ptr);
  APP_REQUIRE(tx_event_flags_create(&isp_ctx.vsync_flags, "isp_vsync") == TX_SUCCESS);

  APP_REQUIRE(tx_thread_create(&isp_ctx.thread, "isp_update",
                               isp_thread_entry, 0,
                               isp_ctx.stack, ISP_THREAD_STACK_SIZE,
                               ISP_THREAD_PRIORITY, ISP_THREAD_PRIORITY,
                               TX_NO_TIME_SLICE, TX_AUTO_START) == TX_SUCCESS);

  /* Create LCD reload thread */
  APP_REQUIRE(tx_event_flags_create(&lcd_reload_ctx.reload_flags, "lcd_reload") == TX_SUCCESS);

  APP_REQUIRE(tx_thread_create(&lcd_reload_ctx.thread, "lcd_reload",
                               lcd_reload_thread_entry, 0,
                               lcd_reload_ctx.stack, LCD_RELOAD_THREAD_STACK_SIZE,
                               LCD_RELOAD_THREAD_PRIORITY, LCD_RELOAD_THREAD_PRIORITY,
                               TX_NO_TIME_SLICE, TX_AUTO_START) == TX_SUCCESS);
}

/* ============================================================================
 * Frame Event Handlers
 * ============================================================================ */

/**
 * @brief  Handle display pipe (PIPE1) frame event
 *         Updates DCMIPP capture buffer and LCD display buffer
 */
static void cam_display_pipe_frame_event(void) {
  DCMIPP_HandleTypeDef *hdcmipp = CMW_CAMERA_GetDCMIPPHandle();
  int next_disp = Buffer_GetNextCameraDisplayIndex();
  int next_capt = Buffer_GetNextCameraCaptureIndex();

  /* Update DCMIPP to capture into next buffer */
  if (hdcmipp != NULL) {
    uint8_t *next_capt_buf = Buffer_GetCameraDisplayBuffer(next_capt);
    APP_REQUIRE(next_capt_buf != NULL);
    HAL_DCMIPP_PIPE_SetMemoryAddress(hdcmipp, DCMIPP_PIPE1,
                                     DCMIPP_MEMORY_ADDRESS_0,
                                     (uint32_t)next_capt_buf);
  }

  /* Defer LCD reload to thread context (avoids cache clean in ISR) */
  lcd_reload_ctx.pending_buffer = Buffer_GetCameraDisplayBuffer(next_disp);
  tx_event_flags_set(&lcd_reload_ctx.reload_flags, 0x01, TX_OR);

  /* Advance buffer indices */
  Buffer_SetCameraDisplayIndex(next_disp);
  Buffer_SetCameraCaptureIndex(next_capt);
}

/**
 * @brief  Handle NN pipe (PIPE2) frame event
 *         Gets next free buffer from NN input queue and programs DCMIPP
 */
static void cam_nn_pipe_frame_event(void) {
  if (!nn_pipe_running) {
    return;
  }

#ifdef CAMERA_NN_SNAPSHOT_MODE
  NN_SignalSnapshotReady();
#else
  bqueue_t *nn_input_queue;
  uint8_t *next_buffer;

  nn_input_queue = NN_GetInputQueue();
  if (nn_input_queue == NULL) {
    return;
  }

  next_buffer = bqueue_get_free(nn_input_queue, 0);
  if (next_buffer != NULL) {
    DCMIPP_HandleTypeDef *hdcmipp = CMW_CAMERA_GetDCMIPPHandle();
    if (hdcmipp != NULL) {
      HAL_DCMIPP_PIPE_SetMemoryAddress(hdcmipp, DCMIPP_PIPE2,
                                       DCMIPP_MEMORY_ADDRESS_0,
                                       (uint32_t)next_buffer);
    }
    bqueue_put_ready(nn_input_queue);
  } else {
    nn_frame_drop_count++;
  }
#endif
}

/* ============================================================================
 * CMW Camera Callback Functions
 * ============================================================================ */

/**
 * @brief  Frame event callback (ISR context) - handles buffering
 * @param  pipe: Pipe that triggered the event
 * @retval HAL_OK
 */
int CMW_CAMERA_PIPE_FrameEventCallback(uint32_t pipe) {
  if (pipe == DCMIPP_PIPE1) {
    cam_display_pipe_frame_event();
  } else if (pipe == DCMIPP_PIPE2) {
    cam_nn_pipe_frame_event();
  }

  return HAL_OK;
}

/**
 * @brief  Vsync event callback (ISR context) - triggers ISP update
 * @param  pipe: Pipe that triggered the event
 * @retval HAL_OK
 */
int CMW_CAMERA_PIPE_VsyncEventCallback(uint32_t pipe) {
  if (pipe == DCMIPP_PIPE1) {
    tx_event_flags_set(&isp_ctx.vsync_flags, 0x01, TX_OR);
  }
  return HAL_OK;
}

/* ============================================================================
 * Thread Entry Points
 * ============================================================================ */

/**
 * @brief  ISP update thread entry
 *         Waits for vsync events and updates ISP parameters
 * @param  arg: Thread argument (unused)
 */
static void isp_thread_entry(ULONG arg) {
  UNUSED(arg);
  ULONG actual_flags;

  while (1) {
    tx_event_flags_get(&isp_ctx.vsync_flags, 0x01, TX_OR_CLEAR,
                       &actual_flags, TX_WAIT_FOREVER);
    CAM_IspUpdate();
  }
}

/**
 * @brief  LCD reload thread entry
 *         Performs cache clean + LTDC reload deferred from display pipe ISR
 * @param  arg: Thread argument (unused)
 */
static void lcd_reload_thread_entry(ULONG arg) {
  UNUSED(arg);
  ULONG actual_flags;

  while (1) {
    tx_event_flags_get(&lcd_reload_ctx.reload_flags, 0x01, TX_OR_CLEAR,
                       &actual_flags, TX_WAIT_FOREVER);
    uint8_t *buffer = (uint8_t *)lcd_reload_ctx.pending_buffer;
    if (buffer != NULL) {
      LCD_ReloadCameraLayer(buffer);
    }
  }
}
