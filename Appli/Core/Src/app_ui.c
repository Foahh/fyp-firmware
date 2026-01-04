/**
 ******************************************************************************
 * @file    app_ui.c
 * @author  Long Liangmao
 * @brief   Diagnostic UI overlay implementation for STM32N6570-DK
 *          Displays CPU load by measuring idle thread execution time
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
#include "app_config.h"
#include "app_lcd.h"
#include "stm32_lcd.h"
#include "stm32n6570_discovery_lcd.h"
#include "stm32n6xx_hal.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* UI panel configuration - left side of screen */
#define UI_PANEL_X0 0
#define UI_PANEL_Y0 0
#define UI_PANEL_WIDTH 160  /* Width matches DISPLAY_LETTERBOX_X0 */
#define UI_PANEL_HEIGHT 240 /* Top half of LCD_HEIGHT */

/* Text layout */
#define UI_TEXT_MARGIN_X 8
#define UI_TEXT_MARGIN_Y 8
#define UI_LINE_SPACING 4
#define UI_FONT_HEIGHT 16 /* Cache Font16.Height to avoid struct access */

/* Colors (ARGB8888) */
#define UI_COLOR_BG 0xC0000000     /* Semi-transparent black */
#define UI_COLOR_TEXT 0xFF00FF00   /* Bright green (terminal style) */
#define UI_COLOR_LABEL 0xFF808080  /* Gray for labels */
#define UI_COLOR_VALUE 0xFFFFFFFF  /* White for values */
#define UI_COLOR_BAR_BG 0xFF202020 /* Dark gray bar background */
#define UI_COLOR_BAR_FG 0xFF00CC00 /* Green bar fill */

/* Maximum text buffer size */
#define UI_TEXT_BUFFER_SIZE 48

/* Pre-computed line Y positions (avoids repeated macro expansion) */
#define UI_LINE_HEIGHT (UI_FONT_HEIGHT + UI_LINE_SPACING)
static const uint16_t g_line_y[] = {
    UI_TEXT_MARGIN_Y + 0 * UI_LINE_HEIGHT,  /* Line 0: Title */
    UI_TEXT_MARGIN_Y + 1 * UI_LINE_HEIGHT,  /* Line 1: Separator */
    UI_TEXT_MARGIN_Y + 2 * UI_LINE_HEIGHT,  /* Line 2: CPU Label */
    UI_TEXT_MARGIN_Y + 3 * UI_LINE_HEIGHT,  /* Line 3: CPU Value */
    UI_TEXT_MARGIN_Y + 4 * UI_LINE_HEIGHT,  /* Line 4: CPU Bar */
    UI_TEXT_MARGIN_Y + 6 * UI_LINE_HEIGHT,  /* Line 6: Runtime Label */
    UI_TEXT_MARGIN_Y + 7 * UI_LINE_HEIGHT,  /* Line 7: Runtime Value */
};

/* Global CPU load tracker */
static cpuload_info_t g_cpu_load;

/* Idle time accumulator (updated from idle thread hooks) */
static volatile uint32_t g_idle_cycles_total = 0;
static volatile uint32_t g_idle_enter_cycle = 0;
static volatile uint8_t g_in_idle = 0;

/* UI state */
static uint8_t g_ui_visible = 1;
static uint8_t g_ui_initialized = 0;

/* Cached last history update tick to avoid unnecessary shifts */
static uint32_t g_last_history_update_tick = 0;

/**
 * @brief  Initialize DWT cycle counter for profiling
 */
void UI_InitCycleCounter(void) {
  /* Enable DWT and cycle counter */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/**
 * @brief  Get current DWT cycle counter value
 * @retval Current cycle count
 */
uint32_t UI_GetCycleCount(void) {
  return DWT->CYCCNT;
}

/* Internal inline version for hot paths */
#define GET_CYCLE_COUNT() (DWT->CYCCNT)

/**
 * @brief  Idle thread entry hook
 */
void UI_IdleThread_Enter(void) {
  if (!g_in_idle) {
    g_idle_enter_cycle = GET_CYCLE_COUNT();
    g_in_idle = 1;
  }
}

/**
 * @brief  Idle thread exit hook
 */
void UI_IdleThread_Exit(void) {
  if (g_in_idle) {
    uint32_t exit_cycle = GET_CYCLE_COUNT();
    uint32_t elapsed = exit_cycle - g_idle_enter_cycle;
    g_idle_cycles_total += elapsed;
    g_in_idle = 0;
  }
}

/**
 * @brief  Initialize CPU load tracking
 */
void UI_CPULoad_Init(cpuload_info_t *cpu_load) {
  memset(cpu_load, 0, sizeof(cpuload_info_t));
  g_idle_cycles_total = 0;
  g_last_history_update_tick = 0;
}

/**
 * @brief  Update CPU load measurement
 */
void UI_CPULoad_Update(cpuload_info_t *cpu_load) {
  uint32_t current_tick = HAL_GetTick();
  uint32_t current_total = GET_CYCLE_COUNT();
  uint32_t current_idle = g_idle_cycles_total;

  cpu_load->history[1] = cpu_load->history[0];
  cpu_load->history[0].total = current_total;
  cpu_load->history[0].idle = current_idle;
  cpu_load->history[0].tick = current_tick;

  if (current_tick - g_last_history_update_tick >= 1000) {
    g_last_history_update_tick = current_tick;
    /* Shift history[2..6] -> history[3..7] using memmove */
    memmove(&cpu_load->history[3], &cpu_load->history[2],
            (CPU_LOAD_HISTORY_DEPTH - 3) * sizeof(cpu_load->history[0]));
    cpu_load->history[2] = cpu_load->history[1];
  }
}

/**
 * @brief  Get instantaneous CPU load percentage
 * @param  cpu_load: Pointer to CPU load info structure
 * @retval CPU load percentage (0.0 - 100.0)
 */
__STATIC_FORCEINLINE float UI_CPULoad_GetInstant(const cpuload_info_t *cpu_load) {
  uint32_t total_delta = cpu_load->history[0].total - cpu_load->history[1].total;
  if (total_delta == 0) {
    return 0.0f;
  }
  uint32_t idle_delta = cpu_load->history[0].idle - cpu_load->history[1].idle;
  return 100.0f * (1.0f - (float)idle_delta / (float)total_delta);
}

/**
 * @brief  Get CPU load percentages
 */
void UI_CPULoad_GetInfo(cpuload_info_t *cpu_load,
                        float *cpu_load_last,
                        float *cpu_load_last_second,
                        float *cpu_load_last_five_seconds) {
  uint32_t total_delta, idle_delta;

  if (cpu_load_last) {
    *cpu_load_last = UI_CPULoad_GetInstant(cpu_load);
  }

  if (cpu_load_last_second) {
    total_delta = cpu_load->history[2].total - cpu_load->history[3].total;
    if (total_delta > 0) {
      idle_delta = cpu_load->history[2].idle - cpu_load->history[3].idle;
      *cpu_load_last_second = 100.0f * (1.0f - (float)idle_delta / (float)total_delta);
    } else {
      *cpu_load_last_second = 0.0f;
    }
  }

  if (cpu_load_last_five_seconds) {
    total_delta = cpu_load->history[2].total - cpu_load->history[7].total;
    if (total_delta > 0) {
      idle_delta = cpu_load->history[2].idle - cpu_load->history[7].idle;
      *cpu_load_last_five_seconds = 100.0f * (1.0f - (float)idle_delta / (float)total_delta);
    } else {
      *cpu_load_last_five_seconds = 0.0f;
    }
  }
}

/**
 * @brief  Fast integer-to-string for runtime display
 * @param  buf: Output buffer (must be at least 16 bytes)
 * @param  min: Minutes value
 * @param  sec: Seconds value (0-59)
 * @retval Pointer to formatted string
 */
static char *UI_FormatRuntime(char *buf, uint32_t min, uint32_t sec) {
  char *p = buf;
  /* Format minutes */
  if (min >= 100) {
    *p++ = '0' + (min / 100);
    min %= 100;
    *p++ = '0' + (min / 10);
    *p++ = '0' + (min % 10);
  } else if (min >= 10) {
    *p++ = '0' + (min / 10);
    *p++ = '0' + (min % 10);
  } else {
    *p++ = '0' + min;
  }
  *p++ = ':';
  /* Format seconds (always 2 digits) */
  *p++ = '0' + (sec / 10);
  *p++ = '0' + (sec % 10);
  *p = '\0';
  return buf;
}

/**
 * @brief  Fast float-to-string for percentage
 * @param  buf: Output buffer (must be at least 8 bytes)
 * @param  value: Percentage value (0.0 - 100.0)
 * @retval Pointer to formatted string
 */
static char *UI_FormatPercent(char *buf, float value) {
  char *p = buf;
  int integer_part, decimal_part;

  /* Clamp value */
  if (value < 0.0f)
    value = 0.0f;
  if (value > 100.0f)
    value = 100.0f;

  integer_part = (int)value;
  decimal_part = (int)((value - integer_part) * 10.0f + 0.5f); /* Round to 1 decimal */

  /* Handle rounding overflow */
  if (decimal_part >= 10) {
    decimal_part = 0;
    integer_part++;
  }

  /* Format integer part */
  if (integer_part >= 100) {
    *p++ = '1';
    *p++ = '0';
    *p++ = '0';
  } else if (integer_part >= 10) {
    *p++ = '0' + (integer_part / 10);
    *p++ = '0' + (integer_part % 10);
  } else {
    *p++ = '0' + integer_part;
  }

  *p++ = '.';
  *p++ = '0' + decimal_part;
  *p++ = '%';
  *p = '\0';
  return buf;
}

/**
 * @brief  Draw a horizontal progress bar
 */
static void UI_DrawProgressBar(uint32_t x, uint32_t y,
                               uint32_t width, uint32_t height,
                               float percentage) {
  uint32_t fill_width;

  /* Clamp percentage */
  if (percentage < 0.0f)
    percentage = 0.0f;
  if (percentage > 100.0f)
    percentage = 100.0f;

  fill_width = (uint32_t)((width - 2) * percentage * 0.01f);

  /* Draw background (includes border area) */
  UTIL_LCD_FillRect(x, y, width, height, UI_COLOR_BAR_BG);

  /* Draw fill */
  if (fill_width > 0) {
    UTIL_LCD_FillRect(x + 1, y + 1, fill_width, height - 2, UI_COLOR_BAR_FG);
  }

  /* Draw border */
  UTIL_LCD_DrawRect(x, y, width, height, UI_COLOR_TEXT);
}

/**
 * @brief  Initialize the UI diagnostic display
 */
void UI_Init(void) {
  if (g_ui_initialized) {
    return;
  }

  /* Initialize cycle counter for accurate timing */
  UI_InitCycleCounter();

  /* Initialize CPU load tracker */
  UI_CPULoad_Init(&g_cpu_load);

  g_ui_initialized = 1;
}

/**
 * @brief  Update and render diagnostic display (optimized)
 */
void UI_Update(void) {
  float cpu_load_pct;
  uint8_t *ui_buffer;
  char text_buf[16];
  uint32_t tick, sec, min;
  uint32_t bar_width;

  if (!g_ui_initialized || !g_ui_visible) {
    return;
  }

  /* Update CPU load measurement */
  UI_CPULoad_Update(&g_cpu_load);
  cpu_load_pct = UI_CPULoad_GetInstant(&g_cpu_load);

  /* Get back buffer for drawing (double buffering) */
  ui_buffer = Buffer_GetUIBackBuffer();
  if (ui_buffer == NULL) {
    return;
  }

  /* Set layer buffer address to back buffer for drawing */
  LCD_SetUILayerAddress(ui_buffer);

  /* Set active layer to UI layer (Layer 1) */
  UTIL_LCD_SetLayer(LCD_LAYER_1_UI);

  /* Configure LCD drawing context once */
  UTIL_LCD_SetFont(&Font16);
  UTIL_LCD_SetBackColor(0x00000000); /* Transparent background */

  /* Clear panel area to fully transparent */
  UTIL_LCD_FillRect(UI_PANEL_X0, UI_PANEL_Y0,
                    UI_PANEL_WIDTH, UI_PANEL_HEIGHT, 0x00000000);

  /* --- Draw UI elements with minimized state changes --- */

  /* Green text group: Title */
  UTIL_LCD_SetTextColor(UI_COLOR_TEXT);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[0],
                           (uint8_t *)"DIAGNOSTICS", LEFT_MODE);

  /* Separator line (same color) */
  UTIL_LCD_DrawHLine(UI_TEXT_MARGIN_X, g_line_y[1],
                     UI_PANEL_WIDTH - 2 * UI_TEXT_MARGIN_X, UI_COLOR_TEXT);

  /* Gray label group */
  UTIL_LCD_SetTextColor(UI_COLOR_LABEL);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[2],
                           (uint8_t *)"CPU Load", LEFT_MODE);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[5],
                           (uint8_t *)"Runtime", LEFT_MODE);

  /* White value group */
  UTIL_LCD_SetTextColor(UI_COLOR_VALUE);

  /* CPU load value */
  UI_FormatPercent(text_buf, cpu_load_pct);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[3],
                           (uint8_t *)text_buf, LEFT_MODE);

  /* Runtime value */
  tick = HAL_GetTick();
  sec = tick / 1000;
  min = sec / 60;
  sec = sec % 60;
  UI_FormatRuntime(text_buf, min, sec);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[6],
                           (uint8_t *)text_buf, LEFT_MODE);

  /* CPU load bar */
  bar_width = UI_PANEL_WIDTH - 2 * UI_TEXT_MARGIN_X;
  UI_DrawProgressBar(UI_TEXT_MARGIN_X, g_line_y[4], bar_width, 12, cpu_load_pct);

  Buffer_SetUIDisplayIndex(Buffer_GetNextUIDisplayIndex());
  LCD_ReloadUILayer(ui_buffer);
}

/**
 * @brief  Show/hide the diagnostic overlay
 */
void UI_SetVisible(uint8_t visible) {
  g_ui_visible = visible;

  if (!visible) {
    uint8_t *ui_buffer = Buffer_GetUIBackBuffer();
    if (ui_buffer != NULL) {
      LCD_SetUILayerAddress(ui_buffer);
      memset(ui_buffer, 0, LCD_WIDTH * LCD_HEIGHT * 4);
      SCB_CleanDCache_by_Addr((void *)ui_buffer, LCD_WIDTH * LCD_HEIGHT * 4);
      Buffer_SetUIDisplayIndex(Buffer_GetNextUIDisplayIndex());
      LCD_ReloadUILayer(ui_buffer);
    }
  }
}

/**
 * @brief  Check if diagnostic overlay is visible
 */
uint8_t UI_IsVisible(void) {
  return g_ui_visible;
}
