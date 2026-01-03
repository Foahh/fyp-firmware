/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    xspi.c
  * @brief   This file provides code for the configuration
  *          of the XSPI instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "xspi.h"

/* USER CODE BEGIN 0 */
#define HSLV_OTP 124
#define VDDIO3_HSLV_MASK (1 << 15)
/* USER CODE END 0 */

XSPI_HandleTypeDef hxspi2;

/* XSPI2 init function */
void MX_XSPI2_Init(void)
{

  /* USER CODE BEGIN XSPI2_Init 0 */

  /* USER CODE END XSPI2_Init 0 */

  XSPIM_CfgTypeDef sXspiManagerCfg = {0};

  /* USER CODE BEGIN XSPI2_Init 1 */

  /* USER CODE END XSPI2_Init 1 */
  hxspi2.Instance = XSPI2;
  hxspi2.Init.FifoThresholdByte = 4;
  hxspi2.Init.MemoryMode = HAL_XSPI_SINGLE_MEM;
  hxspi2.Init.MemoryType = HAL_XSPI_MEMTYPE_MACRONIX;
  hxspi2.Init.MemorySize = HAL_XSPI_SIZE_1GB;
  hxspi2.Init.ChipSelectHighTimeCycle = 2;
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
  if (HAL_XSPI_Init(&hxspi2) != HAL_OK)
  {
    Error_Handler();
  }
  sXspiManagerCfg.nCSOverride = HAL_XSPI_CSSEL_OVR_NCS1;
  sXspiManagerCfg.IOPort = HAL_XSPIM_IOPORT_2;
  sXspiManagerCfg.Req2AckTime = 1;
  if (HAL_XSPIM_Config(&hxspi2, &sXspiManagerCfg, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN XSPI2_Init 2 */

  /* USER CODE END XSPI2_Init 2 */

}

void HAL_XSPI_MspInit(XSPI_HandleTypeDef* xspiHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(xspiHandle->Instance==XSPI2)
  {
  /* USER CODE BEGIN XSPI2_MspInit 0 */
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWREx_EnableVddIO3();
    HAL_PWREx_ConfigVddIORange(PWR_VDDIO3, PWR_VDDIO_RANGE_1V8);

    BSEC_HandleTypeDef hbsec;
    uint32_t fuse_data = 0;

    /* Enable BSEC & SYSCFG clocks to ensure BSEC data accesses */
    __HAL_RCC_BSEC_CLK_ENABLE();
    __HAL_RCC_SYSCFG_CLK_ENABLE();

    /* Set PWR configuration for IO speed optimization */
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWREx_EnableVddIO3();
    HAL_PWREx_ConfigVddIORange(PWR_VDDIO3, PWR_VDDIO_RANGE_1V8);

    hbsec.Instance = BSEC;
    if (HAL_BSEC_OTP_Read(&hbsec, HSLV_OTP, &fuse_data) != HAL_OK) {
      Error_Handler();
    }

    if (fuse_data & VDDIO3_HSLV_MASK) {
      /* High speed IO optimization is enabled */

      /*  Select IC3 clock from PLL1 at 200MHz (1200/6) as XSPI2 source */
      PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_XSPI2;
      PeriphClkInitStruct.Xspi2ClockSelection = RCC_XSPI2CLKSOURCE_IC3;
      PeriphClkInitStruct.ICSelection[RCC_IC3].ClockSelection = RCC_ICCLKSOURCE_PLL1;
      PeriphClkInitStruct.ICSelection[RCC_IC3].ClockDivider = 6;
      if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
      {
        Error_Handler();
      }

      /* Peripheral clock enable */
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
      GPIO_InitStruct.Pin = OCTOSPI_IO2_Pin|OCTOSPI_CLK_Pin|OCTOSPI_IO4_Pin|OCTOSPI_DQS_Pin
                            |OCTOSPI_IO1_Pin|OCTOSPI_IO3_Pin|GPIO_PIN_1|OCTOSPI_IO5_Pin
                            |OCTOSPI_IO0_Pin|OCTOSPI_IO6_Pin|OCTOSPI_IO7_Pin;
      GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
      GPIO_InitStruct.Pull = GPIO_NOPULL;
      GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
      GPIO_InitStruct.Alternate = GPIO_AF9_XSPIM_P2;
      HAL_GPIO_Init(GPION, &GPIO_InitStruct);
    } else {
      /* High speed IO optimization is disabled, lower XSPI clock speed */
      /*  Select IC3 clock from PLL1 at 50MHz (1200/24) as XSPI2 source */

  /* USER CODE END XSPI2_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_XSPI2;
    PeriphClkInitStruct.Xspi2ClockSelection = RCC_XSPI2CLKSOURCE_IC3;
    PeriphClkInitStruct.ICSelection[RCC_IC3].ClockSelection = RCC_ICCLKSOURCE_PLL1;
    PeriphClkInitStruct.ICSelection[RCC_IC3].ClockDivider = 24;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
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
    GPIO_InitStruct.Pin = OCTOSPI_IO2_Pin|OCTOSPI_CLK_Pin|OCTOSPI_IO4_Pin|OCTOSPI_DQS_Pin
                          |OCTOSPI_IO1_Pin|OCTOSPI_IO3_Pin|GPIO_PIN_1|OCTOSPI_IO5_Pin
                          |OCTOSPI_IO0_Pin|OCTOSPI_IO6_Pin|OCTOSPI_IO7_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_XSPIM_P2;
    HAL_GPIO_Init(GPION, &GPIO_InitStruct);

  /* USER CODE BEGIN XSPI2_MspInit 1 */
    }
  /* USER CODE END XSPI2_MspInit 1 */
  }
}

void HAL_XSPI_MspDeInit(XSPI_HandleTypeDef* xspiHandle)
{

  if(xspiHandle->Instance==XSPI2)
  {
  /* USER CODE BEGIN XSPI2_MspDeInit 0 */

  /* USER CODE END XSPI2_MspDeInit 0 */
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
    HAL_GPIO_DeInit(GPION, OCTOSPI_IO2_Pin|OCTOSPI_CLK_Pin|OCTOSPI_IO4_Pin|OCTOSPI_DQS_Pin
                          |OCTOSPI_IO1_Pin|OCTOSPI_IO3_Pin|GPIO_PIN_1|OCTOSPI_IO5_Pin
                          |OCTOSPI_IO0_Pin|OCTOSPI_IO6_Pin|OCTOSPI_IO7_Pin);

  /* USER CODE BEGIN XSPI2_MspDeInit 1 */

  /* USER CODE END XSPI2_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
