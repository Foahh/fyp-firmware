/**
 ******************************************************************************
 * @file    app_tof.c
 * @author  Long Liangmao
 * @brief   VL53L5CX Time-of-Flight ranging thread and hazard proximity fusion.
 *
 *          The ToF sensor provides an 8x8 depth grid. Fusion extracts the
 *          Z-distance inside "hand" and "hazard" bounding boxes (from the NN
 *          model) and raises an alert when a hand is within a configurable
 *          distance of a hazardous object.
 *
 *          NOTE: The hazard detection model (hand + saw/hazard classes) is not
 *          yet trained. TOF_GetHazardDetections() is a STUB returning zero
 *          detections. Replace it when the model is available.
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

#include "app_tof.h"
#include "app_cam_config.h"
#include "app_error.h"
#include "cmw_camera.h"
#include "stm32n6570_discovery.h"
#include "stm32n6570_discovery_bus.h"
#include "stm32n6xx_hal.h"
#include "tx_api.h"
#include "vl53l5cx.h"
#include "vl53l5cx_api.h"

#include <string.h>

/* ============================================================================
 * Configuration
 * ============================================================================ */

#define TOF_THREAD_STACK_SIZE 4096U
#define TOF_THREAD_PRIORITY   8

/* Sensor I2C address */
#define TOF_I2C_ADDRESS VL53L5CX_DEFAULT_I2C_ADDRESS

/* GPIO for sensor power enable (PQ5) */
#define TOF_PWR_EN_PORT GPIOQ
#define TOF_PWR_EN_PIN  GPIO_PIN_5

/* Poll at 2x the 15Hz ranging frequency */
#define TOF_POLL_TICKS ((TX_TIMER_TICKS_PER_SECOND + 29) / 30)

/* Depth data staleness threshold */
#define TOF_STALENESS_MS 500

/* FOV calibration: ToF FOV mapped to NN normalised [0,1] coordinates.
 * These are approximate and should be refined with physical calibration. */
#define TOF_NN_X0 0.0f
#define TOF_NN_Y0 0.0f
#define TOF_NN_X1 0.0f
#define TOF_NN_Y1 0.0f

/* ============================================================================
 * Thread resources
 * ============================================================================ */

static TX_THREAD tof_thread;
static UCHAR tof_thread_stack[TOF_THREAD_STACK_SIZE];

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

/* Configurable alert threshold (hand-to-hazard Z-distance) */
static uint32_t alert_threshold_mm = TOF_DEFAULT_ALERT_THRESHOLD_MM;

/* Stub detection result (always empty until model is trained) */
static hazard_detection_t stub_detections;

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
 * @brief  Run hand-vs-hazard depth fusion.
 *
 *         For each detected hand, extract its Z-distance from the depth grid.
 *         For each detected hazard, extract its Z-distance.
 *         Alert if the closest hand and closest hazard are within threshold_mm
 *         of each other (in Z / depth).
 *
 *         Currently a no-op because TOF_GetHazardDetections() returns empty
 *         results.  When the model is trained, this will work automatically.
 *
 * @param  grid  Current depth grid snapshot
 * @param  out   Alert state to populate
 */
static void tof_run_fusion(const tof_depth_grid_t *grid, tof_alert_t *out) {
  const hazard_detection_t *det = TOF_GetHazardDetections();

  memset(out, 0, sizeof(*out));

  /* Check staleness */
  uint32_t now = HAL_GetTick();
  if (!grid->valid || (now - grid->timestamp_ms) > TOF_STALENESS_MS) {
    return;
  }

  /* --- Extract closest hand depth --- */
  uint32_t hand_min = UINT32_MAX;
  for (int i = 0; i < det->nb_hands; i++) {
    uint32_t d;
    if (tof_extract_depth(grid, &det->hands[i], &d)) {
      out->has_hand_depth = 1;
      if (d < hand_min) {
        hand_min = d;
      }
    }
  }
  if (out->has_hand_depth) {
    out->hand_distance_mm = hand_min;
  }

  /* --- Extract closest hazard depth --- */
  uint32_t hazard_min = UINT32_MAX;
  for (int i = 0; i < det->nb_hazards; i++) {
    uint32_t d;
    if (tof_extract_depth(grid, &det->hazards[i], &d)) {
      out->has_hazard_depth = 1;
      if (d < hazard_min) {
        hazard_min = d;
      }
    }
  }
  if (out->has_hazard_depth) {
    out->hazard_distance_mm = hazard_min;
  }

  /* --- Alert decision --- */
  if (out->has_hand_depth && out->has_hazard_depth) {
    uint32_t diff = (hand_min > hazard_min) ? (hand_min - hazard_min)
                                            : (hazard_min - hand_min);
    out->alert = (diff < alert_threshold_mm) ? 1 : 0;
  }
}

/* ============================================================================
 * Thread entry
 * ============================================================================ */

static void tof_thread_entry(ULONG arg) {
  UNUSED(arg);

  /* Power on sensor via PQ5 */
  HAL_GPIO_WritePin(TOF_PWR_EN_PORT, TOF_PWR_EN_PIN, GPIO_PIN_SET);
  tx_thread_sleep(10); /* 10ms power-up delay */

  /* Register bus I/O */
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

  /* Initialize sensor (uploads firmware, ~1-2s) */
  ret = VL53L5CX_Init(&tof_obj);
  APP_REQUIRE(ret == VL53L5CX_OK);

  /* Configure: 8x8 resolution, 15Hz ranging, 20ms integration */
  uint8_t status;
  status = vl53l5cx_set_resolution(&tof_obj.Dev, VL53L5CX_RESOLUTION_8X8);
  APP_REQUIRE(status == VL53L5CX_OK);

  status = vl53l5cx_set_ranging_frequency_hz(&tof_obj.Dev, 15);
  APP_REQUIRE(status == VL53L5CX_OK);

  status = vl53l5cx_set_integration_time_ms(&tof_obj.Dev, 20);
  APP_REQUIRE(status == VL53L5CX_OK);

  /* Start ranging */
  status = vl53l5cx_start_ranging(&tof_obj.Dev);
  APP_REQUIRE(status == VL53L5CX_OK);

  /* Main ranging loop */
  VL53L5CX_ResultsData results;

  while (1) {
    uint8_t data_ready = 0;
    status = vl53l5cx_check_data_ready(&tof_obj.Dev, &data_ready);

    if (status == VL53L5CX_OK && data_ready) {
      status = vl53l5cx_get_ranging_data(&tof_obj.Dev, &results);
      if (status == VL53L5CX_OK) {
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

        /* Swap read buffer */
        depth_read_idx = write_idx;

        /* Run fusion and update alert */
        uint8_t alert_write_idx = alert_read_idx ^ 1;
        tof_run_fusion(grid, &alerts[alert_write_idx]);
        alert_read_idx = alert_write_idx;

        /* Drive LED based on alert */
        if (alerts[alert_write_idx].alert) {
          BSP_LED_On(LED_RED);
        } else {
          BSP_LED_Off(LED_RED);
        }
      }
    }

    tx_thread_sleep(TOF_POLL_TICKS);
  }
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void TOF_Thread_Start(void) {
  UINT status = tx_thread_create(&tof_thread, "tof_ranging",
                                 tof_thread_entry, 0,
                                 tof_thread_stack, TOF_THREAD_STACK_SIZE,
                                 TOF_THREAD_PRIORITY, TOF_THREAD_PRIORITY,
                                 TX_NO_TIME_SLICE, TX_AUTO_START);
  APP_REQUIRE(status == TX_SUCCESS);
}

const tof_depth_grid_t *TOF_GetDepthGrid(void) {
  return &depth_grids[depth_read_idx];
}

const tof_alert_t *TOF_GetAlert(void) {
  return &alerts[alert_read_idx];
}

void TOF_SetAlertThreshold(uint32_t threshold_mm) {
  alert_threshold_mm = threshold_mm;
}

/**
 * @brief  STUB — Returns zero hand/hazard detections.
 *
 *         When a hand+hazard detection model is trained, replace this with
 *         real inference output.  The expected workflow:
 *           1. NN model outputs bounding boxes with class_index for
 *              "hand" and "saw" (or other hazard).
 *           2. Post-processing splits detections into hands[] and hazards[].
 *           3. tof_run_fusion() extracts depth for each and checks proximity.
 */
const hazard_detection_t *TOF_GetHazardDetections(void) {
  /* Always returns empty — no model trained yet */
  return &stub_detections;
}

void TOF_Stop(void) {
  tx_thread_suspend(&tof_thread);
  vl53l5cx_stop_ranging(&tof_obj.Dev);
  HAL_GPIO_WritePin(TOF_PWR_EN_PORT, TOF_PWR_EN_PIN, GPIO_PIN_RESET);
  BSP_LED_Off(LED_RED);
}

void TOF_Resume(void) {
  HAL_GPIO_WritePin(TOF_PWR_EN_PORT, TOF_PWR_EN_PIN, GPIO_PIN_SET);
  tx_thread_sleep(10);
  vl53l5cx_start_ranging(&tof_obj.Dev);
  tx_thread_resume(&tof_thread);
}
