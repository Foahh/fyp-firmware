/**
 ******************************************************************************
 * @file    thread_config.h
 * @brief   ThreadX thread stack sizes and priorities
 ******************************************************************************
 * @attention
 *
 * ThreadX: lower numeric priority value = higher scheduling priority.
 *
 ******************************************************************************
 */

#ifndef APP_THREAD_CONFIG_H
#define APP_THREAD_CONFIG_H

/* --- Startup --- */
#define STARTUP_THREAD_STACK_SIZE 1024U
#define STARTUP_THREAD_PRIORITY   1U

/* --- Vision --- */
#define PP_THREAD_STACK_SIZE 4096U
#define PP_THREAD_PRIORITY   4U

#define NN_THREAD_STACK_SIZE 4096U
#define NN_THREAD_PRIORITY   5U

/* --- Sensors --- */
#define TOF_THREAD_STACK_SIZE 2048U
#define TOF_THREAD_PRIORITY   6U

/* --- Display --- */
#define LCD_RELOAD_THREAD_STACK_SIZE 1024U
#define LCD_RELOAD_THREAD_PRIORITY   7U

#define ISP_THREAD_STACK_SIZE 2048U
#define ISP_THREAD_PRIORITY   8U

/* --- Host --- */
#define COMM_RX_THREAD_STACK_SIZE 2048U
#define COMM_RX_THREAD_PRIORITY   9U

#define COMM_LOG_THREAD_STACK_SIZE 2048U
#define COMM_LOG_THREAD_PRIORITY   10U

/* --- UI --- */
#define UI_THREAD_STACK_SIZE 4096U
#define UI_THREAD_PRIORITY   11U

/* --- Background --- */
#define CPU_LOAD_THREAD_STACK_SIZE 1024U
#define CPU_LOAD_THREAD_PRIORITY   31U
#define CPU_LOAD_SAMPLE_PERIOD_MS  100U

#endif /* APP_THREAD_CONFIG_H */
