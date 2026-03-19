/**
 ******************************************************************************
 * @file    app_comm_rx.h
 * @author  Long Liangmao
 * @brief   Communication RX thread
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

#ifndef APP_COMM_RX_H
#define APP_COMM_RX_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Create the comm RX thread and arm UART receive
 * @note   Must be called from ThreadX_Start() after COM_TX_Start()
 */
void COM_RX_Thread_Start(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_COMM_RX_H */
