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

#include "app_comm_cmd.h"
#include "app_bqueue.h"
#include "app_cam.h"
#include "app_comm_tx.h"
#include "app_nn.h"
#include "app_nn_config.h"
#include "cmw_camera.h"
#include "stm32n6570_discovery_lcd.h"
#include <string.h>

/* ============================================================================
 * State
 * ============================================================================ */

static bool camera_enabled = true;
static bool display_enabled = true;

/* Image chunk accumulation buffer (points into an NN input queue slot) */
static uint8_t *img_accum_buf;
static uint32_t img_accum_size; /* Expected total_size for current transfer */

/* ============================================================================
 * Handlers
 * ============================================================================ */

static void handle_set_display_enabled(uint32_t cmd_id,
                                       const SetDisplayEnabled *cmd) {
  if (cmd->enabled && !display_enabled) {
    BSP_LCD_DisplayOn(0);
    HAL_GPIO_WritePin(LCD_DISP_BL_GPIO_PORT, LCD_DISP_BL_PIN, GPIO_PIN_SET);
    display_enabled = true;
  } else if (!cmd->enabled && display_enabled) {
    HAL_GPIO_WritePin(LCD_DISP_BL_GPIO_PORT, LCD_DISP_BL_PIN, GPIO_PIN_RESET);
    BSP_LCD_DisplayOff(0);
    display_enabled = false;
  }
  COM_Send_Ack(cmd_id, true);
}

static void handle_set_camera_enabled(uint32_t cmd_id,
                                      const SetCameraEnabled *cmd) {
  if (cmd->enabled && !camera_enabled) {
    CAM_DisplayPipe_Start(CMW_MODE_CONTINUOUS);

    bqueue_t *nn_q = NN_GetInputQueue();
    uint8_t *buf = bqueue_get_free(nn_q, 0);
    if (buf != NULL) {
      CAM_NNPipe_Start(buf, CMW_MODE_CONTINUOUS);
    }
    camera_enabled = true;
  } else if (!cmd->enabled && camera_enabled) {
    CAM_NNPipe_Stop();
    CMW_CAMERA_Suspend(DCMIPP_PIPE1);
    camera_enabled = false;
  }
  COM_Send_Ack(cmd_id, true);
}

static void handle_get_device_info(uint32_t cmd_id) {
  COM_Send_DeviceInfo(cmd_id);
}

static void handle_image_chunk(uint32_t cmd_id, const ImageChunk *chunk) {
  uint32_t expected_size = NN_WIDTH * NN_HEIGHT * NN_BPP;

  /* Reject if camera is still running */
  if (camera_enabled) {
    COM_Send_Ack(cmd_id, false);
    return;
  }

  /* Validate total_size */
  if (chunk->total_size != expected_size) {
    COM_Send_Ack(cmd_id, false);
    return;
  }

  /* On offset == 0, start a new transfer */
  if (chunk->offset == 0) {
    bqueue_t *nn_q = NN_GetInputQueue();
    img_accum_buf = bqueue_get_free(nn_q, 0);
    if (img_accum_buf == NULL) {
      COM_Send_Ack(cmd_id, false);
      return;
    }
    img_accum_size = chunk->total_size;
  }

  /* Validate we have an active transfer */
  if (img_accum_buf == NULL) {
    COM_Send_Ack(cmd_id, false);
    return;
  }

  /* Bounds check */
  if (chunk->offset + chunk->data.size > img_accum_size) {
    COM_Send_Ack(cmd_id, false);
    img_accum_buf = NULL;
    return;
  }

  /* Copy chunk data */
  memcpy(img_accum_buf + chunk->offset, chunk->data.bytes, chunk->data.size);

  /* Check if transfer is complete */
  if (chunk->offset + chunk->data.size == img_accum_size) {
    /* Flush DCache for the buffer */
    SCB_CleanDCache_by_Addr((uint32_t *)img_accum_buf, (int32_t)img_accum_size);

    /* Inject into NN pipeline */
    bqueue_t *nn_q = NN_GetInputQueue();
    bqueue_put_ready(nn_q);

    /* Mark next result with the host image ID */
    NN_SetHostImageId(chunk->image_id);

    img_accum_buf = NULL;
    img_accum_size = 0;
  }

  COM_Send_Ack(cmd_id, true);
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
  case HostMessage_set_camera_enabled_tag:
    handle_set_camera_enabled(cmd_id, &msg->command.set_camera_enabled);
    break;
  case HostMessage_get_device_info_tag:
    handle_get_device_info(cmd_id);
    break;
  case HostMessage_image_chunk_tag:
    handle_image_chunk(cmd_id, &msg->command.image_chunk);
    break;
  default:
    COM_Send_Ack(cmd_id, false);
    break;
  }
}
