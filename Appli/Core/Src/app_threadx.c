/**
 ******************************************************************************
 * @file    app_threadx.c
 * @author  MCD Application Team
 * @brief   ThreadX applicative file
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#include "app_threadx.h"

#include "cam.h"
#include "cmw_camera.h"
#include "comm_log.h"
#include "comm_rx.h"
#include "comm_tx.h"
#include "error.h"
#include "main.h"
#include "nn.h"
#include "pp.h"
#include "tof.h"
#include "ui.h"
#include "utils.h"

/* Startup thread for runtime operations that require ThreadX resources */
#define STARTUP_THREAD_STACK_SIZE 1024U
static TX_THREAD startup_thread;
static ULONG startup_thread_stack[STARTUP_THREAD_STACK_SIZE / sizeof(ULONG)];

static void startup_thread_entry(ULONG arg);

/**
 * @brief  Application ThreadX Initialization.
 *         Creates all ThreadX resources (threads, queues, semaphores, mutexes, etc.).
 *         IMPORTANT: This function must NOT call any HAL/BSP/system APIs.
 *         All HAL/peripheral initialization must happen BEFORE tx_kernel_enter().
 * @param memory_ptr: memory pointer
 * @retval int
 */
UINT ThreadX_Start(VOID *memory_ptr) {
  UNUSED(memory_ptr);
  UINT ret = TX_SUCCESS;

  CAM_ThreadStart();

  NN_ThreadStart();

  PP_ThreadStart();

  COM_TX_ThreadStart();

  COM_Log_ThreadStart();

  COM_RX_ThreadStart();

  UI_ThreadStart();

  TOF_ThreadStart();

  ret = tx_thread_create(&startup_thread, "startup",
                         startup_thread_entry, 0,
                         startup_thread_stack, STARTUP_THREAD_STACK_SIZE,
                         1, 1, /* High priority to run first */
                         TX_NO_TIME_SLICE, TX_AUTO_START);
  APP_REQUIRE(ret == TX_SUCCESS);

  return ret;
}

/**
 * @brief  Function that implements the kernel's initialization.
 * @param  None
 * @retval None
 */
void ThreadX_Init(void) {
  tx_kernel_enter();
}

/**
 * @brief  Startup thread entry for runtime operations.
 *         This thread runs after all ThreadX resources are created and starts
 *         runtime operations that require access to ThreadX resources (queues, etc.).
 *         Runs once at startup, then terminates.
 */
static void startup_thread_entry(ULONG arg) {
  UNUSED(arg);

#ifndef CAMERA_NN_SNAPSHOT_MODE
  bqueue_t *nn_input_queue;
  uint8_t *first_nn_buffer;
#endif

  CAM_DisplayPipe_Start(CMW_MODE_CONTINUOUS);

#ifdef CAMERA_NN_SNAPSHOT_MODE
  CAM_NNPipe_Start(NN_GetSnapshotBuffer(), CMW_MODE_SNAPSHOT);
#else
  nn_input_queue = NN_GetInputQueue();
  first_nn_buffer = BQUE_GetFree(nn_input_queue, 0);
  APP_REQUIRE(first_nn_buffer != NULL);
  CAM_NNPipe_Start(first_nn_buffer, CMW_MODE_CONTINUOUS);
#endif

  /* Startup thread has completed its task, so it can exit */
  tx_thread_delete(&startup_thread);
}
