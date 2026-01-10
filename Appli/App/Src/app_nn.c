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
#include "app_config.h"
#include "app_error.h"
#include "stm32n6xx_hal.h"
#include "utils.h"
#include <string.h>

/* Include ATON runtime API */
#include "ll_aton_rt_user_api.h"

/* Include generated network header */
#include "od_yolo_x_person.h"

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void NN_RunInference(void);
static void nn_thread_entry(ULONG arg);

/* ============================================================================
 * Configuration Constants
 * ============================================================================ */

/* Thread configuration */
#define NN_THREAD_STACK_SIZE 4096
#define NN_THREAD_PRIORITY 6

/* NN output sizes from model header */
#define NN_OUT_NB LL_ATON_OD_YOLO_X_PERSON_OUT_NUM

#define NN_OUT0_SIZE LL_ATON_OD_YOLO_X_PERSON_OUT_1_SIZE_BYTES
#define NN_OUT0_SIZE_ALIGN ALIGN_VALUE(NN_OUT0_SIZE, LL_ATON_OD_YOLO_X_PERSON_OUT_1_ALIGNMENT)

#define NN_OUT1_SIZE LL_ATON_OD_YOLO_X_PERSON_OUT_2_SIZE_BYTES
#define NN_OUT1_SIZE_ALIGN ALIGN_VALUE(NN_OUT1_SIZE, LL_ATON_OD_YOLO_X_PERSON_OUT_2_ALIGNMENT)

#define NN_OUT2_SIZE LL_ATON_OD_YOLO_X_PERSON_OUT_3_SIZE_BYTES
#define NN_OUT2_SIZE_ALIGN ALIGN_VALUE(NN_OUT2_SIZE, LL_ATON_OD_YOLO_X_PERSON_OUT_3_ALIGNMENT)

/* Total output buffer size */
#define NN_OUT_BUFFER_SIZE (NN_OUT0_SIZE_ALIGN + NN_OUT1_SIZE_ALIGN + NN_OUT2_SIZE_ALIGN)

/* NN input buffer size */
#define NN_INPUT_SIZE (NN_WIDTH * NN_HEIGHT * NN_BPP)

/* ============================================================================
 * Global State Variables
 * ============================================================================ */

/* Declare NN instance */
LL_ATON_DECLARE_NAMED_NN_INSTANCE_AND_INTERFACE(od_yolo_x_person);

/* Thread resources */
static struct {
  TX_THREAD thread;
  UCHAR stack[NN_THREAD_STACK_SIZE];
} nn_ctx;

/* Buffer queues */
static bqueue_t nn_input_queue;
static bqueue_t nn_output_queue;

/* NN input buffers */
static uint8_t nn_input_buffers[2][NN_INPUT_SIZE] ALIGN_32 IN_PSRAM;

/* NN output buffers */
static uint8_t nn_output_buffers[2][NN_OUT_BUFFER_SIZE] ALIGN_32;

/* Output sizes array */
static const uint32_t nn_out_sizes[NN_OUT_MAX_NB] = {NN_OUT0_SIZE, NN_OUT1_SIZE, NN_OUT2_SIZE};

/* Timing statistics (volatile for cross-thread access) */
static volatile nn_timing_t nn_timing;

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief  Run ATON inference with WFE support
 *         Handles wait-for-event (WFE) during inference for power efficiency
 */
static void NN_RunInference(void) {
  LL_ATON_RT_RetValues_t ll_aton_rt_ret;

  do {
    ll_aton_rt_ret = LL_ATON_RT_RunEpochBlock(&NN_Instance_od_yolo_x_person);
    if (ll_aton_rt_ret == LL_ATON_RT_WFE) {
      LL_ATON_OSAL_WFE();
    }
  } while (ll_aton_rt_ret != LL_ATON_RT_DONE);

  LL_ATON_RT_Reset_Network(&NN_Instance_od_yolo_x_person);
}

static int model_get_output_nb(const LL_Buffer_InfoTypeDef *nn_out_info) {
  int nb = 0;

  while (nn_out_info->name) {
    nb++;
    nn_out_info++;
  }

  return nb;
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

  const LL_Buffer_InfoTypeDef *nn_in_info = LL_ATON_Input_Buffers_Info(&NN_Instance_od_yolo_x_person);
  const LL_Buffer_InfoTypeDef *nn_out_info = LL_ATON_Output_Buffers_Info(&NN_Instance_od_yolo_x_person);
  uint32_t nn_in_len;
  uint32_t nn_period[2];
  int ret;
  int i;

  /* Initialize ATON runtime */
  LL_ATON_RT_RuntimeInit();
  LL_ATON_RT_Init_Network(&NN_Instance_od_yolo_x_person);

  /* Gsetup buffers size */
  nn_in_len = LL_Buffer_len(&nn_in_info[0]);
  APP_REQUIRE(NN_OUT_NB == model_get_output_nb(nn_out_info));
  for (i = 0; i < NN_OUT_NB; i++) {
    APP_REQUIRE(LL_Buffer_len(&nn_out_info[i]) == nn_out_sizes[i]);
  }

  /* Initialize timing */
  nn_period[1] = HAL_GetTick();

  /* Main inference loop */
  while (1) {
    uint8_t *capture_buffer;
    uint8_t *output_buffer;
    uint8_t *out_ptrs[NN_OUT_NB];
    uint32_t ts;

    /* Update period tracking */
    nn_period[0] = nn_period[1];
    nn_period[1] = HAL_GetTick();
    nn_timing.nn_period_ms = nn_period[1] - nn_period[0];

    /* Get input buffer (blocking) */
    capture_buffer = bqueue_get_ready(&nn_input_queue);
    APP_REQUIRE(capture_buffer != NULL);

    /* Get output buffer (blocking) */
    output_buffer = bqueue_get_free(&nn_output_queue, 1);
    APP_REQUIRE(output_buffer != NULL);

    /* Calculate output buffer pointers */
    out_ptrs[0] = output_buffer;
    for (i = 1; i < NN_OUT_NB; i++) {
      out_ptrs[i] = out_ptrs[i - 1] + ALIGN_VALUE(nn_out_sizes[i - 1], 32);
    }

    /* Set input buffer */
    ret = LL_ATON_Set_User_Input_Buffer_od_yolo_x_person(0, capture_buffer, nn_in_len);
    APP_REQUIRE(ret == LL_ATON_User_IO_NOERROR);

    /* Invalidate output buffer before hardware access */
    SCB_InvalidateDCache_by_Addr(output_buffer, NN_OUT_BUFFER_SIZE);

    /* Set output buffers */
    for (int i = 0; i < NN_OUT_NB; i++) {
      ret = LL_ATON_Set_User_Output_Buffer_od_yolo_x_person(i, out_ptrs[i], nn_out_sizes[i]);
      APP_REQUIRE(ret == LL_ATON_User_IO_NOERROR);
    }

    /* Run inference */
    ts = HAL_GetTick();
    NN_RunInference();
    nn_timing.inference_ms = HAL_GetTick() - ts;

    /* Release input buffer back to free pool */
    bqueue_put_free(&nn_input_queue);

    /* Mark output buffer as ready for postprocess */
    bqueue_put_ready(&nn_output_queue);
  }
}

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

/**
 * @brief  Get pointer to NN input buffer queue
 * @retval Pointer to input queue
 */
bqueue_t *NN_GetInputQueue(void) {
  return &nn_input_queue;
}

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
 * @brief  Initialize NN thread and resources
 * @param  memory_ptr: ThreadX memory pool (unused, static allocation)
 */
void NN_Thread_Start(VOID *memory_ptr) {
  UNUSED(memory_ptr);
  UINT status;
  int ret;

  /* Initialize input buffer queue */
  uint8_t *in_bufs[2] = {nn_input_buffers[0], nn_input_buffers[1]};
  ret = bqueue_init(&nn_input_queue, 2, in_bufs);
  APP_REQUIRE(ret == 0);

  /* Initialize output buffer queue */
  uint8_t *out_bufs[2] = {nn_output_buffers[0], nn_output_buffers[1]};
  ret = bqueue_init(&nn_output_queue, 2, out_bufs);
  APP_REQUIRE(ret == 0);

  /* Clear buffers */
  memset(nn_input_buffers, 0, sizeof(nn_input_buffers));
  SCB_CleanInvalidateDCache_by_Addr((void *)nn_input_buffers, sizeof(nn_input_buffers));
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
