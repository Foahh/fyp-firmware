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
#include "app_buffers.h"
#include "app_cam.h"
#include "app_cpuload.h"
#include "app_error.h"
#include "app_lcd.h"
#include "app_lcd_config.h"
#include "app_postprocess.h"
#include "model_config.h"
#include "stm32_lcd.h"
#include "stm32n6570_discovery_lcd.h"
#include "stm32n6xx_hal.h"
#include "tx_user.h" /* For TX_TIMER_TICKS_PER_SECOND */
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

/* Colors (ARGB8888 format) */
#define UI_COLOR_BG     0xC0000000 /* Semi-transparent black */
#define UI_COLOR_TEXT   0xFF00FF00 /* Bright green (terminal style) */
#define UI_COLOR_LABEL  0xFF808080 /* Gray for labels */
#define UI_COLOR_VALUE  0xFFFFFFFF /* White for values */
#define UI_COLOR_BAR_BG 0xFF202020 /* Dark gray bar background */
#define UI_COLOR_BAR_FG 0xFF00CC00 /* Green bar fill */

/* Buffer sizes */
#define UI_TEXT_BUFFER_SIZE    64
#define UI_PROGRESS_BAR_HEIGHT 12

/* Update rate for periodic diagnostic updates */
#define UI_UPDATE_FPS 30

/* Calculate sleep ticks from FPS */
#define UI_UPDATE_SLEEP_TICKS (TX_TIMER_TICKS_PER_SECOND / UI_UPDATE_FPS)

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

/* Pre-computed line Y positions (avoids repeated macro expansion) */
static const uint16_t g_line_y[] = {
    UI_TEXT_MARGIN_Y + 0 * UI_LINE_HEIGHT,  /* [0]  Title */
    UI_TEXT_MARGIN_Y + 1 * UI_LINE_HEIGHT,  /* [1]  Model Name */
    UI_TEXT_MARGIN_Y + 2 * UI_LINE_HEIGHT,  /* [2]  Separator */
    UI_TEXT_MARGIN_Y + 3 * UI_LINE_HEIGHT,  /* [3]  Runtime Label */
    UI_TEXT_MARGIN_Y + 4 * UI_LINE_HEIGHT,  /* [4]  Runtime Value */
    UI_TEXT_MARGIN_Y + 5 * UI_LINE_HEIGHT,  /* [5]  CPU Label */
    UI_TEXT_MARGIN_Y + 6 * UI_LINE_HEIGHT,  /* [6]  CPU Value */
    UI_TEXT_MARGIN_Y + 7 * UI_LINE_HEIGHT,  /* [7]  CPU Bar */
    UI_TEXT_MARGIN_Y + 8 * UI_LINE_HEIGHT,  /* [8]  Objects Label */
    UI_TEXT_MARGIN_Y + 9 * UI_LINE_HEIGHT,  /* [9]  Objects Value */
    UI_TEXT_MARGIN_Y + 10 * UI_LINE_HEIGHT, /* [10] Inference Label */
    UI_TEXT_MARGIN_Y + 11 * UI_LINE_HEIGHT, /* [11] Inference Value */
    UI_TEXT_MARGIN_Y + 12 * UI_LINE_HEIGHT, /* [12] Postprocess Label */
    UI_TEXT_MARGIN_Y + 13 * UI_LINE_HEIGHT, /* [13] Postprocess Value */
    UI_TEXT_MARGIN_Y + 14 * UI_LINE_HEIGHT, /* [14] Overhead Label */
    UI_TEXT_MARGIN_Y + 15 * UI_LINE_HEIGHT, /* [15] Overhead Value */
    UI_TEXT_MARGIN_Y + 16 * UI_LINE_HEIGHT, /* [16] FPS Label */
    UI_TEXT_MARGIN_Y + 17 * UI_LINE_HEIGHT, /* [17] FPS Value */
    UI_TEXT_MARGIN_Y + 18 * UI_LINE_HEIGHT, /* [18] Drops Label */
    UI_TEXT_MARGIN_Y + 19 * UI_LINE_HEIGHT, /* [19] Drops Value */
};

/* ============================================================================
 * Global State Variables
 * ============================================================================ */

/* CPU load tracking */
static cpuload_info_t g_cpu_load;

/* UI state */
static uint8_t g_ui_visible = 1;
static uint8_t g_ui_initialized = 0;

/* Detection update tracking */
static volatile uint8_t g_detection_update_pending = 0;

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
 * @brief  Draw UI title and separator
 */
static void UI_DrawHeader(void) {
  UTIL_LCD_SetTextColor(UI_COLOR_TEXT);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[0],
                           (uint8_t *)"DIAGNOSTICS", LEFT_MODE);

  /* Model name */
  UTIL_LCD_SetTextColor(UI_COLOR_LABEL);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[1],
                           (uint8_t *)MDL_DISPLAY_NAME, LEFT_MODE);

  /* Separator line */
  UTIL_LCD_DrawHLine(UI_TEXT_MARGIN_X, g_line_y[2] + UI_LINE_HEIGHT / 4,
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
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[5],
                           (uint8_t *)"CPU Load", LEFT_MODE);

  /* Value */
  UTIL_LCD_SetTextColor(UI_COLOR_VALUE);
  UI_FormatCPULoad(text_buf, sizeof(text_buf), cpu_load_pct);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[6],
                           (uint8_t *)text_buf, LEFT_MODE);

  /* Progress bar */
  bar_width = UI_PANEL_WIDTH - 2 * UI_TEXT_MARGIN_X;
  UI_DrawProgressBar(UI_TEXT_MARGIN_X, g_line_y[7], bar_width,
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
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[3],
                           (uint8_t *)"Runtime", LEFT_MODE);

  /* Value */
  UTIL_LCD_SetTextColor(UI_COLOR_VALUE);
  tick = HAL_GetTick();
  UI_FormatRuntime(text_buf, sizeof(text_buf), tick);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[4],
                           (uint8_t *)text_buf, LEFT_MODE);
}

/**
 * @brief  Draw detection information section in diagnostic panel
 */
static void UI_DrawDetectionInfoSection(const detection_info_t *info) {
  char text_buf[UI_TEXT_BUFFER_SIZE];

  /* Objects count */
  UTIL_LCD_SetTextColor(UI_COLOR_LABEL);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[8],
                           (uint8_t *)"Objects", LEFT_MODE);
  UTIL_LCD_SetTextColor(UI_COLOR_VALUE);
  snprintf(text_buf, sizeof(text_buf), "%d", (int)info->nb_detect);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[9],
                           (uint8_t *)text_buf, LEFT_MODE);

  /* Inference time */
  UTIL_LCD_SetTextColor(UI_COLOR_LABEL);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[10],
                           (uint8_t *)"Inference", LEFT_MODE);
  UTIL_LCD_SetTextColor(UI_COLOR_VALUE);
  snprintf(text_buf, sizeof(text_buf), "%lums", (unsigned long)info->inference_ms);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[11],
                           (uint8_t *)text_buf, LEFT_MODE);

  /* Postprocess time */
  UTIL_LCD_SetTextColor(UI_COLOR_LABEL);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[12],
                           (uint8_t *)"Postprocess", LEFT_MODE);
  UTIL_LCD_SetTextColor(UI_COLOR_VALUE);
  snprintf(text_buf, sizeof(text_buf), "%lums", (unsigned long)info->postprocess_ms);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[13],
                           (uint8_t *)text_buf, LEFT_MODE);

  /* Overhead time */
  UTIL_LCD_SetTextColor(UI_COLOR_LABEL);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[14],
                           (uint8_t *)"Overhead", LEFT_MODE);
  UTIL_LCD_SetTextColor(UI_COLOR_VALUE);
  uint32_t overhead_ms = info->nn_period_ms > info->inference_ms ? info->nn_period_ms - info->inference_ms : 0;
  snprintf(text_buf, sizeof(text_buf), "%lums", (unsigned long)overhead_ms);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[15],
                           (uint8_t *)text_buf, LEFT_MODE);

  /* FPS */
  UTIL_LCD_SetTextColor(UI_COLOR_LABEL);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[16],
                           (uint8_t *)"FPS", LEFT_MODE);
  UTIL_LCD_SetTextColor(UI_COLOR_VALUE);
  float fps = info->nn_period_ms > 0 ? 1000.0f / info->nn_period_ms : 0.0f;
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
    det_info = Postprocess_GetInfo();

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

    /* Setup LCD context */
    LCD_SetUILayerAddress(ui_buffer);
    UI_SetupLCDContext();

    /* Render all UI elements */
    /* Draw diagnostic panel background (left side) */
    UI_DrawPanelBackground();

    /* Clear detection overlay area */
    UTIL_LCD_FillRect(DISPLAY_LETTERBOX_X0, 0,
                      DISPLAY_LETTERBOX_WIDTH, DISPLAY_LETTERBOX_HEIGHT,
                      0x00000000);

    /* Draw panel text (may extend into camera area) */
    UI_DrawHeader();
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
