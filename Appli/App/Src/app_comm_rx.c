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

#include "app_comm_rx.h"
#include "app_comm_cmd.h"
#include "app_error.h"
#include "messages.pb.h"
#include "pb_decode.h"
#include "stm32n6570_discovery.h"
#include "stm32n6xx_hal.h"
#include "tx_api.h"

/* ============================================================================
 * Configuration
 * ============================================================================ */

#define COMM_RX_THREAD_STACK_SIZE 4096U
#define COMM_RX_THREAD_PRIORITY   8

/* Ring buffer size (must be power of 2) */
#define RX_RING_SIZE 2048U
#define RX_RING_MASK (RX_RING_SIZE - 1)

/* ============================================================================
 * Static resources
 * ============================================================================ */

static TX_THREAD comm_rx_thread;
static UCHAR comm_rx_thread_stack[COMM_RX_THREAD_STACK_SIZE];

/* Single-byte HAL RX IT buffer */
static uint8_t rx_byte;

/* Ring buffer */
static uint8_t rx_ring[RX_RING_SIZE];
static volatile uint32_t rx_ring_head; /* Written by ISR */
static volatile uint32_t rx_ring_tail; /* Read by RX thread */

/* Semaphore signalled from ISR on each received byte */
static TX_SEMAPHORE rx_sem;

/* ============================================================================
 * Ring buffer helpers
 * ============================================================================ */

static uint32_t rx_ring_available(void) {
  return (rx_ring_head - rx_ring_tail) & RX_RING_MASK;
}

static void rx_read_blocking(uint8_t *dst, uint32_t count) {
  for (uint32_t i = 0; i < count; i++) {
    /* Wait until a byte is available */
    while (rx_ring_available() == 0) {
      tx_semaphore_get(&rx_sem, TX_WAIT_FOREVER);
    }
    dst[i] = rx_ring[rx_ring_tail & RX_RING_MASK];
    rx_ring_tail = (rx_ring_tail + 1) & RX_RING_MASK;
  }
}

/* ============================================================================
 * HAL UART RX callback (ISR context)
 * ============================================================================ */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == hcom_uart[COM1].Instance) {
    /* Push byte into ring buffer */
    rx_ring[rx_ring_head & RX_RING_MASK] = rx_byte;
    rx_ring_head = (rx_ring_head + 1) & RX_RING_MASK;

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

static void comm_rx_thread_entry(ULONG arg) {
  UNUSED(arg);

  uint8_t len_buf[4];
  HostMessage host_msg;

  while (1) {
    /* Read 4-byte length prefix */
    rx_read_blocking(len_buf, 4);

    uint32_t len = (uint32_t)len_buf[0] |
                   ((uint32_t)len_buf[1] << 8) |
                   ((uint32_t)len_buf[2] << 16) |
                   ((uint32_t)len_buf[3] << 24);

    /* Validate length */
    if (len == 0 || len > HostMessage_size) {
      continue;
    }

    /* Read payload bytes */
    rx_read_blocking(rx_payload_buf, len);

    /* Decode */
    host_msg = (HostMessage)HostMessage_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(rx_payload_buf, len);
    bool ok = pb_decode(&stream, HostMessage_fields, &host_msg);
    if (!ok) {
      continue;
    }

    /* Dispatch */
    Comm_Cmd_Dispatch(&host_msg);
  }
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void Comm_RX_Start(void) {
  UINT status;

  /* Initialize ring buffer indices */
  rx_ring_head = 0;
  rx_ring_tail = 0;

  /* Create RX semaphore */
  status = tx_semaphore_create(&rx_sem, "comm_rx_sem", 0);
  APP_REQUIRE(status == TX_SUCCESS);

  /* Create RX thread */
  status = tx_thread_create(&comm_rx_thread, "comm_rx", comm_rx_thread_entry, 0,
                            comm_rx_thread_stack, COMM_RX_THREAD_STACK_SIZE,
                            COMM_RX_THREAD_PRIORITY, COMM_RX_THREAD_PRIORITY,
                            TX_NO_TIME_SLICE, TX_AUTO_START);
  APP_REQUIRE(status == TX_SUCCESS);

  /* Arm first UART receive */
  HAL_UART_Receive_IT(&hcom_uart[COM1], &rx_byte, 1);
}
