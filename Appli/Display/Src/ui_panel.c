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

#include "cam_config.h"
#include "lcd_config.h"
#include "model_config.h"
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
 * @brief  Clamp percentage value to valid range [0.0, 100.0]
 */
static float UI_ClampPercentage(float percentage) {
  if (percentage < 0.0f) {
    return 0.0f;
  }
  if (percentage > 100.0f) {
    return 100.0f;
  }
  return percentage;
}

/**
 * @brief  Draw a horizontal progress bar
 */
static void UI_DrawProgressBar(uint32_t x, uint32_t y,
                               uint32_t width, uint32_t height,
                               float percentage) {
  uint32_t fill_width;

  percentage = UI_ClampPercentage(percentage);
  fill_width = (uint32_t)((width - 2) * percentage * 0.01f);

  /* Draw background */
  UTIL_LCD_FillRect(x, y, width, height, UI_COLOR_BAR_BG);

  /* Draw fill */
  if (fill_width > 0) {
    UTIL_LCD_FillRect(x + 1, y + 1, fill_width, height - 2, UI_COLOR_BAR_FG);
  }

  /* Draw border */
  UTIL_LCD_DrawRect(x, y, width, height, UI_COLOR_TEXT);
}

/**
 * @brief  Format CPU load percentage as string
 */
static void UI_FormatCPULoad(char *buf, size_t buf_size, float cpu_load) {
  snprintf(buf, buf_size, "%.1f%%", cpu_load);
}

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
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[2],
                           (uint8_t *)"Runtime", LEFT_MODE);

  /* Value */
  UTIL_LCD_SetTextColor(UI_COLOR_VALUE);
  tick = HAL_GetTick();
  UI_FormatRuntime(text_buf, sizeof(text_buf), tick);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[3],
                           (uint8_t *)text_buf, LEFT_MODE);
}

/**
 * @brief  Draw CPU load section (label, value, and progress bar)
 */
void UI_DrawCPULoadSection(float cpu_load_pct) {
  char text_buf[UI_TEXT_BUFFER_SIZE];
  uint32_t bar_width;

  /* Label */
  UTIL_LCD_SetTextColor(UI_COLOR_LABEL);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[4],
                           (uint8_t *)"CPU Load", LEFT_MODE);

  /* Value */
  UTIL_LCD_SetTextColor(UI_COLOR_VALUE);
  UI_FormatCPULoad(text_buf, sizeof(text_buf), cpu_load_pct);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[5],
                           (uint8_t *)text_buf, LEFT_MODE);

  /* Progress bar */
  bar_width = UI_PANEL_WIDTH - 2 * UI_TEXT_MARGIN_X;
  UI_DrawProgressBar(UI_TEXT_MARGIN_X, g_line_y[6], bar_width,
                     UI_PROGRESS_BAR_HEIGHT, cpu_load_pct);
}

/**
 * @brief  Draw detection information section in diagnostic panel
 */
void UI_DrawDetectionInfoSection(const detection_info_t *info) {
  char text_buf[UI_TEXT_BUFFER_SIZE];

  /* Objects count */
  UTIL_LCD_SetTextColor(UI_COLOR_LABEL);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[7],
                           (uint8_t *)"Objects", LEFT_MODE);
  UTIL_LCD_SetTextColor(UI_COLOR_VALUE);
  snprintf(text_buf, sizeof(text_buf), "%d", (int)info->nb_detect);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[8],
                           (uint8_t *)text_buf, LEFT_MODE);

  /* Inference time */
  UTIL_LCD_SetTextColor(UI_COLOR_LABEL);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[9],
                           (uint8_t *)"Inference", LEFT_MODE);
  UTIL_LCD_SetTextColor(UI_COLOR_VALUE);
  snprintf(text_buf, sizeof(text_buf), "%lums", (unsigned long)info->inference_ms);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[10],
                           (uint8_t *)text_buf, LEFT_MODE);

  /* Postprocess time */
  UTIL_LCD_SetTextColor(UI_COLOR_LABEL);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[11],
                           (uint8_t *)"Postprocess", LEFT_MODE);
  UTIL_LCD_SetTextColor(UI_COLOR_VALUE);
  snprintf(text_buf, sizeof(text_buf), "%lums", (unsigned long)info->postprocess_ms);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[12],
                           (uint8_t *)text_buf, LEFT_MODE);

  /* Overhead time */
  UTIL_LCD_SetTextColor(UI_COLOR_LABEL);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[13],
                           (uint8_t *)"Overhead", LEFT_MODE);
  UTIL_LCD_SetTextColor(UI_COLOR_VALUE);
  uint32_t overhead_ms = info->nn_period_ms > info->inference_ms ? info->nn_period_ms - info->inference_ms : 0;
  snprintf(text_buf, sizeof(text_buf), "%lums", (unsigned long)overhead_ms);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[14],
                           (uint8_t *)text_buf, LEFT_MODE);

  /* FPS */
  UTIL_LCD_SetTextColor(UI_COLOR_LABEL);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[15],
                           (uint8_t *)"Inf. FPS", LEFT_MODE);
  UTIL_LCD_SetTextColor(UI_COLOR_VALUE);
  float fps = info->nn_period_ms > 0 ? 1000.0f / info->nn_period_ms : 0.0f;
  snprintf(text_buf, sizeof(text_buf), "%.1f", fps);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[16],
                           (uint8_t *)text_buf, LEFT_MODE);

  /* Frame drops */
  UTIL_LCD_SetTextColor(UI_COLOR_LABEL);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[17],
                           (uint8_t *)"Drops", LEFT_MODE);
  UTIL_LCD_SetTextColor(UI_COLOR_VALUE);
  snprintf(text_buf, sizeof(text_buf), "%lu", (unsigned long)info->frame_drops);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[18],
                           (uint8_t *)text_buf, LEFT_MODE);
}

/**
 * @brief  Draw proximity info section in diagnostic panel
 */
void UI_DrawProximitySection(const tof_alert_t *alert) {
  char text_buf[UI_TEXT_BUFFER_SIZE];

  UTIL_LCD_SetTextColor(UI_COLOR_LABEL);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[19],
                           (uint8_t *)"Hand/Hazard", LEFT_MODE);

  if (!alert->has_hand_depth && !alert->has_hazard_depth) {
    UTIL_LCD_SetTextColor(UI_COLOR_LABEL);
    UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[20],
                             (uint8_t *)"--/--", LEFT_MODE);
  } else {
    uint32_t color = alert->alert ? 0xFFFF0000 : 0xFF00FF00;
    UTIL_LCD_SetTextColor(color);

    char hand_str[16] = "--";
    char haz_str[16] = "--";
    if (alert->has_hand_depth) {
      snprintf(hand_str, sizeof(hand_str), "%.2fm",
               alert->hand_distance_mm / 1000.0f);
    }
    if (alert->has_hazard_depth) {
      snprintf(haz_str, sizeof(haz_str), "%.2fm",
               alert->hazard_distance_mm / 1000.0f);
    }
    snprintf(text_buf, sizeof(text_buf), "%s/%s", hand_str, haz_str);
    UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[20],
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

#ifdef PERFORMANCE_MODE
  perf_label = "PERFORMANCE MODE";
#else
  perf_label = "NOMINAL MODE";
#endif

  UTIL_LCD_SetTextColor(UI_COLOR_METADATA);

  y = UI_TEXT_MARGIN_Y;
  snprintf(line, sizeof(line), "%s", perf_label);
  tw = (uint32_t)strlen(line) * UI_FONT_WIDTH;
  x0 = LCD_WIDTH - UI_TEXT_MARGIN_X - tw;
  UTIL_LCD_DisplayStringAt(x0, y, (uint8_t *)line, LEFT_MODE);

  y += UI_FONT_HEIGHT + UI_LINE_SPACING;
#ifdef CAMERA_NN_SNAPSHOT_MODE
  snprintf(line, sizeof(line), "CAMERA SNAPSHOT");
#else
  snprintf(line, sizeof(line), "CAMERA %dFPS", CAMERA_FPS);
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
void UI_DrawModelNameBottomRight(void) {
  uint32_t tw = (uint32_t)strlen(MDL_DISPLAY_NAME) * UI_FONT_WIDTH;
  uint32_t x0 = LCD_WIDTH - UI_TEXT_MARGIN_X - tw;
  uint32_t y = LCD_HEIGHT - UI_TEXT_MARGIN_Y - UI_FONT_HEIGHT;

  UTIL_LCD_SetTextColor(UI_COLOR_METADATA);
  UTIL_LCD_DisplayStringAt(x0, y, (uint8_t *)MDL_DISPLAY_NAME, LEFT_MODE);
}
