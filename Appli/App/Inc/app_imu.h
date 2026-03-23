/**
 ******************************************************************************
 * @file    app_imu.h
 * @author  Long Liangmao
 * @brief   IMU abstraction layer over ISM330DLC driver
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

#ifndef APP_IMU_H
#define APP_IMU_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief  IMU I2C address.
 *         ISM330DLC default: 0xD4 (7-bit 0x6A, shifted).
 *         TODO: Verify board-specific address and wiring.
 */
#define IMU_I2C_ADDRESS 0xD4U

/** Accelerometer output data rate for wake detection (Hz) */
#define IMU_ACC_ODR_HZ 52.0f

/** Accelerometer full-scale range (g) */
#define IMU_ACC_FS_G 4

/** Wake motion threshold in mg */
#define IMU_WAKE_ACCEL_THRESHOLD_MG 200

/** Tilt threshold: absolute Z component below this (mg) indicates tilt */
#define IMU_TILT_THRESHOLD_MG 700

/** Inactivity duration (ms) before returning to standby */
#define IMU_INACTIVITY_TIMEOUT_MS 10000

/**
 * @brief  Accelerometer reading in milligravity units.
 */
typedef struct {
  int32_t x_mg;
  int32_t y_mg;
  int32_t z_mg;
} imu_accel_t;

/**
 * @brief  Initialize IMU (ISM330DLC) via I2C and enable accelerometer.
 * @retval 0 on success, -1 on failure
 */
int32_t IMU_Init(void);

/**
 * @brief  Read current accelerometer values.
 * @param  accel: Output accelerometer data in mg
 * @retval 0 on success, -1 on failure
 */
int32_t IMU_ReadAccel(imu_accel_t *accel);

/**
 * @brief  Evaluate wake condition from current accelerometer data.
 *         Wake = significant motion (delta > threshold) AND tilted posture
 *         (Z axis below tilt threshold, indicating device not flat).
 * @retval 1 if wake condition met, 0 otherwise
 */
uint8_t IMU_IsWakeCondition(void);

/**
 * @brief  De-initialize IMU to reduce power.
 */
void IMU_DeInit(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_IMU_H */
