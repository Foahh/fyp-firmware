/**
 ******************************************************************************
 * @file    app_events.h
 * @author  Long Liangmao
 * @brief   Application-specific event definitions for event bus
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

#ifndef APP_EVENTS_H
#define APP_EVENTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "app_event_bus.h"
#include <stdint.h>

/* Event data structures (packed into event_bus_data_t) */

/**
 * @brief  DCMIPP frame ready event data
 */
typedef struct {
  uint8_t *frame_buffer;  /* Pointer to frame buffer (8 bytes on 64-bit, 4 bytes on 32-bit) */
  uint32_t pipe;          /* DCMIPP pipe number */
  uint32_t reserved;      /* Reserved for future use */
} event_dcmipp_frame_ready_t;

/**
 * @brief  LTDC layer reload event data
 */
typedef struct {
  uint8_t *frame_buffer;  /* Pointer to frame buffer */
  uint32_t layer;         /* Layer index */
  uint32_t reserved;      /* Reserved for future use */
} event_ltdc_layer_reload_t;

/**
 * @brief  UI update event data
 */
typedef struct {
  uint32_t flags;         /* Update flags (reserved for future use) */
  uint32_t reserved[3];   /* Reserved for future use */
} event_ui_update_t;

/* Helper functions to pack/unpack event data */

/**
 * @brief  Pack DCMIPP frame ready event data
 */
static inline void app_event_pack_dcmipp_frame_ready(event_bus_data_t *data,
                                                      uint8_t *frame_buffer,
                                                      uint32_t pipe) {
  event_dcmipp_frame_ready_t *evt = (event_dcmipp_frame_ready_t *)data->data;
  evt->frame_buffer = frame_buffer;
  evt->pipe = pipe;
  evt->reserved = 0;
}

/**
 * @brief  Unpack DCMIPP frame ready event data
 */
static inline void app_event_unpack_dcmipp_frame_ready(const event_bus_data_t *data,
                                                       uint8_t **frame_buffer,
                                                       uint32_t *pipe) {
  const event_dcmipp_frame_ready_t *evt = (const event_dcmipp_frame_ready_t *)data->data;
  if (frame_buffer) *frame_buffer = evt->frame_buffer;
  if (pipe) *pipe = evt->pipe;
}

/**
 * @brief  Pack LTDC layer reload event data
 */
static inline void app_event_pack_ltdc_layer_reload(event_bus_data_t *data,
                                                    uint8_t *frame_buffer,
                                                    uint32_t layer) {
  event_ltdc_layer_reload_t *evt = (event_ltdc_layer_reload_t *)data->data;
  evt->frame_buffer = frame_buffer;
  evt->layer = layer;
  evt->reserved = 0;
}

/**
 * @brief  Unpack LTDC layer reload event data
 */
static inline void app_event_unpack_ltdc_layer_reload(const event_bus_data_t *data,
                                                       uint8_t **frame_buffer,
                                                       uint32_t *layer) {
  const event_ltdc_layer_reload_t *evt = (const event_ltdc_layer_reload_t *)data->data;
  if (frame_buffer) *frame_buffer = evt->frame_buffer;
  if (layer) *layer = evt->layer;
}

/**
 * @brief  Pack UI update event data
 */
static inline void app_event_pack_ui_update(event_bus_data_t *data, uint32_t flags) {
  event_ui_update_t *evt = (event_ui_update_t *)data->data;
  evt->flags = flags;
  evt->reserved[0] = 0;
  evt->reserved[1] = 0;
  evt->reserved[2] = 0;
}

/**
 * @brief  Unpack UI update event data
 */
static inline void app_event_unpack_ui_update(const event_bus_data_t *data, uint32_t *flags) {
  const event_ui_update_t *evt = (const event_ui_update_t *)data->data;
  if (flags) *flags = evt->flags;
}

#ifdef __cplusplus
}
#endif

#endif /* APP_EVENTS_H */

