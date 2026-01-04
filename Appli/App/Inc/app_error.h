/**
 ******************************************************************************
 * @file    app_error.h
 * @author  Long Liangmao
 * @brief   Application-level fatal error handling helpers (fail-fast)
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

#ifndef APP_ERROR_H
#define APP_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

extern void Error_Handler(void);
extern volatile uint8_t *g_error_file;
extern volatile uint32_t g_error_line;

/**
 * @brief  Record the failing location and jump to the global error handler.
 *         This is intended to be used when the app cannot recover.
 */
static inline void APP_Panic(const char *file, uint32_t line) {
  g_error_file = (volatile uint8_t *)file;
  g_error_line = line;

  Error_Handler();

  /* In case Error_Handler() ever returns, halt here. */
  while (1) { /* hang */
  }
}

/**
 * @brief  Require a condition to be true; otherwise panic.
 *         Unlike assert(), this is always enabled.
 */
#define APP_REQUIRE(cond)                      \
  do {                                         \
    if (!(cond)) {                             \
      APP_Panic(__FILE__, (uint32_t)__LINE__); \
    }                                          \
  } while (0)

/**
 * @brief  Helper to require an expression equals an expected value.
 *         Useful for HAL/ThreadX/BSP status checks without including their headers.
 */
#define APP_REQUIRE_EQ(expr, expected) APP_REQUIRE((expr) == (expected))

#ifdef __cplusplus
}
#endif

#endif /* APP_ERROR_H */
