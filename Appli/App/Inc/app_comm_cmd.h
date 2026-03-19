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

#ifndef APP_COMM_CMD_H
#define APP_COMM_CMD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "messages.pb.h"

/**
 * @brief  Dispatch a decoded HostMessage to the appropriate handler
 * @param  msg: Pointer to decoded HostMessage
 */
void COM_Cmd_Dispatch(const HostMessage *msg);

#ifdef __cplusplus
}
#endif

#endif /* APP_COMM_CMD_H */
