/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
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

#include "main.h"
#include "app_threadx.h"
#include "cam.h"
#include "display.h"
#include "haptic.h"
#include "init_clock.h"
#include "init_mpu.h"
#include "init_peripherals.h"
#include "npu_cache.h"
#include "power_measurement_sync.h"
#include "stm32n6570_discovery.h"
#include "stm32n6570_discovery_bus.h"
#include "tof.h"
#include "ui.h"

/* Private variables ---------------------------------------------------------*/

volatile uint8_t *g_error_file = NULL;
volatile uint32_t g_error_line = 0;

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  __enable_irq();

  /* Power on ICACHE & DCACHE */
  MEMSYSCTL->MSCR |= MEMSYSCTL_MSCR_ICACTIVE_Msk;
  MEMSYSCTL->MSCR |= MEMSYSCTL_MSCR_DCACTIVE_Msk;

  /* Set back system and CPU clock source to HSI */
  __HAL_RCC_CPUCLK_CONFIG(RCC_CPUCLKSOURCE_HSI);
  __HAL_RCC_SYSCLK_CONFIG(RCC_SYSCLKSOURCE_HSI);

  MPU_Config();

  SCB_EnableICache();
  SCB_EnableDCache();

  HAL_Init();

  SMPS_Config();

  SystemClock_Config();

  GPIO_Config();

  SystemIsolation_Config();

  IAC_Config();

  NPU_Config();
  npu_cache_enable();

#if (USE_BSP_COM_FEATURE > 0)
  COM_Config();
#endif /* USE_BSP_COM_FEATURE */

  XSPI_Config();

  LED_Config();

  HAPTIC_Init();

  Button_Config();

  ClockSleep_Config();

  LCD_Init();

  BSP_I2C1_Init();

  CAM_Init();

  TOF_Init();

  PWR_SyncInit();

  ThreadX_Init();

  for (;;) {
    // do nothing
  }
}

/**
 * @brief  Period elapsed callback in non blocking mode
 * @param  htim : TIM handle
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  if (htim->Instance == TIM2) {
    HAL_IncTick();
  }
}

void HAL_MspInit(void) {
  HAL_PWREx_EnableVddIO2();
  HAL_PWREx_EnableVddIO3();
  HAL_PWREx_EnableVddIO4();
  HAL_PWREx_EnableVddIO5();
}

void BSP_PB_Callback(Button_TypeDef Button) {
  if (Button == BUTTON_USER1) {
    UI_ToggleTOFOverlay();
  }
}
