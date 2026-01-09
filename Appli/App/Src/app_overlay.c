/**
 ******************************************************************************
 * @file    app_overlay.c
 * @author  Long Liangmao
 * @brief   Detection overlay rendering thread and drawing functions implementation
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

#include "app_overlay.h"
#include "app_buffers.h"
#include "app_cam.h"
#include "app_config.h"
#include "app_error.h"
#include "app_lcd.h"
#include "app_postprocess.h"
#include "stm32_lcd.h"
#include "stm32n6xx_hal.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void clamp_point(int *x, int *y);
static void draw_detection(const od_pp_outBuffer_t *det);
static void overlay_thread_entry(ULONG arg);

/* ============================================================================
 * Configuration Constants
 * ============================================================================ */

/* Thread configurations */
#define OVERLAY_THREAD_STACK_SIZE 4096
#define OVERLAY_THREAD_PRIORITY 10 /* Lower than PP, same as UI */

/* Class names table - single class for person detection */
static const char *classes_table[] = {"person"};
#define NB_CLASSES (sizeof(classes_table) / sizeof(classes_table[0]))

/* Colors for bounding boxes (ARGB8888) */
#define NUMBER_COLORS 10
static const uint32_t colors[NUMBER_COLORS] = {
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

/* ============================================================================
 * Global State Variables
 * ============================================================================ */

/* Thread resources */
static struct {
  TX_THREAD thread;
  UCHAR stack[OVERLAY_THREAD_STACK_SIZE];
} overlay_ctx;

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

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
 */
static void draw_detection(const od_pp_outBuffer_t *det) {
  int xc, yc, x0, y0, x1, y1, w, h;
  uint32_t color;
  int class_idx;

  /* Convert normalized coordinates to pixel coordinates */
  xc = (int)(det->x_center * DISPLAY_LETTERBOX_WIDTH) + DISPLAY_LETTERBOX_X0;
  yc = (int)(det->y_center * DISPLAY_LETTERBOX_HEIGHT);
  w = (int)(det->width * DISPLAY_LETTERBOX_WIDTH);
  h = (int)(det->height * DISPLAY_LETTERBOX_HEIGHT);

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
  color = colors[class_idx % NUMBER_COLORS];

  /* Draw bounding box */
  UTIL_LCD_DrawRect(x0, y0, x1 - x0, y1 - y0, color);

  /* Draw class label */
  if (class_idx < (int)NB_CLASSES) {
    UTIL_LCD_DisplayStringAt(x0 + 2, y0 + 2,
                             (uint8_t *)classes_table[class_idx], LEFT_MODE);
  }

  /* Draw confidence */
  char conf_str[8];
  int conf_pct = (int)(det->conf * 100.0f + 0.5f);
  snprintf(conf_str, sizeof(conf_str), "%d%%", conf_pct);
  UTIL_LCD_DisplayStringAt(x1 - 40, y0 + 2, (uint8_t *)conf_str, LEFT_MODE);
}

/* ============================================================================
 * Thread Entry Points
 * ============================================================================ */

/**
 * @brief  Overlay drawing thread entry function
 *         Renders detection overlays on UI layer
 * @param  arg: Thread argument (unused)
 */
static void overlay_thread_entry(ULONG arg) {
  UNUSED(arg);

  detection_info_t local_info;
  uint8_t *ui_buffer;
  char stats_str[32];
  UINT status;

  while (1) {
    /* Wait for detection update */
    ULONG actual_flags;
    TX_EVENT_FLAGS_GROUP *update_flags = Postprocess_GetUpdateEventFlags();
    status = tx_event_flags_get(update_flags, 0x01, TX_OR_CLEAR,
                                &actual_flags, TX_WAIT_FOREVER);
    if (status != TX_SUCCESS) {
      continue;
    }

    /* Copy detection info under lock */
    Postprocess_Lock();
    local_info = *Postprocess_GetInfo();
    Postprocess_Unlock();

    /* Get back buffer for drawing */
    ui_buffer = Buffer_GetUIBackBuffer();
    if (ui_buffer == NULL) {
      continue;
    }

    /* Set layer buffer address */
    LCD_SetUILayerAddress(ui_buffer);

    /* Set drawing layer */
    UTIL_LCD_SetLayer(LCD_LAYER_1_UI);
    UTIL_LCD_SetFont(&Font16);
    UTIL_LCD_SetBackColor(0x00000000); /* Transparent */

    /* Clear overlay area - only the detection overlay region */
    /* Leave the diagnostic panel area (left side) untouched */
    UTIL_LCD_FillRect(DISPLAY_LETTERBOX_X0, 0,
                      DISPLAY_LETTERBOX_WIDTH, DISPLAY_LETTERBOX_HEIGHT,
                      0x00000000);

    /* Draw detection info at top */
    UTIL_LCD_SetTextColor(0xFFFFFFFF); /* White */

    snprintf(stats_str, sizeof(stats_str), "Objects: %d", (int)local_info.nb_detect);
    UTIL_LCD_DisplayStringAt(DISPLAY_LETTERBOX_X0 + 10, 10,
                             (uint8_t *)stats_str, LEFT_MODE);

    snprintf(stats_str, sizeof(stats_str), "Inf: %ums FPS: %.1f",
             (unsigned int)local_info.inference_ms,
             local_info.nn_period_ms > 0 ? 1000.0f / local_info.nn_period_ms : 0.0f);
    UTIL_LCD_DisplayStringAt(DISPLAY_LETTERBOX_X0 + 10, 30,
                             (uint8_t *)stats_str, LEFT_MODE);

    /* Draw bounding boxes */
    UTIL_LCD_SetTextColor(0xFF00FF00); /* Green for boxes */
    for (int i = 0; i < local_info.nb_detect; i++) {
      draw_detection(&local_info.detects[i]);
    }

    /* Draw NN crop ROI rectangle */
    int roi_x0, roi_y0, roi_x1, roi_y1;
    if (CAM_GetNNCropROI_Display(&roi_x0, &roi_y0, &roi_x1, &roi_y1)) {
      /* Draw rectangle in cyan to show NN crop region */
      UTIL_LCD_SetTextColor(0xFF00FFFF); /* Cyan */
      UTIL_LCD_DrawRect(DISPLAY_LETTERBOX_X0 + roi_x0, roi_y0,
                        roi_x1 - roi_x0, roi_y1 - roi_y0,
                        0xFF00FFFF);
    }

    /* Commit buffer to display */
    Buffer_SetUIDisplayIndex(Buffer_GetNextUIDisplayIndex());
    LCD_ReloadUILayer(ui_buffer);
  }
}

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

/**
 * @brief  Initialize overlay module (thread)
 * @param  memory_ptr: ThreadX memory pool (unused, static allocation)
 */
void Overlay_Thread_Init(VOID *memory_ptr) {
  UNUSED(memory_ptr);
  UINT status;

  /* Create overlay thread */
  status = tx_thread_create(&overlay_ctx.thread, "overlay",
                            overlay_thread_entry, 0,
                            overlay_ctx.stack, OVERLAY_THREAD_STACK_SIZE,
                            OVERLAY_THREAD_PRIORITY, OVERLAY_THREAD_PRIORITY,
                            TX_NO_TIME_SLICE, TX_AUTO_START);
  APP_REQUIRE(status == TX_SUCCESS);
}
