/**
 ******************************************************************************
 * @file    app.c
 * @author  Long Liangmao
 * @brief   Application entry point
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

#include "app.h"
#include "app_bqueue.h"
#include "app_buffers.h"
#include "app_cam.h"
#include "app_config.h"
#include "app_detection.h"
#include "app_error.h"
#include "app_lcd.h"
#include "app_nn.h"
#include "app_ui.h"
#include "cmw_camera.h"
#include "main.h"
#include "stm32n6570_discovery_errno.h"
#include "stm32n6570_discovery_xspi.h"
#include "stm32n6xx_hal.h"
#include "stm32n6xx_hal_rif.h"
#include "utils.h"

void App_Init(VOID *memory_ptr) {
  bqueue_t *nn_input_queue;
  uint8_t *first_nn_buffer;

  Buffer_Init();

  LCD_Init();

  UI_Init();

  CAM_Init();

  Thread_IspUpdate_Init(memory_ptr);

  CAM_DisplayPipe_Start(CMW_MODE_CONTINUOUS);

  NN_Thread_Init(memory_ptr);

  Detection_Thread_Init(memory_ptr);

  nn_input_queue = NN_GetInputQueue();
  first_nn_buffer = bqueue_get_free(nn_input_queue, 0);
  APP_REQUIRE(first_nn_buffer != NULL);
  CAM_NNPipe_Start(first_nn_buffer, CMW_MODE_CONTINUOUS);
}
