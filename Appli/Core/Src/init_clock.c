/**
 ******************************************************************************
 * @file    init_clock.c
 * @brief   System clock and clock-sleep configuration
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

#include "stm32n6xx_hal.h"

#include "error.h"
#include "init_clock.h"

void SystemClock_Config(void) {
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_PeriphCLKInitTypeDef RCC_PeriphCLKInitStruct = {0};

  // Oscillator config already done in bootrom
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_NONE;

  /* PLL1 = 64 x 25 / 2 = 800MHz */
  RCC_OscInitStruct.PLL1.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL1.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL1.PLLM = 2;
  RCC_OscInitStruct.PLL1.PLLN = 25;
  RCC_OscInitStruct.PLL1.PLLFractional = 0;
  RCC_OscInitStruct.PLL1.PLLP1 = 1;
  RCC_OscInitStruct.PLL1.PLLP2 = 1;

  RCC_OscInitStruct.PLL2.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL2.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL2.PLLM = 8;
  RCC_OscInitStruct.PLL2.PLLFractional = 0;
#ifdef PERFORMANCE_MODE
  /* PLL2 = 64 x 125 / 8 = 1000MHz */
  RCC_OscInitStruct.PLL2.PLLN = 125;
#else
  /* PLL2 = 64 x 100 / 8 = 800MHz */
  RCC_OscInitStruct.PLL2.PLLN = 100;
#endif
  RCC_OscInitStruct.PLL2.PLLP1 = 1;
  RCC_OscInitStruct.PLL2.PLLP2 = 1;

  /* PLL3 = (64 x 225 / 8) / (1 * 2) = 900MHz */
  RCC_OscInitStruct.PLL3.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL3.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL3.PLLM = 8;
  RCC_OscInitStruct.PLL3.PLLN = 225;
  RCC_OscInitStruct.PLL3.PLLFractional = 0;
  RCC_OscInitStruct.PLL3.PLLP1 = 1;
  RCC_OscInitStruct.PLL3.PLLP2 = 2;

  /* PLL4 = (64 x 225 / 8) / (6 * 6) = 50 MHz */
  RCC_OscInitStruct.PLL4.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL4.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL4.PLLM = 8;
  RCC_OscInitStruct.PLL4.PLLFractional = 0;
  RCC_OscInitStruct.PLL4.PLLN = 225;
  RCC_OscInitStruct.PLL4.PLLP1 = 6;
  RCC_OscInitStruct.PLL4.PLLP2 = 6;

  APP_REQUIRE(HAL_RCC_OscConfig(&RCC_OscInitStruct) == HAL_OK);

  RCC_PeriphCLKInitStruct.PeriphClockSelection = 0;

  /* XSPI1 kernel clock (ck_ker_xspi1) = HCLK = 200MHz */
  RCC_PeriphCLKInitStruct.PeriphClockSelection |= RCC_PERIPHCLK_XSPI1;
  RCC_PeriphCLKInitStruct.Xspi1ClockSelection = RCC_XSPI1CLKSOURCE_HCLK;

  /* XSPI2 kernel clock (ck_ker_xspi1) = HCLK =  200MHz */
  RCC_PeriphCLKInitStruct.PeriphClockSelection |= RCC_PERIPHCLK_XSPI2;
  RCC_PeriphCLKInitStruct.Xspi2ClockSelection = RCC_XSPI2CLKSOURCE_HCLK;

  /* Initializes TIMPRE as TIM is used as Systick Clock Source */
  RCC_PeriphCLKInitStruct.PeriphClockSelection |= RCC_PERIPHCLK_TIM;
  RCC_PeriphCLKInitStruct.TIMPresSelection = RCC_TIMPRES_DIV1;

  APP_REQUIRE(HAL_RCCEx_PeriphCLKConfig(&RCC_PeriphCLKInitStruct) == HAL_OK);

  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_CPUCLK | RCC_CLOCKTYPE_SYSCLK |
                                 RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 |
                                 RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_PCLK4 |
                                 RCC_CLOCKTYPE_PCLK5);

  /* CPU CLock (sysa_ck) = ic1_ck = PLL1 output/ic1_divider = 800 MHz */
  RCC_ClkInitStruct.CPUCLKSource = RCC_CPUCLKSOURCE_IC1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_IC2_IC6_IC11;
  RCC_ClkInitStruct.IC1Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_ClkInitStruct.IC1Selection.ClockDivider = 1;

  /* AXI Clock (sysb_ck) = ic2_ck = PLL1 output/ic2_divider = 400 MHz */
  RCC_ClkInitStruct.IC2Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_ClkInitStruct.IC2Selection.ClockDivider = 2;

  /* NPU Clock (sysc_ck) = ic6_ck = PLL2 output/ic6_divider */
  RCC_ClkInitStruct.IC6Selection.ClockSelection = RCC_ICCLKSOURCE_PLL2;
  RCC_ClkInitStruct.IC6Selection.ClockDivider = 1;

  /* AXISRAM3/4/5/6 Clock (sysd_ck) = ic11_ck */
#ifdef PERFORMANCE_MODE
  /* PLL3 output/ic11_divider = 900 MHz */
  RCC_ClkInitStruct.IC11Selection.ClockSelection = RCC_ICCLKSOURCE_PLL3;
#else
  /* PLL1 output/ic11_divider = 800 MHz */
  RCC_ClkInitStruct.IC11Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
#endif
  RCC_ClkInitStruct.IC11Selection.ClockDivider = 1;

  /* HCLK = sysb_ck / HCLK divider = 200 MHz */
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;

  /* PCLKx = HCLK / PCLKx divider = 200 MHz */
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;
  RCC_ClkInitStruct.APB5CLKDivider = RCC_APB5_DIV1;

  APP_REQUIRE(HAL_RCC_ClockConfig(&RCC_ClkInitStruct) == HAL_OK);

#ifdef PERFORMANCE_MODE
  /* Performance mode: ensure VDDCORE at 0.89V (SCALE0) */
  APP_REQUIRE(HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE0) == HAL_OK);
#else
  /* Nominal mode: lower VDDCORE to 0.81V (FSBL sets SCALE0 for boot) */
  APP_REQUIRE(HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) == HAL_OK);
#endif
}

void ClockSleep_Config(void) {
  // Misc
#if defined(CPU_IN_SECURE_STATE)
  __HAL_RCC_DBG_CLK_SLEEP_ENABLE();
#endif
  __HAL_RCC_XSPIPHYCOMP_CLK_SLEEP_ENABLE();

  // Memories (used by NPU, cache, and general operation)
  __HAL_RCC_AXISRAM1_MEM_CLK_SLEEP_ENABLE();
  __HAL_RCC_AXISRAM2_MEM_CLK_SLEEP_ENABLE();
  __HAL_RCC_AXISRAM3_MEM_CLK_SLEEP_ENABLE();
  __HAL_RCC_AXISRAM4_MEM_CLK_SLEEP_ENABLE();
  __HAL_RCC_AXISRAM5_MEM_CLK_SLEEP_ENABLE();
  __HAL_RCC_AXISRAM6_MEM_CLK_SLEEP_ENABLE();
  __HAL_RCC_FLEXRAM_MEM_CLK_SLEEP_ENABLE();
  __HAL_RCC_CACHEAXIRAM_MEM_CLK_SLEEP_ENABLE();

  // AHB2 (RAMCFG needed for NPU SRAMs)
  __HAL_RCC_RAMCFG_CLK_SLEEP_ENABLE();

  // AHB3 (security peripherals)
#if defined(CPU_IN_SECURE_STATE)
  __HAL_RCC_RIFSC_CLK_SLEEP_ENABLE();
  __HAL_RCC_RISAF_CLK_SLEEP_ENABLE();
  __HAL_RCC_IAC_CLK_SLEEP_ENABLE();
#endif

  // AHB5 (NPU, external memory)
  __HAL_RCC_XSPI1_CLK_SLEEP_ENABLE();
  __HAL_RCC_XSPI2_CLK_SLEEP_ENABLE();
  __HAL_RCC_CACHEAXI_CLK_SLEEP_ENABLE();
  __HAL_RCC_NPU_CLK_SLEEP_ENABLE();

  // APB5 (camera and display pipeline)
  __HAL_RCC_LTDC_CLK_SLEEP_ENABLE();
  __HAL_RCC_DCMIPP_CLK_SLEEP_ENABLE();
  __HAL_RCC_CSI_CLK_SLEEP_ENABLE();
  __HAL_RCC_DMA2D_CLK_SLEEP_ENABLE();

  // === Disable sleep clocks for unused peripherals ===

  // Unused memories
  __HAL_RCC_AHBSRAM1_MEM_CLK_SLEEP_DISABLE();
  __HAL_RCC_AHBSRAM2_MEM_CLK_SLEEP_DISABLE();
  __HAL_RCC_BKPSRAM_MEM_CLK_SLEEP_DISABLE();
  __HAL_RCC_VENCRAM_MEM_CLK_SLEEP_DISABLE();

  // AHB1: no ADC or GPDMA used
  __HAL_RCC_ADC12_CLK_SLEEP_DISABLE();
  __HAL_RCC_GPDMA1_CLK_SLEEP_DISABLE();

  // AHB2: no digital filters
  __HAL_RCC_ADF1_CLK_SLEEP_DISABLE();
  __HAL_RCC_MDF1_CLK_SLEEP_DISABLE();

  // AHB3: no crypto
#if defined(CPU_IN_SECURE_STATE)
  __HAL_RCC_CRYP_CLK_SLEEP_DISABLE();
  __HAL_RCC_HASH_CLK_SLEEP_DISABLE();
  __HAL_RCC_PKA_CLK_SLEEP_DISABLE();
  __HAL_RCC_RNG_CLK_SLEEP_DISABLE();
  __HAL_RCC_SAES_CLK_SLEEP_DISABLE();
#endif

  // AHB4: unused GPIOs and CRC
  __HAL_RCC_CRC_CLK_SLEEP_DISABLE();
  __HAL_RCC_GPION_CLK_SLEEP_DISABLE();
  __HAL_RCC_GPIOO_CLK_SLEEP_DISABLE();
  __HAL_RCC_GPIOP_CLK_SLEEP_DISABLE();

  // AHB5: unused connectivity, graphics, and storage
  __HAL_RCC_ETH1_CLK_SLEEP_DISABLE();
  __HAL_RCC_ETH1MAC_CLK_SLEEP_DISABLE();
  __HAL_RCC_ETH1TX_CLK_SLEEP_DISABLE();
  __HAL_RCC_ETH1RX_CLK_SLEEP_DISABLE();
  __HAL_RCC_FMC_CLK_SLEEP_DISABLE();
  __HAL_RCC_GFXMMU_CLK_SLEEP_DISABLE();
  __HAL_RCC_GPU2D_CLK_SLEEP_DISABLE();
  __HAL_RCC_JPEG_CLK_SLEEP_DISABLE();
  __HAL_RCC_XSPI3_CLK_SLEEP_DISABLE();
  __HAL_RCC_XSPIM_CLK_SLEEP_DISABLE();
  __HAL_RCC_MCE1_CLK_SLEEP_DISABLE();
  __HAL_RCC_MCE2_CLK_SLEEP_DISABLE();
  __HAL_RCC_MCE3_CLK_SLEEP_DISABLE();
  __HAL_RCC_MCE4_CLK_SLEEP_DISABLE();
  __HAL_RCC_DCMI_PSSI_CLK_SLEEP_DISABLE();
  __HAL_RCC_SDMMC1_CLK_SLEEP_DISABLE();
  __HAL_RCC_SDMMC2_CLK_SLEEP_DISABLE();
  __HAL_RCC_USB1_OTG_HS_CLK_SLEEP_DISABLE();
  __HAL_RCC_USB1_OTG_HS_PHY_CLK_SLEEP_DISABLE();
  __HAL_RCC_USB2_OTG_HS_CLK_SLEEP_DISABLE();
  __HAL_RCC_USB2_OTG_HS_PHY_CLK_SLEEP_DISABLE();

  // APB1: no SPI, extra UART, I3C, I2C3, timers (except TIM2), or CAN
  __HAL_RCC_I2C3_CLK_SLEEP_DISABLE();
  __HAL_RCC_I3C1_CLK_SLEEP_DISABLE();
  __HAL_RCC_I3C2_CLK_SLEEP_DISABLE();
  __HAL_RCC_LPTIM1_CLK_SLEEP_DISABLE();
  __HAL_RCC_SPDIFRX1_CLK_SLEEP_DISABLE();
  __HAL_RCC_SPI2_CLK_SLEEP_DISABLE();
  __HAL_RCC_SPI3_CLK_SLEEP_DISABLE();
  __HAL_RCC_TIM3_CLK_SLEEP_DISABLE();
  __HAL_RCC_TIM4_CLK_SLEEP_DISABLE();
  __HAL_RCC_TIM5_CLK_SLEEP_DISABLE();
  __HAL_RCC_TIM6_CLK_SLEEP_DISABLE();
  __HAL_RCC_TIM7_CLK_SLEEP_DISABLE();
  __HAL_RCC_TIM10_CLK_SLEEP_DISABLE();
  __HAL_RCC_TIM11_CLK_SLEEP_DISABLE();
  __HAL_RCC_TIM12_CLK_SLEEP_DISABLE();
  __HAL_RCC_TIM13_CLK_SLEEP_DISABLE();
  __HAL_RCC_TIM14_CLK_SLEEP_DISABLE();
  __HAL_RCC_USART2_CLK_SLEEP_DISABLE();
  __HAL_RCC_USART3_CLK_SLEEP_DISABLE();
  __HAL_RCC_UART4_CLK_SLEEP_DISABLE();
  __HAL_RCC_UART5_CLK_SLEEP_DISABLE();
  __HAL_RCC_UART7_CLK_SLEEP_DISABLE();
  __HAL_RCC_UART8_CLK_SLEEP_DISABLE();
  __HAL_RCC_FDCAN_CLK_SLEEP_DISABLE();
  __HAL_RCC_MDIOS_CLK_SLEEP_DISABLE();
  __HAL_RCC_UCPD1_CLK_SLEEP_DISABLE();

  // APB2: no SAI, extra SPI, extra timers, or extra UARTs
  __HAL_RCC_SAI1_CLK_SLEEP_DISABLE();
  __HAL_RCC_SAI2_CLK_SLEEP_DISABLE();
  __HAL_RCC_SPI1_CLK_SLEEP_DISABLE();
  __HAL_RCC_SPI4_CLK_SLEEP_DISABLE();
  __HAL_RCC_SPI5_CLK_SLEEP_DISABLE();
  __HAL_RCC_TIM1_CLK_SLEEP_DISABLE();
  __HAL_RCC_TIM8_CLK_SLEEP_DISABLE();
  __HAL_RCC_TIM9_CLK_SLEEP_DISABLE();
  __HAL_RCC_TIM15_CLK_SLEEP_DISABLE();
  __HAL_RCC_TIM16_CLK_SLEEP_DISABLE();
  __HAL_RCC_TIM17_CLK_SLEEP_DISABLE();
  __HAL_RCC_TIM18_CLK_SLEEP_DISABLE();
  __HAL_RCC_USART1_CLK_SLEEP_DISABLE();
  __HAL_RCC_USART6_CLK_SLEEP_DISABLE();
  __HAL_RCC_UART9_CLK_SLEEP_DISABLE();
  __HAL_RCC_USART10_CLK_SLEEP_DISABLE();

  // APB4: no I2C4, low-power timers, LPUART, extra SPI
  __HAL_RCC_HDP_CLK_SLEEP_DISABLE();
  __HAL_RCC_I2C4_CLK_SLEEP_DISABLE();
  __HAL_RCC_LPTIM2_CLK_SLEEP_DISABLE();
  __HAL_RCC_LPTIM3_CLK_SLEEP_DISABLE();
  __HAL_RCC_LPTIM4_CLK_SLEEP_DISABLE();
  __HAL_RCC_LPTIM5_CLK_SLEEP_DISABLE();
  __HAL_RCC_LPUART1_CLK_SLEEP_DISABLE();
  __HAL_RCC_SPI6_CLK_SLEEP_DISABLE();
  __HAL_RCC_VREFBUF_CLK_SLEEP_DISABLE();
  __HAL_RCC_DTS_CLK_SLEEP_DISABLE();

  // APB5: no video encoder or graphics timer
  __HAL_RCC_GFXTIM_CLK_SLEEP_DISABLE();
  __HAL_RCC_VENC_CLK_SLEEP_DISABLE();
}
