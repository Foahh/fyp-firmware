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
#include "app_lcd.h"
#include "app_nn.h"
#include "app_nn_config.h"
#include "app_pp.h"
#include "app_tof.h"
#include "app_ui.h"
#include "cmw_camera.h"
#include "stm32n6570_discovery_lcd.h"
#include "stm32n6xx_hal.h"
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

static void handle_set_camera_enabled(uint32_t cmd_id,
                                      const SetCameraEnabled *cmd) {
  if (cmd->enabled && !camera_enabled) {
    /* Re-enable clock sleep before init */
    __HAL_RCC_DCMIPP_CLK_SLEEP_ENABLE();
    __HAL_RCC_CSI_CLK_SLEEP_ENABLE();

    /* Reinitialize camera (sensor, DCMIPP pipes) */
    CAM_Init();

    /* Resume threads (ISP, lcd_reload, ToF) */
    CAM_ThreadsResume();
    TOF_Resume();

    /* Start capture */
    CAM_DisplayPipe_Start(CMW_MODE_CONTINUOUS);

#ifdef CAMERA_NN_SNAPSHOT_MODE
    CAM_NNPipe_Start(NN_GetSnapshotBuffer(), CMW_MODE_SNAPSHOT);
#else
    bqueue_t *nn_q = NN_GetInputQueue();
    uint8_t *buf = bqueue_get_free(nn_q, 0);
    if (buf != NULL) {
      CAM_NNPipe_Start(buf, CMW_MODE_CONTINUOUS);
    }
#endif
    camera_enabled = true;
  } else if (!cmd->enabled && camera_enabled) {
    /* Stop capture first */
    CAM_NNPipe_Stop();

    /* Suspend threads that depend on camera input */
    TOF_Stop();
    CAM_ThreadsSuspend();

    /* Full camera deinit (DCMIPP, CSI, sensor power down) */
    CAM_DeInit();

    /* Disable clock sleep so peripherals draw no power during WFI */
    __HAL_RCC_DCMIPP_CLK_SLEEP_DISABLE();
    __HAL_RCC_CSI_CLK_SLEEP_DISABLE();

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
#ifdef CAMERA_NN_SNAPSHOT_MODE
    img_accum_buf = NN_GetSnapshotBuffer();
#else
    bqueue_t *nn_q = NN_GetInputQueue();
    img_accum_buf = bqueue_get_free(nn_q, 0);
#endif
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
#ifdef CAMERA_NN_SNAPSHOT_MODE
    NN_SignalSnapshotReady();
#else
    bqueue_t *nn_q = NN_GetInputQueue();
    bqueue_put_ready(nn_q);
#endif

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
