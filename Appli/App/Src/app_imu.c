/**
 ******************************************************************************
 * @file    app_imu.c
 * @author  Long Liangmao
 * @brief   IMU abstraction layer wrapping ISM330DLC driver.
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
#include "ism330dlc.h"
#include "stm32n6570_discovery_bus.h"
#include "stm32n6xx_hal.h"

#include <stdlib.h>

static ISM330DLC_Object_t imu_obj;
static uint8_t imu_initialized = 0;
static imu_accel_t imu_prev_accel;

/* I2C bus adapter: ISM330DLC uses 8-bit register addresses, so we use the
 * standard (non-16-bit) BSP_I2C1 functions.  The BSP provides WriteReg and
 * ReadReg with matching signatures.
 *
 * TODO: If the IMU is wired to a different I2C bus, swap these callbacks. */
static int32_t imu_write_reg(uint16_t addr, uint16_t reg,
                             uint8_t *data, uint16_t len) {
  return BSP_I2C1_WriteReg(addr, reg, data, len);
}

static int32_t imu_read_reg(uint16_t addr, uint16_t reg,
                            uint8_t *data, uint16_t len) {
  return BSP_I2C1_ReadReg(addr, reg, data, len);
}

int32_t IMU_Init(void) {
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

  /* Verify chip ID */
  uint8_t id = 0;
  if (ISM330DLC_ReadID(&imu_obj, &id) != ISM330DLC_OK) {
    return -1;
  }
  /* ISM330DLC WHO_AM_I = 0x6A */
  if (id != 0x6A) {
    return -1;
  }

  /* Enable accelerometer at low ODR for wake detection */
  if (ISM330DLC_ACC_Enable(&imu_obj) != ISM330DLC_OK) {
    return -1;
  }
  if (ISM330DLC_ACC_SetOutputDataRate(&imu_obj, IMU_ACC_ODR_HZ) != ISM330DLC_OK) {
    return -1;
  }
  if (ISM330DLC_ACC_SetFullScale(&imu_obj, IMU_ACC_FS_G) != ISM330DLC_OK) {
    return -1;
  }

  imu_prev_accel = (imu_accel_t){0, 0, 0};
  imu_initialized = 1;
  return 0;
}

int32_t IMU_ReadAccel(imu_accel_t *accel) {
  if (!imu_initialized || accel == NULL) {
    return -1;
  }

  ISM330DLC_Axes_t axes;
  if (ISM330DLC_ACC_GetAxes(&imu_obj, &axes) != ISM330DLC_OK) {
    return -1;
  }

  accel->x_mg = axes.x;
  accel->y_mg = axes.y;
  accel->z_mg = axes.z;
  return 0;
}

uint8_t IMU_IsWakeCondition(void) {
  if (!imu_initialized) {
    return 0;
  }

  imu_accel_t cur;
  if (IMU_ReadAccel(&cur) != 0) {
    return 0;
  }

  /* Motion check: magnitude of acceleration delta */
  int32_t dx = cur.x_mg - imu_prev_accel.x_mg;
  int32_t dy = cur.y_mg - imu_prev_accel.y_mg;
  int32_t dz = cur.z_mg - imu_prev_accel.z_mg;
  imu_prev_accel = cur;

  uint32_t delta_sq = (uint32_t)(dx * dx + dy * dy + dz * dz);
  uint32_t thresh_sq =
      (uint32_t)IMU_WAKE_ACCEL_THRESHOLD_MG * IMU_WAKE_ACCEL_THRESHOLD_MG;
  uint8_t motion = (delta_sq > thresh_sq) ? 1 : 0;

  /* Tilt check: Z axis below threshold indicates device tilted */
  uint8_t tilted =
      ((uint32_t)abs(cur.z_mg) < (uint32_t)IMU_TILT_THRESHOLD_MG) ? 1 : 0;

  return (motion && tilted) ? 1 : 0;
}

void IMU_DeInit(void) {
  if (imu_initialized) {
    ISM330DLC_ACC_Disable(&imu_obj);
    ISM330DLC_DeInit(&imu_obj);
    imu_initialized = 0;
  }
}
