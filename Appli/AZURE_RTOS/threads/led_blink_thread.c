/**
******************************************************************************
* @file    led_blink_thread.h
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

#include "led_blink_thread.h"
#include "main.h"
#include "stm32n6xx_hal_def.h"

#define LED_BLINK_THREAD_STACK_SIZE 1024
#define LED_BLINK_THREAD_PRIORITY 1

static TX_THREAD led_blink_thread;
static UCHAR led_blink_thread_stack[LED_BLINK_THREAD_STACK_SIZE];

static VOID led_blink_thread_entry(ULONG thread_input);

/**
 * @brief  Initialize and create the LED blink thread
 * @param  memory_ptr: Memory pointer (unused, thread uses static allocation)
 * @retval TX_SUCCESS if successful, error code otherwise
 */
UINT LedBlinkThread_Init(VOID *memory_ptr) {
  UINT ret = TX_SUCCESS;
  UNUSED(memory_ptr);

  ret = tx_thread_create(&led_blink_thread, "led_blink_thread",
                         led_blink_thread_entry, 0, led_blink_thread_stack,
                         LED_BLINK_THREAD_STACK_SIZE, LED_BLINK_THREAD_PRIORITY,
                         LED_BLINK_THREAD_PRIORITY, TX_NO_TIME_SLICE,
                         TX_AUTO_START);

  return ret;
}

/**
 * @brief  LED blink thread entry function
 * @param  thread_input: Thread input parameter (unused)
 * @retval None
 */
static VOID led_blink_thread_entry(ULONG thread_input) {
  UNUSED(thread_input);
  while (1) {
    BSP_LED_Toggle(LED_GREEN);
    tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND / 4);
  }
}
