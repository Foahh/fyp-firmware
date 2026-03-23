/**
 ******************************************************************************
 * @file    app_cam.h
 * @author  Long Liangmao
 * @brief   Camera application header for STM32N6570-DK
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

#ifndef APP_CAM_H
#define APP_CAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "app_lcd_config.h"
#include "tx_api.h"
#include <stdint.h>

/* Camera display double-buffer storage */
extern uint8_t camera_display_buffers[2][DISPLAY_LETTERBOX_WIDTH * DISPLAY_LETTERBOX_HEIGHT * DISPLAY_BPP];
extern volatile int camera_display_idx;
extern volatile int camera_capture_idx;

#define Buffer_GetCameraDisplayIndex()     (camera_display_idx)
#define Buffer_GetCameraCaptureIndex()     (camera_capture_idx)
#define Buffer_GetNextCameraDisplayIndex() ((camera_display_idx) ^ 1)
#define Buffer_GetNextCameraCaptureIndex() ((camera_capture_idx) ^ 1)

#define Buffer_GetCameraDisplayBuffer(idx)                       \
  ({                                                             \
    int _idx = (idx);                                            \
    ((unsigned)_idx >= 2) ? NULL : camera_display_buffers[_idx]; \
  })

#define Buffer_SetCameraDisplayIndex(idx)      \
  ({                                           \
    int _idx = (idx);                          \
    int _ret = ((unsigned)_idx >= 2) ? -1 : 0; \
    if (_ret == 0) {                           \
      camera_display_idx = _idx;               \
    }                                          \
    _ret;                                      \
  })

#define Buffer_SetCameraCaptureIndex(idx)      \
  ({                                           \
    int _idx = (idx);                          \
    int _ret = ((unsigned)_idx >= 2) ? -1 : 0; \
    if (_ret == 0) {                           \
      camera_capture_idx = _idx;               \
    }                                          \
    _ret;                                      \
  })

/**
 * @brief  Initialize the camera module (HAL/BSP only)\
 */
void CAM_Init(void);

/**
 * @brief  Start the display pipe capture
 * @param  cam_mode: Camera mode (CMW_MODE_CONTINUOUS or CMW_MODE_SNAPSHOT)
 * @retval None
 */
void CAM_DisplayPipe_Start(uint32_t cam_mode);

/**
 * @brief  Start the neural network pipe capture
 * @param  nn_pipe_dst: Pointer to the NN buffer
 * @param  cam_mode: Camera mode (CMW_MODE_CONTINUOUS or CMW_MODE_SNAPSHOT)
 * @retval None
 */
void CAM_NNPipe_Start(uint8_t *nn_pipe_dst, uint32_t cam_mode);

/**
 * @brief  Stop the neural network pipe capture
 * @retval None
 */
void CAM_NNPipe_Stop(void);

/**
 * @brief  Resume the neural network pipe capture
 */
void CAM_NNPipe_Resume(void);

/**
 * @brief  Suspend the display pipe capture
 */
void CAM_DisplayPipe_Suspend(void);

/**
 * @brief  Resume the display pipe capture
 */
void CAM_DisplayPipe_Resume(void);

/**
 * @brief  Update ISP parameters (call periodically for auto exposure/white
 * balance)
 * @retval None
 */
void CAM_IspUpdate(void);

/**
 * @brief  Initialize and create the ISP update thread
 * @param  memory_ptr: Memory pointer (unused, thread uses static allocation)
 * @note   Also initializes the ISP event flags internally
 */
void CAM_ISP_Thread_Start(VOID *memory_ptr);

/**
 * @brief  NN crop ROI information in display coordinates
 */
typedef struct {
  int roi_x0; /**< X coordinate of top-left corner (relative to letterbox) */
  int roi_y0; /**< Y coordinate of top-left corner */
  int roi_x1; /**< X coordinate of bottom-right corner (relative to letterbox) */
  int roi_y1; /**< Y coordinate of bottom-right corner */
  int roi_w;  /**< ROI width */
  int roi_h;  /**< ROI height */
} nn_crop_info_display_t;

/**
 * @brief  Get NN crop ROI in display coordinates
 * @retval Pointer to nn_crop_info_display_t struct if initialized, NULL otherwise
 */
nn_crop_info_display_t *CAM_GetNNCropROI_Display(void);

/**
 * @brief  Get cumulative NN frame drop count
 * @retval Number of frames dropped since boot
 */
uint32_t CAM_GetFrameDropCount(void);

/**
 * @brief  Full camera deinit (DCMIPP, CSI, sensor power down)
 */
void CAM_DeInit(void);

/**
 * @brief  Enter standby: stop all pipes, suspend threads, power off sensor
 */
void CAM_Standby(void);

/**
 * @brief  Wake from standby: re-init sensor, restart pipes, resume threads
 */
void CAM_WakeUp(void);

/**
 * @brief  Suspend ISP and LCD reload threads for power measurement
 */
void CAM_ThreadsSuspend(void);

/**
 * @brief  Resume ISP and LCD reload threads
 */
void CAM_ThreadsResume(void);

#ifdef CAMERA_NN_SNAPSHOT_MODE
/**
 * @brief  Request a single snapshot from the NN pipe
 * @param  nn_buffer: Pointer to the NN buffer to capture into
 */
void CAM_NNPipe_RequestSnapshot(uint8_t *nn_buffer);
#endif

#ifdef __cplusplus
}
#endif

#endif /* APP_CAM_H */
