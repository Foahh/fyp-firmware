/**
 ******************************************************************************
 * @file    app_comm_log.h
 * @author  Long Liangmao
 * @brief   Periodic device-to-host reporting
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

#ifndef APP_COMM_LOG_H
#define APP_COMM_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Create the comm log thread
 * @note   Must be called after Comm_TX_Start()
 */
void Comm_Log_Start(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_COMM_LOG_H */
