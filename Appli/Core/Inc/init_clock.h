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

#ifdef __cplusplus
}
#endif

#endif /* INIT_CLOCK_H */
