/**
******************************************************************************
* @file    thread_led.h
* @author  Long Liangmao
*
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

#ifndef THREAD_LED_H
#define THREAD_LED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tx_api.h"

UINT LedBlinkThread_Init(VOID *memory_ptr);

#ifdef __cplusplus
}
#endif

#endif /* THREAD_LED_H */
