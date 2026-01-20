/**
 ******************************************************************************
 * @file           : extmem_manager.h
 * @version        : 1.0.0
 * @brief          : Header for secure_manager_api.c file.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#ifndef __EXTMEM_MANAGER_H__
#define __EXTMEM_MANAGER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32_extmem_conf.h"

void EXTMEM_MANAGER_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __EXTMEM_MANAGER_H__ */
