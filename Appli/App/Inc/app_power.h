/**
 ******************************************************************************
 * @file    app_power.h
 * @author  Long Liangmao
 * @brief   IMU-driven standby/wake power state machine
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

#ifndef APP_POWER_H
#define APP_POWER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum {
  POWER_STATE_ACTIVE,  /**< Camera + NN + ToF running */
  POWER_STATE_STANDBY, /**< Pipes stopped, IMU polling for wake */
} power_state_t;

/**
 * @brief  Get current power state.
 */
power_state_t POWER_GetState(void);

/**
 * @brief  Create and start the power management thread.
 *         Must be called from ThreadX_Start after other threads are created.
 */
void POWER_Thread_Start(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_POWER_H */
