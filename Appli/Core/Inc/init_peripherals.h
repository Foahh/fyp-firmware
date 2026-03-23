/**
 ******************************************************************************
 * @file    init_peripherals.h
 * @brief   Peripheral initialization declarations
 ******************************************************************************
 */

#ifndef INIT_PERIPHERALS_H
#define INIT_PERIPHERALS_H

#ifdef __cplusplus
extern "C" {
#endif

void SystemIsolation_Config(void);
void SMPS_Config(void);
void IAC_Config(void);
void XSPI_Config(void);
void LED_Config(void);
void Button_Config(void);
void NPU_Config(void);
void Priority_Config(void);
void GPIO_Config(void);

void NPU_CacheEnableClocksAndReset(void);
void NPU_CacheDisableClocksAndReset(void);

#ifdef DEBUG
void PeripheralResetAll_Config(void);
#endif

#if (USE_BSP_COM_FEATURE > 0)
void COM_Config(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* INIT_PERIPHERALS_H */
