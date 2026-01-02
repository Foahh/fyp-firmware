/**
 ******************************************************************************
 * @file    thread_cam.h
 * @author  Long Liangmao
 * @brief   Camera thread header for ThreadX
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

#ifndef THREAD_CAM_H
#define THREAD_CAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tx_api.h"

/**
 * @brief  Initialize and create the camera thread
 * @param  memory_ptr: Memory pointer (unused, thread uses static allocation)
 * @retval TX_SUCCESS if successful, error code otherwise
 */
UINT CameraThread_Init(VOID *memory_ptr);

#ifdef __cplusplus
}
#endif

#endif /* THREAD_CAM_H */

