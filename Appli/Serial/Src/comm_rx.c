/**
 ******************************************************************************
 * @file    app_comm_rx.c
 * @author  Long Liangmao
 * @brief   Communication RX thread
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

#include "comm_rx.h"
#include "comm_cmd.h"
#include "error.h"
#include "messages.pb.h"
#include "pb_decode.h"
#include "stm32n6570_discovery.h"
#include "stm32n6xx_hal.h"
#include "thread_config.h"
#include "tx_api.h"
#include <string.h>

/* ============================================================================
 * Configuration
 * ============================================================================ */

/* Ring buffer size (must be power of 2) */
#define RX_RING_SIZE 2048U
#define RX_RING_MASK (RX_RING_SIZE - 1)

/* ============================================================================
 * Static resources
 * ============================================================================ */

/* Thread resources */
static struct {
  TX_THREAD thread;
  UCHAR stack[COMM_RX_THREAD_STACK_SIZE];
} comm_rx_ctx;

/* Single-byte HAL RX IT buffer */
static uint8_t rx_byte;

/* Ring buffer */
static uint8_t rx_ring[RX_RING_SIZE];
static volatile uint32_t rx_ring_head; /* Written by ISR */
static volatile uint32_t rx_ring_tail; /* Read by RX thread */
static volatile bool rx_ring_overflowed;

/* Semaphore signalled from ISR on each received byte */
static TX_SEMAPHORE rx_sem;

/* ============================================================================
 * Ring buffer helpers
 * ============================================================================ */

static uint32_t rx_ring_available(void) {
  return (rx_ring_head - rx_ring_tail) & RX_RING_MASK;
}

static uint8_t rx_read_byte_blocking(void) {
  /* Wait until a byte is available */
  while (rx_ring_available() == 0) {
    tx_semaphore_get(&rx_sem, TX_WAIT_FOREVER);
  }
  uint8_t out = rx_ring[rx_ring_tail & RX_RING_MASK];
  rx_ring_tail = (rx_ring_tail + 1) & RX_RING_MASK;
  return out;
}

/* ============================================================================
 * HAL UART RX callback (ISR context)
 * ============================================================================ */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == hcom_uart[COM1].Instance) {
    /* Push byte into ring buffer, drop on overflow. */
    uint32_t next_head = (rx_ring_head + 1) & RX_RING_MASK;
    if (next_head != rx_ring_tail) {
      rx_ring[rx_ring_head & RX_RING_MASK] = rx_byte;
      rx_ring_head = next_head;
    } else {
      rx_ring_overflowed = true;
    }

    /* Signal RX thread */
    tx_semaphore_put(&rx_sem);

    /* Re-arm single-byte receive */
    HAL_UART_Receive_IT(&hcom_uart[COM1], &rx_byte, 1);
  }
}

/* ============================================================================
 * RX thread entry
 * ============================================================================ */

static uint8_t rx_payload_buf[HostMessage_size];
static uint8_t rx_frame_buf[4U + HostMessage_size];

static void comm_rx_thread_entry(ULONG arg) {
  UNUSED(arg);

  HostMessage host_msg;
  uint32_t frame_len = 0;

  while (1) {
    /* If ISR had to drop bytes, force parser resync. */
    if (rx_ring_overflowed) {
      rx_ring_overflowed = false;
      frame_len = 0;
    }

    if (frame_len < sizeof(rx_frame_buf)) {
      rx_frame_buf[frame_len++] = rx_read_byte_blocking();
    }

    if (frame_len < 4U) {
      continue;
    }

    uint32_t len = (uint32_t)rx_frame_buf[0] |
                   ((uint32_t)rx_frame_buf[1] << 8) |
                   ((uint32_t)rx_frame_buf[2] << 16) |
                   ((uint32_t)rx_frame_buf[3] << 24);

    /* Invalid prefix: shift by one byte and search next frame. */
    if (len == 0 || len > HostMessage_size) {
      memmove(rx_frame_buf, rx_frame_buf + 1, frame_len - 1U);
      frame_len--;
      continue;
    }

    uint32_t need = 4U + len;
    while (frame_len < need) {
      rx_frame_buf[frame_len++] = rx_read_byte_blocking();
    }

    memcpy(rx_payload_buf, rx_frame_buf + 4U, len);

    /* Decode */
    host_msg = (HostMessage)HostMessage_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(rx_payload_buf, len);
    bool ok = pb_decode(&stream, HostMessage_fields, &host_msg);
    if (!ok) {
      /* Decode failed: shift by one byte and continue searching. */
      memmove(rx_frame_buf, rx_frame_buf + 1, frame_len - 1U);
      frame_len--;
      continue;
    }

    /* Dispatch */
    COM_Cmd_Dispatch(&host_msg);

    /* Consume this frame and keep any trailing bytes. */
    uint32_t remain = frame_len - need;
    if (remain > 0U) {
      memmove(rx_frame_buf, rx_frame_buf + need, remain);
    }
    frame_len = remain;
  }
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void COM_RX_ThreadStart(void) {
  UINT status;

  /* Initialize ring buffer indices */
  rx_ring_head = 0;
  rx_ring_tail = 0;
  rx_ring_overflowed = false;

  /* Create RX semaphore */
  status = tx_semaphore_create(&rx_sem, "comm_rx_sem", 0);
  APP_REQUIRE(status == TX_SUCCESS);

  /* Create RX thread */
  status = tx_thread_create(&comm_rx_ctx.thread, "comm_rx", comm_rx_thread_entry, 0,
                            comm_rx_ctx.stack, COMM_RX_THREAD_STACK_SIZE,
                            COMM_RX_THREAD_PRIORITY, COMM_RX_THREAD_PRIORITY,
                            TX_NO_TIME_SLICE, TX_AUTO_START);
  APP_REQUIRE(status == TX_SUCCESS);

  /* Arm first UART receive */
  HAL_UART_Receive_IT(&hcom_uart[COM1], &rx_byte, 1);
}
