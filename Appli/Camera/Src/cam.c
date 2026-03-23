/**
 ******************************************************************************
 * @file    cam.c
 * @author  Long Liangmao
 * @brief   Camera thread lifecycle (ISP update and LCD reload threads)
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

#include "error.h"
#include "app_lcd.h"
#include "cam_internal.h"
#include "utils.h"

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void isp_thread_entry(ULONG arg);
static void lcd_reload_thread_entry(ULONG arg);

/* ============================================================================
 * Global State Definitions
 * ============================================================================ */

/* ISP thread resources */
isp_ctx_t isp_ctx;

/* LCD reload thread resources */
lcd_reload_ctx_t lcd_reload_ctx;

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

/**
 * @brief  Initialize and create the ISP update thread
 * @param  memory_ptr: Memory pointer (unused, thread uses static allocation)
 */
void CAM_ThreadStart(void) {
  APP_REQUIRE(tx_event_flags_create(&isp_ctx.vsync_flags, "isp_vsync") == TX_SUCCESS);

  APP_REQUIRE(tx_thread_create(&isp_ctx.thread, "isp_update",
                               isp_thread_entry, 0,
                               isp_ctx.stack, ISP_THREAD_STACK_SIZE,
                               ISP_THREAD_PRIORITY, ISP_THREAD_PRIORITY,
                               TX_NO_TIME_SLICE, TX_AUTO_START) == TX_SUCCESS);

  /* Create LCD reload thread */
  APP_REQUIRE(tx_event_flags_create(&lcd_reload_ctx.reload_flags, "lcd_reload") == TX_SUCCESS);

  APP_REQUIRE(tx_thread_create(&lcd_reload_ctx.thread, "lcd_reload",
                               lcd_reload_thread_entry, 0,
                               lcd_reload_ctx.stack, LCD_RELOAD_THREAD_STACK_SIZE,
                               LCD_RELOAD_THREAD_PRIORITY, LCD_RELOAD_THREAD_PRIORITY,
                               TX_NO_TIME_SLICE, TX_AUTO_START) == TX_SUCCESS);
}

/**
 * @brief  Suspend ISP and LCD reload threads
 */
void CAM_ThreadsSuspend(void) {
  tx_thread_suspend(&lcd_reload_ctx.thread);
  tx_thread_suspend(&isp_ctx.thread);
}

/**
 * @brief  Resume ISP and LCD reload threads
 */
void CAM_ThreadsResume(void) {
  tx_thread_resume(&isp_ctx.thread);
  tx_thread_resume(&lcd_reload_ctx.thread);
}

/**
 * @brief  Update ISP parameters (call periodically for auto exposure/white balance)
 */
void CAM_IspUpdate(void) {
  APP_REQUIRE(CMW_CAMERA_Run() == CMW_ERROR_NONE);
}

/* ============================================================================
 * Thread Entry Points
 * ============================================================================ */

/**
 * @brief  ISP update thread entry
 *         Waits for vsync events and updates ISP parameters
 * @param  arg: Thread argument (unused)
 */
static void isp_thread_entry(ULONG arg) {
  UNUSED(arg);
  ULONG actual_flags;

  while (1) {
    tx_event_flags_get(&isp_ctx.vsync_flags, 0x01, TX_OR_CLEAR,
                       &actual_flags, TX_WAIT_FOREVER);
    CAM_IspUpdate();
  }
}

/**
 * @brief  LCD reload thread entry
 *         Performs cache clean + LTDC reload deferred from display pipe ISR
 * @param  arg: Thread argument (unused)
 */
static void lcd_reload_thread_entry(ULONG arg) {
  UNUSED(arg);
  ULONG actual_flags;

  while (1) {
    tx_event_flags_get(&lcd_reload_ctx.reload_flags, 0x01, TX_OR_CLEAR,
                       &actual_flags, TX_WAIT_FOREVER);
    uint8_t *buffer = (uint8_t *)lcd_reload_ctx.pending_buffer;
    if (buffer != NULL) {
      LCD_ReloadCameraLayer(buffer);
    }
  }
}
