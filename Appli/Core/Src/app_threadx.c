/* USER CODE BEGIN Header */
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
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_threadx.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_error.h"
#include "main.h"
#include "utils.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* Main ThreadX thread for peripheral configuration */
#define MAIN_THREAD_STACK_SIZE 2048U
static TX_THREAD main_thread;
static ULONG main_thread_stack[MAIN_THREAD_STACK_SIZE / sizeof(ULONG)];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static void main_thread_entry(ULONG arg);
/* USER CODE END PFP */

/**
  * @brief  Application ThreadX Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT App_ThreadX_Init(VOID *memory_ptr)
{
  UINT ret = TX_SUCCESS;
  /* USER CODE BEGIN App_ThreadX_MEM_POOL */

  /* USER CODE END App_ThreadX_MEM_POOL */
  /* USER CODE BEGIN App_ThreadX_Init */

  /* Create main thread for peripheral configuration.
   * This thread will configure all peripherals (which may use HAL_Delay)
   * via Main_PeriphInit(), and then exit.
   * Priority is set high (low number = high priority in ThreadX) to ensure it runs first.
   */
  ret = tx_thread_create(&main_thread, "main_config",
                        main_thread_entry, (ULONG)memory_ptr,
                        main_thread_stack, MAIN_THREAD_STACK_SIZE,
                        1, 1,  /* Highest priority to run first (lower number = higher priority) */
                        TX_NO_TIME_SLICE, TX_AUTO_START);
  APP_REQUIRE(ret == TX_SUCCESS);

  /* App_Init will be called from main_thread_entry after peripheral configuration */

  /* USER CODE END App_ThreadX_Init */

  return ret;
}

  /**
  * @brief  Function that implements the kernel's initialization.
  * @param  None
  * @retval None
  */
void MX_ThreadX_Init(void)
{
  /* USER CODE BEGIN Before_Kernel_Start */

  /* USER CODE END Before_Kernel_Start */

  tx_kernel_enter();

  /* USER CODE BEGIN Kernel_Start_Error */

  /* USER CODE END Kernel_Start_Error */
}

/* USER CODE BEGIN 1 */
/**
 * @brief  Main ThreadX entry for peripheral configuration.
 *         Runs once at startup, then terminates.
 */
static void main_thread_entry(ULONG arg)
{
  VOID *memory_ptr = (VOID *)arg;

  /* Run hardware / peripheral init in HAL domain */
  Peripheral_Init(memory_ptr);

  /* Main thread has completed its configuration task.
   * The application threads are now running, so this thread can exit.
   */
  tx_thread_delete(&main_thread);
}

/**
 * @brief  HAL tick override for ThreadX
 * @retval Current tick value in milliseconds
 */
uint32_t HAL_GetTick(void) {
  return (tx_time_get() * 1000) / TX_TIMER_TICKS_PER_SECOND;
}

/**
 * @brief  HAL delay override for ThreadX
 * @param  Delay: Delay in milliseconds
 * @retval None
 */
void HAL_Delay(uint32_t Delay) {
  APP_REQUIRE(!IS_IRQ_MODE());

  uint32_t ticks = (Delay * TX_TIMER_TICKS_PER_SECOND) / 1000;
  tx_thread_sleep(ticks);
}

/**
 * @brief  HAL tick initialization override for ThreadX
 * @param  TickPriority: Tick priority (unused)
 * @retval HAL status
 */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority) {
  UNUSED(TickPriority);
  return HAL_OK;
}

/**
 * @brief  HAL tick increment override for ThreadX
 * @retval None
 */
void HAL_IncTick(void) {
  // do nothing
}

/* USER CODE END 1 */
