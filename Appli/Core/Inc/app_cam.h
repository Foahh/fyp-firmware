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

/* Camera FPS configuration */
#define CAMERA_FPS 30

/* Camera flip/mirror configuration */
#define CAMERA_FLIP CMW_MIRRORFLIP_NONE

/* Display dimensions for STM32N6570-DK LCD */
#define LCD_BG_WIDTH 800
#define LCD_BG_HEIGHT 480

/* Display pixel format */
#define DISPLAY_FORMAT DCMIPP_PIXEL_PACKER_FORMAT_RGB565_1
#define DISPLAY_BPP 2

/**
 * @brief  DCMIPP Clock Config for DCMIPP.
 * @param  hdcmipp  DCMIPP Handle
 *         Being __weak it can be overwritten by the application
 * @retval HAL_status
 */
HAL_StatusTypeDef MX_DCMIPP_ClockConfig(DCMIPP_HandleTypeDef *hdcmipp);

/**
 * @brief  Initialize the camera module
 * @retval 0 on success, negative error code on failure
 */
int CAM_Init(void);

/**
 * @brief  Start the display pipe capture
 * @param  display_pipe_dst: Pointer to the display buffer
 * @param  cam_mode: Camera mode (CMW_MODE_CONTINUOUS or CMW_MODE_SNAPSHOT)
 * @retval None
 */
void CAM_DisplayPipe_Start(uint8_t *display_pipe_dst, uint32_t cam_mode);

/**
 * @brief  Update ISP parameters (call periodically for auto exposure/white
 * balance)
 * @retval None
 */
void CAM_IspUpdate(void);

/**
 * @brief  Get sensor resolution after initialization
 * @param  width: Pointer to store width
 * @param  height: Pointer to store height
 * @retval None
 */
void CAM_GetResolution(uint32_t *width, uint32_t *height);

/**
 * @brief  Configure camera peripherals (called before thread starts)
 * @retval 0 on success, negative error code on failure
 */
int CAM_ConfigurePeripherals(void);

/**
 * @brief  Initialize and create the camera thread
 * @param  memory_ptr: Memory pointer (unused, thread uses static allocation)
 * @retval TX_SUCCESS if successful, error code otherwise
 */
UINT Thread_Camera_Init(VOID *memory_ptr);

#ifdef __cplusplus
}
#endif

#endif /* APP_CAM_H */
