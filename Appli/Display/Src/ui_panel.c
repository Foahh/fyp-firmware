/**
 ******************************************************************************
 * @file    ui_panel.c
 * @author  Long Liangmao
 * @brief   Diagnostic panel rendering for the UI overlay
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

#include "build_timestamp.h"
#include "cam_config.h"
#include "init_clock.h"
#include "lcd_config.h"
#include "model_config.h"
#include "power_mode.h"
#include "stm32_lcd.h"
#include "stm32n6xx_hal.h"
#include "tx_user.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief  Format runtime as MM:SS string
 */
static void UI_FormatRuntime(char *buf, size_t buf_size, uint32_t tick_ms) {
  uint32_t sec = tick_ms / 1000;
  uint32_t min = sec / 60;
  sec = sec % 60;
  snprintf(buf, buf_size, "%lu:%02lu", (unsigned long)min, (unsigned long)sec);
}

/* ============================================================================
 * Panel Drawing Functions
 * ============================================================================ */

/**
 * @brief  Draw UI panel background (clear area)
 */
void UI_DrawPanelBackground(void) {
  UTIL_LCD_FillRect(UI_PANEL_X0, UI_PANEL_Y0,
                    UI_PANEL_WIDTH, UI_PANEL_HEIGHT, UI_COLOR_BG);
}

/**
 * @brief  Draw UI title and separator
 */
void UI_DrawHeader(void) {
  UTIL_LCD_SetTextColor(UI_COLOR_TEXT);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[0],
                           (uint8_t *)"DIAGNOSTICS", LEFT_MODE);

  /* Separator line */
  UTIL_LCD_DrawHLine(UI_TEXT_MARGIN_X, g_line_y[1] + UI_LINE_HEIGHT / 4,
                     UI_PANEL_WIDTH - 2 * UI_TEXT_MARGIN_X, UI_COLOR_TEXT);
}

/**
 * @brief  Draw runtime section (label and value)
 */
void UI_DrawRuntimeSection(void) {
  char text_buf[UI_TEXT_BUFFER_SIZE];
  uint32_t tick;

  /* Label */
  UTIL_LCD_SetTextColor(UI_COLOR_LABEL);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[4],
                           (uint8_t *)"Runtime", LEFT_MODE);

  /* Value */
  UTIL_LCD_SetTextColor(UI_COLOR_VALUE);
  tick = HAL_GetTick();
  UI_FormatRuntime(text_buf, sizeof(text_buf), tick);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[5],
                           (uint8_t *)text_buf, LEFT_MODE);
}

/**
 * @brief  Draw detection information section in diagnostic panel
 */
void UI_DrawDetectionInfoSection(const detection_info_t *info) {
  char text_buf[UI_TEXT_BUFFER_SIZE];

  /* Objects count */
  UTIL_LCD_SetTextColor(UI_COLOR_LABEL);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[6],
                           (uint8_t *)"Objects", LEFT_MODE);
  UTIL_LCD_SetTextColor(UI_COLOR_VALUE);
  snprintf(text_buf, sizeof(text_buf), "%d", (int)info->nb_detect);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[7],
                           (uint8_t *)text_buf, LEFT_MODE);

  /* Inference time */
  UTIL_LCD_SetTextColor(UI_COLOR_LABEL);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[8],
                           (uint8_t *)"Inference", LEFT_MODE);
  UTIL_LCD_SetTextColor(UI_COLOR_VALUE);
  snprintf(text_buf, sizeof(text_buf), "%luus", (unsigned long)info->inference_us);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[9],
                           (uint8_t *)text_buf, LEFT_MODE);

  /* Postprocess time */
  UTIL_LCD_SetTextColor(UI_COLOR_LABEL);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[10],
                           (uint8_t *)"Postprocess", LEFT_MODE);
  UTIL_LCD_SetTextColor(UI_COLOR_VALUE);
  snprintf(text_buf, sizeof(text_buf), "%luus", (unsigned long)info->postprocess_us);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[11],
                           (uint8_t *)text_buf, LEFT_MODE);

  /* Tracker time */
  UTIL_LCD_SetTextColor(UI_COLOR_LABEL);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[12],
                           (uint8_t *)"Tracker", LEFT_MODE);
  UTIL_LCD_SetTextColor(UI_COLOR_VALUE);
  snprintf(text_buf, sizeof(text_buf), "%luus", (unsigned long)info->tracker_us);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[13],
                           (uint8_t *)text_buf, LEFT_MODE);

  /* Overhead time */
  UTIL_LCD_SetTextColor(UI_COLOR_LABEL);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[14],
                           (uint8_t *)"Overhead", LEFT_MODE);
  UTIL_LCD_SetTextColor(UI_COLOR_VALUE);
  uint32_t work_us =
      info->inference_us + info->postprocess_us + info->tracker_us;
  uint32_t period_us = info->nn_period_us;
  uint32_t overhead_us =
      (work_us < period_us) ? (period_us - work_us) : 0U;
  snprintf(text_buf, sizeof(text_buf), "%luus", (unsigned long)overhead_us);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[15],
                           (uint8_t *)text_buf, LEFT_MODE);

  /* FPS */
  UTIL_LCD_SetTextColor(UI_COLOR_LABEL);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[16],
                           (uint8_t *)"Inf. FPS", LEFT_MODE);
  UTIL_LCD_SetTextColor(UI_COLOR_VALUE);
  float fps = info->nn_period_us > 0 ? 1000000.0f / info->nn_period_us : 0.0f;
  snprintf(text_buf, sizeof(text_buf), "%.1f", fps);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[17],
                           (uint8_t *)text_buf, LEFT_MODE);

  /* Frame drops */
  UTIL_LCD_SetTextColor(UI_COLOR_LABEL);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[18],
                           (uint8_t *)"Drops", LEFT_MODE);
  UTIL_LCD_SetTextColor(UI_COLOR_VALUE);
  snprintf(text_buf, sizeof(text_buf), "%lu", (unsigned long)info->frame_drops);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[19],
                           (uint8_t *)text_buf, LEFT_MODE);
}

/**
 * @brief  Draw CPU load info section in diagnostic panel
 *         Uses old cpuload module (smoothed EMA percentage).
 */
void UI_DrawCpuLoadSection(void) {
  static float smoothed_ratio = 0.0f;
  static uint8_t smoothing_initialized = 0U;
  const float smoothing_alpha = 0.20f;
  float usage_ratio = 0.0f;
  uint32_t bar_x = UI_TEXT_MARGIN_X;
  uint32_t bar_y = g_line_y[3];
  uint32_t bar_w = UI_PANEL_WIDTH - (2U * UI_TEXT_MARGIN_X);
  uint32_t fill_w = 0U;

  UTIL_LCD_SetTextColor(UI_COLOR_LABEL);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[2], (uint8_t *)"CPU Load", LEFT_MODE);

  usage_ratio = CPU_LoadGetUsageRatio();
  if (usage_ratio < 0.0f) {
    usage_ratio = 0.0f;
  } else if (usage_ratio > 1.0f) {
    usage_ratio = 1.0f;
  }

  if (!smoothing_initialized) {
    smoothed_ratio = usage_ratio;
    smoothing_initialized = 1U;
  } else {
    smoothed_ratio = smoothed_ratio + (smoothing_alpha * (usage_ratio - smoothed_ratio));
  }

  fill_w = (uint32_t)(smoothed_ratio * (float)bar_w);

  UTIL_LCD_FillRect(bar_x, bar_y, bar_w, UI_PROGRESS_BAR_HEIGHT, UI_COLOR_BAR_BG);
  if (fill_w > 0U) {
    UTIL_LCD_FillRect(bar_x, bar_y, fill_w, UI_PROGRESS_BAR_HEIGHT, UI_COLOR_BAR_FG);
  }
}

/**
 * @brief  Draw person-distance info section in diagnostic panel
 */
void UI_DrawProximitySection(const tof_alert_t *alert) {
  char text_buf[UI_TEXT_BUFFER_SIZE];

  UTIL_LCD_SetTextColor(UI_COLOR_LABEL);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[20],
                           (uint8_t *)"Person Dist", LEFT_MODE);

  if (!alert->has_person_depth) {
    UTIL_LCD_SetTextColor(UI_COLOR_LABEL);
    UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[21],
                             (uint8_t *)"--", LEFT_MODE);
  } else {
    uint32_t color = alert->alert ? 0xFFFF0000 : 0xFF00FF00;
    UTIL_LCD_SetTextColor(color);
    snprintf(text_buf, sizeof(text_buf), "%.2fm",
             alert->person_distance_mm / 1000.0f);
    UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[21],
                             (uint8_t *)text_buf, LEFT_MODE);
  }
}

/**
 * @brief  Show build-time options (snapshot/perf, camera FPS, NN input size) at top right
 */
void UI_DrawBuildOptions(void) {
  char line[48];
  const char *perf_label;
  uint32_t y;
  uint32_t tw;
  uint32_t x0;

#if POWER_MODE == POWER_MODE_OVERDRIVE
  perf_label = "OVERDRIVE";
#elif POWER_MODE == POWER_MODE_UNDERDRIVE
  perf_label = "UNDERDRIVE";
#else
  perf_label = "NOMINAL";
#endif

  UTIL_LCD_SetTextColor(UI_COLOR_METADATA);

  y = UI_TEXT_MARGIN_Y;
  snprintf(line, sizeof(line), "%s", perf_label);
  tw = (uint32_t)strlen(line) * UI_FONT_WIDTH;
  x0 = LCD_WIDTH - UI_TEXT_MARGIN_X - tw;
  UTIL_LCD_DisplayStringAt(x0, y, (uint8_t *)line, LEFT_MODE);

  y += UI_FONT_HEIGHT + UI_LINE_SPACING;
  snprintf(line, sizeof(line), "CPU %lu MHz",
           (unsigned long)AppClock_GetCpuFreqMHz());
  tw = (uint32_t)strlen(line) * UI_FONT_WIDTH;
  x0 = LCD_WIDTH - UI_TEXT_MARGIN_X - tw;
  UTIL_LCD_DisplayStringAt(x0, y, (uint8_t *)line, LEFT_MODE);

  y += UI_FONT_HEIGHT + UI_LINE_SPACING;
  snprintf(line, sizeof(line), "NPU %lu MHz",
           (unsigned long)AppClock_GetNpuFreqMHz());
  tw = (uint32_t)strlen(line) * UI_FONT_WIDTH;
  x0 = LCD_WIDTH - UI_TEXT_MARGIN_X - tw;
  UTIL_LCD_DisplayStringAt(x0, y, (uint8_t *)line, LEFT_MODE);

  y += UI_FONT_HEIGHT + UI_LINE_SPACING;
#ifdef SNAPSHOT_MODE
  snprintf(line, sizeof(line), "SNAPSHOT");
#else
  snprintf(line, sizeof(line), "CAMERA %d FPS", CAMERA_FPS);
#endif
  tw = (uint32_t)strlen(line) * UI_FONT_WIDTH;
  x0 = LCD_WIDTH - UI_TEXT_MARGIN_X - tw;
  UTIL_LCD_DisplayStringAt(x0, y, (uint8_t *)line, LEFT_MODE);

  y += UI_FONT_HEIGHT + UI_LINE_SPACING;
  snprintf(line, sizeof(line), "NN %dx%d", (int)MDL_NN_IN_WIDTH, (int)MDL_NN_IN_HEIGHT);
  tw = (uint32_t)strlen(line) * UI_FONT_WIDTH;
  x0 = LCD_WIDTH - UI_TEXT_MARGIN_X - tw;
  UTIL_LCD_DisplayStringAt(x0, y, (uint8_t *)line, LEFT_MODE);
}

/**
 * @brief  Show neural network model id at bottom right (camera area)
 */
void UI_DrawBottomRightInfo(void) {
  uint32_t tw;
  uint32_t x0;
  uint32_t y = LCD_HEIGHT - UI_TEXT_MARGIN_Y - UI_FONT_HEIGHT;

  /* Model name (bottom line) */
  tw = (uint32_t)strlen(MDL_DISPLAY_NAME) * UI_FONT_WIDTH;
  x0 = LCD_WIDTH - UI_TEXT_MARGIN_X - tw;
  UTIL_LCD_SetTextColor(UI_COLOR_METADATA);
  UTIL_LCD_DisplayStringAt(x0, y, (uint8_t *)MDL_DISPLAY_NAME, LEFT_MODE);

  /* Build timestamp (one line above model name) */
  y -= UI_LINE_HEIGHT;
  tw = (uint32_t)strlen(BUILD_TIMESTAMP) * UI_FONT_WIDTH;
  x0 = LCD_WIDTH - UI_TEXT_MARGIN_X - tw;
  UTIL_LCD_DisplayStringAt(x0, y, (uint8_t *)BUILD_TIMESTAMP, LEFT_MODE);
}
