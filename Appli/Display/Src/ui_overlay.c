/**
 ******************************************************************************
 * @file    ui_overlay.c
 * @author  Long Liangmao
 * @brief   Detection bounding box and ROI rectangle overlays
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

#include "lcd_config.h"
#include "model_config.h"
#include "pp.h"
#include "stm32_lcd.h"
#include "ui.h"
#include <stdio.h>

/* ============================================================================
 * Detection Color Palette
 * ============================================================================ */

const uint32_t detection_colors[NUMBER_COLORS] = {
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
 * Static Helpers
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

  /* Skip if detection center is outside ROI bounds */
  if (det->x_center < 0.0f || det->x_center > 1.0f ||
      det->y_center < 0.0f || det->y_center > 1.0f) {
    return;
  }

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

/**
 * @brief  Draw a single tracked bounding box (Kalman-smoothed, normalized coords)
 * @param  tb: Tracked box (normalized center/size, track id)
 * @param  roi_x0: ROI top-left X
 * @param  roi_y0: ROI top-left Y
 * @param  roi_w: ROI width
 * @param  roi_h: ROI height
 */
static void draw_tracked(const tracked_box_t *tb, int roi_x0, int roi_y0,
                         int roi_w, int roi_h) {
  int xc, yc, x0, y0, x1, y1, w, h;
  uint32_t color;
  char id_str[16];

  /* Skip coasting tracks whose predicted center has left the ROI */
  if (tb->x_center < 0.0f || tb->x_center > 1.0f ||
      tb->y_center < 0.0f || tb->y_center > 1.0f) {
    return;
  }

  xc = (int)(tb->x_center * roi_w) + roi_x0 + DISPLAY_LETTERBOX_X0;
  yc = (int)(tb->y_center * roi_h) + roi_y0;
  w = (int)(tb->width * roi_w);
  h = (int)(tb->height * roi_h);

  x0 = xc - (w + 1) / 2;
  y0 = yc - (h + 1) / 2;
  x1 = xc + (w + 1) / 2;
  y1 = yc + (h + 1) / 2;

  x0 -= DISPLAY_LETTERBOX_X0;
  x1 -= DISPLAY_LETTERBOX_X0;

  clamp_point(&x0, &y0);
  clamp_point(&x1, &y1);

  x0 += DISPLAY_LETTERBOX_X0;
  x1 += DISPLAY_LETTERBOX_X0;

  color = detection_colors[tb->id % NUMBER_COLORS];

  UTIL_LCD_DrawRect(x0, y0, x1 - x0, y1 - y0, color);

  UTIL_LCD_SetTextColor(color);
  snprintf(id_str, sizeof(id_str), "%u", (unsigned)tb->id);
  UTIL_LCD_DisplayStringAt(x0 + 2, y0 + 2, (uint8_t *)id_str, LEFT_MODE);
}

/* ============================================================================
 * Public Functions
 * ============================================================================ */

/**
 * @brief  Draw detection overlays (bounding boxes and ROI rectangle)
 */
void UI_DrawDetectionOverlays(const detection_info_t *info,
                              const nn_crop_info_display_t *roi_info) {
  int32_t n_tracked = info->nb_tracked;
  if (n_tracked < 0) {
    n_tracked = 0;
  }
  if (n_tracked > (int32_t)DETECTION_MAX_BOXES) {
    n_tracked = (int32_t)DETECTION_MAX_BOXES;
  }

  if (n_tracked > 0) {
    for (int32_t i = 0; i < n_tracked; i++) {
      draw_tracked(&info->tracked[i], roi_info->roi_x0, roi_info->roi_y0,
                   roi_info->roi_w, roi_info->roi_h);
    }
  } else {
    UTIL_LCD_SetTextColor(0xFF00FF00); /* Green for boxes */
    for (int i = 0; i < info->nb_detect; i++) {
      draw_detection(&info->detects[i], roi_info->roi_x0, roi_info->roi_y0,
                     roi_info->roi_w, roi_info->roi_h);
    }
  }

  /* Draw NN crop ROI rectangle */
  UTIL_LCD_SetTextColor(0xFF00FFFF); /* Cyan */
  UTIL_LCD_DrawRect(DISPLAY_LETTERBOX_X0 + roi_info->roi_x0, roi_info->roi_y0,
                    roi_info->roi_w, roi_info->roi_h,
                    0xFF00FFFF);
}
