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
#include "app_lcd.h"
#include "comm_tx.h"
#include "stm32n6570_discovery_lcd.h"
#include "stm32n6xx_hal.h"
#include "ui.h"

/* ============================================================================
 * State
 * ============================================================================ */

static bool display_enabled = true;

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
    UI_ThreadResume();
    display_enabled = true;
  } else if (!cmd->enabled && display_enabled) {
    /* Suspend UI thread first (stops DMA2D usage) */
    UI_ThreadSuspend();

    /* Full LTDC deinit (disables LTDC clock, resets hardware) */
    LCD_DeInit();

    /* Disable clock sleep so LTDC/DMA2D draw no power during WFI */
    __HAL_RCC_DMA2D_CLK_SLEEP_DISABLE();
    __HAL_RCC_LTDC_CLK_SLEEP_DISABLE();

    display_enabled = false;
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
    handle_set_display_enabled(cmd_id, &msg->command.set_display_enabled);
    break;
  case HostMessage_get_device_info_tag:
    handle_get_device_info(cmd_id);
    break;
  default:
    COM_Send_Ack(cmd_id, false);
    break;
  }
}
