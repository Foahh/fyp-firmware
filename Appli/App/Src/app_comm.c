/**
 ******************************************************************************
 * @file    app_comm.c
 * @author  Long Liangmao
 * @brief   Communication thread
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
#include "app_postprocess.h"
#include "messages.pb.h"
#include "pb_encode.h"
#include "stm32n6570_discovery.h"
#include "stm32n6xx_hal.h"
#include "tx_api.h"

/* ============================================================================
 * Configuration
 * ============================================================================ */

#define COMM_THREAD_STACK_SIZE 2048U
#define COMM_THREAD_PRIORITY   8

/* ============================================================================
 * Static resources
 * ============================================================================ */

static TX_THREAD comm_thread;
static UCHAR comm_thread_stack[COMM_THREAD_STACK_SIZE];

/* Double buffer */
#define FRAME_BUF_SIZE (4 + DatalogMessage_size)
static uint8_t frame_bufs[2][FRAME_BUF_SIZE];
static uint8_t buf_idx = 0;

/* ============================================================================
 * Internal helpers
 * ============================================================================ */

static void comm_send_detection_result(const detection_info_t *info) {
  DatalogMessage msg = DatalogMessage_init_zero;
  msg.which_payload = DatalogMessage_detection_result_tag;

  DetectionResult *df = &msg.payload.detection_result;
  df->timestamp = HAL_GetTick();

  df->has_timing = true;
  df->timing.inference_ms = info->inference_ms;
  df->timing.postprocess_ms = info->postprocess_ms;
  df->timing.nn_period_ms = info->nn_period_ms;
  df->timing.frame_drops = info->frame_drops;

  int n = info->nb_detect;
  if (n > DETECTION_MAX_BOXES) {
    n = DETECTION_MAX_BOXES;
  }
  df->detections_count = (pb_size_t)n;
  for (int i = 0; i < n; i++) {
    const od_pp_outBuffer_t *d = &info->detects[i];
    df->detections[i].x_center = d->x_center;
    df->detections[i].y_center = d->y_center;
    df->detections[i].width = d->width;
    df->detections[i].height = d->height;
    df->detections[i].conf = d->conf;
    df->detections[i].class_index = d->class_index;
  }

  uint8_t *buf = frame_bufs[buf_idx];

  pb_ostream_t stream = pb_ostream_from_buffer(buf + 4, FRAME_BUF_SIZE - 4);
  bool ok = pb_encode(&stream, DatalogMessage_fields, &msg);
  APP_REQUIRE(ok);

  uint32_t len = (uint32_t)stream.bytes_written;
  buf[0] = (uint8_t)(len);
  buf[1] = (uint8_t)(len >> 8);
  buf[2] = (uint8_t)(len >> 16);
  buf[3] = (uint8_t)(len >> 24);

  APP_REQUIRE(4 + len <= UINT16_MAX);
  HAL_UART_Transmit(&hcom_uart[COM1], buf, (uint16_t)(4 + len),
                    HAL_MAX_DELAY);

  buf_idx ^= 1;
}

/* ============================================================================
 * Thread entry
 * ============================================================================ */

static void comm_thread_entry(ULONG arg) {
  UNUSED(arg);

  TX_EVENT_FLAGS_GROUP *event_flags = Postprocess_GetUpdateEventFlags();
  ULONG actual_flags;

  while (1) {
    UINT status = tx_event_flags_get(event_flags, 0x01, TX_OR_CLEAR,
                                     &actual_flags, TX_WAIT_FOREVER);
    if (status != TX_SUCCESS) {
      continue;
    }

    detection_info_t *info = Postprocess_GetInfo();
    if (info != NULL) {
      comm_send_detection_result(info);
    }
  }
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void Comm_Thread_Start(void) {
  UINT status;

  status = tx_thread_create(&comm_thread, "comm", comm_thread_entry, 0,
                            comm_thread_stack, COMM_THREAD_STACK_SIZE,
                            COMM_THREAD_PRIORITY, COMM_THREAD_PRIORITY,
                            TX_NO_TIME_SLICE, TX_AUTO_START);
  APP_REQUIRE(status == TX_SUCCESS);
}
