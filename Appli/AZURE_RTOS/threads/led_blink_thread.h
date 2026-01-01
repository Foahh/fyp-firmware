/**
******************************************************************************
* @file    led_blink_thread.h
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

#ifndef LED_BLINK_THREAD_H
#define LED_BLINK_THREAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tx_api.h"

UINT LedBlinkThread_Init(VOID *memory_ptr);

#ifdef __cplusplus
}
#endif

#endif /* LED_BLINK_THREAD_H */
