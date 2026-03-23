/**
 ******************************************************************************
 * @file    ui_internal.h
 * @author  Long Liangmao
 * @brief   Internal shared constants and declarations for UI modules
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

#ifndef UI_INTERNAL_H
#define UI_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cam.h"
#include "lcd_config.h"
#include "model_config.h"
#include "pp.h"
#include "tof.h"
#include <stdint.h>

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

#define DEPTH_FONT_HEIGHT 16 /* Font16.Height */
#define DEPTH_FONT_WIDTH  11 /* Font16.Width */

/* Detection overlay colors (ARGB8888) */
#define NUMBER_COLORS 10

/* Class names table -- from model config */
#define NB_CLASSES (sizeof(MDL_PP_CLASS_LABELS) / sizeof(MDL_PP_CLASS_LABELS[0]))

/* Line height calculation */
#define UI_LINE_HEIGHT (UI_FONT_HEIGHT + UI_LINE_SPACING)

/* Y for diagnostic panel text row index (0 = first line below top margin) */
#define UI_PANEL_LINE_Y(row) \
  ((uint16_t)(UI_TEXT_MARGIN_Y + (uint16_t)(row) * (uint16_t)UI_LINE_HEIGHT))

/* ============================================================================
 * Extern Data
 * ============================================================================ */

/** Detection color palette (defined in ui_overlay.c) */
extern const uint32_t detection_colors[NUMBER_COLORS];

/** Pre-computed line Y coordinates (defined in ui.c) */
extern const uint16_t g_line_y[];

/* ============================================================================
 * Forward Declarations — internal render functions shared across files
 * ============================================================================ */

/* ui_panel.c */
void UI_DrawPanelBackground(void);
void UI_DrawHeader(void);
void UI_DrawRuntimeSection(void);
void UI_DrawCPULoadSection(float cpu_load_pct);
void UI_DrawDetectionInfoSection(const detection_info_t *info);
void UI_DrawProximitySection(const tof_alert_t *alert);
void UI_DrawBuildOptions(void);
void UI_DrawModelNameBottomRight(void);

/* ui_overlay.c */
void UI_DrawDetectionOverlays(const detection_info_t *info,
                              const nn_crop_info_display_t *roi_info);

/* ui_depth.c */
void UI_DrawDepthGrid(const tof_depth_grid_t *grid,
                      const nn_crop_info_display_t *roi_info);
void UI_DrawProximityAlertBanner(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_INTERNAL_H */
