/**
 ******************************************************************************
 * @file    app.c
 * @author  Long Liangmao
 * @brief   Centralized buffer management for display and camera pipelines
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

#include "app.h"
#include "app_buffers.h"
#include "app_cam.h"
#include "app_config.h"
#include "app_lcd.h"
#include "app_ui.h"
#include "cmw_camera.h"
#include "main.h"
#include "stm32n6570_discovery_errno.h"
#include "stm32n6570_discovery_xspi.h"
#include "stm32n6xx_hal.h"
#include "stm32n6xx_hal_rif.h"
#include "utils.h"
#include <assert.h>

/* UI thread configuration */
#define UI_THREAD_STACK_SIZE 2048
#define UI_THREAD_PRIORITY 10  /* Low priority for UI updates */

/* Idle measurement thread configuration */
#define IDLE_THREAD_STACK_SIZE 512
#define IDLE_THREAD_PRIORITY 31  /* Lowest priority (runs when truly idle) */

/* UI thread resources */
static struct {
  TX_THREAD thread;
  UCHAR stack[UI_THREAD_STACK_SIZE];
} ui_ctx;

/* Idle measurement thread resources */
static struct {
  TX_THREAD thread;
  UCHAR stack[IDLE_THREAD_STACK_SIZE];
} idle_ctx;


static void XSPI_Config(void);
static void IAC_Config(void);

static void IAC_Config(void) {
  /* Configure IAC to trap illegal access events */
  __HAL_RCC_IAC_CLK_ENABLE();
  __HAL_RCC_IAC_FORCE_RESET();
  __HAL_RCC_IAC_RELEASE_RESET();
}

static void XSPI_Config(void) {
  int32_t ret = BSP_ERROR_NONE;

  ret = BSP_XSPI_RAM_Init(0);
  assert(ret == BSP_ERROR_NONE);

  ret = BSP_XSPI_RAM_EnableMemoryMappedMode(0);
  assert(ret == BSP_ERROR_NONE);
}

static void LED_Config(void) {
  BSP_LED_Init(LED_GREEN);
  BSP_LED_Init(LED_RED);
  BSP_LED_Off(LED_GREEN);
  BSP_LED_Off(LED_RED);
}

/**
 * @brief  Idle measurement thread entry
 *         Runs at lowest priority to measure idle time via DWT cycle counter
 */
static void idle_measure_thread_entry(ULONG arg) {
  UNUSED(arg);

  while (1) {
    /* Mark entry into idle state */
    UI_IdleThread_Enter();

    /* Yield to higher priority threads */
    tx_thread_relinquish();

    /* Mark exit from idle state */
    UI_IdleThread_Exit();
  }
}

/**
 * @brief  UI update thread entry
 *         Periodically updates the diagnostic overlay
 */
static void ui_thread_entry(ULONG arg) {
  UNUSED(arg);

  while (1) {
    UI_Update();
    tx_thread_sleep(10);
  }
}

void App_Init(VOID *memory_ptr) {
  UINT tx_status;

  LED_Config();

  BSP_SMPS_Init(SMPS_VOLTAGE_OVERDRIVE);
  HAL_Delay(2); /* Assuming Voltage Ramp Speed of 1mV/us --> 100mV increase takes 100us */

  XSPI_Config();
  IAC_Config();
  Buffer_Init();

  LCD_Init();

  /* Initialize UI diagnostic overlay */
  UI_Init();
  LCD_SetUIAlpha(255);  /* Make UI layer visible */

  /* Create idle measurement thread (lowest priority) */
  tx_status = tx_thread_create(&idle_ctx.thread, "idle_measure",
                               idle_measure_thread_entry, 0,
                               idle_ctx.stack, IDLE_THREAD_STACK_SIZE,
                               IDLE_THREAD_PRIORITY, IDLE_THREAD_PRIORITY,
                               TX_NO_TIME_SLICE, TX_AUTO_START);
  assert(tx_status == TX_SUCCESS);

  /* Create UI update thread */
  tx_status = tx_thread_create(&ui_ctx.thread, "ui_update",
                               ui_thread_entry, 0,
                               ui_ctx.stack, UI_THREAD_STACK_SIZE,
                               UI_THREAD_PRIORITY, UI_THREAD_PRIORITY,
                               TX_NO_TIME_SLICE, TX_AUTO_START);
  assert(tx_status == TX_SUCCESS);

  CAM_InitIspSemaphore();
  CAM_Init();
  Thread_IspUpdate_Init(memory_ptr);
  CAM_DisplayPipe_Start(CMW_MODE_CONTINUOUS);
}
