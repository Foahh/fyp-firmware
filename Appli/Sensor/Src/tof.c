/**
 ******************************************************************************
 * @file    tof.c
 * @author  Long Liangmao
 * @brief   VL53L5CX Time-of-Flight ranging thread and depth grid publishing.
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

#include "tof.h"
#include "cam_config.h"
#include "cmw_camera.h"
#include "error.h"
#include "haptic.h"
#include "stm32n6570_discovery.h"
#include "stm32n6570_discovery_bus.h"
#include "stm32n6xx_hal.h"
#include "thread_config.h"
#include "timebase.h"
#include "tof_fusion.h"
#include "tx_api.h"
#include "vl53l5cx.h"
#include "vl53l5cx_api.h"

/* ============================================================================
 * Configuration
 * ============================================================================ */

/* Sensor I2C address */
#define TOF_I2C_ADDRESS VL53L5CX_DEFAULT_I2C_ADDRESS

/* GPIO for sensor power enable (PQ5) */
#define TOF_PWR_EN_PORT GPIOQ
#define TOF_PWR_EN_PIN  GPIO_PIN_5

/* GPIO for sensor data-ready interrupt (PQ0, active-low) */
#define TOF_INT_PORT GPIOQ
#define TOF_INT_PIN  GPIO_PIN_0

/* Timeout for data-ready wait. Sensor runs at 15 Hz (~67 ms period);
 * if no interrupt arrives within this window, poll once as a fallback. */
#define TOF_DATA_READY_TIMEOUT_MS 200

/* ============================================================================
 * Thread resources
 * ============================================================================ */

static struct {
  TX_THREAD thread;
  UCHAR stack[TOF_THREAD_STACK_SIZE];
} tof_ctx;

/* Event flag for interrupt-driven data-ready notification */
#define TOF_EVT_DATA_READY 0x1U
static TX_EVENT_FLAGS_GROUP tof_events;
static TX_EVENT_FLAGS_GROUP tof_result_update_event_flags;

/* ============================================================================
 * Sensor state
 * ============================================================================ */

static VL53L5CX_Object_t tof_obj;

/* Double-buffered depth grid */
static tof_depth_grid_t depth_grids[2];
static volatile uint8_t depth_read_idx = 0;

/* Precomputed zone remap for current CAMERA_FLIP setting */
static uint8_t tof_zone_rows[TOF_GRID_SIZE * TOF_GRID_SIZE];
static uint8_t tof_zone_cols[TOF_GRID_SIZE * TOF_GRID_SIZE];

/* ============================================================================
 * Hardware init & interrupt callback
 * ============================================================================ */

void TOF_Init(void) {
  __HAL_RCC_GPIOQ_CLK_ENABLE();

  /* PQ5: power enable output */
  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = TOF_PWR_EN_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_WritePin(TOF_PWR_EN_PORT, TOF_PWR_EN_PIN, GPIO_PIN_RESET);
  HAL_GPIO_Init(TOF_PWR_EN_PORT, &gpio);

  /* PQ0: data-ready interrupt input (active-low, falling edge) */
  gpio.Pin = TOF_INT_PIN;
  gpio.Mode = GPIO_MODE_IT_FALLING;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(TOF_INT_PORT, &gpio);

  HAL_NVIC_SetPriority(EXTI0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  /* Power on sensor */
  HAL_GPIO_WritePin(TOF_PWR_EN_PORT, TOF_PWR_EN_PIN, GPIO_PIN_SET);
  HAL_Delay(10);

  /* Register bus I/O (I2C1 must already be initialised) */
  VL53L5CX_IO_t io = {
      .Init = BSP_I2C1_Init,
      .DeInit = BSP_I2C1_DeInit,
      .Address = TOF_I2C_ADDRESS,
      .WriteReg = BSP_I2C1_WriteReg16,
      .ReadReg = BSP_I2C1_ReadReg16,
      .GetTick = (VL53L5CX_GetTick_Func)HAL_GetTick,
  };

  int32_t ret = VL53L5CX_RegisterBusIO(&tof_obj, &io);
  APP_REQUIRE(ret == VL53L5CX_OK);

  /* Upload firmware to sensor (~1-2 s) */
  ret = VL53L5CX_Init(&tof_obj);
  APP_REQUIRE(ret == VL53L5CX_OK);

  /* Configure: 8x8 resolution, 15 Hz ranging, 20 ms integration */
  uint8_t status;
  status = vl53l5cx_set_resolution(&tof_obj.Dev, VL53L5CX_RESOLUTION_8X8);
  APP_REQUIRE(status == VL53L5CX_OK);

  status = vl53l5cx_set_ranging_frequency_hz(&tof_obj.Dev, 15);
  APP_REQUIRE(status == VL53L5CX_OK);

  status = vl53l5cx_set_integration_time_ms(&tof_obj.Dev, 20);
  APP_REQUIRE(status == VL53L5CX_OK);

  for (int zone = 0; zone < TOF_GRID_SIZE * TOF_GRID_SIZE; zone++) {
    int row = zone / TOF_GRID_SIZE;
    int col = zone % TOF_GRID_SIZE;

    if (CAMERA_FLIP & CMW_MIRRORFLIP_MIRROR) {
      col = TOF_GRID_SIZE - 1 - col;
    }
    if (CAMERA_FLIP & CMW_MIRRORFLIP_FLIP) {
      row = TOF_GRID_SIZE - 1 - row;
    }

    tof_zone_rows[zone] = (uint8_t)row;
    tof_zone_cols[zone] = (uint8_t)col;
  }
}

void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin) {
  if (GPIO_Pin == TOF_INT_PIN) {
    tx_event_flags_set(&tof_events, TOF_EVT_DATA_READY, TX_OR);
  }
}

/* ============================================================================
 * Thread entry
 * ============================================================================ */

static void tof_thread_entry(ULONG arg) {
  UNUSED(arg);

  /* Sensor already initialised and configured by TOF_Init(); start ranging. */
  uint8_t status = vl53l5cx_start_ranging(&tof_obj.Dev);
  APP_REQUIRE(status == VL53L5CX_OK);

  /* Main ranging loop — wait for INT pin instead of polling */
  static VL53L5CX_ResultsData results;

  while (1) {
    ULONG flags;
    UINT evt_status = tx_event_flags_get(
        &tof_events, TOF_EVT_DATA_READY, TX_OR_CLEAR, &flags,
        MS_TO_TICKS(TOF_DATA_READY_TIMEOUT_MS));

    if (evt_status != TX_SUCCESS && evt_status != TX_NO_EVENTS) {
      continue;
    }

    /* On timeout, do one poll as a fallback; on event, data should be ready */
    if (evt_status == TX_NO_EVENTS) {
      uint8_t data_ready = 0;
      vl53l5cx_check_data_ready(&tof_obj.Dev, &data_ready);
      if (!data_ready) {
        continue;
      }
    }

    status = vl53l5cx_get_ranging_data(&tof_obj.Dev, &results);
    if (status != VL53L5CX_OK) {
      continue;
    }

    /* Write to back buffer */
    uint8_t write_idx = depth_read_idx ^ 1;
    tof_depth_grid_t *grid = &depth_grids[write_idx];

    for (int zone = 0; zone < TOF_GRID_SIZE * TOF_GRID_SIZE; zone++) {
      uint8_t row = tof_zone_rows[zone];
      uint8_t col = tof_zone_cols[zone];
      grid->distance_mm[row][col] = results.distance_mm[zone];
      grid->status[row][col] = results.target_status[zone];
    }
    grid->timestamp_ms = HAL_GetTick();
    grid->valid = 1;

    /* Publish depth grid: ensure all writes visible before index swap. */
    __DMB();
    depth_read_idx = write_idx;

    tx_event_flags_set(&tof_result_update_event_flags, 0x01, TX_OR);
    TOF_FUSION_NotifyDepthReady();
  }
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void TOF_ThreadStart(void) {
  UINT status = tx_event_flags_create(&tof_events, "tof_events");
  APP_REQUIRE(status == TX_SUCCESS);

  status = tx_event_flags_create(&tof_result_update_event_flags, "tof_result_update");
  APP_REQUIRE(status == TX_SUCCESS);

  status = tx_thread_create(&tof_ctx.thread, "tof_ranging", tof_thread_entry, 0,
                            tof_ctx.stack, TOF_THREAD_STACK_SIZE,
                            TOF_THREAD_PRIORITY, TOF_THREAD_PRIORITY,
                            TX_NO_TIME_SLICE, TX_AUTO_START);
  APP_REQUIRE(status == TX_SUCCESS);

  TOF_FUSION_ThreadStart();
}

const tof_depth_grid_t *TOF_GetDepthGrid(void) {
  uint8_t idx = depth_read_idx;
  __DMB();
  return &depth_grids[idx];
}

TX_EVENT_FLAGS_GROUP *TOF_GetResultUpdateEventFlags(void) {
  return &tof_result_update_event_flags;
}

void TOF_Stop(void) {
  HAL_NVIC_DisableIRQ(EXTI0_IRQn);
  TOF_FUSION_Stop();
  tx_thread_suspend(&tof_ctx.thread);
  vl53l5cx_stop_ranging(&tof_obj.Dev);
  HAL_GPIO_WritePin(TOF_PWR_EN_PORT, TOF_PWR_EN_PIN, GPIO_PIN_RESET);
  BSP_LED_Off(LED_RED);
  HAPTIC_Off();
}

void TOF_Resume(void) {
  HAL_GPIO_WritePin(TOF_PWR_EN_PORT, TOF_PWR_EN_PIN, GPIO_PIN_SET);
  tx_thread_sleep(MS_TO_TICKS(10)); /* 10ms power-up delay */
  vl53l5cx_start_ranging(&tof_obj.Dev);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);
  tx_thread_resume(&tof_ctx.thread);
  TOF_FUSION_Resume();
}
