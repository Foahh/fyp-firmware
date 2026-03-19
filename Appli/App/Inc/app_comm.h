/**
 ******************************************************************************
 * @file    app_comm.h
 * @author  Long Liangmao
 * @brief   Communication thread
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

/**
 * @brief  Create the comm thread
 * @note   Must be called from ThreadX_Start() before the kernel runs
 */
void Comm_Thread_Start(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_COMM_H */
