/**
 ******************************************************************************
 * @file    app_ui.c
 * @author  Long Liangmao
 * @brief   Diagnostic UI overlay implementation
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
#include "app_error.h"
#include "app_lcd.h"
#include "stm32_lcd.h"
#include "stm32n6570_discovery_lcd.h"
#include "stm32n6xx_hal.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/* EMA smoothing factor (0.0 - 1.0)
 * Lower values = more smoothing, higher values = more responsive
 */
#define CPU_LOAD_EMA_ALPHA 0.2f

/**
 * @brief  CPU load measurement structure (internal)
 */
typedef struct {
  uint32_t last_total; /* Last total cycle count */
  uint32_t last_idle;  /* Last idle cycle count */
  uint32_t last_tick;  /* Last update timestamp (ms) */
  float cpu_load_ema;  /* Exponential moving average of CPU load (%) */
  uint8_t initialized; /* Flag to indicate first sample */
} cpuload_info_t;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void UI_Update(void);

/* ============================================================================
 * UI Configuration Constants
 * ============================================================================ */

/* Panel geometry - left side of screen */
#define UI_PANEL_X0 0
#define UI_PANEL_Y0 0
#define UI_PANEL_WIDTH 160  /* Width matches DISPLAY_LETTERBOX_X0 */
#define UI_PANEL_HEIGHT 240 /* Top half of LCD_HEIGHT */

/* Text layout */
#define UI_TEXT_MARGIN_X 8
#define UI_TEXT_MARGIN_Y 8
#define UI_LINE_SPACING 4
#define UI_FONT_HEIGHT 16 /* Cache Font16.Height to avoid struct access */

/* Colors (ARGB8888 format) */
#define UI_COLOR_BG 0xC0000000     /* Semi-transparent black */
#define UI_COLOR_TEXT 0xFF00FF00   /* Bright green (terminal style) */
#define UI_COLOR_LABEL 0xFF808080  /* Gray for labels */
#define UI_COLOR_VALUE 0xFFFFFFFF  /* White for values */
#define UI_COLOR_BAR_BG 0xFF202020 /* Dark gray bar background */
#define UI_COLOR_BAR_FG 0xFF00CC00 /* Green bar fill */

/* Buffer sizes */
#define UI_TEXT_BUFFER_SIZE 48
#define UI_PROGRESS_BAR_HEIGHT 12

/* ============================================================================
 * Pre-computed Layout Constants
 * ============================================================================ */

/* Line height calculation */
#define UI_LINE_HEIGHT (UI_FONT_HEIGHT + UI_LINE_SPACING)

/* Pre-computed line Y positions (avoids repeated macro expansion) */
static const uint16_t g_line_y[] = {
    UI_TEXT_MARGIN_Y + 0 * UI_LINE_HEIGHT, /* Line 0: Title */
    UI_TEXT_MARGIN_Y + 1 * UI_LINE_HEIGHT, /* Line 1: Separator */
    UI_TEXT_MARGIN_Y + 2 * UI_LINE_HEIGHT, /* Line 2: CPU Label */
    UI_TEXT_MARGIN_Y + 3 * UI_LINE_HEIGHT, /* Line 3: CPU Value */
    UI_TEXT_MARGIN_Y + 4 * UI_LINE_HEIGHT, /* Line 4: CPU Bar */
    UI_TEXT_MARGIN_Y + 6 * UI_LINE_HEIGHT, /* Line 6: Runtime Label */
    UI_TEXT_MARGIN_Y + 7 * UI_LINE_HEIGHT, /* Line 7: Runtime Value */
};

/* ============================================================================
 * Global State Variables
 * ============================================================================ */

/* CPU load tracking */
static cpuload_info_t g_cpu_load;

/* Idle time accumulator
 * Using volatile for basic thread-safety
 * For diagnostic display, occasional glitches are acceptable
 */
static volatile uint32_t g_idle_cycles_total = 0;
static volatile uint32_t g_idle_enter_cycle = 0;
static volatile uint8_t g_in_idle = 0;

/* UI state */
static uint8_t g_ui_visible = 1;
static uint8_t g_ui_initialized = 0;

/* ============================================================================
 * Thread Configuration
 * ============================================================================ */

/* UI update thread */
#define UI_THREAD_STACK_SIZE 2048
#define UI_THREAD_PRIORITY 10 /* Low priority for UI updates */

/* Idle measurement thread */
#define IDLE_THREAD_STACK_SIZE 512
#define IDLE_THREAD_PRIORITY 31 /* Lowest priority (runs when truly idle) */

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
 * DWT Cycle Counter Functions
 * ============================================================================ */

/* Internal inline version for hot paths */
#define GET_CYCLE_COUNT() (DWT->CYCCNT)

void UI_InitCycleCounter(void) {
  /* Enable DWT and cycle counter */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t UI_GetCycleCount(void) {
  return DWT->CYCCNT;
}

/* ============================================================================
 * Idle Time Measurement Functions
 * ============================================================================ */

void UI_IdleThread_Enter(void) {
  if (!g_in_idle) {
    g_idle_enter_cycle = GET_CYCLE_COUNT();
    g_in_idle = 1;
  }
}

void UI_IdleThread_Exit(void) {
  if (g_in_idle) {
    uint32_t exit_cycle = GET_CYCLE_COUNT();
    uint32_t elapsed = exit_cycle - g_idle_enter_cycle;
    g_idle_cycles_total += elapsed;
    g_in_idle = 0;
  }
}

/* ============================================================================
 * CPU Load Measurement Functions
 * ============================================================================ */

void UI_CPULoad_Init(cpuload_info_t *cpu_load) {
  memset(cpu_load, 0, sizeof(cpuload_info_t));
  g_idle_cycles_total = 0;
  cpu_load->cpu_load_ema = 0.0f;
  cpu_load->initialized = 0;
}

/**
 * @brief  Clamp CPU load value to valid range [0.0, 100.0]
 */
static float UI_ClampCPULoad(float load) {
  if (load < 0.0f) {
    return 0.0f;
  }
  if (load > 100.0f) {
    return 100.0f;
  }
  return load;
}

void UI_CPULoad_Update(cpuload_info_t *cpu_load) {
  uint32_t current_tick = HAL_GetTick();
  uint32_t current_total = GET_CYCLE_COUNT();
  uint32_t current_idle = g_idle_cycles_total;
  float instant_load = 0.0f;

  if (!cpu_load->initialized) {
    /* First sample: initialize values */
    cpu_load->last_total = current_total;
    cpu_load->last_idle = current_idle;
    cpu_load->last_tick = current_tick;
    cpu_load->cpu_load_ema = 0.0f;
    cpu_load->initialized = 1;
    return;
  }

  /* Calculate instant CPU load from delta */
  uint32_t total_delta = current_total - cpu_load->last_total;
  if (total_delta > 0) {
    uint32_t idle_delta = current_idle - cpu_load->last_idle;
    instant_load = 100.0f * (1.0f - (float)idle_delta / (float)total_delta);
    instant_load = UI_ClampCPULoad(instant_load);
  }

  /* Update EMA: new_ema = alpha * instant + (1 - alpha) * old_ema */
  cpu_load->cpu_load_ema = CPU_LOAD_EMA_ALPHA * instant_load +
                           (1.0f - CPU_LOAD_EMA_ALPHA) * cpu_load->cpu_load_ema;

  /* Update last values for next sample */
  cpu_load->last_total = current_total;
  cpu_load->last_idle = current_idle;
  cpu_load->last_tick = current_tick;
}

float UI_CPULoad_GetSmoothed(const cpuload_info_t *cpu_load) {
  if (!cpu_load->initialized) {
    return 0.0f;
  }
  return cpu_load->cpu_load_ema;
}

/* ============================================================================
 * UI Drawing Functions
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
 * @brief  Draw UI panel background (clear area)
 */
static void UI_DrawPanelBackground(void) {
  UTIL_LCD_FillRect(UI_PANEL_X0, UI_PANEL_Y0,
                    UI_PANEL_WIDTH, UI_PANEL_HEIGHT, 0x00000000);
}

/**
 * @brief  Draw UI title and separator
 */
static void UI_DrawHeader(void) {
  UTIL_LCD_SetTextColor(UI_COLOR_TEXT);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[0],
                           (uint8_t *)"DIAGNOSTICS", LEFT_MODE);

  /* Separator line */
  UTIL_LCD_DrawHLine(UI_TEXT_MARGIN_X, g_line_y[1],
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
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[2],
                           (uint8_t *)"CPU Load", LEFT_MODE);

  /* Value */
  UTIL_LCD_SetTextColor(UI_COLOR_VALUE);
  UI_FormatCPULoad(text_buf, sizeof(text_buf), cpu_load_pct);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[3],
                           (uint8_t *)text_buf, LEFT_MODE);

  /* Progress bar */
  bar_width = UI_PANEL_WIDTH - 2 * UI_TEXT_MARGIN_X;
  UI_DrawProgressBar(UI_TEXT_MARGIN_X, g_line_y[4], bar_width,
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
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[5],
                           (uint8_t *)"Runtime", LEFT_MODE);

  /* Value */
  UTIL_LCD_SetTextColor(UI_COLOR_VALUE);
  tick = HAL_GetTick();
  UI_FormatRuntime(text_buf, sizeof(text_buf), tick);
  UTIL_LCD_DisplayStringAt(UI_TEXT_MARGIN_X, g_line_y[6],
                           (uint8_t *)text_buf, LEFT_MODE);
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
    UI_IdleThread_Enter();
    tx_thread_relinquish(); /* Yield to higher priority threads */
    UI_IdleThread_Exit();
  }
}

/**
 * @brief  UI update thread entry
 *         Periodically updates the diagnostic overlay
 */
static void ui_thread_entry(ULONG arg) {
  UNUSED(arg);

  while (1) {
    UI_Update();
    tx_thread_sleep(10);
  }
}

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

void UI_Init(void) {
  UINT tx_status;

  if (g_ui_initialized) {
    return;
  }

  /* Initialize cycle counter for CPU load measurement */
  UI_InitCycleCounter();

  /* Initialize CPU load tracking */
  UI_CPULoad_Init(&g_cpu_load);

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

/**
 * @brief  Setup LCD context for UI rendering
 */
static void UI_SetupLCDContext(void) {
  UTIL_LCD_SetLayer(LCD_LAYER_1_UI);
  UTIL_LCD_SetFont(&Font16);
  UTIL_LCD_SetBackColor(0x00000000); /* Transparent background */
}

/**
 * @brief  Render all UI elements to the current buffer
 */
static void UI_RenderElements(float cpu_load_pct) {
  UI_DrawPanelBackground();
  UI_DrawHeader();
  UI_DrawCPULoadSection(cpu_load_pct);
  UI_DrawRuntimeSection();
}

void UI_Update(void) {
  float cpu_load_pct;
  uint8_t *ui_buffer;

  if (!g_ui_initialized || !g_ui_visible) {
    return;
  }

  /* Update CPU load measurement */
  UI_CPULoad_Update(&g_cpu_load);
  cpu_load_pct = UI_CPULoad_GetSmoothed(&g_cpu_load);

  /* Get UI back buffer */
  ui_buffer = Buffer_GetUIBackBuffer();
  if (ui_buffer == NULL) {
    return;
  }

  /* Setup LCD context */
  LCD_SetUILayerAddress(ui_buffer);
  UI_SetupLCDContext();

  /* Render all UI elements */
  UI_RenderElements(cpu_load_pct);

  /* Swap buffers and reload display */
  Buffer_SetUIDisplayIndex(Buffer_GetNextUIDisplayIndex());
  LCD_ReloadUILayer(ui_buffer);
}

void UI_SetVisible(uint8_t visible) {
  g_ui_visible = visible;

  if (!visible) {
    /* Clear UI layer when hiding */
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

uint8_t UI_IsVisible(void) {
  return g_ui_visible;
}
