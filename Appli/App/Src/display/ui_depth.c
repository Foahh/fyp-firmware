/**
 ******************************************************************************
 * @file    ui_depth.c
 * @author  Long Liangmao
 * @brief   ToF depth grid heatmap and proximity alert banner
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
#include "stm32_lcd.h"
#include "stm32n6xx_hal.h"
#include "tof.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * Static Helpers
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

/* ============================================================================
 * Public Functions
 * ============================================================================ */

/**
 * @brief  Draw 8x8 ToF depth heatmap with per-cell cm values.
 *         When `roi_info` is provided, the grid is mapped directly onto the
 *         NN inference display ROI to aid calibration.
 */
void UI_DrawDepthGrid(const tof_depth_grid_t *grid,
                      const nn_crop_info_display_t *roi_info) {
  if (grid == NULL || !grid->valid) {
    return;
  }

  /* Skip if data is stale (>1s) */
  uint32_t now = HAL_GetTick();
  if ((now - grid->timestamp_ms) > 1000) {
    return;
  }

  /* Map the ToF 8x8 grid onto the NN ROI (preferred) */
  uint32_t draw_x0, draw_y0, draw_w, draw_h;
  uint8_t use_roi = 0;

  if (roi_info != NULL && roi_info->roi_w > 0 && roi_info->roi_h > 0) {
    draw_x0 = DISPLAY_LETTERBOX_X0 + (uint32_t)roi_info->roi_x0;
    draw_y0 = (uint32_t)roi_info->roi_y0;
    draw_w = (uint32_t)roi_info->roi_w;
    draw_h = (uint32_t)roi_info->roi_h;
    use_roi = 1;
  } else {
    /* Fallback: keep the previous fixed placement on the camera view */
    draw_x0 = DEPTH_GRID_X0;
    draw_y0 = DEPTH_GRID_Y0;
    draw_w = DEPTH_GRID_PIXELS;
    draw_h = DEPTH_GRID_PIXELS;
  }

  /* Draw color-filled cells (cell boundaries computed to align to ROI) */
  for (int row = 0; row < TOF_GRID_SIZE; row++) {
    for (int col = 0; col < TOF_GRID_SIZE; col++) {
      uint32_t color = depth_to_color(grid->distance_mm[row][col],
                                      grid->status[row][col]);

      uint32_t x0 = draw_x0 + (uint32_t)(((uint64_t)col * draw_w) / TOF_GRID_SIZE);
      uint32_t x1 = draw_x0 + (uint32_t)(((uint64_t)(col + 1) * draw_w) / TOF_GRID_SIZE);
      uint32_t y0 = draw_y0 + (uint32_t)(((uint64_t)row * draw_h) / TOF_GRID_SIZE);
      uint32_t y1 = draw_y0 + (uint32_t)(((uint64_t)(row + 1) * draw_h) / TOF_GRID_SIZE);

      if (x1 > x0 && y1 > y0) {
        UTIL_LCD_FillRect(x0, y0, x1 - x0, y1 - y0, color);
      }
    }
  }

  /* Border around entire grid */
  UTIL_LCD_DrawRect(draw_x0, draw_y0, draw_w, draw_h, UI_COLOR_TEXT);

  /* Highlight center 2x2 zone with white border (calibration target) */
  for (int row = DEPTH_CENTER_ROW_MIN; row <= DEPTH_CENTER_ROW_MAX; row++) {
    for (int col = DEPTH_CENTER_COL_MIN; col <= DEPTH_CENTER_COL_MAX; col++) {
      uint32_t x0 = draw_x0 + (uint32_t)(((uint64_t)col * draw_w) / TOF_GRID_SIZE);
      uint32_t x1 = draw_x0 + (uint32_t)(((uint64_t)(col + 1) * draw_w) / TOF_GRID_SIZE);
      uint32_t y0 = draw_y0 + (uint32_t)(((uint64_t)row * draw_h) / TOF_GRID_SIZE);
      uint32_t y1 = draw_y0 + (uint32_t)(((uint64_t)(row + 1) * draw_h) / TOF_GRID_SIZE);

      if (x1 > x0 && y1 > y0) {
        UTIL_LCD_DrawRect(x0, y0, x1 - x0, y1 - y0, 0xFFFFFFFF);
      }
    }
  }

  /* Overlay per-cell distance values in cm using larger centered text */
  UTIL_LCD_SetFont(&Font16);
  for (int row = 0; row < TOF_GRID_SIZE; row++) {
    for (int col = 0; col < TOF_GRID_SIZE; col++) {
      int16_t dist = grid->distance_mm[row][col];
      uint8_t stat = grid->status[row][col];
      uint32_t x0 = draw_x0 + (uint32_t)(((uint64_t)col * draw_w) / TOF_GRID_SIZE);
      uint32_t x1 = draw_x0 + (uint32_t)(((uint64_t)(col + 1) * draw_w) / TOF_GRID_SIZE);
      uint32_t y0 = draw_y0 + (uint32_t)(((uint64_t)row * draw_h) / TOF_GRID_SIZE);
      uint32_t y1 = draw_y0 + (uint32_t)(((uint64_t)(row + 1) * draw_h) / TOF_GRID_SIZE);

      uint32_t cell_w = (x1 > x0) ? (x1 - x0) : 0;
      uint32_t cell_h = (y1 > y0) ? (y1 - y0) : 0;

      /* Values are fixed to 3 chars ("---", "FAR", or "%3d"). */
      uint32_t text_w = 3U * DEPTH_FONT_WIDTH;

      /* Only draw text when the cell is large enough. */
      if (cell_w < text_w || cell_h < DEPTH_FONT_HEIGHT) {
        continue;
      }

      uint32_t x = x0 + (cell_w - text_w) / 2U;
      uint32_t y = y0 + (cell_h - DEPTH_FONT_HEIGHT) / 2U;

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

  UNUSED(use_roi);
}

/**
 * @brief  Draw hazard proximity alert banner on the camera view
 */
void UI_DrawProximityAlertBanner(void) {
  uint32_t banner_y = DISPLAY_LETTERBOX_HEIGHT - 40;
  uint32_t banner_x = DISPLAY_LETTERBOX_X0;

  UTIL_LCD_FillRect(banner_x, banner_y,
                    DISPLAY_LETTERBOX_WIDTH, 36, 0xC0FF0000);
  UTIL_LCD_SetTextColor(0xFFFFFFFF);
  UTIL_LCD_DisplayStringAt(
      banner_x + (DISPLAY_LETTERBOX_WIDTH - 16 * 12) / 2,
      banner_y + 10, (uint8_t *)"HAZARD ALERT", LEFT_MODE);
}
