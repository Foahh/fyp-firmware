/**
 ******************************************************************************
 * @file    app_comm_cmd.c
 * @author  Long Liangmao
 * @brief   Host command dispatcher and handlers
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

#include "comm_cmd.h"
#include "cam.h"
#include "comm_tx.h"
#include "display.h"
#include "stm32n6570_discovery_lcd.h"
#include "stm32n6xx_hal.h"
#include "ui.h"

/* ============================================================================
 * State
 * ============================================================================ */

static volatile bool display_enabled = true;
static volatile bool display_pipe_enabled = true;
static volatile bool host_recognized = false;
static volatile uint32_t host_last_seen_tick = 0;

/* ============================================================================
 * Handlers
 * ============================================================================ */

static void handle_set_display_enabled(uint32_t cmd_id,
                                       const SetDisplayEnabled *cmd) {
  if (cmd->enabled && !display_enabled) {
    /* Re-enable clock sleep before init */
    __HAL_RCC_LTDC_CLK_SLEEP_ENABLE();
    __HAL_RCC_DMA2D_CLK_SLEEP_ENABLE();

    LCD_Init();

    CAM_LCDReloadThreadResume();
    CAM_DisplayPipe_Resume();

    UI_ThreadResume();

    display_enabled = true;
    display_pipe_enabled = true;
  } else if (!cmd->enabled && display_enabled) {
    CAM_DisplayPipe_Suspend();
    CAM_LCDReloadThreadSuspend();

    UI_ThreadSuspend();

    LCD_DeInit();

    __HAL_RCC_DMA2D_CLK_SLEEP_DISABLE();
    __HAL_RCC_LTDC_CLK_SLEEP_DISABLE();

    display_enabled = false;
    display_pipe_enabled = false;
  }
  COM_Send_Ack(cmd_id, true);
}

static void handle_set_debug_op_enabled(uint32_t cmd_id,
                                        const SetDebugOpEnabled *cmd) {
  /* Pipe-only command: do not touch UI/LCD/global display power state. */
  if (!display_enabled) {
    COM_Send_Ack(cmd_id, false);
    return;
  }

  if (cmd->enabled && !display_pipe_enabled) {
    CAM_LCDReloadThreadResume();
    CAM_DisplayPipe_Resume();
    UI_ThreadResume();
    display_pipe_enabled = true;
  } else if (!cmd->enabled && display_pipe_enabled) {
    CAM_DisplayPipe_Suspend();
    CAM_LCDReloadThreadSuspend();
    UI_ThreadSuspend();
    display_pipe_enabled = false;
  }
  COM_Send_Ack(cmd_id, true);
}

static void handle_get_device_info(uint32_t cmd_id) {
  COM_Send_DeviceInfo(cmd_id);
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void COM_Cmd_Dispatch(const HostMessage *msg) {
  uint32_t cmd_id = msg->command_id;

  switch (msg->which_command) {
  case HostMessage_set_display_enabled_tag:
    host_recognized = true;
    host_last_seen_tick = HAL_GetTick();
    handle_set_display_enabled(cmd_id, &msg->command.set_display_enabled);
    break;
  case HostMessage_get_device_info_tag:
    host_recognized = true;
    host_last_seen_tick = HAL_GetTick();
    handle_get_device_info(cmd_id);
    break;
  case HostMessage_set_debug_op_enabled_tag:
    host_recognized = true;
    host_last_seen_tick = HAL_GetTick();
    handle_set_debug_op_enabled(cmd_id, &msg->command.set_debug_op_enabled);
    break;
  default:
    COM_Send_Ack(cmd_id, false);
    break;
  }
}

bool COM_Cmd_IsHostRecognized(void) {
  return host_recognized;
}

uint32_t COM_Cmd_GetLastHostSeenTick(void) {
  return host_last_seen_tick;
}
