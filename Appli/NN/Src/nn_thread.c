/**
 ******************************************************************************
 * @file    nn_thread.c
 * @author  Long Liangmao
 * @brief   Neural network thread and ATON runtime implementation
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

#include "cam.h"
#include "error.h"
#include "model_config.h"
#include "nn.h"
#include "nn_config.h"
#include "stm32n6xx_hal.h"
#include "thread_config.h"
#include "timebase.h"
#include "utils.h"
#include <string.h>

#include "power_measurement_sync.h"

/* Include ST.AI runtime API */
#include "stai.h"

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static uint32_t NN_RunInferenceCycles(stai_network *network);
static void nn_thread_entry(ULONG arg);

/* ============================================================================
 * Configuration Constants
 * ============================================================================ */

/* NN output sizes from model header (MDL_NN_OUT_BUFFER_SIZE / MDL_NN_OUT_SIZES) */

#define NN_OUT_BUFFER_SIZE MDL_NN_OUT_BUFFER_SIZE
#define NN_INPUT_SIZE      (NN_WIDTH * NN_HEIGHT * NN_BPP)

/* ============================================================================
 * Global State Variables
 * ============================================================================ */

/* Declare NN context */
MDL_CONTEXT_DECLARE(nn_ctx_network)

/* Thread resources */
static struct {
  TX_THREAD thread;
  UCHAR stack[NN_THREAD_STACK_SIZE];
} nn_ctx;

/* Buffer queues and input buffers */
#ifdef SNAPSHOT_MODE
static TX_EVENT_FLAGS_GROUP nn_snapshot_flags;
static uint8_t nn_input_buffer[NN_INPUT_SIZE] ALIGN_32 IN_PSRAM;
#else
static bqueue_t nn_input_queue;
static uint8_t nn_input_buffers[3][NN_INPUT_SIZE] ALIGN_32 IN_PSRAM;
#endif

/* NN output buffers */
static bqueue_t nn_output_queue;
static nn_output_frame_t nn_output_buffers[3];

/* Output sizes array */
static const uint32_t nn_out_sizes[NN_OUT_NB] = MDL_NN_OUT_SIZES;

#ifndef SNAPSHOT_MODE
/* Frame drop counter (accumulated from bqueue skips in NN thread) */
static uint32_t nn_frame_drop_count = 0;
#endif

/* Network info (initialized in NN_ThreadStart, used by postprocess) */
static stai_network_info nn_info;

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief  Run ATON inference with WFE support
 *         Handles wait-for-event (WFE) during inference for power efficiency
 */
static uint32_t NN_RunInferenceCycles(stai_network *network) {
  stai_return_code ret;
  uint32_t start_cycles;

  start_cycles = DWT->CYCCNT;

  PWR_SyncBegin();
  do {
    ret = mdl_run(network, STAI_MODE_ASYNC);
    if (ret == STAI_RUNNING_WFE) {
      LL_ATON_OSAL_WFE();
    }
  } while (ret == STAI_RUNNING_WFE || ret == STAI_RUNNING_NO_WFE);
  APP_REQUIRE(ret == STAI_DONE);
  PWR_SyncEnd();

  return DWT->CYCCNT - start_cycles;
}

/* ============================================================================
 * Thread Entry Points
 * ============================================================================ */

/**
 * @brief  NN thread entry function
 *         Main inference loop: waits for input, runs inference, signals output
 * @param  arg: Thread argument (unused)
 */
static void nn_thread_entry(ULONG arg) {
  UNUSED(arg);

  stai_return_code stai_ret;
  uint32_t last_inference_end_cycles;

  last_inference_end_cycles = DWT->CYCCNT;

  /* Main inference loop */
  while (1) {
    uint8_t *capture_buffer;
    nn_output_frame_t *output_frame;
    uint8_t *output_buffer;
    stai_ptr out_ptrs[NN_OUT_NB];
    uint32_t inference_cycles;
    uint32_t inference_end_cycles;
    uint32_t period_cycles;
    nn_timing_t timing_sample;
    int i;

#ifdef SNAPSHOT_MODE
    /* Wait for snapshot frame */
    ULONG actual_flags;
    tx_event_flags_get(&nn_snapshot_flags, 0x01, TX_OR_CLEAR,
                       &actual_flags, TX_WAIT_FOREVER);
    capture_buffer = nn_input_buffer;
#else
    /* Get latest input buffer (LIFO), discarding stale frames */
    uint32_t skipped = 0;
    capture_buffer = BQUE_GetReadyLatest(&nn_input_queue, &skipped);
    nn_frame_drop_count += skipped;
    APP_REQUIRE(capture_buffer != NULL);
#endif

    /* Get output buffer (blocking) */
    output_frame = (nn_output_frame_t *)BQUE_GetFree(&nn_output_queue, 1);
    APP_REQUIRE(output_frame != NULL);
    output_buffer = output_frame->data;

    /* Calculate output buffer pointers */
    out_ptrs[0] = output_buffer;
    for (i = 1; i < NN_OUT_NB; i++) {
      out_ptrs[i] = out_ptrs[i - 1] + ALIGN_VALUE(nn_out_sizes[i - 1], 32);
    }

    /* Set input buffer */
    stai_ptr nn_in = capture_buffer;
    stai_ret = mdl_set_inputs(nn_ctx_network, &nn_in, 1);
    APP_REQUIRE(stai_ret == STAI_SUCCESS);

    /* Invalidate output buffer before hardware access */
    SCB_InvalidateDCache_by_Addr(output_buffer, NN_OUT_BUFFER_SIZE);

    /* Set output buffers */
    stai_ret = mdl_set_outputs(nn_ctx_network, out_ptrs, NN_OUT_NB);
    APP_REQUIRE(stai_ret == STAI_SUCCESS);

    /* Measure only async NPU run window (submit -> completion). */
    inference_cycles = NN_RunInferenceCycles(nn_ctx_network);
    timing_sample.inference_us = CYCLES_TO_US(inference_cycles);

    inference_end_cycles = DWT->CYCCNT;
    period_cycles = inference_end_cycles - last_inference_end_cycles;
    timing_sample.nn_period_us = CYCLES_TO_US(period_cycles);
    last_inference_end_cycles = inference_end_cycles;

#ifndef SNAPSHOT_MODE
    timing_sample.frame_drops = nn_frame_drop_count;
#else
    timing_sample.frame_drops = 0;
#endif

    output_frame->timing = timing_sample;

    /* Prepare next inference outside the measured run window. */
    stai_ret = mdl_new_inference(nn_ctx_network);
    APP_REQUIRE(stai_ret == STAI_SUCCESS);

#ifndef SNAPSHOT_MODE
    /* Release input buffer back to free pool */
    BQUE_PutFree(&nn_input_queue);
#else
    /* Request next snapshot now that inference is done */
    CAM_NNPipe_RequestSnapshot(nn_input_buffer);
#endif

    /* Mark output buffer as ready for postprocess */
    BQUE_PutReady(&nn_output_queue);
  }
}

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

#ifdef SNAPSHOT_MODE

void NN_SignalSnapshotReady(void) {
  tx_event_flags_set(&nn_snapshot_flags, 0x01, TX_OR);
}

uint8_t *NN_GetSnapshotBuffer(void) {
  return nn_input_buffer;
}

#else

/**
 * @brief  Get pointer to NN input buffer queue
 * @retval Pointer to input queue
 */
bqueue_t *NN_GetInputQueue(void) {
  return &nn_input_queue;
}

#endif /* SNAPSHOT_MODE */

/**
 * @brief  Get pointer to NN output buffer queue
 * @retval Pointer to output queue
 */
bqueue_t *NN_GetOutputQueue(void) {
  return &nn_output_queue;
}

/**
 * @brief  Get number of NN outputs
 * @retval Number of output buffers
 */
int NN_GetOutputCount(void) {
  return NN_OUT_NB;
}

/**
 * @brief  Get NN output buffer sizes
 * @retval Pointer to array of output sizes
 */
const uint32_t *NN_GetOutputSizes(void) {
  return nn_out_sizes;
}

/**
 * @brief  Get network info for postprocessing
 * @retval Pointer to network info structure
 * @note   Must be called after NN_ThreadStart
 */
stai_network_info *NN_GetNetworkInfo(void) {
  return &nn_info;
}

void NN_ThreadSuspend(void) {
  tx_thread_suspend(&nn_ctx.thread);
}

void NN_ThreadResume(void) {
  tx_thread_resume(&nn_ctx.thread);
}

/**
 * @brief  Initialize NN thread and resources
 * @param  memory_ptr: ThreadX memory pool (unused, static allocation)
 */
void NN_ThreadStart(void) {
  UINT status;
  stai_return_code stai_ret;
  int ret;
  int i;

  /* Initialize ST.AI runtime */
  stai_ret = stai_runtime_init();
  APP_REQUIRE(stai_ret == STAI_SUCCESS);

  /* Initialize network */
  stai_ret = mdl_init(nn_ctx_network);
  APP_REQUIRE(stai_ret == STAI_SUCCESS);

  /* Get network info */
  stai_ret = mdl_get_info(nn_ctx_network, &nn_info);
  APP_REQUIRE(stai_ret == STAI_SUCCESS);
  APP_REQUIRE(nn_info.n_inputs == 1);
  APP_REQUIRE(nn_info.n_outputs == NN_OUT_NB);

  /* Verify output sizes */
  for (i = 0; i < NN_OUT_NB; i++) {
    APP_REQUIRE(nn_info.outputs[i].size_bytes == nn_out_sizes[i]);
  }

  /* Initialize input buffer(s) */
#ifdef SNAPSHOT_MODE
  APP_REQUIRE(tx_event_flags_create(&nn_snapshot_flags, "nn_snapshot") == TX_SUCCESS);
  memset(nn_input_buffer, 0, sizeof(nn_input_buffer));
  SCB_CleanInvalidateDCache_by_Addr((void *)nn_input_buffer, sizeof(nn_input_buffer));
#else
  /* Initialize input buffer queue - using 3 buffers to handle 30 FPS pipeline latency */
  uint8_t *in_bufs[3] = {nn_input_buffers[0], nn_input_buffers[1], nn_input_buffers[2]};
  ret = BQUE_Init(&nn_input_queue, 3, in_bufs);
  APP_REQUIRE(ret == 0);
  memset(nn_input_buffers, 0, sizeof(nn_input_buffers));
  SCB_CleanInvalidateDCache_by_Addr((void *)nn_input_buffers, sizeof(nn_input_buffers));
#endif

  /* Initialize output buffer queue */
  uint8_t *out_bufs[3] = {
      (uint8_t *)&nn_output_buffers[0],
      (uint8_t *)&nn_output_buffers[1],
      (uint8_t *)&nn_output_buffers[2],
  };
  ret = BQUE_Init(&nn_output_queue, 3, out_bufs);
  APP_REQUIRE(ret == 0);

  /* Clear output buffers */
  memset(nn_output_buffers, 0, sizeof(nn_output_buffers));
  SCB_CleanInvalidateDCache_by_Addr((void *)nn_output_buffers, sizeof(nn_output_buffers));

  /* Create NN thread */
  status = tx_thread_create(&nn_ctx.thread, "nn_inference",
                            nn_thread_entry, 0,
                            nn_ctx.stack, NN_THREAD_STACK_SIZE,
                            NN_THREAD_PRIORITY, NN_THREAD_PRIORITY,
                            TX_NO_TIME_SLICE, TX_AUTO_START);
  APP_REQUIRE(status == TX_SUCCESS);
}
