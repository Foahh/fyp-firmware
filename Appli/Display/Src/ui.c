/**
 ******************************************************************************
 * @file    ui.c
 * @author  Long Liangmao
 * @brief   UI thread entries and public API
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

#include "ui_internal.h"

#include "cam.h"
#include "cam_config.h"
#include "display.h"
#include "error.h"
#include "lcd_config.h"
#include "model_config.h"
#include "pp.h"
#include "stm32_lcd.h"
#include "stm32n6570_discovery_lcd.h"
#include "stm32n6xx_hal.h"
#include "thread_config.h"
#include "timebase.h"
#include "tof.h"
#include "ui.h"
#include "utils.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * Pre-computed Layout Constants
 * ============================================================================ */

const uint16_t g_line_y[] = {
    UI_PANEL_LINE_Y(0),  /* Title */
    UI_PANEL_LINE_Y(1),  /* Separator */
    UI_PANEL_LINE_Y(2),  /* CPU label */
    UI_PANEL_LINE_Y(3),  /* CPU usage bar */
    UI_PANEL_LINE_Y(4),  /* Runtime label */
    UI_PANEL_LINE_Y(5),  /* Runtime value */
    UI_PANEL_LINE_Y(6),  /* Objects label */
    UI_PANEL_LINE_Y(7),  /* Objects value */
    UI_PANEL_LINE_Y(8),  /* Inference label */
    UI_PANEL_LINE_Y(9),  /* Inference value */
    UI_PANEL_LINE_Y(10), /* Postprocess label */
    UI_PANEL_LINE_Y(11), /* Postprocess value */
    UI_PANEL_LINE_Y(12), /* Tracker label */
    UI_PANEL_LINE_Y(13), /* Tracker value */
    UI_PANEL_LINE_Y(14), /* Overhead label */
    UI_PANEL_LINE_Y(15), /* Overhead value */
    UI_PANEL_LINE_Y(16), /* FPS label */
    UI_PANEL_LINE_Y(17), /* FPS value */
    UI_PANEL_LINE_Y(18), /* Drops label */
    UI_PANEL_LINE_Y(19), /* Drops value */
    UI_PANEL_LINE_Y(20), /* Proximity label */
    UI_PANEL_LINE_Y(21), /* Proximity value */
};

/* ============================================================================
 * Global State Variables
 * ============================================================================ */

/* UI display double buffers */
uint8_t ui_display_buffers[2][LCD_WIDTH * LCD_HEIGHT * 4] ALIGN_32 IN_PSRAM;
volatile int ui_display_idx = 0;

/* UI state */
static uint8_t g_ui_visible = 1;
static uint8_t g_ui_initialized = 0;
static volatile uint8_t g_tof_overlay_visible = 0;

/* ============================================================================
 * Thread Configuration
 * ============================================================================ */

/* Update rate for periodic diagnostic updates */
#define UI_UPDATE_FPS 20

/* Calculate sleep ticks from FPS */
#define UI_UPDATE_SLEEP_TICKS FPS_TO_TICKS(UI_UPDATE_FPS)

/* Thread resources */
static struct {
  TX_THREAD thread;
  UCHAR stack[UI_THREAD_STACK_SIZE];
} ui_ctx;

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief  Setup LCD context for UI rendering
 */
static void UI_SetupLCDContext(void) {
  UTIL_LCD_SetLayer(LCD_LAYER_1_UI);
  UTIL_LCD_SetFont(&Font16);
  UTIL_LCD_SetBackColor(0x00000000); /* Transparent background */
}

/* ============================================================================
 * Thread Entry Points
 * ============================================================================ */

/**
 * @brief  UI update thread entry
 */
static void ui_thread_entry(ULONG arg) {
  UNUSED(arg);

  uint8_t *ui_buffer;
  const detection_info_t *det_info = NULL;
  const nn_crop_info_display_t *roi_info = NULL;
  const tof_alert_t *tof_alert = NULL;

  /* Get NN crop ROI (constant after initialization) */
  roi_info = CAM_GetDisplayROI();

  /* Track whether previous frame had detections (for conditional clear) */
  uint8_t prev_had_detections = 0;
  uint8_t prev_tof_overlay_visible = 0;
  uint8_t pending_overlay_clear_frames = 0;

  /* Setup LCD context */
  UI_SetupLCDContext();

  while (1) {
    /* Get latest detection info */
    det_info = PP_GetInfo();

    /* Get latest proximity alert */
    tof_alert = TOF_GetAlert();

    if (!g_ui_initialized || !g_ui_visible) {
      tx_thread_sleep(UI_UPDATE_SLEEP_TICKS);
      continue;
    }

    /* Get UI back buffer */
    ui_buffer = Buffer_GetUIBackBuffer();
    if (ui_buffer == NULL) {
      tx_thread_sleep(UI_UPDATE_SLEEP_TICKS);
      continue;
    }

    LCD_SetUILayerAddress(ui_buffer);

    /* Render all UI elements */
    /* Draw diagnostic panel background (left side) */
    UI_DrawPanelBackground();

    /* Clear detection overlay area only when needed */
    uint8_t cur_has_detections =
        (det_info != NULL && det_info->nb_detect > 0) ? 1 : 0;
    uint8_t cur_tof_overlay_visible = g_tof_overlay_visible ? 1 : 0;

    uint8_t cur_overlay_active = cur_has_detections || cur_tof_overlay_visible;
    uint8_t prev_overlay_active =
        prev_had_detections || prev_tof_overlay_visible;

    /* With double buffering, clear once more after overlays become inactive so
     * both UI buffers are cleaned and stale boxes cannot reappear. */
    if (!cur_overlay_active && prev_overlay_active) {
      pending_overlay_clear_frames = 1;
    }

    if (cur_overlay_active || prev_overlay_active ||
        pending_overlay_clear_frames > 0) {
      UTIL_LCD_FillRect(DISPLAY_LETTERBOX_X0, 0,
                        DISPLAY_LETTERBOX_WIDTH, DISPLAY_LETTERBOX_HEIGHT,
                        0x00000000);

      if (!cur_overlay_active && !prev_overlay_active &&
          pending_overlay_clear_frames > 0) {
        pending_overlay_clear_frames--;
      }
    }

    /* Draw panel text (may extend into camera area) */
    UI_DrawHeader();
    UI_DrawBuildOptions();
    UI_DrawBottomRightInfo();
    UI_DrawRuntimeSection();

    /* Draw detection info in diagnostic panel if available */
    if (det_info != NULL) {
      UI_DrawDetectionInfoSection(det_info);
    }

    /* Draw detection overlays last so boxes appear on top of text */
    if (det_info != NULL && roi_info != NULL) {
      UI_DrawDetectionOverlays(det_info, roi_info);
    }

    if (tof_alert != NULL) {
      UI_DrawProximitySection(tof_alert);
      if (tof_alert->alert) {
        UI_DrawProximityAlertBanner();
      }
    }

    UI_DrawCpuLoadSection();

    /* Draw depth grid (toggled by user button) */
    if (cur_tof_overlay_visible) {
      const tof_depth_grid_t *depth_grid = TOF_GetDepthGrid();
      if (depth_grid->valid) {
        UI_DrawDepthGrid(depth_grid, roi_info);
      }
    }

    prev_had_detections = cur_has_detections;
    prev_tof_overlay_visible = cur_tof_overlay_visible;

    /* Swap buffers and reload display */
    Buffer_SetUIDisplayIndex(Buffer_GetNextUIDisplayIndex());
    LCD_ReloadUILayer(ui_buffer);

    tx_thread_sleep(UI_UPDATE_SLEEP_TICKS);
  }
}

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

void UI_ThreadStart(void) {
  UINT tx_status;

  if (g_ui_initialized) {
    return;
  }

  /* Setup UI layer */
  g_ui_initialized = 1;
  LCD_SetUIAlpha(255);

  /* Create UI update thread */
  tx_status = tx_thread_create(&ui_ctx.thread, "ui_update",
                               ui_thread_entry, 0,
                               ui_ctx.stack, UI_THREAD_STACK_SIZE,
                               UI_THREAD_PRIORITY, UI_THREAD_PRIORITY,
                               TX_NO_TIME_SLICE, TX_AUTO_START);
  APP_REQUIRE(tx_status == TX_SUCCESS);
}

void UI_SetVisible(uint8_t visible) {
  g_ui_visible = visible;

  if (!visible) {
    /* Clear UI layer when hiding */
    uint8_t *ui_buffer = Buffer_GetUIBackBuffer();
    if (ui_buffer != NULL) {
      LCD_SetUILayerAddress(ui_buffer);
      memset(ui_buffer, 0, LCD_WIDTH * LCD_HEIGHT * 4);
      Buffer_SetUIDisplayIndex(Buffer_GetNextUIDisplayIndex());
      LCD_ReloadUILayer(ui_buffer);
    }
  }
}

uint8_t UI_IsVisible(void) {
  return g_ui_visible;
}

void UI_ToggleTOFOverlay(void) {
  g_tof_overlay_visible ^= 1;
}

void UI_ThreadSuspend(void) {
  g_ui_visible = 0;
  tx_thread_suspend(&ui_ctx.thread);
}

void UI_ThreadResume(void) {
  UI_SetupLCDContext();
  LCD_SetUIAlpha(255);
  g_ui_visible = 1;
  tx_thread_resume(&ui_ctx.thread);
}
