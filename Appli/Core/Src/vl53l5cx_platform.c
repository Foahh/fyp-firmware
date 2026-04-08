/**
 ******************************************************************************
 * @file    vl53l5cx_platform.c
 * @author  Long Liangmao
 * @brief   VL53L5CX platform layer — replaces the vendor busy-wait WaitMs()
 *          with an RTOS-friendly tx_thread_sleep().
 *
 *          This file is a project-local override of
 *          External/stm32-vl53l5cx/porting/platform.c (which must NOT be
 *          modified). All other functions (RdByte, WrByte, etc.) are
 *          identical to the vendor version.
 ******************************************************************************
 * @attention
 *
 * Original platform.c: Copyright (c) 2021 STMicroelectronics.
 * WaitMs modification: Copyright (c) 2026 Long Liangmao.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#include "vl53l5cx_platform.h"
#include "stm32n6xx_hal.h"
#include "timebase.h"
#include "tx_api.h"

uint8_t RdByte(VL53L5CX_Platform *p_platform, uint16_t RegisterAdress,
               uint8_t *p_value) {
  return p_platform->Read(p_platform->address, RegisterAdress, p_value, 1U);
}

uint8_t WrByte(VL53L5CX_Platform *p_platform, uint16_t RegisterAdress,
               uint8_t value) {
  return p_platform->Write(p_platform->address, RegisterAdress, &value, 1U);
}

uint8_t WrMulti(VL53L5CX_Platform *p_platform, uint16_t RegisterAdress,
                uint8_t *p_values, uint32_t size) {
  return p_platform->Write(p_platform->address, RegisterAdress, p_values,
                           size);
}

uint8_t RdMulti(VL53L5CX_Platform *p_platform, uint16_t RegisterAdress,
                uint8_t *p_values, uint32_t size) {
  return p_platform->Read(p_platform->address, RegisterAdress, p_values, size);
}

void SwapBuffer(uint8_t *buffer, uint16_t size) {
  uint32_t i, tmp;

  for (i = 0; i < size; i = i + 4) {
    tmp = (buffer[i] << 24) | (buffer[i + 1] << 16) | (buffer[i + 2] << 8) |
          (buffer[i + 3]);

    memcpy(&(buffer[i]), &tmp, 4);
  }
}

/**
 * @brief  Delay for VL53L5CX driver.  Inside a ThreadX thread, sleeps with
 *         tx_thread_sleep() so other threads run.  Outside a thread (e.g.
 *         sensor init before tx_kernel_enter), uses HAL_Delay() so the wait
 *         still completes instead of misusing the scheduler.
 */
uint8_t WaitMs(VL53L5CX_Platform *p_platform, uint32_t TimeMs) {
  (void)p_platform;
  if (TimeMs == 0) {
    return 0;
  }
  if (tx_thread_identify() != TX_NULL) {
    ULONG ticks = MS_TO_TICKS(TimeMs);
    tx_thread_sleep(ticks);
  } else {
    HAL_Delay(TimeMs);
  }
  return 0;
}
