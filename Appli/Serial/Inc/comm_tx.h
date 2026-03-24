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

#ifndef COMM_TX_H
#define COMM_TX_H

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
void COM_TX_ThreadStart(void);

/**
 * @brief  Encode and transmit a DeviceMessage over UART (thread-safe)
 * @param  msg: Pointer to populated DeviceMessage
 */
void COM_TX_Send(const DeviceMessage *msg);

/**
 * @brief  Send a DeviceInfo response
 * @param  command_id: The command_id from the originating HostMessage
 */
void COM_Send_DeviceInfo(uint32_t command_id);

/**
 * @brief  Send an Ack response
 * @param  command_id: The command_id from the originating HostMessage
 * @param  success: true if command succeeded
 */
void COM_Send_Ack(uint32_t command_id, bool success);

/**
 * @brief  Send one TraceX dump chunk
 * @param  offset: Byte offset into trace buffer
 * @param  total_size: Total dump buffer size in bytes
 * @param  data: Chunk bytes
 * @param  data_len: Number of bytes in chunk
 * @param  done: true for last chunk
 */
void COM_Send_TraceXChunk(uint32_t offset, uint32_t total_size, const uint8_t *data, uint32_t data_len, bool done);

#ifdef __cplusplus
}
#endif

#endif /* COMM_TX_H */
