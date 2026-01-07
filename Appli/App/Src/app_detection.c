/**
 ******************************************************************************
 * @file    app_detection.c
 * @author  Long Liangmao
 * @brief   Object detection state management, postprocess thread, and overlay
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

#include "app_detection.h"
#include "app_bqueue.h"
#include "app_buffers.h"
#include "app_config.h"
#include "app_error.h"
#include "app_lcd.h"
#include "app_nn.h"
#include "app_postprocess.h"
#include "stm32_lcd.h"
#include "stm32n6xx_hal.h"
#include "utils.h"
#include "app_error.h"
#include <string.h>

/* Thread configurations */
#define PP_THREAD_STACK_SIZE     4096
#define PP_THREAD_PRIORITY       8   /* Lower than NN */

#define OVERLAY_THREAD_STACK_SIZE 4096
#define OVERLAY_THREAD_PRIORITY   10  /* Lower than PP, same as UI */

/* Align macro */
#define ALIGN_VALUE(v, a) (((v) + (a) - 1) & ~((a) - 1))

/* Class names table - single class for person detection */
static const char *classes_table[] = { "person" };
#define NB_CLASSES (sizeof(classes_table) / sizeof(classes_table[0]))

/* Colors for bounding boxes (ARGB8888) */
#define NUMBER_COLORS 10
static const uint32_t colors[NUMBER_COLORS] = {
  0xFF00FF00,  /* Green */
  0xFFFF0000,  /* Red */
  0xFF00FFFF,  /* Cyan */
  0xFFFF00FF,  /* Magenta */
  0xFFFFFF00,  /* Yellow */
  0xFF808080,  /* Gray */
  0xFF000000,  /* Black */
  0xFFA52A2A,  /* Brown */
  0xFF0000FF,  /* Blue */
  0xFFFFA500,  /* Orange */
};

/* Thread resources */
static struct {
  TX_THREAD thread;
  UCHAR stack[PP_THREAD_STACK_SIZE];
} pp_ctx;

static struct {
  TX_THREAD thread;
  UCHAR stack[OVERLAY_THREAD_STACK_SIZE];
} overlay_ctx;

/* Synchronization */
static TX_MUTEX detection_mutex;
static TX_SEMAPHORE update_sem;

/* Shared detection state */
static detection_info_t detection_info;

/* Postprocess parameters */
static od_st_yolox_pp_static_param_t pp_params;

#define  NN_MODEL_DETECTION

/**
 * @brief  Clamp a point to display bounds
 */
static void clamp_point(int *x, int *y) {
  if (*x < 0) *x = 0;
  if (*y < 0) *y = 0;
  if (*x >= (int)DISPLAY_LETTERBOX_WIDTH) *x = DISPLAY_LETTERBOX_WIDTH - 1;
  if (*y >= (int)DISPLAY_LETTERBOX_HEIGHT) *y = DISPLAY_LETTERBOX_HEIGHT - 1;
}

/**
 * @brief  Draw a single detection bounding box
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

/**
 * @brief  Postprocess thread entry function
 */
static void pp_thread_entry(ULONG arg) {
  UNUSED(arg);

  bqueue_t *output_queue = NN_GetOutputQueue();
  const uint32_t *out_sizes = NN_GetOutputSizes();
  int out_count = NN_GetOutputCount();
  od_pp_out_t pp_output;
  uint8_t *pp_input[NN_OUT_MAX_NB];
  uint32_t pp_ts[2];
  nn_timing_t nn_timing;
  int ret;

  /* Initialize postprocess with NN instance (object detection model) */
  LL_ATON_DECLARE_NAMED_NN_INSTANCE_AND_INTERFACE(od_yolo_x_person);
  ret = app_postprocess_init(&pp_params, &NN_Instance_od_yolo_x_person);
  APP_REQUIRE(ret == 0);
  
  while (1) {
    uint8_t *output_buffer;

    /* Wait for NN output */
    output_buffer = bqueue_get_ready(output_queue);
    APP_REQUIRE(output_buffer != NULL);

    /* Calculate output buffer pointers */
    pp_input[0] = output_buffer;
    for (int i = 1; i < out_count; i++) {
      pp_input[i] = pp_input[i - 1] + ALIGN_VALUE(out_sizes[i - 1], 32);
    }

    pp_output.pOutBuff = NULL;

    /* Run postprocessing */
    pp_ts[0] = HAL_GetTick();
    ret = app_postprocess_run((void **)pp_input, out_count, &pp_output, &pp_params);
    APP_REQUIRE(ret == 0);
    pp_ts[1] = HAL_GetTick();

    /* Get NN timing */
    NN_GetTiming(&nn_timing);

    /* Update shared detection state */
    Detection_Lock();
    detection_info.nb_detect = pp_output.nb_detect;
    for (int i = 0; i < pp_output.nb_detect && i < DETECTION_MAX_BOXES; i++) {
      detection_info.detects[i] = pp_output.pOutBuff[i];
    }
    detection_info.nn_period_ms = nn_timing.nn_period_ms;
    detection_info.inference_ms = nn_timing.inference_ms;
    detection_info.postprocess_ms = pp_ts[1] - pp_ts[0];
    Detection_Unlock();

    /* Release output buffer */
    bqueue_put_free(output_queue);

    /* Signal overlay thread */
    Detection_SignalUpdate();
  }
}

/**
 * @brief  Overlay drawing thread entry function
 */
static void overlay_thread_entry(ULONG arg) {
  UNUSED(arg);

  detection_info_t local_info;
  uint8_t *ui_buffer;
  char stats_str[32];
  UINT status;

  while (1) {
    /* Wait for detection update */
    status = tx_semaphore_get(&update_sem, TX_WAIT_FOREVER);
    if (status != TX_SUCCESS) continue;

    /* Copy detection info under lock */
    Detection_Lock();
    local_info = detection_info;
    Detection_Unlock();

    /* Get back buffer for drawing */
    ui_buffer = Buffer_GetUIBackBuffer();
    if (ui_buffer == NULL) continue;

    /* Set layer buffer address */
    LCD_SetUILayerAddress(ui_buffer);

    /* Set drawing layer */
    UTIL_LCD_SetLayer(LCD_LAYER_1_UI);
    UTIL_LCD_SetFont(&Font16);
    UTIL_LCD_SetBackColor(0x00000000);  /* Transparent */

    /* Clear overlay area - only the detection overlay region */
    /* Leave the diagnostic panel area (left side) untouched */
    UTIL_LCD_FillRect(DISPLAY_LETTERBOX_X0, 0,
                      DISPLAY_LETTERBOX_WIDTH, DISPLAY_LETTERBOX_HEIGHT / 2,
                      0x00000000);

    /* Draw detection info at top */
    UTIL_LCD_SetTextColor(0xFFFFFFFF);  /* White */

    snprintf(stats_str, sizeof(stats_str), "Objects: %d", (int)local_info.nb_detect);
    UTIL_LCD_DisplayStringAt(DISPLAY_LETTERBOX_X0 + 10, 10,
                             (uint8_t *)stats_str, LEFT_MODE);

    snprintf(stats_str, sizeof(stats_str), "Inf: %ums FPS: %.1f",
             (unsigned int)local_info.inference_ms,
             local_info.nn_period_ms > 0 ? 1000.0f / local_info.nn_period_ms : 0.0f);
    UTIL_LCD_DisplayStringAt(DISPLAY_LETTERBOX_X0 + 10, 30,
                             (uint8_t *)stats_str, LEFT_MODE);

    /* Draw bounding boxes */
    UTIL_LCD_SetTextColor(0xFF00FF00);  /* Green for boxes */
    for (int i = 0; i < local_info.nb_detect; i++) {
      draw_detection(&local_info.detects[i]);
    }

    /* Commit buffer to display */
    Buffer_SetUIDisplayIndex(Buffer_GetNextUIDisplayIndex());
    LCD_ReloadUILayer(ui_buffer);
  }
}

void Detection_SignalUpdate(void) {
  /* Use ceiling put to avoid overflow if overlay is slower than PP */
  tx_semaphore_ceiling_put(&update_sem, 1);
}

void Detection_Lock(void) {
  UINT status = tx_mutex_get(&detection_mutex, TX_WAIT_FOREVER);
  APP_REQUIRE(status == TX_SUCCESS);
  (void)status;
}

void Detection_Unlock(void) {
  UINT status = tx_mutex_put(&detection_mutex);
  APP_REQUIRE(status == TX_SUCCESS);
  (void)status;
}

detection_info_t *Detection_GetInfo(void) {
  return &detection_info;
}

void Detection_Thread_Init(VOID *memory_ptr) {
  UNUSED(memory_ptr);
  UINT status;

  /* Initialize detection state */
  memset(&detection_info, 0, sizeof(detection_info));

  /* Create synchronization primitives */
  status = tx_mutex_create(&detection_mutex, "detection_lock", TX_NO_INHERIT);
  APP_REQUIRE_EQ(status, TX_SUCCESS);

  status = tx_semaphore_create(&update_sem, "detection_update", 0);
  APP_REQUIRE_EQ(status, TX_SUCCESS);

  /* Create postprocess thread */
  status = tx_thread_create(&pp_ctx.thread, "postprocess",
                            pp_thread_entry, 0,
                            pp_ctx.stack, PP_THREAD_STACK_SIZE,
                            PP_THREAD_PRIORITY, PP_THREAD_PRIORITY,
                            TX_NO_TIME_SLICE, TX_AUTO_START);
  APP_REQUIRE_EQ(status, TX_SUCCESS);

  /* Create overlay thread */
  status = tx_thread_create(&overlay_ctx.thread, "overlay",
                            overlay_thread_entry, 0,
                            overlay_ctx.stack, OVERLAY_THREAD_STACK_SIZE,
                            OVERLAY_THREAD_PRIORITY, OVERLAY_THREAD_PRIORITY,
                            TX_NO_TIME_SLICE, TX_AUTO_START);
  APP_REQUIRE_EQ(status, TX_SUCCESS);
}

