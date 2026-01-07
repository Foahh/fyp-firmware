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

#include "app_config.h"
#include "tx_api.h"
#include <stdint.h>

/**
 * @brief  Initialize the camera module
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
 * @param  ml_pipe_dst: Pointer to the NN buffer
 * @param  cam_mode: Camera mode (CMW_MODE_CONTINUOUS or CMW_MODE_SNAPSHOT)
 * @retval None
 */
void CAM_MLPipe_Start(uint8_t *ml_pipe_dst, uint32_t cam_mode);

/**
 * @brief  Stop the neural network pipe capture
 * @retval None
 */
void CAM_MLPipe_Stop(void);

/**
 * @brief  Update ISP parameters (call periodically for auto exposure/white
 * balance)
 * @retval None
 */
void CAM_IspUpdate(void);

/**
 * @brief  Initialize and create the ISP update thread
 * @param  memory_ptr: Memory pointer (unused, thread uses static allocation)
 * @note   Also initializes the ISP semaphore internally
 */
void Thread_IspUpdate_Init(VOID *memory_ptr);

#ifdef __cplusplus
}
#endif

#endif /* APP_CAM_H */
