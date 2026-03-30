/**
 ******************************************************************************
 * @file    power_mode.h
 * @brief   Power-mode constants (compile-time selection via -DPOWER_MODE=N)
 ******************************************************************************
 */

#ifndef POWER_MODE_H
#define POWER_MODE_H

#define POWER_MODE_UNDERDRIVE 0
#define POWER_MODE_NOMINAL    1
#define POWER_MODE_OVERDRIVE  2

#ifndef POWER_MODE
#define POWER_MODE POWER_MODE_NOMINAL
#endif

#if POWER_MODE != POWER_MODE_UNDERDRIVE && \
    POWER_MODE != POWER_MODE_NOMINAL &&    \
    POWER_MODE != POWER_MODE_OVERDRIVE
#error "POWER_MODE must be 0 (underdrive), 1 (nominal), or 2 (overdrive)"
#endif

#endif /* POWER_MODE_H */
