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
 * @brief  IMU snapshot: latest accelerometer reading and wake state.
 *         Updated by the IMU sampling thread, read-only for consumers.
 */
typedef struct {
  int32_t x_mg;
  int32_t y_mg;
  int32_t z_mg;
  uint8_t wake;
  uint32_t timestamp_ms;
} imu_data_t;

/**
 * @brief  Create and start the IMU sampling thread.
 *         Initializes the ISM330DLC sensor and periodically reads
 *         accelerometer data into a double-buffered snapshot.
 */
void IMU_Thread_Start(void);

/**
 * @brief  Get pointer to latest IMU snapshot (read-only, double-buffered).
 * @retval Pointer to the current read buffer (always valid, check timestamp_ms
 *         for freshness; 0 means no data yet).
 */
const imu_data_t *IMU_GetData(void);

/**
 * @brief  Stop IMU thread and power down sensor.
 */
void IMU_Stop(void);

/**
 * @brief  Resume IMU thread and re-initialize sensor.
 */
void IMU_Resume(void);

/**
 * @brief  Enter low-power mode: drop ODR to 12.5 Hz for standby wake detection.
 */
void IMU_EnterLowPower(void);

/**
 * @brief  Exit low-power mode: restore ODR to normal rate.
 */
void IMU_ExitLowPower(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_IMU_H */
