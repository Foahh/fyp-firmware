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

#include "app_error.h"
#include "app_lcd.h"
#include "cam.h"
#include "cam_config.h"
#include "cpuload.h"
#include "lcd_config.h"
#include "model_config.h"
#include "pp.h"
#include "stm32_lcd.h"
#include "stm32n6570_discovery_lcd.h"
#include "stm32n6xx_hal.h"
#include "tof.h"
#include "tx_user.h" /* For TX_TIMER_TICKS_PER_SECOND */
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
    UI_PANEL_LINE_Y(2),  /* Runtime label */
    UI_PANEL_LINE_Y(3),  /* Runtime value */
    UI_PANEL_LINE_Y(4),  /* CPU label */
    UI_PANEL_LINE_Y(5),  /* CPU value */
    UI_PANEL_LINE_Y(6),  /* CPU bar */
    UI_PANEL_LINE_Y(7),  /* Objects label */
    UI_PANEL_LINE_Y(8),  /* Objects value */
    UI_PANEL_LINE_Y(9),  /* Inference label */
    UI_PANEL_LINE_Y(10), /* Inference value */
    UI_PANEL_LINE_Y(11), /* Postprocess label */
    UI_PANEL_LINE_Y(12), /* Postprocess value */
    UI_PANEL_LINE_Y(13), /* Overhead label */
    UI_PANEL_LINE_Y(14), /* Overhead value */
    UI_PANEL_LINE_Y(15), /* FPS label */
    UI_PANEL_LINE_Y(16), /* FPS value */
    UI_PANEL_LINE_Y(17), /* Drops label */
    UI_PANEL_LINE_Y(18), /* Drops value */
    UI_PANEL_LINE_Y(19), /* Proximity label */
    UI_PANEL_LINE_Y(20), /* Proximity value */
};

/* ============================================================================
 * Global State Variables
 * ============================================================================ */

/* UI display double buffers */
uint8_t ui_display_buffers[2][LCD_WIDTH * LCD_HEIGHT * 4] ALIGN_32 IN_PSRAM;
volatile int ui_display_idx = 0;

/* CPU load tracking */
static cpuload_info_t g_cpu_load;

/* UI state */
static uint8_t g_ui_visible = 1;
static uint8_t g_ui_initialized = 0;
static volatile uint8_t g_tof_overlay_visible = 0;

/* ============================================================================
 * Thread Configuration
 * ============================================================================ */

/* UI update thread */
#define UI_THREAD_STACK_SIZE 4096
#define UI_THREAD_PRIORITY   9

/* Update rate for periodic diagnostic updates */
#define UI_UPDATE_FPS 30

/* Calculate sleep ticks from FPS */
#define UI_UPDATE_SLEEP_TICKS (TX_TIMER_TICKS_PER_SECOND / UI_UPDATE_FPS)

/* Idle measurement thread */
#define IDLE_THREAD_STACK_SIZE 512
#define IDLE_THREAD_PRIORITY   31 /* Lowest priority (runs when truly idle) */

/* Thread resources */
static struct {
  TX_THREAD thread;
  UCHAR stack[UI_THREAD_STACK_SIZE];
} ui_ctx;

static struct {
  TX_THREAD thread;
  UCHAR stack[IDLE_THREAD_STACK_SIZE];
} idle_ctx;

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
 * @brief  Idle measurement thread entry
 *         Runs at lowest priority to measure idle time via DWT cycle counter
 */
static void idle_measure_thread_entry(ULONG arg) {
  UNUSED(arg);

  while (1) {
    CPULoad_IdleEnter();
    tx_thread_relinquish(); /* Yield to higher priority threads */
    CPULoad_IdleExit();
  }
}

/**
 * @brief  UI update thread entry
 */
static void ui_thread_entry(ULONG arg) {
  UNUSED(arg);

  float cpu_load_pct;
  uint8_t *ui_buffer;
  const detection_info_t *det_info = NULL;
  const nn_crop_info_display_t *roi_info = NULL;
  const tof_alert_t *tof_alert = NULL;

  /* Get NN crop ROI (constant after initialization) */
  roi_info = CAM_GetDisplayROI();

  /* Setup LCD context */
  UI_SetupLCDContext();

  while (1) {
    uint32_t frame_start = HAL_GetTick();

    /* Update CPU load measurement */
    CPULoad_Update(&g_cpu_load);
    cpu_load_pct = CPULoad_GetSmoothed(&g_cpu_load);

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

    /* Clear detection overlay area */
    UTIL_LCD_FillRect(DISPLAY_LETTERBOX_X0, 0,
                      DISPLAY_LETTERBOX_WIDTH, DISPLAY_LETTERBOX_HEIGHT,
                      0x00000000);

    /* Draw panel text (may extend into camera area) */
    UI_DrawHeader();
    UI_DrawBuildOptions();
    UI_DrawModelNameBottomRight();
    UI_DrawRuntimeSection();
    UI_DrawCPULoadSection(cpu_load_pct);

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

    /* Draw depth grid (toggled by user button) */
    if (g_tof_overlay_visible) {
      const tof_depth_grid_t *depth_grid = TOF_GetDepthGrid();
      if (depth_grid->valid) {
        UI_DrawDepthGrid(depth_grid, roi_info);
      }
    }

    /* Swap buffers and reload display */
    Buffer_SetUIDisplayIndex(Buffer_GetNextUIDisplayIndex());
    LCD_ReloadUILayer(ui_buffer);

    /* Adaptive frame timing: sleep remaining time to hit target FPS */
    uint32_t elapsed = HAL_GetTick() - frame_start;
    if (elapsed < UI_UPDATE_SLEEP_TICKS) {
      tx_thread_sleep(UI_UPDATE_SLEEP_TICKS - elapsed);
    } else {
      tx_thread_sleep(1);
    }
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

  /* Initialize cycle counter for CPU load measurement */
  CPULoad_InitCounter();

  /* Initialize CPU load tracking */
  CPULoad_Init(&g_cpu_load);

  /* Setup UI layer */
  g_ui_initialized = 1;
  LCD_SetUIAlpha(255);

  /* Create idle measurement thread */
  tx_status = tx_thread_create(&idle_ctx.thread, "idle_measure",
                               idle_measure_thread_entry, 0,
                               idle_ctx.stack, IDLE_THREAD_STACK_SIZE,
                               IDLE_THREAD_PRIORITY, IDLE_THREAD_PRIORITY,
                               TX_NO_TIME_SLICE, TX_AUTO_START);
  APP_REQUIRE(tx_status == TX_SUCCESS);

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
  tx_thread_suspend(&idle_ctx.thread);
}

void UI_ThreadResume(void) {
  tx_thread_resume(&idle_ctx.thread);
  tx_thread_resume(&ui_ctx.thread);
  g_ui_visible = 1;
}
