/**
 ******************************************************************************
 * @file    app_comm_cmd.h
 * @author  Long Liangmao
 * @brief   Host command dispatcher
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

#ifndef COMM_CMD_H
#define COMM_CMD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "messages.pb.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief  Dispatch a decoded HostMessage to the appropriate handler
 * @param  msg: Pointer to decoded HostMessage
 */
void COM_Cmd_Dispatch(const HostMessage *msg);

/**
 * @brief  Whether any valid host command has been received
 * @retval true if host has been recognized, false otherwise
 */
bool COM_Cmd_IsHostRecognized(void);

/**
 * @brief  Millisecond tick when host was last seen
 * @retval HAL tick at last valid host command, or 0 if never seen
 */
uint32_t COM_Cmd_GetLastHostSeenTick(void);

#ifdef __cplusplus
}
#endif

#endif /* COMM_CMD_H */
