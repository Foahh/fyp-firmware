/**
******************************************************************************
* @file    usart_io.c
* @author  Long Liangmao
*
******************************************************************************
* @attention
*
* Copyright (c) 2025 Long Liangmao.
* All rights reserved.
*
* This software is licensed under terms that can be found in the LICENSE file
* in the root directory of this software component.
* If no LICENSE file comes with this software, it is provided AS-IS.
*
******************************************************************************
*/

#include "usart_io.h"
#include "main.h"

extern UART_HandleTypeDef huart1;

/**
  * @brief  Retargets the C library printf function to the USART.
  * @param  ch: Character to send
  * @retval Character sent
  */
int __io_putchar(int ch)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}

/**
  * @brief  Retargets the C library getchar function to the USART.
  * @retval Character received
  */
int __io_getchar(void)
{
  uint8_t ch;
  HAL_UART_Receive(&huart1, &ch, 1, HAL_MAX_DELAY);
  return ch;
}

