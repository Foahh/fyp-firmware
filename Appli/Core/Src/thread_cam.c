/**
 ******************************************************************************
 * @file    thread_cam.c
 * @author  Long Liangmao
 * @brief   Camera thread implementation for ThreadX
 *          Initializes camera and runs ISP update loop
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 Long Liangmao.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#include "main.h"
#include "thread_cam.h"
#include "app_cam.h"
#include "stm32n6xx_hal.h"
#include "cmw_camera.h"
#include <stdio.h>

/* Thread configuration */
#define CAMERA_THREAD_STACK_SIZE 2048
#define CAMERA_THREAD_PRIORITY   5

/* Display buffer - placed in PSRAM for large buffer */
/* Buffer size: 800 x 480 x 2 (RGB565) = 768000 bytes */
#define DISPLAY_BUFFER_SIZE (LCD_BG_WIDTH * LCD_BG_HEIGHT * DISPLAY_BPP)

/* Thread control block and stack */
static TX_THREAD camera_thread;
static UCHAR camera_thread_stack[CAMERA_THREAD_STACK_SIZE];

/* Display frame buffer - should be in external RAM for real application */
/* For now using internal RAM, but you may need to place this in PSRAM */
__attribute__((aligned(32))) static uint8_t display_buffer[DISPLAY_BUFFER_SIZE];

/* Thread entry function */
static VOID camera_thread_entry(ULONG thread_input);

/**
 * @brief  Initialize and create the camera thread
 * @param  memory_ptr: Memory pointer (unused, thread uses static allocation)
 * @retval TX_SUCCESS if successful, error code otherwise
 */
UINT CameraThread_Init(VOID *memory_ptr)
{
  UINT ret = TX_SUCCESS;
  UNUSED(memory_ptr);

  ret = tx_thread_create(&camera_thread, "camera_thread",
                         camera_thread_entry, 0, camera_thread_stack,
                         CAMERA_THREAD_STACK_SIZE, CAMERA_THREAD_PRIORITY,
                         CAMERA_THREAD_PRIORITY, TX_NO_TIME_SLICE,
                         TX_AUTO_START);

  return ret;
}

/**
 * @brief  Camera thread entry function
 * @param  thread_input: Thread input parameter (unused)
 * @retval None
 */
static VOID camera_thread_entry(ULONG thread_input)
{
  int ret;
  uint32_t sensor_width, sensor_height;

  UNUSED(thread_input);

  /* Small delay to let system stabilize */
  tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND / 10);

  /* Initialize camera */
  ret = CAM_Init();
  if (ret != 0)
  {
    /* Camera initialization failed - blink LED rapidly to indicate error */
    while (1)
    {
      BSP_LED_Toggle(LED_GREEN);
      tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND / 10);
    }
  }

  /* Get sensor resolution for debug */
  CAM_GetResolution(&sensor_width, &sensor_height);

  /* Clear display buffer */
  memset(display_buffer, 0, DISPLAY_BUFFER_SIZE);

  /* Start camera capture in continuous mode */
  CAM_DisplayPipe_Start(display_buffer, CMW_MODE_CONTINUOUS);

  /* Main camera loop - update ISP periodically */
  while (1)
  {
    /* Update ISP for auto exposure and auto white balance */
    CAM_IspUpdate();

    /* Sleep to allow other threads to run and reduce CPU load */
    /* ISP update rate: approximately 30 times per second */
    tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND / 30);
  }
}

