/**
 ******************************************************************************
 * @file    app_comm_tx.h
 * @author  Long Liangmao
 * @brief   Communication TX encoding and send infrastructure
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

#ifndef APP_COMM_TX_H
#define APP_COMM_TX_H

#ifdef __cplusplus
extern "C" {
#endif

#include "messages.pb.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief  Initialize TX mutex and buffers
 * @note   Must be called before any send function
 */
void Comm_TX_Start(void);

/**
 * @brief  Encode and transmit a DeviceMessage over UART (thread-safe)
 * @param  msg: Pointer to populated DeviceMessage
 */
void Comm_TX_Send(const DeviceMessage *msg);

/**
 * @brief  Send a DeviceInfo response
 * @param  command_id: The command_id from the originating HostMessage
 */
void Comm_Send_DeviceInfo(uint32_t command_id);

/**
 * @brief  Send an Ack response
 * @param  command_id: The command_id from the originating HostMessage
 * @param  success: true if command succeeded
 */
void Comm_Send_Ack(uint32_t command_id, bool success);

#ifdef __cplusplus
}
#endif

#endif /* APP_COMM_TX_H */
