/**
 ******************************************************************************
 * @file    app.h
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

#ifndef __APP_H
#define __APP_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "tx_api.h"
#include "stm32n6xx_hal.h"

/**
 * @brief  Application initialization
 * @param  memory_ptr: memory pointer
 * @retval None
 */
void App_Init(VOID *memory_ptr);

#ifdef __cplusplus
}
#endif

#endif /* __APP_H */

