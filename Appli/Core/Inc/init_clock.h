/**
 ******************************************************************************
 * @file    init_clock.h
 * @brief   Clock initialization declarations
 ******************************************************************************
 */

#ifndef INIT_CLOCK_H
#define INIT_CLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

void SystemClock_Config(void);
void ClockSleep_Config(void);

/* Captured in SystemClock_Config() from HAL (CPU = sysa_ck / IC1, NPU = sysc_ck / IC6). */
uint32_t AppClock_GetCpuFreqMHz(void);
uint32_t AppClock_GetNpuFreqMHz(void);

#ifdef __cplusplus
}
#endif

#endif /* INIT_CLOCK_H */
