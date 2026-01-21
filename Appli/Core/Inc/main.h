/**
 ******************************************************************************
 * @file           : main.h
 * @author         : Long Liangmao
 * @brief          : Header for main.c file.
 *                   This file contains the common defines of the application.
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

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__ICCARM__)
#define CMSE_NS_CALL __cmse_nonsecure_call
#define CMSE_NS_ENTRY __cmse_nonsecure_entry
#else
#define CMSE_NS_CALL __attribute((cmse_nonsecure_call))
#define CMSE_NS_ENTRY __attribute((cmse_nonsecure_entry))
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32n6xx_hal.h"

/* Exported types ------------------------------------------------------------*/
/* Function pointer declaration in non-secure*/
#if defined(__ICCARM__)
typedef void(CMSE_NS_CALL *funcptr)(void);
#else
typedef void CMSE_NS_CALL (*funcptr)(void);
#endif

/* typedef for non-secure callback functions */
typedef funcptr funcptr_NS;

/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);
extern volatile uint8_t *g_error_file;
extern volatile uint32_t g_error_line;
/* Private defines -----------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
