/**
 ******************************************************************************
 * @file    app_ui.c
 * @author  Long Liangmao
 * @brief   UI overlay implementation
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

#include "app_ui.h"
#include "app_cam.h"
#include "app_cam_config.h"
#include "app_cpuload.h"
#include "app_error.h"
#include "app_lcd.h"
#include "app_lcd_config.h"
#include "app_pp.h"
#include "app_tof.h"
#include "model_config.h"
#include "stm32_lcd.h"
#include "stm32n6570_discovery_lcd.h"
#include "stm32n6xx_hal.h"
#include "tx_user.h" /* For TX_TIMER_TICKS_PER_SECOND */
#include "utils.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void clamp_point(int *x, int *y);
static void draw_detection(const od_pp_outBuffer_t *det,
                           int roi_x0, int roi_y0, int roi_w, int roi_h);
static void UI_DrawBuildOptions(void);
static void UI_DrawModelNameBottomRight(void);

/* ============================================================================
 * UI Configuration Constants
 * ============================================================================ */

/* Panel geometry - left side of screen */
#define UI_PANEL_X0     0
#define UI_PANEL_Y0     0
#define UI_PANEL_WIDTH  160
#define UI_PANEL_HEIGHT 480

/* Text layout */
#define UI_TEXT_MARGIN_X 8
#define UI_TEXT_MARGIN_Y 8
#define UI_LINE_SPACING  4
#define UI_FONT_HEIGHT   16 /* Cache Font16.Height to avoid struct access */
#define UI_FONT_WIDTH    11 /* Cache Font16.Width to avoid struct access */

/* Colors (ARGB8888 format) */
#define UI_COLOR_BG       0xC0000000 /* Semi-transparent black */
#define UI_COLOR_TEXT     0xFF00FF00 /* Bright green (terminal style) */
#define UI_COLOR_LABEL    0xFF808080 /* Gray for labels */
#define UI_COLOR_METADATA 0xFFD8D8D8 /* Light gray for build/model overlays */
#define UI_COLOR_VALUE    0xFFFFFFFF /* White for values */
#define UI_COLOR_BAR_BG   0xFF202020 /* Dark gray bar background */
#define UI_COLOR_BAR_FG   0xFF00CC00 /* Green bar fill */

/* Buffer sizes */
#define UI_TEXT_BUFFER_SIZE    64
#define UI_PROGRESS_BAR_HEIGHT 12

/* Update rate for periodic diagnostic updates */
#define UI_UPDATE_FPS 30

/* Calculate sleep ticks from FPS */
#define UI_UPDATE_SLEEP_TICKS (TX_TIMER_TICKS_PER_SECOND / UI_UPDATE_FPS)

/* Depth grid overlay (8x8 ToF heatmap on camera view) */
#define DEPTH_CELL_SIZE   20
#define DEPTH_GRID_PIXELS (DEPTH_CELL_SIZE * TOF_GRID_SIZE) /* 160 */
#define DEPTH_GRID_X0     (DISPLAY_LETTERBOX_X0 + 8)
#define DEPTH_GRID_Y0     (DISPLAY_LETTERBOX_HEIGHT - DEPTH_GRID_PIXELS - 48)

/* Center zone: rows/cols 3 and 4 (0-indexed) */
#define DEPTH_CENTER_ROW_MIN 3
#define DEPTH_CENTER_ROW_MAX 4
#define DEPTH_CENTER_COL_MIN 3
#define DEPTH_CENTER_COL_MAX 4

#define DEPTH_FONT_HEIGHT 8 /* Font8.Height */

/* Detection overlay colors (ARGB8888) */
#define NUMBER_COLORS 10
static const uint32_t detection_colors[NUMBER_COLORS] = {
    0xFF00FF00, /* Green */
    0xFFFF0000, /* Red */
    0xFF00FFFF, /* Cyan */
    0xFFFF00FF, /* Magenta */
    0xFFFFFF00, /* Yellow */
    0xFF808080, /* Gray */
    0xFF000000, /* Black */
    0xFFA52A2A, /* Brown */
    0xFF0000FF, /* Blue */
    0xFFFFA500, /* Orange */
};

/* Class names table — from model config */
#define NB_CLASSES (sizeof(MDL_PP_CLASS_LABELS) / sizeof(MDL_PP_CLASS_LABELS[0]))

/* ============================================================================
 * Pre-computed Layout Constants
 * ============================================================================ */

/* Line height calculation */
#define UI_LINE_HEIGHT (UI_FONT_HEIGHT + UI_LINE_SPACING)

/* Y for diagnostic panel text row index (0 = first line below top margin) */
#define UI_PANEL_LINE_Y(row) \
  ((uint16_t)(UI_TEXT_MARGIN_Y + (uint16_t)(row) * (uint16_t)UI_LINE_HEIGHT))

static const uint16_t g_line_y[] = {
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

/**
 * @brief  Clamp a point to display bounds
 * @param  x: Pointer to X coordinate (modified in place)
 * @param  y: Pointer to Y coordinate (modified in place)
 */
static void clamp_point(int *x, int *y) {
  if (*x < 0) {
    *x = 0;
  }
  if (*y < 0) {
    *y = 0;
  }
  if (*x >= (int)DISPLAY_LETTERBOX_WIDTH) {
    *x = DISPLAY_LETTERBOX_WIDTH - 1;
  }
  if (*y >= (int)DISPLAY_LETTERBOX_HEIGHT) {
    *y = DISPLAY_LETTERBOX_HEIGHT - 1;
  }
}

/**
 * @brief  Draw a single detection bounding box
 * @param  det: Pointer to detection result
 * @param  roi_x0: ROI top-left X coordinate (pre-computed, passed from caller)
 * @param  roi_y0: ROI top-left Y coordinate (pre-computed, passed from caller)
 * @param  roi_w: ROI width (pre-computed, passed from caller)
 * @param  roi_h: ROI height (pre-computed, passed from caller)
 */
static void draw_detection(const od_pp_outBuffer_t *det,
                           int roi_x0, int roi_y0, int roi_w, int roi_h) {
  int xc, yc, x0, y0, x1, y1, w, h;
  uint32_t color;
  int class_idx;

  /* Convert normalized coordinates (relative to NN input 480x480) to ROI pixel coordinates */
  /* Detections are normalized (0-1) relative to NN input size, scale to ROI size */
  xc = (int)(det->x_center * roi_w) + roi_x0 + DISPLAY_LETTERBOX_X0;
  yc = (int)(det->y_center * roi_h) + roi_y0;
  w = (int)(det->width * roi_w);
  h = (int)(det->height * roi_h);

  x0 = xc - (w + 1) / 2;
  y0 = yc - (h + 1) / 2;
  x1 = xc + (w + 1) / 2;
  y1 = yc + (h + 1) / 2;

  /* Adjust to screen coordinates (relative to letterbox offset) */
  x0 -= DISPLAY_LETTERBOX_X0;
  x1 -= DISPLAY_LETTERBOX_X0;

  clamp_point(&x0, &y0);
  clamp_point(&x1, &y1);

  /* Offset back for drawing on full screen */
  x0 += DISPLAY_LETTERBOX_X0;
  x1 += DISPLAY_LETTERBOX_X0;

  class_idx = det->class_index;
  color = detection_colors[class_idx % NUMBER_COLORS];

  /* Draw bounding box */
  UTIL_LCD_DrawRect(x0, y0, x1 - x0, y1 - y0, color);

  /* Draw class label */
  if (class_idx < (int)NB_CLASSES) {
    UTIL_LCD_DisplayStringAt(x0 + 2, y0 + 2,
                             (uint8_t *)MDL_PP_CLASS_LABELS[class_idx], LEFT_MODE);
  }

  /* Draw confidence */
  char conf_str[8];
  int conf_pct = (int)(det->conf * 100.0f + 0.5f);
  snprintf(conf_str, sizeof(conf_str), "%d%%", conf_pct);
  UTIL_LCD_DisplayStringAt(x1 - 40, y0 + 2, (uint8_t *)conf_str, LEFT_MODE);
}

/* ============================================================================
 * Depth Grid Visualization
 * ============================================================================ */

/**
 * @brief  Map a depth value to an ARGB8888 color
 */
static uint32_t depth_to_color(int16_t distance_mm, uint8_t status) {
  if (status != 5 && status != 9) {
    return 0x60404040;
  }
  if (distance_mm <= 0) {
    return 0x60404040;
  }
  if (distance_mm < 200) {
    return 0xC0FF0000; /* Red — very close */
  }
  if (distance_mm < 500) {
    return 0xC0FF6600; /* Orange */
  }
  if (distance_mm < 1000) {
    return 0xC0FFCC00; /* Yellow */
  }
  if (distance_mm < 1500) {
    return 0xC000FF00; /* Green */
  }
  if (distance_mm < 2500) {
    return 0xC000CCFF; /* Light blue */
  }
  return 0xC00066FF; /* Blue — far */
}

/**
 * @brief  Draw 8x8 ToF depth heatmap with per-cell cm values and center readout
 */
static void UI_DrawDepthGrid(const tof_depth_grid_t *grid) {
  if (grid == NULL || !grid->valid) {
    return;
  }

  /* Skip if data is stale (>1s) */
  uint32_t now = HAL_GetTick();
  if ((now - grid->timestamp_ms) > 1000) {
    return;
  }

  /* Draw color-filled cells */
  for (int row = 0; row < TOF_GRID_SIZE; row++) {
    for (int col = 0; col < TOF_GRID_SIZE; col++) {
      uint32_t color = depth_to_color(grid->distance_mm[row][col],
                                      grid->status[row][col]);
      uint32_t x = DEPTH_GRID_X0 + col * DEPTH_CELL_SIZE;
      uint32_t y = DEPTH_GRID_Y0 + row * DEPTH_CELL_SIZE;
      UTIL_LCD_FillRect(x, y, DEPTH_CELL_SIZE - 1, DEPTH_CELL_SIZE - 1, color);
    }
  }

  /* Border around entire grid */
  UTIL_LCD_DrawRect(DEPTH_GRID_X0 - 1, DEPTH_GRID_Y0 - 1,
                    DEPTH_GRID_PIXELS + 1, DEPTH_GRID_PIXELS + 1, UI_COLOR_TEXT);

  /* Highlight center 2x2 zone with white border (calibration target) */
  for (int row = DEPTH_CENTER_ROW_MIN; row <= DEPTH_CENTER_ROW_MAX; row++) {
    for (int col = DEPTH_CENTER_COL_MIN; col <= DEPTH_CENTER_COL_MAX; col++) {
      uint32_t x = DEPTH_GRID_X0 + col * DEPTH_CELL_SIZE;
      uint32_t y = DEPTH_GRID_Y0 + row * DEPTH_CELL_SIZE;
      UTIL_LCD_DrawRect(x, y, DEPTH_CELL_SIZE - 1, DEPTH_CELL_SIZE - 1,
                        0xFFFFFFFF);
    }
  }

  /* Overlay per-cell distance values in cm using Font8 */
  UTIL_LCD_SetFont(&Font8);
  uint32_t text_y_offset = (DEPTH_CELL_SIZE - DEPTH_FONT_HEIGHT) / 2;
  for (int row = 0; row < TOF_GRID_SIZE; row++) {
    for (int col = 0; col < TOF_GRID_SIZE; col++) {
      int16_t dist = grid->distance_mm[row][col];
      uint8_t stat = grid->status[row][col];
      uint32_t x = DEPTH_GRID_X0 + col * DEPTH_CELL_SIZE + 1;
      uint32_t y = DEPTH_GRID_Y0 + row * DEPTH_CELL_SIZE + text_y_offset;

      char val_str[5];
      if ((stat != 5 && stat != 9) || dist <= 0) {
        strncpy(val_str, "---", sizeof(val_str));
        UTIL_LCD_SetTextColor(0xFF606060);
      } else {
        int cm = dist / 10;
        if (cm > 999) {
          strncpy(val_str, "FAR", sizeof(val_str));
        } else {
          snprintf(val_str, sizeof(val_str), "%3d", cm);
        }
        UTIL_LCD_SetTextColor(0xFFFFFFFF);
      }
      UTIL_LCD_DisplayStringAt(x, y, (uint8_t *)val_str, LEFT_MODE);
    }
  }
  UTIL_LCD_SetFont(&Font16);

  /* Label above grid */
  UTIL_LCD_SetTextColor(UI_COLOR_TEXT);
  UTIL_LCD_DisplayStringAt(DEPTH_GRID_X0, DEPTH_GRID_Y0 - UI_FONT_HEIGHT - 2,
                           (uint8_t *)"ToF [cm]", LEFT_MODE);

  /* Center zone average distance readout below grid */
  int32_t center_sum = 0;
  int32_t center_count = 0;
  for (int row = DEPTH_CENTER_ROW_MIN; row <= DEPTH_CENTER_ROW_MAX; row++) {
    for (int col = DEPTH_CENTER_COL_MIN; col <= DEPTH_CENTER_COL_MAX; col++) {
      int16_t d = grid->distance_mm[row][col];
      uint8_t s = grid->status[row][col];
      if ((s == 5 || s == 9) && d > 0) {
        center_sum += d;
        center_count++;
      }
    }
  }

  char ctr_str[24];
  if (center_count > 0) {
    int32_t avg_mm = center_sum / center_count;
    snprintf(ctr_str, sizeof(ctr_str), "Ctr: %ldmm", (long)avg_mm);
  } else {
    strncpy(ctr_str, "Ctr: ---", sizeof(ctr_str));
  }
  UTIL_LCD_SetTextColor(UI_COLOR_VALUE);
  UTIL_LCD_DisplayStringAt(DEPTH_GRID_X0, DEPTH_GRID_Y0 + DEPTH_GRID_PIXELS + 4,
                           (uint8_t *)ctr_str, LEFT_MODE);
}

/* ============================================================================
 * UI Drawing Functions
 * ============================================================================ */

/**
 * @brief  Draw UI panel background (clear area)
 */
static void UI_DrawPanelBackground(void) {
  UTIL_LCD_FillRect(UI_PANEL_X0, UI_PANEL_Y0,
                    UI_PANEL_WIDTH, UI_PANEL_HEIGHT, UI_COLOR_BG);
}

/**
 * @brief  Show build-time options (snapshot/perf, camera FPS, NN input size) at top right
 */
static void UI_DrawBuildOptions(void) {
  char line[48];
  const char *snap_label;
  const char *perf_label;
  uint32_t y;
  uint32_t tw;
  uint32_t x0;

#ifdef CAMERA_NN_SNAPSHOT_MODE
  snap_label = "SNAPSHOT";
#else
  snap_label = "CONTINUOUS";
#endif
#ifdef PERFORMANCE_MODE
  perf_label = "PERFORMANCE";
#else
  perf_label = "NOMINAL";
#endif

  UTIL_LCD_SetTextColor(UI_COLOR_METADATA);

  y = UI_TEXT_MARGIN_Y;
  snprintf(line, sizeof(line), "%s", snap_label);
  tw = (uint32_t)strlen(line) * UI_FONT_WIDTH;
  x0 = LCD_WIDTH - UI_TEXT_MARGIN_X - tw;
  UTIL_LCD_DisplayStringAt(x0, y, (uint8_t *)line, LEFT_MODE);

  y += UI_FONT_HEIGHT + UI_LINE_SPACING;
  snprintf(line, sizeof(line), "%s", perf_label);
  tw = (uint32_t)strlen(line) * UI_FONT_WIDTH;
  x0 = LCD_WIDTH - UI_TEXT_MARGIN_X - tw;
  UTIL_LCD_DisplayStringAt(x0, y, (uint8_t *)line, LEFT_MODE);

  y += UI_FONT_HEIGHT + UI_LINE_SPACING;
  snprintf(line, sizeof(line), "CAMERA %dFPS", CAMERA_FPS);
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
static void UI_DrawModelNameBottomRight(void) {
  uint32_t tw = (uint32_t)strlen(MDL_DISPLAY_NAME) * UI_FONT_WIDTH;
  uint32_t x0 = LCD_WIDTH - UI_TEXT_MARGIN_X - tw;
  uint32_t y = LCD_HEIGHT - UI_TEXT_MARGIN_Y - UI_FONT_HEIGHT;

  UTIL_LCD_SetTextColor(UI_COLOR_METADATA);
  UTIL_LCD_DisplayStringAt(x0, y, (uint8_t *)MDL_DISPLAY_NAME, LEFT_MODE);
}

/**
 * @brief  Draw UI title and separator
 */
static void UI_DrawHeader(void) {
  UTIL_LCD_SetTextColor(UI_COLOR_TEXT);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[0],
                           (uint8_t *)"DIAGNOSTICS", LEFT_MODE);

  /* Separator line */
  UTIL_LCD_DrawHLine(UI_TEXT_MARGIN_X, g_line_y[1] + UI_LINE_HEIGHT / 4,
                     UI_PANEL_WIDTH - 2 * UI_TEXT_MARGIN_X, UI_COLOR_TEXT);
}

/**
 * @brief  Draw CPU load section (label, value, and progress bar)
 */
static void UI_DrawCPULoadSection(float cpu_load_pct) {
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
 * @brief  Draw runtime section (label and value)
 */
static void UI_DrawRuntimeSection(void) {
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
 * @brief  Draw detection information section in diagnostic panel
 */
static void UI_DrawDetectionInfoSection(const detection_info_t *info) {
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
 * @brief  Draw detection overlays (bounding boxes and ROI rectangle)
 */
static void UI_DrawDetectionOverlays(const detection_info_t *info,
                                     const nn_crop_info_display_t *roi_info) {
  /* Draw bounding boxes */
  UTIL_LCD_SetTextColor(0xFF00FF00); /* Green for boxes */
  for (int i = 0; i < info->nb_detect; i++) {
    draw_detection(&info->detects[i], roi_info->roi_x0, roi_info->roi_y0,
                   roi_info->roi_w, roi_info->roi_h);
  }

  /* Draw NN crop ROI rectangle */
  UTIL_LCD_SetTextColor(0xFF00FFFF); /* Cyan */
  UTIL_LCD_DrawRect(DISPLAY_LETTERBOX_X0 + roi_info->roi_x0, roi_info->roi_y0,
                    roi_info->roi_w, roi_info->roi_h,
                    0xFF00FFFF);
}

/**
 * @brief  Draw proximity info section in diagnostic panel
 */
static void UI_DrawProximitySection(const tof_alert_t *alert) {
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
 * @brief  Draw hazard proximity alert banner on the camera view
 */
static void UI_DrawProximityAlertBanner(void) {
  uint32_t banner_y = DISPLAY_LETTERBOX_HEIGHT - 40;
  uint32_t banner_x = DISPLAY_LETTERBOX_X0;

  UTIL_LCD_FillRect(banner_x, banner_y,
                    DISPLAY_LETTERBOX_WIDTH, 36, 0xC0FF0000);
  UTIL_LCD_SetTextColor(0xFFFFFFFF);
  UTIL_LCD_DisplayStringAt(
      banner_x + (DISPLAY_LETTERBOX_WIDTH - 16 * 12) / 2,
      banner_y + 10, (uint8_t *)"HAZARD ALERT", LEFT_MODE);
}

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
    CPULoad_IdleThread_Enter();
    tx_thread_relinquish(); /* Yield to higher priority threads */
    CPULoad_IdleThread_Exit();
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
  roi_info = CAM_GetNNCropROI_Display();

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
        UI_DrawDepthGrid(depth_grid);
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

void UI_Thread_Start(void) {
  UINT tx_status;

  if (g_ui_initialized) {
    return;
  }

  /* Initialize cycle counter for CPU load measurement */
  CPULoad_InitCycleCounter();

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

void UI_Update(void) {
  /* This function is kept for compatibility but is no longer needed */
  /* The UI thread handles all updates automatically */
  /* It's now a no-op as updates are handled by the unified thread */
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
