/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Long Liangmao
 * @brief          : Main program body
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

#include "main.h"
#include "extmem_manager.h"
#include "stm32_extmem_conf.h"
#include "stm32n6570_discovery.h"
#include <stdint.h>

/*
 * Exported variables
 */
XSPI_HandleTypeDef hxspi2;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */
static void XSPI_Power_Config(void);
static void XSPI_Config(void);
static void LED_Config(void);
static void SystemClock_Config(void);
static void OTP_Config(void);
static void GPIO_Config(void);

/* ============================================================================
 * OTP Configuration Constants
 * ============================================================================ */
#define BSEC_HW_CONFIG_ID    124U
#define BSEC_HWS_HSLV_VDDIO3 (1U << 15)
#define BSEC_HWS_HSLV_VDDIO2 (1U << 16)
#define BSEC_HWS_HSLV_MASK   (BSEC_HWS_HSLV_VDDIO3 | BSEC_HWS_HSLV_VDDIO2)

/* ============================================================================
 * Main Function
 * ============================================================================ */
int main(void) {
  HAL_Init();

  SystemClock_Config();

  LED_Config();

  GPIO_Config();

  OTP_Config();

  XSPI_Power_Config();
  XSPI_Config();

  EXTMEM_MANAGER_Init();

  if (BOOT_Application() != BOOT_OK) {
    Error_Handler();
  }

  for (;;) {
    // do nothing
  }
}

/* ============================================================================
 * Private Functions
 * ============================================================================ */

/**
 * @brief System Clock Configuration
 * @retval None
 */

static void SystemClock_Config(void) {
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the System Power Supply
   */
  if (HAL_PWREx_ConfigSupply(PWR_EXTERNAL_SOURCE_SUPPLY) != HAL_OK) {
    Error_Handler();
  }

  /** Configure the main internal regulator output voltage
   */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE0) != HAL_OK) {
    Error_Handler();
  }

  /* Enable HSI */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL1.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL2.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL3.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL4.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /** Initializes TIMPRE when TIM is used as Systick Clock Source
   */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_TIM;
  PeriphClkInitStruct.TIMPresSelection = RCC_TIMPRES_DIV1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /** Get current CPU/System buses clocks configuration and if necessary switch
 to intermediate HSI clock to ensure target clock can be set
  */
  HAL_RCC_GetClockConfig(&RCC_ClkInitStruct);
  if ((RCC_ClkInitStruct.CPUCLKSource == RCC_CPUCLKSOURCE_IC1) ||
      (RCC_ClkInitStruct.SYSCLKSource == RCC_SYSCLKSOURCE_IC2_IC6_IC11)) {
    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_CPUCLK | RCC_CLOCKTYPE_SYSCLK);
    RCC_ClkInitStruct.CPUCLKSource = RCC_CPUCLKSOURCE_HSI;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct) != HAL_OK) {
      /* Initialization Error */
      Error_Handler();
    }
  }

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_NONE;
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
  RCC_OscInitStruct.PLL2.PLLN = 125;
  RCC_OscInitStruct.PLL2.PLLFractional = 0;
  RCC_OscInitStruct.PLL2.PLLP1 = 1;
  RCC_OscInitStruct.PLL2.PLLP2 = 1;
  RCC_OscInitStruct.PLL3.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL3.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL3.PLLM = 8;
  RCC_OscInitStruct.PLL3.PLLN = 225;
  RCC_OscInitStruct.PLL3.PLLFractional = 0;
  RCC_OscInitStruct.PLL3.PLLP1 = 1;
  RCC_OscInitStruct.PLL3.PLLP2 = 2;
  RCC_OscInitStruct.PLL4.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL4.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL4.PLLM = 8;
  RCC_OscInitStruct.PLL4.PLLN = 225;
  RCC_OscInitStruct.PLL4.PLLFractional = 0;
  RCC_OscInitStruct.PLL4.PLLP1 = 6;
  RCC_OscInitStruct.PLL4.PLLP2 = 6;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_CPUCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_PCLK5 | RCC_CLOCKTYPE_PCLK4;
  RCC_ClkInitStruct.CPUCLKSource = RCC_CPUCLKSOURCE_IC1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_IC2_IC6_IC11;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;
  RCC_ClkInitStruct.APB5CLKDivider = RCC_APB5_DIV1;
  RCC_ClkInitStruct.IC1Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_ClkInitStruct.IC1Selection.ClockDivider = 1;
  RCC_ClkInitStruct.IC2Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_ClkInitStruct.IC2Selection.ClockDivider = 2;
  RCC_ClkInitStruct.IC6Selection.ClockSelection = RCC_ICCLKSOURCE_PLL2;
  RCC_ClkInitStruct.IC6Selection.ClockDivider = 1;
  RCC_ClkInitStruct.IC11Selection.ClockSelection = RCC_ICCLKSOURCE_PLL3;
  RCC_ClkInitStruct.IC11Selection.ClockDivider = 1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct) != HAL_OK) {
    Error_Handler();
  }
}

static void XSPI_Power_Config(void) {
  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_PWREx_EnableVddIO3();
  HAL_PWREx_ConfigVddIORange(PWR_VDDIO3, PWR_VDDIO_RANGE_1V8);
  HAL_PWREx_EnableVddIO2();
  HAL_PWREx_ConfigVddIORange(PWR_VDDIO2, PWR_VDDIO_RANGE_1V8);
}

static void XSPI_Config(void) {
  XSPIM_CfgTypeDef sXspiManagerCfg = {0};

  hxspi2.Instance = XSPI2;
  hxspi2.Init.FifoThresholdByte = 4;
  hxspi2.Init.MemoryMode = HAL_XSPI_SINGLE_MEM;
  hxspi2.Init.MemoryType = HAL_XSPI_MEMTYPE_MACRONIX;
  hxspi2.Init.MemorySize = HAL_XSPI_SIZE_1GB;
  hxspi2.Init.ChipSelectHighTimeCycle = 1;
  hxspi2.Init.FreeRunningClock = HAL_XSPI_FREERUNCLK_DISABLE;
  hxspi2.Init.ClockMode = HAL_XSPI_CLOCK_MODE_0;
  hxspi2.Init.WrapSize = HAL_XSPI_WRAP_NOT_SUPPORTED;
  hxspi2.Init.ClockPrescaler = 0;
  hxspi2.Init.SampleShifting = HAL_XSPI_SAMPLE_SHIFT_NONE;
  hxspi2.Init.DelayHoldQuarterCycle = HAL_XSPI_DHQC_ENABLE;
  hxspi2.Init.ChipSelectBoundary = HAL_XSPI_BONDARYOF_NONE;
  hxspi2.Init.MaxTran = 0;
  hxspi2.Init.Refresh = 0;
  hxspi2.Init.MemorySelect = HAL_XSPI_CSSEL_NCS1;
  if (HAL_XSPI_Init(&hxspi2) != HAL_OK) {
    Error_Handler();
  }
  sXspiManagerCfg.nCSOverride = HAL_XSPI_CSSEL_OVR_NCS1;
  sXspiManagerCfg.IOPort = HAL_XSPIM_IOPORT_2;
  sXspiManagerCfg.Req2AckTime = 1;
  if (HAL_XSPIM_Config(&hxspi2, &sXspiManagerCfg, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
    Error_Handler();
  }
}

static void LED_Config(void) {
  BSP_LED_Init(LED_GREEN);
  BSP_LED_Init(LED_RED);
  BSP_LED_On(LED_GREEN);
  BSP_LED_Off(LED_RED);
}
UNUSED
static void GPIO_Config(void) {
  __HAL_RCC_GPIOP_CLK_ENABLE();
  __HAL_RCC_GPIOO_CLK_ENABLE();
  __HAL_RCC_GPION_CLK_ENABLE();
}

/**
 * @brief  User OTP fuse Configuration
 *         The User Option Bytes are configured as follows :
 *            VDDIO_HSLV = 1 (enable the configuration of pads below 2.5V,
 *                            I/O speed optimization at low-voltage allowed)
 *            XSPI1_HSLV = 1 (enable I/O XSPIM Port 1 high-speed option)
 *            XSPI2_HSLV = 1 (enable I/O XSPIM Port 2 high-speed option)
 *            Other User Option Bytes remain unchanged
 * @retval None
 */
static void OTP_Config(void) {
  /* Enable required clocks */
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_RCC_BSEC_CLK_ENABLE();

  /* Initialize BSEC handler */
  BSEC_HandleTypeDef sBsecHandler;
  sBsecHandler.Instance = BSEC;

  /* Read current fuse value */
  uint32_t fuse_id = BSEC_HW_CONFIG_ID;
  uint32_t fuse_data;

  if (HAL_BSEC_OTP_Read(&sBsecHandler, fuse_id, &fuse_data) != HAL_OK) {
    Error_Handler();
    return;
  }

  /* Check if HSLV bits are already configured */
  if ((fuse_data & BSEC_HWS_HSLV_MASK) == BSEC_HWS_HSLV_MASK) {
    /* Configuration already set, no action needed */
    return;
  }

  /* Set HSLV bits */
  fuse_data |= BSEC_HWS_HSLV_MASK;

  /* Program the fuse */
  if (HAL_BSEC_OTP_Program(&sBsecHandler, fuse_id, fuse_data, HAL_BSEC_NORMAL_PROG) != HAL_OK) {
    Error_Handler();
    return;
  }

  /* Verify programming by reading back */
  uint32_t verify_data;
  if (HAL_BSEC_OTP_Read(&sBsecHandler, fuse_id, &verify_data) != HAL_OK) {
    Error_Handler();
    return;
  }

  /* Check if programming was successful */
  if ((verify_data & BSEC_HWS_HSLV_MASK) != BSEC_HWS_HSLV_MASK) {
    Error_Handler();
    return;
  }
}

/* ============================================================================
 * HAL MSP Functions
 * ============================================================================ */

void HAL_MspInit(void) {
  HAL_PWREx_EnableVddIO2();
  HAL_PWREx_EnableVddIO3();
  HAL_PWREx_EnableVddIO4();
  HAL_PWREx_EnableVddIO5();
}

void HAL_XSPI_MspInit(XSPI_HandleTypeDef *xspiHandle) {

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if (xspiHandle->Instance == XSPI2) {
    /** Initializes the peripherals clock */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_XSPI2;
    PeriphClkInitStruct.Xspi2ClockSelection = RCC_XSPI2CLKSOURCE_HCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
      Error_Handler();
    }

    /* XSPI2 clock enable */
    __HAL_RCC_XSPIM_CLK_ENABLE();
    __HAL_RCC_XSPI2_CLK_ENABLE();

    __HAL_RCC_GPION_CLK_ENABLE();
    /**XSPI2 GPIO Configuration
    PN4     ------> XSPIM_P2_IO2
    PN6     ------> XSPIM_P2_CLK
    PN8     ------> XSPIM_P2_IO4
    PN0     ------> XSPIM_P2_DQS0
    PN3     ------> XSPIM_P2_IO1
    PN5     ------> XSPIM_P2_IO3
    PN1     ------> XSPIM_P2_NCS1
    PN9     ------> XSPIM_P2_IO5
    PN2     ------> XSPIM_P2_IO0
    PN10     ------> XSPIM_P2_IO6
    PN11     ------> XSPIM_P2_IO7
    */
    GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_6 | GPIO_PIN_8 | GPIO_PIN_0 | GPIO_PIN_3 | GPIO_PIN_5 | GPIO_PIN_1 | GPIO_PIN_9 | GPIO_PIN_2 | GPIO_PIN_10 | GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_XSPIM_P2;
    HAL_GPIO_Init(GPION, &GPIO_InitStruct);
  }
}

void HAL_XSPI_MspDeInit(XSPI_HandleTypeDef *xspiHandle) {

  if (xspiHandle->Instance == XSPI2) {
    /* Peripheral clock disable */
    __HAL_RCC_XSPIM_CLK_DISABLE();
    __HAL_RCC_XSPI2_CLK_DISABLE();

    /**XSPI2 GPIO Configuration
    PN4     ------> XSPIM_P2_IO2
    PN6     ------> XSPIM_P2_CLK
    PN8     ------> XSPIM_P2_IO4
    PN0     ------> XSPIM_P2_DQS0
    PN3     ------> XSPIM_P2_IO1
    PN5     ------> XSPIM_P2_IO3
    PN1     ------> XSPIM_P2_NCS1
    PN9     ------> XSPIM_P2_IO5
    PN2     ------> XSPIM_P2_IO0
    PN10     ------> XSPIM_P2_IO6
    PN11     ------> XSPIM_P2_IO7
    */
    HAL_GPIO_DeInit(GPION, GPIO_PIN_4 | GPIO_PIN_6 | GPIO_PIN_8 | GPIO_PIN_0 | GPIO_PIN_3 | GPIO_PIN_5 | GPIO_PIN_1 | GPIO_PIN_9 | GPIO_PIN_2 | GPIO_PIN_10 | GPIO_PIN_11);
  }
}

/* ============================================================================
 * Debug Functions
 * ============================================================================ */

void Error_Handler(void) {
  BSP_LED_Off(LED_GREEN);
  BSP_LED_Off(LED_RED);

  /* Disable interrupts to prevent further execution */
  __disable_irq();

  for (;;) {
    BSP_LED_Toggle(LED_GREEN);
    for (volatile uint32_t i = 0; i < 4000000; i++) {
      __NOP();
    }
  }
}
