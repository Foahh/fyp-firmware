/**
 ******************************************************************************
 * @file    app_comm.h
 * @author  Long Liangmao
 * @brief   Communication thread for non-blocking UART transmission
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

#ifndef APP_COMM_H
#define APP_COMM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief  Create the comm thread and TX message queue
 * @note   Must be called from ThreadX_Start() before the kernel runs
 */
void Comm_Thread_Start(void);

/**
 * @brief  Enqueue a frame for transmission (non-blocking)
 * @param  data: Pointer to frame buffer (must remain valid until transmitted)
 * @param  len:  Frame length in bytes
 * @retval 0 on success, non-zero if queue is full (frame dropped)
 */
int Comm_Send(const uint8_t *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* APP_COMM_H */
