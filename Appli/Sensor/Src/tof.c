/**
 ******************************************************************************
 * @file    app_tof.c
 * @author  Long Liangmao
 * @brief   VL53L5CX Time-of-Flight ranging thread and person-distance fusion.
 *
 *          The ToF sensor provides an 8x8 depth grid. Fusion extracts the
 *          nearest depth sample inside detected person bounding boxes and
 *          raises an alert when a person is within a configurable distance
 *          threshold.
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
#include "pp.h"
#include "stm32n6570_discovery.h"
#include "stm32n6570_discovery_bus.h"
#include "stm32n6xx_hal.h"
#include "thread_config.h"
#include "timebase.h"
#include "tx_api.h"
#include "vl53l5cx.h"
#include "vl53l5cx_api.h"

#include <string.h>

/* ============================================================================
 * Model class indices (must match MDL_PP_CLASS_LABELS in model header)
 * ============================================================================ */

#define TOF_CLASS_PERSON 0

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

/* Depth data staleness threshold */
#define TOF_STALENESS_MS 500

/* FOV calibration: ToF FOV mapped to NN normalised [0,1] coordinates.
 * These are approximate and should be refined with physical calibration. */
#define TOF_NN_X0 0.0f
#define TOF_NN_Y0 -0.10
#define TOF_NN_X1 1.0f
#define TOF_NN_Y1 0.90

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

/* ============================================================================
 * Sensor state
 * ============================================================================ */

static VL53L5CX_Object_t tof_obj;

/* Double-buffered depth grid */
static tof_depth_grid_t depth_grids[2];
static volatile uint8_t depth_read_idx = 0;

/* Double-buffered alert state */
static tof_alert_t alerts[2];
static volatile uint8_t alert_read_idx = 0;

/* Configurable alert threshold (nearest person distance) */
static uint32_t alert_threshold_mm = TOF_DEFAULT_ALERT_THRESHOLD_MM;

/* ============================================================================
 * Depth extraction helper
 * ============================================================================ */

/**
 * @brief  Extract the minimum valid depth (mm) from the ToF grid cells that
 *         overlap a normalised bounding box.
 * @param  grid       Current depth grid
 * @param  bbox       Bounding box in NN normalised [0,1] coords
 * @param  out_mm     Output: minimum depth in mm (only written when valid)
 * @retval 1 if at least one valid depth sample was found, 0 otherwise
 */
static uint8_t tof_extract_depth(const tof_depth_grid_t *grid,
                                 const tof_bbox_t *bbox,
                                 uint32_t *out_mm) {
  float hw = bbox->width * 0.5f;
  float hh = bbox->height * 0.5f;

  /* Bbox edges in NN coords */
  float bx0 = bbox->x_center - hw;
  float by0 = bbox->y_center - hh;
  float bx1 = bbox->x_center + hw;
  float by1 = bbox->y_center + hh;

  /* Map NN coords → ToF grid indices [0..7] */
  float sx = (float)TOF_GRID_SIZE / (TOF_NN_X1 - TOF_NN_X0);
  float sy = (float)TOF_GRID_SIZE / (TOF_NN_Y1 - TOF_NN_Y0);

  int gx0 = (int)((bx0 - TOF_NN_X0) * sx);
  int gy0 = (int)((by0 - TOF_NN_Y0) * sy);
  int gx1 = (int)((bx1 - TOF_NN_X0) * sx);
  int gy1 = (int)((by1 - TOF_NN_Y0) * sy);

  /* Clamp to grid bounds */
  if (gx0 < 0) {
    gx0 = 0;
  }
  if (gy0 < 0) {
    gy0 = 0;
  }
  if (gx1 >= TOF_GRID_SIZE) {
    gx1 = TOF_GRID_SIZE - 1;
  }
  if (gy1 >= TOF_GRID_SIZE) {
    gy1 = TOF_GRID_SIZE - 1;
  }
  if (gx0 > gx1 || gy0 > gy1) {
    return 0;
  }

  uint32_t min_d = UINT32_MAX;
  uint8_t found = 0;

  for (int gy = gy0; gy <= gy1; gy++) {
    for (int gx = gx0; gx <= gx1; gx++) {
      uint8_t st = grid->status[gy][gx];
      if (st != 5 && st != 9) {
        continue;
      }
      int16_t d = grid->distance_mm[gy][gx];
      if (d <= 0) {
        continue;
      }
      found = 1;
      if ((uint32_t)d < min_d) {
        min_d = (uint32_t)d;
      }
    }
  }

  if (found) {
    *out_mm = min_d;
  }
  return found;
}

/* ============================================================================
 * Fusion logic
 * ============================================================================ */

/**
 * @brief  Run person-distance fusion with temporal pairing.
 *
 *         Pairs the latest NN detection with the depth grid, checking the
 *         timestamp delta is within FUSION_MAX_DT_MS. The alert is asserted
 *         when the nearest person depth is within the configured threshold.
 *
 * @param  grid  Current depth grid snapshot
 * @param  out   Alert state to populate
 */
static void tof_run_fusion(const tof_depth_grid_t *grid, tof_alert_t *out) {
  const tof_person_detection_t *det = TOF_GetPersonDetections();

  memset(out, 0, sizeof(*out));

  /* Check depth grid staleness */
  uint32_t now = HAL_GetTick();
  if (!grid->valid || (now - grid->timestamp_ms) > TOF_STALENESS_MS) {
    out->stale = 1;
    return;
  }

  /* Temporal pairing: check NN detection timestamp vs depth grid */
  const detection_info_t *pp_info = PP_GetInfo();
  if (pp_info != NULL && pp_info->timestamp_ms != 0) {
    uint32_t dt = (grid->timestamp_ms > pp_info->timestamp_ms)
                      ? (grid->timestamp_ms - pp_info->timestamp_ms)
                      : (pp_info->timestamp_ms - grid->timestamp_ms);
    if (dt > FUSION_MAX_DT_MS) {
      out->stale = 1;
      return;
    }
  }

  uint32_t person_min = UINT32_MAX;
  for (int i = 0; i < det->nb_persons; i++) {
    uint32_t d;
    if (tof_extract_depth(grid, &det->persons[i], &d)) {
      out->has_person_depth = 1;
      if (d < person_min) {
        person_min = d;
      }
    }
  }
  if (out->has_person_depth) {
    out->person_distance_mm = person_min;
    out->alert = (person_min < alert_threshold_mm) ? 1U : 0U;
  }
}

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
      int row = zone / TOF_GRID_SIZE;
      int col = zone % TOF_GRID_SIZE;
      /* Match camera orientation (CAMERA_FLIP / CMW_MIRRORFLIP_*). */
      if (CAMERA_FLIP & CMW_MIRRORFLIP_MIRROR) {
        col = TOF_GRID_SIZE - 1 - col;
      }
      if (CAMERA_FLIP & CMW_MIRRORFLIP_FLIP) {
        row = TOF_GRID_SIZE - 1 - row;
      }
      grid->distance_mm[row][col] = results.distance_mm[zone];
      grid->status[row][col] = results.target_status[zone];
    }
    grid->timestamp_ms = HAL_GetTick();
    grid->valid = 1;

    /* Publish depth grid: ensure all writes visible before index swap. */
    __DMB();
    depth_read_idx = write_idx;

    /* Run fusion and update alert */
    uint8_t alert_write_idx = alert_read_idx ^ 1;
    tof_run_fusion(grid, &alerts[alert_write_idx]);

    /* Publish alert: ensure all writes visible before index swap. */
    __DMB();
    alert_read_idx = alert_write_idx;

    /* Drive LED and haptic based on alert */
    if (alerts[alert_write_idx].alert) {
      BSP_LED_On(LED_RED);
      HAPTIC_On();
    } else {
      BSP_LED_Off(LED_RED);
      HAPTIC_Off();
    }
  }
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void TOF_ThreadStart(void) {
  UINT status = tx_event_flags_create(&tof_events, "tof_events");
  APP_REQUIRE(status == TX_SUCCESS);

  status = tx_thread_create(&tof_ctx.thread, "tof_ranging",
                            tof_thread_entry, 0,
                            tof_ctx.stack, TOF_THREAD_STACK_SIZE,
                            TOF_THREAD_PRIORITY, TOF_THREAD_PRIORITY,
                            TX_NO_TIME_SLICE, TX_AUTO_START);
  APP_REQUIRE(status == TX_SUCCESS);
}

const tof_depth_grid_t *TOF_GetDepthGrid(void) {
  uint8_t idx = depth_read_idx;
  __DMB();
  return &depth_grids[idx];
}

const tof_alert_t *TOF_GetAlert(void) {
  uint8_t idx = alert_read_idx;
  __DMB();
  return &alerts[idx];
}

void TOF_SetAlertThreshold(uint32_t threshold_mm) {
  alert_threshold_mm = threshold_mm;
}

/**
 * @brief  Filter latest NN detections down to person boxes.
 *
 *         Reads directly from PP_GetInfo() and keeps only class_index 0.
 *         Called from tof_run_fusion() on every ToF cycle.
 */
const tof_person_detection_t *TOF_GetPersonDetections(void) {
  static tof_person_detection_t det;
  det.nb_persons = 0;

  const detection_info_t *pp_info = PP_GetInfo();
  if (pp_info == NULL) {
    return &det;
  }

  for (int i = 0; i < pp_info->nb_detect; i++) {
    const od_pp_outBuffer_t *d = &pp_info->detects[i];
    if (d->class_index == TOF_CLASS_PERSON &&
        det.nb_persons < TOF_MAX_DETECTIONS) {
      tof_bbox_t *b = &det.persons[det.nb_persons++];
      b->x_center = d->x_center;
      b->y_center = d->y_center;
      b->width    = d->width;
      b->height   = d->height;
      b->conf     = d->conf;
    }
  }

  return &det;
}

void TOF_Stop(void) {
  HAL_NVIC_DisableIRQ(EXTI0_IRQn);
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
}
