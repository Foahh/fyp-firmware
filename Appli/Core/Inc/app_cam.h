/**
 ******************************************************************************
 * @file    app_cam.h
 * @author  Long Liangmao
 * @brief   Camera application header for STM32N6570-DK
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

#ifndef APP_CAM_H
#define APP_CAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32n6xx_hal.h"
#include "tx_api.h"
#include <stdint.h>
#include "app_config.h"

/**
 * @brief  Initialize the camera module
 * @retval 0 on success, negative error code on failure
 */
int CAM_Init(void);

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
 * @brief  Update ISP parameters (call periodically for auto exposure/white
 * balance)
 * @retval None
 */
void CAM_IspUpdate(void);

/**
 * @brief  Initialize ISP semaphore for vsync callback
 * @retval TX_SUCCESS on success, error code otherwise
 */
UINT CAM_InitIspSemaphore(void);

/**
 * @brief  Initialize and create the ISP update thread
 * @param  memory_ptr: Memory pointer (unused, thread uses static allocation)
 * @retval TX_SUCCESS if successful, error code otherwise
 */
UINT Thread_IspUpdate_Init(VOID *memory_ptr);

#ifdef __cplusplus
}
#endif

#endif /* APP_CAM_H */
