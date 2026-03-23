/**
 ******************************************************************************
 * @file    app_haptic.h
 * @author  Long Liangmao
 * @brief   Haptic motor GPIO abstraction
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

#ifndef HAPTIC_H
#define HAPTIC_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Haptic motor GPIO pin assignment.
 *         TODO: Update port/pin when board wiring is finalized.
 */
#define HAPTIC_PORT GPIOG
#define HAPTIC_PIN  GPIO_PIN_2

/**
 * @brief  Initialize haptic motor GPIO as push-pull output (initially OFF).
 */
void HAPTIC_Init(void);

/**
 * @brief  Enable haptic motor output.
 */
void HAPTIC_On(void);

/**
 * @brief  Disable haptic motor output.
 */
void HAPTIC_Off(void);

#ifdef __cplusplus
}
#endif

#endif /* HAPTIC_H */
