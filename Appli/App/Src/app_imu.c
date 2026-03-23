/**
 ******************************************************************************
 * @file    app_imu.c
 * @author  Long Liangmao
 * @brief   IMU sampling thread wrapping ISM330DLC driver.
 *
 *          Periodically reads the accelerometer over I2C, evaluates the
 *          wake condition, and publishes a double-buffered snapshot that
 *          consumers (power management, comm log) can read lock-free.
 *
 *          TODO: Verify I2C bus assignment and pin wiring for the board.
 *          Currently uses BSP_I2C1 (same bus as ToF sensor). If the IMU
 *          is on a different bus, update the IO callbacks below.
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

#include "app_imu.h"
#include "app_error.h"
#include "ism330dlc.h"
#include "stm32n6570_discovery_bus.h"
#include "stm32n6xx_hal.h"
#include "tx_api.h"

#include <stdlib.h>

/* ============================================================================
 * Configuration
 * ============================================================================ */

#define IMU_THREAD_STACK_SIZE 1024U
#define IMU_THREAD_PRIORITY   9

/** Sampling interval (ms) — matches power management poll rate */
#define IMU_POLL_MS 100

/* ============================================================================
 * Thread resources
 * ============================================================================ */

static TX_THREAD imu_thread;
static UCHAR imu_thread_stack[IMU_THREAD_STACK_SIZE];

/* ============================================================================
 * Sensor state
 * ============================================================================ */

static ISM330DLC_Object_t imu_obj;

/* Double-buffered IMU data */
static imu_data_t imu_data[2];
static volatile uint8_t imu_read_idx = 0;

/* Previous accel for delta-based wake detection */
static int32_t prev_x_mg;
static int32_t prev_y_mg;
static int32_t prev_z_mg;

/* ============================================================================
 * I2C bus adapter
 * ============================================================================ */

static int32_t imu_write_reg(uint16_t addr, uint16_t reg,
                             uint8_t *data, uint16_t len) {
  return BSP_I2C1_WriteReg(addr, reg, data, len);
}

static int32_t imu_read_reg(uint16_t addr, uint16_t reg,
                            uint8_t *data, uint16_t len) {
  return BSP_I2C1_ReadReg(addr, reg, data, len);
}

/* ============================================================================
 * Internal helpers
 * ============================================================================ */

static int32_t imu_hw_init(void) {
  ISM330DLC_IO_t io = {
      .Init = BSP_I2C1_Init,
      .DeInit = BSP_I2C1_DeInit,
      .BusType = ISM330DLC_I2C_BUS,
      .Address = IMU_I2C_ADDRESS,
      .WriteReg = imu_write_reg,
      .ReadReg = imu_read_reg,
      .GetTick = (ISM330DLC_GetTick_Func)HAL_GetTick,
  };

  if (ISM330DLC_RegisterBusIO(&imu_obj, &io) != ISM330DLC_OK) {
    return -1;
  }

  if (ISM330DLC_Init(&imu_obj) != ISM330DLC_OK) {
    return -1;
  }

  /* Verify chip ID (ISM330DLC WHO_AM_I = 0x6A) */
  uint8_t id = 0;
  if (ISM330DLC_ReadID(&imu_obj, &id) != ISM330DLC_OK) {
    return -1;
  }
  if (id != 0x6A) {
    return -1;
  }

  /* Enable accelerometer at low ODR for wake detection */
  if (ISM330DLC_ACC_Enable(&imu_obj) != ISM330DLC_OK) {
    return -1;
  }
  if (ISM330DLC_ACC_SetOutputDataRate(&imu_obj, IMU_ACC_ODR_HZ) !=
      ISM330DLC_OK) {
    return -1;
  }
  if (ISM330DLC_ACC_SetFullScale(&imu_obj, IMU_ACC_FS_G) != ISM330DLC_OK) {
    return -1;
  }

  return 0;
}

static uint8_t imu_eval_wake(int32_t x, int32_t y, int32_t z) {
  /* Motion check: magnitude of acceleration delta */
  int32_t dx = x - prev_x_mg;
  int32_t dy = y - prev_y_mg;
  int32_t dz = z - prev_z_mg;
  prev_x_mg = x;
  prev_y_mg = y;
  prev_z_mg = z;

  uint32_t delta_sq = (uint32_t)(dx * dx + dy * dy + dz * dz);
  uint32_t thresh_sq =
      (uint32_t)IMU_WAKE_ACCEL_THRESHOLD_MG * IMU_WAKE_ACCEL_THRESHOLD_MG;
  uint8_t motion = (delta_sq > thresh_sq) ? 1 : 0;

  /* Tilt check: Z axis below threshold indicates device tilted */
  uint8_t tilted =
      ((uint32_t)abs(z) < (uint32_t)IMU_TILT_THRESHOLD_MG) ? 1 : 0;

  return (motion && tilted) ? 1 : 0;
}

/* ============================================================================
 * Thread entry
 * ============================================================================ */

static void imu_thread_entry(ULONG arg) {
  UNUSED(arg);

  if (imu_hw_init() != 0) {
    return;
  }

  while (1) {
    ISM330DLC_Axes_t axes;
    if (ISM330DLC_ACC_GetAxes(&imu_obj, &axes) == ISM330DLC_OK) {
      uint8_t write_idx = imu_read_idx ^ 1;
      imu_data_t *d = &imu_data[write_idx];
      d->x_mg = axes.x;
      d->y_mg = axes.y;
      d->z_mg = axes.z;
      d->wake = imu_eval_wake(axes.x, axes.y, axes.z);
      d->timestamp_ms = HAL_GetTick();
      imu_read_idx = write_idx;
    }

    tx_thread_sleep((IMU_POLL_MS * TX_TIMER_TICKS_PER_SECOND + 999) / 1000);
  }
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void IMU_Thread_Start(void) {
  UINT status = tx_thread_create(&imu_thread, "imu_sampling",
                                 imu_thread_entry, 0,
                                 imu_thread_stack, IMU_THREAD_STACK_SIZE,
                                 IMU_THREAD_PRIORITY, IMU_THREAD_PRIORITY,
                                 TX_NO_TIME_SLICE, TX_AUTO_START);
  APP_REQUIRE(status == TX_SUCCESS);
}

const imu_data_t *IMU_GetData(void) {
  return &imu_data[imu_read_idx];
}

void IMU_Stop(void) {
  tx_thread_suspend(&imu_thread);
  ISM330DLC_ACC_Disable(&imu_obj);
  ISM330DLC_DeInit(&imu_obj);
}

void IMU_Resume(void) {
  imu_hw_init();
  tx_thread_resume(&imu_thread);
}

void IMU_EnterLowPower(void) {
  ISM330DLC_ACC_SetOutputDataRate(&imu_obj, 12.5f);
}

void IMU_ExitLowPower(void) {
  ISM330DLC_ACC_SetOutputDataRate(&imu_obj, IMU_ACC_ODR_HZ);
}
