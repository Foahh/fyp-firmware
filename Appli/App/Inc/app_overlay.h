/**
 ******************************************************************************
 * @file    app_overlay.h
 * @author  Long Liangmao
 * @brief   Detection overlay rendering thread and drawing functions
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
#ifndef APP_OVERLAY_H
#define APP_OVERLAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tx_api.h"

/**
 * @brief  Initialize overlay module (thread)
 * @param  memory_ptr: ThreadX memory pool (unused, static allocation)
 */
void Overlay_Thread_Init(VOID *memory_ptr);

#ifdef __cplusplus
}
#endif

#endif /* APP_OVERLAY_H */
