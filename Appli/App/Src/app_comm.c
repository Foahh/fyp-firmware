/**
 ******************************************************************************
 * @file    app_comm.c
 * @author  Long Liangmao
 * @brief   Communication thread — drains a TX message queue over UART
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

#include "app_comm.h"
#include "app_error.h"
#include "stm32n6570_discovery.h"
#include "stm32n6xx_hal.h"
#include "tx_api.h"

/* ============================================================================
 * Configuration
 * ============================================================================ */

#define COMM_THREAD_STACK_SIZE 1024U
#define COMM_THREAD_PRIORITY   8

/* Queue depth — 2 slots matches the double-buffer in app_datalog.c */
#define COMM_QUEUE_DEPTH 2

/* ============================================================================
 * Message type exchanged through the queue
 * ============================================================================ */

typedef struct {
  const uint8_t *data;
  uint32_t len;
} comm_msg_t;

/* ============================================================================
 * Static resources
 * ============================================================================ */

static TX_THREAD comm_thread;
static UCHAR comm_thread_stack[COMM_THREAD_STACK_SIZE];

static TX_QUEUE comm_queue;

/*
 * ThreadX queues work in 32-bit words.  Each message occupies
 * ceil(sizeof(comm_msg_t) / 4) words.
 */
#define MSG_WORDS ((sizeof(comm_msg_t) + 3U) / 4U)
static ULONG comm_queue_storage[COMM_QUEUE_DEPTH * MSG_WORDS];

/* ============================================================================
 * Thread entry
 * ============================================================================ */

static void comm_thread_entry(ULONG arg) {
  UNUSED(arg);

  comm_msg_t msg;

  while (1) {
    UINT status = tx_queue_receive(&comm_queue, &msg, TX_WAIT_FOREVER);
    if (status != TX_SUCCESS) {
      continue;
    }

    APP_REQUIRE(msg.len <= UINT16_MAX);
    HAL_UART_Transmit(&hcom_uart[COM1], msg.data, (uint16_t)msg.len,
                      HAL_MAX_DELAY);
  }
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void Comm_Thread_Start(void) {
  UINT status;

  status = tx_queue_create(&comm_queue, "comm_tx", MSG_WORDS,
                           comm_queue_storage, sizeof(comm_queue_storage));
  APP_REQUIRE(status == TX_SUCCESS);

  status = tx_thread_create(&comm_thread, "comm", comm_thread_entry, 0,
                            comm_thread_stack, COMM_THREAD_STACK_SIZE,
                            COMM_THREAD_PRIORITY, COMM_THREAD_PRIORITY,
                            TX_NO_TIME_SLICE, TX_AUTO_START);
  APP_REQUIRE(status == TX_SUCCESS);
}

int Comm_Send(const uint8_t *data, uint32_t len) {
  comm_msg_t msg = {.data = data, .len = len};
  UINT status = tx_queue_send(&comm_queue, &msg, TX_NO_WAIT);
  return (status == TX_SUCCESS) ? 0 : -1;
}
