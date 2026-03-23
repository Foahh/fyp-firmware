/**
 ******************************************************************************
 * @file    app_nn.c
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

#include "app_nn.h"
#include "app_cam.h"
#include "app_error.h"
#include "app_nn_config.h"
#include "model_config.h"
#include "power_measurement_sync.h"
#include "stm32n6xx_hal.h"
#include "utils.h"
#include <string.h>

/* Include ST.AI runtime API */
#include "stai.h"

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void NN_RunInference(stai_network *network);
static void nn_thread_entry(ULONG arg);

/* ============================================================================
 * Configuration Constants
 * ============================================================================ */

/* Thread configuration */
#define NN_THREAD_STACK_SIZE 4096
#define NN_THREAD_PRIORITY   5

/* NN output sizes from model header */

#define NN_OUT0_SIZE       MDL_NN_OUT_1_SIZE
#define NN_OUT0_SIZE_ALIGN ALIGN_VALUE(NN_OUT0_SIZE, MDL_NN_OUT_1_ALIGNMENT)

#define NN_OUT1_SIZE       MDL_NN_OUT_2_SIZE
#define NN_OUT1_SIZE_ALIGN ALIGN_VALUE(NN_OUT1_SIZE, MDL_NN_OUT_2_ALIGNMENT)

#define NN_OUT2_SIZE       MDL_NN_OUT_3_SIZE
#define NN_OUT2_SIZE_ALIGN ALIGN_VALUE(NN_OUT2_SIZE, MDL_NN_OUT_3_ALIGNMENT)

#define NN_OUT_BUFFER_SIZE (NN_OUT0_SIZE_ALIGN + NN_OUT1_SIZE_ALIGN + NN_OUT2_SIZE_ALIGN)
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
#ifdef CAMERA_NN_SNAPSHOT_MODE
static TX_EVENT_FLAGS_GROUP nn_snapshot_flags;
static uint8_t nn_input_buffers[2][NN_INPUT_SIZE] ALIGN_32 IN_PSRAM;
static volatile int nn_snap_write_idx;
#else
static bqueue_t nn_input_queue;
static uint8_t nn_input_buffers[3][NN_INPUT_SIZE] ALIGN_32 IN_PSRAM;
#endif

/* NN output buffers */
static bqueue_t nn_output_queue;
static uint8_t nn_output_buffers[2][NN_OUT_BUFFER_SIZE] ALIGN_32;

/* Output sizes array */
static const uint32_t nn_out_sizes[NN_OUT_NB] = {NN_OUT0_SIZE, NN_OUT1_SIZE, NN_OUT2_SIZE};

/* Timing statistics (volatile for cross-thread access) */
static volatile nn_timing_t nn_timing;

#ifndef CAMERA_NN_SNAPSHOT_MODE
/* Frame drop counter (accumulated from bqueue skips in NN thread) */
static uint32_t nn_frame_drop_count = 0;
#endif

/* Network info (initialized in NN_Thread_Start, used by postprocess) */
static stai_network_info nn_info;

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief  Run ATON inference with WFE support
 *         Handles wait-for-event (WFE) during inference for power efficiency
 */
static void NN_RunInference(stai_network *network) {
  stai_return_code ret;

  power_measurement_sync_begin();
  do {
    ret = mdl_run(network, STAI_MODE_ASYNC);
    if (ret == STAI_RUNNING_WFE) {
      LL_ATON_OSAL_WFE();
    }
  } while (ret == STAI_RUNNING_WFE || ret == STAI_RUNNING_NO_WFE);
  power_measurement_sync_end();

  ret = mdl_new_inference(network);
  APP_REQUIRE(ret == STAI_SUCCESS);
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
  uint32_t nn_period[2];

  /* Initialize timing */
  nn_period[1] = HAL_GetTick();

  /* Main inference loop */
  while (1) {
    uint8_t *capture_buffer;
    uint8_t *output_buffer;
    stai_ptr out_ptrs[NN_OUT_NB];
    uint32_t ts;
    int i;

    /* Update period tracking */
    nn_period[0] = nn_period[1];
    nn_period[1] = HAL_GetTick();
    nn_timing.nn_period_ms = nn_period[1] - nn_period[0];

#ifdef CAMERA_NN_SNAPSHOT_MODE
    /* Wait for snapshot frame */
    ULONG actual_flags;
    tx_event_flags_get(&nn_snapshot_flags, 0x01, TX_OR_CLEAR,
                       &actual_flags, TX_WAIT_FOREVER);
    /* The buffer that just finished capturing is the current write buffer */
    int captured_idx = nn_snap_write_idx;
    /* Swap to other buffer for next capture */
    nn_snap_write_idx ^= 1;
    /* Pre-request next snapshot (overlaps with inference) */
    CAM_NNPipe_RequestSnapshot(nn_input_buffers[nn_snap_write_idx]);
    capture_buffer = nn_input_buffers[captured_idx];
#else
    /* Get latest input buffer (LIFO), discarding stale frames */
    uint32_t skipped = 0;
    capture_buffer = bqueue_get_ready_latest(&nn_input_queue, &skipped);
    nn_frame_drop_count += skipped;
    APP_REQUIRE(capture_buffer != NULL);
#endif

    /* Get output buffer (blocking) */
    output_buffer = bqueue_get_free(&nn_output_queue, 1);
    APP_REQUIRE(output_buffer != NULL);

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

    /* Run inference */
    ts = HAL_GetTick();
    NN_RunInference(nn_ctx_network);
    nn_timing.inference_ms = HAL_GetTick() - ts;

#ifndef CAMERA_NN_SNAPSHOT_MODE
    /* Release input buffer back to free pool */
    bqueue_put_free(&nn_input_queue);
#endif

    /* Mark output buffer as ready for postprocess */
    bqueue_put_ready(&nn_output_queue);
  }
}

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

#ifdef CAMERA_NN_SNAPSHOT_MODE

void NN_SignalSnapshotReady(void) {
  tx_event_flags_set(&nn_snapshot_flags, 0x01, TX_OR);
}

uint8_t *NN_GetSnapshotBuffer(void) {
  return nn_input_buffers[nn_snap_write_idx];
}

#else

/**
 * @brief  Get pointer to NN input buffer queue
 * @retval Pointer to input queue
 */
bqueue_t *NN_GetInputQueue(void) {
  return &nn_input_queue;
}

#endif /* CAMERA_NN_SNAPSHOT_MODE */

/**
 * @brief  Get pointer to NN output buffer queue
 * @retval Pointer to output queue
 */
bqueue_t *NN_GetOutputQueue(void) {
  return &nn_output_queue;
}

/**
 * @brief  Get current NN timing statistics
 * @param  timing: Pointer to timing structure to fill
 */
void NN_GetTiming(nn_timing_t *timing) {
  if (timing != NULL) {
    timing->nn_period_ms = nn_timing.nn_period_ms;
    timing->inference_ms = nn_timing.inference_ms;
#ifndef CAMERA_NN_SNAPSHOT_MODE
    timing->frame_drops = nn_frame_drop_count;
#else
    timing->frame_drops = 0;
#endif
  }
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
 * @note   Must be called after NN_Thread_Start
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
void NN_Thread_Start(VOID *memory_ptr) {
  UNUSED(memory_ptr);
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
#ifdef CAMERA_NN_SNAPSHOT_MODE
  APP_REQUIRE(tx_event_flags_create(&nn_snapshot_flags, "nn_snapshot") == TX_SUCCESS);
  memset(nn_input_buffers, 0, sizeof(nn_input_buffers));
  SCB_CleanInvalidateDCache_by_Addr((void *)nn_input_buffers, sizeof(nn_input_buffers));
  nn_snap_write_idx = 0;
#else
  /* Initialize input buffer queue - using 3 buffers to handle 30 FPS pipeline latency */
  uint8_t *in_bufs[3] = {nn_input_buffers[0], nn_input_buffers[1], nn_input_buffers[2]};
  ret = bqueue_init(&nn_input_queue, 3, in_bufs);
  APP_REQUIRE(ret == 0);
  memset(nn_input_buffers, 0, sizeof(nn_input_buffers));
  SCB_CleanInvalidateDCache_by_Addr((void *)nn_input_buffers, sizeof(nn_input_buffers));
#endif

  /* Initialize output buffer queue */
  uint8_t *out_bufs[2] = {nn_output_buffers[0], nn_output_buffers[1]};
  ret = bqueue_init(&nn_output_queue, 2, out_bufs);
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
