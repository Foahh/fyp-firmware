/**
 ******************************************************************************
 * @file    stm32n6xx_it.c
 * @brief   Interrupt Service Routines.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "stm32n6xx_it.h"
#include "stm32n6570_discovery.h"
#include "stm32n6xx_hal.h"

#include "cmw_camera.h"

#include "main.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* External variables --------------------------------------------------------*/
extern TIM_HandleTypeDef htim2;
extern DMA_HandleTypeDef hdma_usart1_tx;

/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
 * @brief This function handles Non maskable interrupt.
 */
void NMI_Handler(void) {
  while (1) {
  }
}

/**
 * @brief This function handles Hard fault interrupt.
 */
void HardFault_Handler(void) {
  while (1) {
  }
}

/**
 * @brief This function handles Memory management fault.
 */
void MemManage_Handler(void) {
  while (1) {
  }
}

/**
 * @brief This function handles Prefetch fault, memory access fault.
 */
void BusFault_Handler(void) {
  while (1) {
  }
}

/**
 * @brief This function handles Undefined instruction or illegal state.
 */
void UsageFault_Handler(void) {
  while (1) {
  }
}

/**
 * @brief This function handles Secure fault.
 */
void SecureFault_Handler(void) {
  while (1) {
  }
}

/**
 * @brief This function handles Debug monitor.
 */
void DebugMon_Handler(void) {
}

/******************************************************************************/
/* STM32N6xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32n6xx.s).                    */
/******************************************************************************/

/**
 * @brief This function handles DCMIPP global interrupt.
 */
void DCMIPP_IRQHandler(void) {
  DCMIPP_HandleTypeDef *hdcmipp_ptr = CMW_CAMERA_GetDCMIPPHandle();
  if (hdcmipp_ptr != NULL) {
    HAL_DCMIPP_IRQHandler(hdcmipp_ptr);
  }
}

/**
 * @brief This function handles TIM2 global interrupt.
 */
void TIM2_IRQHandler(void) {
  HAL_TIM_IRQHandler(&htim2);
}

/**
 * @brief This function handles CSI global interrupt.
 */
void CSI_IRQHandler(void) {
  DCMIPP_HandleTypeDef *hdcmipp_ptr = CMW_CAMERA_GetDCMIPPHandle();
  if (hdcmipp_ptr != NULL) {
    HAL_DCMIPP_CSI_IRQHandler(hdcmipp_ptr);
  }
}

/**
 * @brief This function handles IAC global interrupt.
 */
void IAC_IRQHandler(void) {
  while (1) {
  }
}

/**
 * @brief This function handles USART1 global interrupt.
 */
void USART1_IRQHandler(void) {
  HAL_UART_IRQHandler(&hcom_uart[COM1]);
}

/**
 * @brief This function handles GPDMA1 Channel 0 interrupt (USART1 TX DMA).
 */
void GPDMA1_Channel0_IRQHandler(void) {
  HAL_DMA_IRQHandler(&hdma_usart1_tx);
}

/**
 * @brief This function handles EXTI Line 13 interrupt (USER1 button).
 */
void EXTI13_IRQHandler(void) {
  BSP_PB_IRQHandler(BUTTON_USER1);
}

/**
 * @brief This function handles EXTI Line 0 interrupt (TOF data ready).
 */
void EXTI0_IRQHandler(void) {
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}
