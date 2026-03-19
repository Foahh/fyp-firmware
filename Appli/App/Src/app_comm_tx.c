/**
 ******************************************************************************
 * @file    app_comm_tx.c
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

#include "app_comm_tx.h"
#include "app_error.h"
#include "app_lcd_config.h"
#include "app_nn_config.h"
#include "model_config.h"
#include "pb_encode.h"
#include "stm32n6570_discovery.h"
#include "stm32n6xx_hal.h"
#include "tx_api.h"
#include <string.h>

/* ============================================================================
 * Static resources
 * ============================================================================ */

/* TX frame double buffer */
#define TX_FRAME_BUF_SIZE (4 + DeviceMessage_size)
static uint8_t tx_frame_bufs[2][TX_FRAME_BUF_SIZE];
static uint8_t tx_buf_idx = 0;

/* TX mutex for shared send helper */
static TX_MUTEX tx_mutex;

/* ============================================================================
 * Public API
 * ============================================================================ */

void Comm_TX_Start(void) {
  UINT status;

  status = tx_mutex_create(&tx_mutex, "comm_tx_mutex", TX_NO_INHERIT);
  APP_REQUIRE(status == TX_SUCCESS);
}

void Comm_TX_Send(const DeviceMessage *msg) {
  UINT status = tx_mutex_get(&tx_mutex, TX_WAIT_FOREVER);
  APP_REQUIRE(status == TX_SUCCESS);

  uint8_t *buf = tx_frame_bufs[tx_buf_idx];

  pb_ostream_t stream = pb_ostream_from_buffer(buf + 4, TX_FRAME_BUF_SIZE - 4);
  bool ok = pb_encode(&stream, DeviceMessage_fields, msg);
  APP_REQUIRE(ok);

  uint32_t len = (uint32_t)stream.bytes_written;
  buf[0] = (uint8_t)(len);
  buf[1] = (uint8_t)(len >> 8);
  buf[2] = (uint8_t)(len >> 16);
  buf[3] = (uint8_t)(len >> 24);

  APP_REQUIRE(4 + len <= UINT16_MAX);
  HAL_UART_Transmit(&hcom_uart[COM1], buf, (uint16_t)(4 + len),
                    HAL_MAX_DELAY);

  tx_buf_idx ^= 1;

  tx_mutex_put(&tx_mutex);
}

void Comm_Send_DeviceInfo(uint32_t command_id) {
  DeviceMessage msg = DeviceMessage_init_zero;
  msg.which_payload = DeviceMessage_device_info_tag;

  DeviceInfo *di = &msg.payload.device_info;
  di->display_width = LCD_WIDTH;
  di->display_height = LCD_HEIGHT;
  di->letterbox_width = DISPLAY_LETTERBOX_WIDTH;
  di->letterbox_height = DISPLAY_LETTERBOX_HEIGHT;
  di->nn_width = NN_WIDTH;
  di->nn_height = NN_HEIGHT;
  di->command_id = command_id;

  /* Model name */
  strncpy(di->model_name, MDL_DISPLAY_NAME, sizeof(di->model_name) - 1);
  di->model_name[sizeof(di->model_name) - 1] = '\0';

  /* Class labels */
  int n_labels = (int)(sizeof(MDL_PP_CLASS_LABELS) / sizeof(MDL_PP_CLASS_LABELS[0]));
  if (n_labels > 10) {
    n_labels = 10;
  }
  di->class_labels_count = (pb_size_t)n_labels;
  for (int i = 0; i < n_labels; i++) {
    strncpy(di->class_labels[i], MDL_PP_CLASS_LABELS[i], sizeof(di->class_labels[i]) - 1);
    di->class_labels[i][sizeof(di->class_labels[i]) - 1] = '\0';
  }

  Comm_TX_Send(&msg);
}

void Comm_Send_Ack(uint32_t command_id, bool success) {
  DeviceMessage msg = DeviceMessage_init_zero;
  msg.which_payload = DeviceMessage_ack_tag;
  msg.payload.ack.command_id = command_id;
  msg.payload.ack.success = success;

  Comm_TX_Send(&msg);
}
