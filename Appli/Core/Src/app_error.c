/**
 ******************************************************************************
 * @file    app_error.c
 * @author  Long Liangmao
 * @brief   Application-level fatal error handling implementation
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


#include "app_error.h"
#include "stm32n6570_discovery.h"

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
  BSP_LED_Init(LED_GREEN);
  BSP_LED_Init(LED_RED);

  BSP_LED_Off(LED_GREEN);
  BSP_LED_Off(LED_RED);

  /* Disable interrupts to prevent further execution */
  __disable_irq();

  volatile uint32_t error_counter = 0;
  while (1) {
    error_counter++;

    BSP_LED_Toggle(LED_RED);
    for (volatile uint32_t i = 0; i < 4000000; i++) {
      __NOP();
    }
  }
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line) {
  g_error_file = file;
  g_error_line = line;
  Error_Handler();
}
#endif /* USE_FULL_ASSERT */
