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

void PWR_SyncInit(void) {
  PWR_SYNC_GPIO_RCC_ENABLE();

  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = PWR_SYNC_GPIO_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(PWR_SYNC_GPIO_PORT, &gpio);
  HAL_GPIO_WritePin(PWR_SYNC_GPIO_PORT, PWR_SYNC_GPIO_PIN, GPIO_PIN_RESET);
}

void PWR_SyncBegin(void) {
  HAL_GPIO_WritePin(PWR_SYNC_GPIO_PORT, PWR_SYNC_GPIO_PIN, GPIO_PIN_SET);
}

void PWR_SyncEnd(void) {
  HAL_GPIO_WritePin(PWR_SYNC_GPIO_PORT, PWR_SYNC_GPIO_PIN, GPIO_PIN_RESET);
}
