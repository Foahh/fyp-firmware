/**
 ******************************************************************************
 * @file    power_measurement_sync.c
 * @author  Long Liangmao
 * @brief   GPIO sync for external power measurement (high during NPU run)
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

#include "power_measurement_sync.h"

#if PWR_MEASUREMENT_SYNC_ENABLE

void power_measurement_sync_init(void) {
  PWR_SYNC_GPIO_RCC_ENABLE();

  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = PWR_SYNC_GPIO_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(PWR_SYNC_GPIO_PORT, &gpio);
  HAL_GPIO_WritePin(PWR_SYNC_GPIO_PORT, PWR_SYNC_GPIO_PIN, GPIO_PIN_RESET);
}

void power_measurement_sync_begin(void) {
  HAL_GPIO_WritePin(PWR_SYNC_GPIO_PORT, PWR_SYNC_GPIO_PIN, GPIO_PIN_SET);
}

void power_measurement_sync_end(void) {
  HAL_GPIO_WritePin(PWR_SYNC_GPIO_PORT, PWR_SYNC_GPIO_PIN, GPIO_PIN_RESET);
}

#else /* !PWR_MEASUREMENT_SYNC_ENABLE */

void power_measurement_sync_init(void) {
}
void power_measurement_sync_begin(void) {
}
void power_measurement_sync_end(void) {
}

#endif /* PWR_MEASUREMENT_SYNC_ENABLE */
