/**
 ******************************************************************************
 * @file    app_haptic.c
 * @author  Long Liangmao
 * @brief   Haptic motor GPIO control (ready for later PWM driver).
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

#include "haptic.h"
#include "stm32n6xx_hal.h"

void HAPTIC_Init(void) {
  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = HAPTIC_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_WritePin(HAPTIC_PORT, HAPTIC_PIN, GPIO_PIN_RESET);
  HAL_GPIO_Init(HAPTIC_PORT, &gpio);
}

void HAPTIC_On(void) {
  HAL_GPIO_WritePin(HAPTIC_PORT, HAPTIC_PIN, GPIO_PIN_SET);
}

void HAPTIC_Off(void) {
  HAL_GPIO_WritePin(HAPTIC_PORT, HAPTIC_PIN, GPIO_PIN_RESET);
}
