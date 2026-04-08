/**
 ******************************************************************************
 * @file    ui_depth.c
 * @author  Long Liangmao
 * @brief   ToF depth grid heatmap and person-distance alert banner
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
 * @brief  Check whether a ToF cell contains a usable measurement.
 */
static uint8_t tof_cell_valid(int16_t distance_mm, uint8_t status) {
  return ((status == 5 || status == 9) && distance_mm > 0) ? 1U : 0U;
}

/**
 * @brief  Map a distance measurement to an ARGB8888 color.
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
 * @brief  Map a sigma measurement to an ARGB8888 color.
 */
static uint32_t sigma_to_color(uint16_t sigma_mm, uint8_t cell_valid) {
  if (!cell_valid || sigma_mm == 0U) {
    return 0x60404040;
  }
  if (sigma_mm < 10U) {
    return 0xC000CC44; /* Strong green — low uncertainty */
  }
  if (sigma_mm < 20U) {
    return 0xC088DD00; /* Lime */
  }
  if (sigma_mm < 30U) {
    return 0xC0FFCC00; /* Yellow */
  }
  if (sigma_mm < 50U) {
    return 0xC0FF6600; /* Orange */
  }
  return 0xC0FF0000; /* Red — noisy */
}

/**
 * @brief  Map a return signal measurement to an ARGB8888 color.
 */
static uint32_t signal_to_color(uint32_t signal_per_spad, uint8_t cell_valid) {
  if (!cell_valid) {
    return 0x60404040;
  }
  if (signal_per_spad < 10U) {
    return 0xC0800000; /* Dark red — very weak */
  }
  if (signal_per_spad < 30U) {
    return 0xC0CC3300; /* Red/orange */
  }
  if (signal_per_spad < 60U) {
    return 0xC0FF9900; /* Orange */
  }
  if (signal_per_spad < 120U) {
    return 0xC0FFCC00; /* Yellow */
  }
  if (signal_per_spad < 240U) {
    return 0xC000CC66; /* Green */
  }
  return 0xC000CCFF; /* Cyan — strong return */
}

/**
 * @brief  Resolve the current overlay mode name and units.
 */
static void tof_mode_label(ui_tof_overlay_mode_t mode,
                           const char **title,
                           const char **units) {
  switch (mode) {
  case UI_TOF_OVERLAY_DISTANCE:
    *title = "ToF DIST";
    *units = "cm";
    break;
  case UI_TOF_OVERLAY_SIGMA:
    *title = "ToF SIGMA";
    *units = "mm";
    break;
  case UI_TOF_OVERLAY_SIGNAL_PER_SPAD:
    *title = "ToF PER/SPAD";
    *units = "kcps";
    break;
  case UI_TOF_OVERLAY_NONE:
  case UI_TOF_OVERLAY_MODE_COUNT:
  default:
    *title = "ToF";
    *units = "";
    break;
  }
}

/**
 * @brief  Format the current overlay mode's cell text.
 */
static uint32_t tof_mode_text(const tof_depth_grid_t *grid,
                              int row,
                              int col,
                              ui_tof_overlay_mode_t mode,
                              char *val_str,
                              size_t val_str_size) {
  uint8_t cell_valid =
      tof_cell_valid(grid->distance_mm[row][col], grid->status[row][col]);

  if (!cell_valid) {
    strncpy(val_str, "---", val_str_size);
    return 0xFF606060;
  }

  switch (mode) {
  case UI_TOF_OVERLAY_DISTANCE: {
    int cm = grid->distance_mm[row][col] / 10;
    if (cm > 999) {
      strncpy(val_str, "FAR", val_str_size);
    } else {
      snprintf(val_str, val_str_size, "%3d", cm);
    }
    return 0xFFFFFFFF;
  }
  case UI_TOF_OVERLAY_SIGMA: {
    uint32_t sigma_mm = grid->range_sigma_mm[row][col];
    if (sigma_mm == 0U) {
      strncpy(val_str, "---", val_str_size);
      return 0xFF606060;
    }
    if (sigma_mm > 999U) {
      strncpy(val_str, "999", val_str_size);
    } else {
      snprintf(val_str, val_str_size, "%3lu", (unsigned long)sigma_mm);
    }
    return 0xFFFFFFFF;
  }
  case UI_TOF_OVERLAY_SIGNAL_PER_SPAD: {
    uint32_t signal = grid->signal_per_spad[row][col];
    if (signal > 999U) {
      strncpy(val_str, "1k+", val_str_size);
    } else {
      snprintf(val_str, val_str_size, "%3lu", (unsigned long)signal);
    }
    return 0xFFFFFFFF;
  }
  case UI_TOF_OVERLAY_NONE:
  case UI_TOF_OVERLAY_MODE_COUNT:
  default:
    strncpy(val_str, "---", val_str_size);
    return 0xFF606060;
  }
}

/**
 * @brief  Select the fill color for the current overlay mode.
 */
static uint32_t tof_mode_color(const tof_depth_grid_t *grid,
                               int row,
                               int col,
                               ui_tof_overlay_mode_t mode) {
  uint8_t cell_valid =
      tof_cell_valid(grid->distance_mm[row][col], grid->status[row][col]);

  switch (mode) {
  case UI_TOF_OVERLAY_DISTANCE:
    return depth_to_color(grid->distance_mm[row][col], grid->status[row][col]);
  case UI_TOF_OVERLAY_SIGMA:
    return sigma_to_color(grid->range_sigma_mm[row][col], cell_valid);
  case UI_TOF_OVERLAY_SIGNAL_PER_SPAD:
    return signal_to_color(grid->signal_per_spad[row][col], cell_valid);
  case UI_TOF_OVERLAY_NONE:
  case UI_TOF_OVERLAY_MODE_COUNT:
  default:
    return 0x60404040;
  }
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
                      const nn_crop_info_display_t *roi_info,
                      ui_tof_overlay_mode_t mode) {
  if (grid == NULL || !grid->valid || mode == UI_TOF_OVERLAY_NONE) {
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
      uint32_t color = tof_mode_color(grid, row, col, mode);

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

  {
    const char *title;
    const char *units;
    tof_mode_label(mode, &title, &units);

    char label[24];
    if (units[0] != '\0') {
      snprintf(label, sizeof(label), "%s (%s)", title, units);
    } else {
      snprintf(label, sizeof(label), "%s", title);
    }

    uint32_t label_w = (uint32_t)strlen(label) * UI_FONT_WIDTH + 8U;
    uint32_t label_h = UI_FONT_HEIGHT + 4U;
    uint32_t label_x = draw_x0;
    uint32_t label_y = (draw_y0 > label_h + 4U) ? (draw_y0 - label_h - 4U)
                                                : (draw_y0 + 4U);

    UTIL_LCD_FillRect(label_x, label_y, label_w, label_h, 0xA0000000);
    UTIL_LCD_SetTextColor(UI_COLOR_TEXT);
    UTIL_LCD_DisplayStringAt(label_x + 4U, label_y + 2U, (uint8_t *)label,
                             LEFT_MODE);
  }

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
      UTIL_LCD_SetTextColor(
          tof_mode_text(grid, row, col, mode, val_str, sizeof(val_str)));
      UTIL_LCD_DisplayStringAt(x, y, (uint8_t *)val_str, LEFT_MODE);
    }
  }
  UTIL_LCD_SetFont(&Font16);

  UNUSED(use_roi);
}

/**
 * @brief  Draw person-distance alert banner on the camera view
 */
void UI_DrawProximityAlertBanner(void) {
  uint32_t banner_y = DISPLAY_LETTERBOX_HEIGHT - 40;
  uint32_t banner_x = DISPLAY_LETTERBOX_X0;

  UTIL_LCD_FillRect(banner_x, banner_y,
                    DISPLAY_LETTERBOX_WIDTH, 36, 0xC0FF0000);
  UTIL_LCD_SetTextColor(0xFFFFFFFF);
  UTIL_LCD_DisplayStringAt(
      banner_x + (DISPLAY_LETTERBOX_WIDTH - 16 * 17) / 2,
      banner_y + 10, (uint8_t *)"PERSON TOO CLOSE", LEFT_MODE);
}
