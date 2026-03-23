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

#ifndef COMM_LOG_H
#define COMM_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Create the comm log thread
 * @note   Must be called after COM_TX_Start()
 */
void COM_Log_ThreadStart(void);

#ifdef __cplusplus
}
#endif

#endif /* COMM_LOG_H */
