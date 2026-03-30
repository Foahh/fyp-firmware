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
#include "tracex.h"
#include "ui.h"

/* ============================================================================
 * State
 * ============================================================================ */

static volatile bool display_enabled = true;
static volatile bool host_recognized = false;
static volatile uint32_t host_last_seen_tick = 0;

/* ============================================================================
 * Handlers
 * ============================================================================ */

static void handle_set_display_enabled(uint32_t cmd_id,
                                       const SetDisplayEnabled *cmd) {
  if (cmd->enabled && !display_enabled) {
    __HAL_RCC_PLL4_ENABLE();
    while (!__HAL_RCC_GET_FLAG(RCC_FLAG_PLL4RDY)) {
    }

    __HAL_RCC_LTDC_CLK_SLEEP_ENABLE();
    __HAL_RCC_DMA2D_CLK_SLEEP_ENABLE();

    LCD_Init();

    CAM_LCDReloadThreadResume();
    CAM_DisplayPipe_Resume();

    UI_ThreadResume();

    display_enabled = true;
  } else if (!cmd->enabled && display_enabled) {
    CAM_DisplayPipe_Suspend();
    CAM_LCDReloadThreadSuspend();

    UI_ThreadSuspend();

    LCD_DeInit();

    __HAL_RCC_DMA2D_CLK_SLEEP_DISABLE();
    __HAL_RCC_LTDC_CLK_SLEEP_DISABLE();

    __HAL_RCC_PLL4_DISABLE();

    display_enabled = false;
  }
  COM_Send_Ack(cmd_id, true);
}

static void handle_get_device_info(uint32_t cmd_id) {
  COM_Send_DeviceInfo(cmd_id);
}

static void handle_get_tracex_dump(uint32_t cmd_id, const GetTraceXDump *cmd) {
  uint32_t total_size;
  uint32_t offset = 0U;
  uint32_t chunk_size;
  uint8_t chunk[256];

  if (!TraceX_IsEnabled()) {
    COM_Send_Ack(cmd_id, false);
    return;
  }

  total_size = TraceX_GetBufferSize();
  chunk_size = cmd->chunk_size;
  if (chunk_size == 0U || chunk_size > sizeof(chunk)) {
    chunk_size = sizeof(chunk);
  }

  while (offset < total_size) {
    size_t copied = TraceX_Read(offset, chunk, chunk_size);
    bool done;
    if (copied == 0U) {
      break;
    }
    done = (offset + (uint32_t)copied) >= total_size;
    COM_Send_TraceXChunk(offset, total_size, chunk, (uint32_t)copied, done);
    offset += (uint32_t)copied;
  }

  COM_Send_Ack(cmd_id, (offset == total_size));
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
  case HostMessage_get_tracex_dump_tag:
    host_recognized = true;
    host_last_seen_tick = HAL_GetTick();
    handle_get_tracex_dump(cmd_id, &msg->command.get_tracex_dump);
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
