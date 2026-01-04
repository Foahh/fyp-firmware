/**
 ******************************************************************************
 * @file    app_buffers.h
 * @author  Long Liangmao
 * @brief   Centralized buffer management for display and camera pipelines
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

#ifndef APP_Buffer_H
#define APP_Buffer_H

#ifdef __cplusplus
extern "C" {
#endif

#include "app_config.h"
#include <stddef.h>
#include <stdint.h>

extern volatile int camera_display_idx;
extern volatile int camera_capture_idx;
extern volatile int ui_display_idx;
extern uint8_t camera_display_buffers[DISPLAY_BUFFER_NB][DISPLAY_LETTERBOX_WIDTH * DISPLAY_LETTERBOX_HEIGHT * DISPLAY_BPP];
extern uint8_t ui_display_buffers[2][LCD_WIDTH * LCD_HEIGHT * 4];

/**
 * @brief  Get current display buffer index
 * @retval Current display buffer index
 */
#define Buffer_GetCameraDisplayIndex() (camera_display_idx)

/**
 * @brief  Get current capture buffer index
 * @retval Current capture buffer index
 */
#define Buffer_GetCameraCaptureIndex() (camera_capture_idx)

/**
 * @brief  Get next display buffer index (wraps around)
 * @retval Next display buffer index
 */
#define Buffer_GetNextCameraDisplayIndex() \
  (((camera_display_idx) + 1) % DISPLAY_BUFFER_NB)

/**
 * @brief  Get next capture buffer index (wraps around)
 * @retval Next capture buffer index
 */
#define Buffer_GetNextCameraCaptureIndex() \
  (((camera_capture_idx) + 1) % DISPLAY_BUFFER_NB)

/**
 * @brief  Get pointer to a specific camera display buffer
 * @param  idx: Buffer index (0 to DISPLAY_BUFFER_NB-1)
 * @retval Pointer to the buffer, NULL if index is invalid
 */
#define Buffer_GetCameraDisplayBuffer(idx)                                       \
  ({                                                                             \
    int _idx = (idx);                                                            \
    ((unsigned)_idx >= DISPLAY_BUFFER_NB) ? NULL : camera_display_buffers[_idx]; \
  })

/**
 * @brief  Set camera display buffer index
 * @param  idx: New display buffer index
 * @retval 0 on success, -1 if index is invalid
 */
#define Buffer_SetCameraDisplayIndex(idx)                      \
  ({                                                           \
    int _idx = (idx);                                          \
    int _ret = ((unsigned)_idx >= DISPLAY_BUFFER_NB) ? -1 : 0; \
    if (_ret == 0) {                                           \
      camera_display_idx = _idx;                               \
    }                                                          \
    _ret;                                                      \
  })

/**
 * @brief  Set camera capture buffer index
 * @param  idx: New capture buffer index
 * @retval 0 on success, -1 if index is invalid
 */
#define Buffer_SetCameraCaptureIndex(idx)                      \
  ({                                                           \
    int _idx = (idx);                                          \
    int _ret = ((unsigned)_idx >= DISPLAY_BUFFER_NB) ? -1 : 0; \
    if (_ret == 0) {                                           \
      camera_capture_idx = _idx;                               \
    }                                                          \
    _ret;                                                      \
  })

/**
 * @brief  Get current UI display buffer index
 * @retval Current UI display buffer index (0 or 1)
 */
#define Buffer_GetUIDisplayIndex() (ui_display_idx)

/**
 * @brief  Get next UI display buffer index (for double buffering)
 * @retval Next UI display buffer index (0 or 1)
 */
#define Buffer_GetNextUIDisplayIndex() ((ui_display_idx) ^ 1)

/**
 * @brief  Get pointer to a specific UI display buffer (ARGB8888)
 * @param  idx: Buffer index (0 or 1)
 * @retval Pointer to the UI buffer, NULL if index is invalid
 */
#define Buffer_GetUIBuffer(idx)                              \
  ({                                                         \
    int _idx = (idx);                                        \
    ((unsigned)_idx >= 2) ? NULL : ui_display_buffers[_idx]; \
  })

/**
 * @brief  Get pointer to UI front buffer (currently displayed)
 * @retval Pointer to the front UI buffer
 */
#define Buffer_GetUIFrontBuffer() (ui_display_buffers[ui_display_idx])

/**
 * @brief  Get pointer to UI back buffer (for drawing)
 * @retval Pointer to the back UI buffer
 */
#define Buffer_GetUIBackBuffer() (ui_display_buffers[ui_display_idx ^ 1])

/**
 * @brief  Set UI display buffer index
 * @param  idx: New display buffer index (0 or 1)
 * @retval 0 on success, -1 if index is invalid
 */
#define Buffer_SetUIDisplayIndex(idx)          \
  ({                                           \
    int _idx = (idx);                          \
    int _ret = ((unsigned)_idx >= 2) ? -1 : 0; \
    if (_ret == 0) {                           \
      ui_display_idx = _idx;                   \
    }                                          \
    _ret;                                      \
  })

/**
 * @brief  Initialize all buffers and cache
 * @note   Fail-fast: panics on unrecoverable issues (if any are added later)
 */
void Buffer_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_Buffer_H */
