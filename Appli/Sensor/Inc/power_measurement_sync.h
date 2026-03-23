/**
 ******************************************************************************
 * @file    power_measurement_sync.h
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

#ifndef POWER_MEASUREMENT_SYNC_H
#define POWER_MEASUREMENT_SYNC_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PWR_MEASUREMENT_SYNC_ENABLE
#define PWR_MEASUREMENT_SYNC_ENABLE 1
#endif

#if PWR_MEASUREMENT_SYNC_ENABLE
#include "stm32n6xx_hal.h"

#ifndef PWR_SYNC_GPIO_PORT
#define PWR_SYNC_GPIO_PORT GPIOD
#endif
#ifndef PWR_SYNC_GPIO_PIN
#define PWR_SYNC_GPIO_PIN GPIO_PIN_6
#endif
#ifndef PWR_SYNC_GPIO_RCC_ENABLE
#define PWR_SYNC_GPIO_RCC_ENABLE() __HAL_RCC_GPIOD_CLK_ENABLE()
#endif

#endif /* PWR_MEASUREMENT_SYNC_ENABLE */

void PWR_SyncInit(void);
void PWR_SyncBegin(void);
void PWR_SyncEnd(void);

#ifdef __cplusplus
}
#endif

#endif /* POWER_MEASUREMENT_SYNC_H */
