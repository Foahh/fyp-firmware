/**
 ******************************************************************************
 * @file    app_power.c
 * @author  Long Liangmao
 * @brief   IMU-driven standby/wake power state machine.
 *
 *          ACTIVE  → STANDBY:  inactivity timeout (no IMU wake for N ms)
 *          STANDBY → ACTIVE:   IMU wake condition (tilt + motion)
 *
 *          Transitions stop/resume the camera, NN, post-process, and ToF
 *          pipelines using existing thread suspend/resume APIs.
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

#include "app_power.h"
#include "app_cam.h"
#include "app_error.h"
#include "app_haptic.h"
#include "app_imu.h"
#include "app_nn.h"
#include "app_pp.h"
#include "app_tof.h"
#include "stm32n6xx_hal.h"
#include "tx_api.h"

#define POWER_THREAD_STACK_SIZE 1024U
#define POWER_THREAD_PRIORITY   10 /* Lower priority than all pipeline threads */

/** Power poll interval (ms) — checks IMU snapshot for wake/inactivity */
#define POWER_POLL_MS 100

static TX_THREAD power_thread;
static UCHAR power_thread_stack[POWER_THREAD_STACK_SIZE];

static volatile power_state_t power_state = POWER_STATE_ACTIVE;

static void power_enter_standby(void) {
  /* Stop pipeline in reverse priority order to avoid feeding stopped consumers */
  TOF_Stop();
  PP_ThreadSuspend();
  NN_ThreadSuspend();
  CAM_NNPipe_Stop();
  CAM_ThreadsSuspend();
  HAPTIC_Off();

  power_state = POWER_STATE_STANDBY;
}

static void power_enter_active(void) {
  /* Resume in forward order: camera → NN → PP → ToF */
  CAM_ThreadsResume();
  NN_ThreadResume();
  PP_ThreadResume();
  TOF_Resume();

  power_state = POWER_STATE_ACTIVE;
}

static void power_thread_entry(ULONG arg) {
  UNUSED(arg);

  uint32_t last_wake_ms = HAL_GetTick();

  while (1) {
    uint32_t now = HAL_GetTick();
    const imu_data_t *imu = IMU_GetData();
    uint8_t wake = (imu->timestamp_ms != 0) ? imu->wake : 0;

    if (wake) {
      last_wake_ms = now;
    }

    switch (power_state) {
    case POWER_STATE_ACTIVE:
      if ((now - last_wake_ms) > IMU_INACTIVITY_TIMEOUT_MS) {
        power_enter_standby();
      }
      break;

    case POWER_STATE_STANDBY:
      if (wake) {
        power_enter_active();
      }
      break;
    }

    tx_thread_sleep((POWER_POLL_MS * TX_TIMER_TICKS_PER_SECOND + 999) / 1000);
  }
}

power_state_t POWER_GetState(void) {
  return power_state;
}

void POWER_Thread_Start(void) {
  UINT status = tx_thread_create(
      &power_thread, "power_mgmt", power_thread_entry, 0,
      power_thread_stack, POWER_THREAD_STACK_SIZE,
      POWER_THREAD_PRIORITY, POWER_THREAD_PRIORITY,
      TX_NO_TIME_SLICE, TX_AUTO_START);
  APP_REQUIRE(status == TX_SUCCESS);
}
