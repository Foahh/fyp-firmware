/**
 ******************************************************************************
 * @file    cam_pipe.c
 * @author  Long Liangmao
 * @brief   Camera pipe operations and frame callbacks
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
#include "app_lcd.h"
#include "bqueue.h"
#include "cam_internal.h"
#include "nn.h"
#include "utils.h"

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void cam_display_pipe_frame_event(void);
static void cam_nn_pipe_frame_event(void);

/* ============================================================================
 * Global State Definitions
 * ============================================================================ */

/* Camera display double buffers */
uint8_t camera_display_buffers[2][DISPLAY_LETTERBOX_WIDTH * DISPLAY_LETTERBOX_HEIGHT * DISPLAY_BPP] ALIGN_32 IN_PSRAM;
volatile int camera_display_idx = 1;
volatile int camera_capture_idx = 0;

/* NN pipe running state */
volatile uint8_t nn_pipe_running = 0;

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

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

/**
 * @brief  Resume the neural network pipe capture
 */
void CAM_NNPipe_Resume(void) {
  CMW_CAMERA_Resume(DCMIPP_PIPE2);
  nn_pipe_running = 1;
}

/**
 * @brief  Suspend the display pipe capture
 */
void CAM_DisplayPipe_Suspend(void) {
  CMW_CAMERA_Suspend(DCMIPP_PIPE1);
}

/**
 * @brief  Resume the display pipe capture
 */
void CAM_DisplayPipe_Resume(void) {
  CMW_CAMERA_Resume(DCMIPP_PIPE1);
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
