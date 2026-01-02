/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    dcmipp.c
  * @brief   This file provides code for the configuration
  *          of the DCMIPP instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "dcmipp.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/* USER CODE BEGIN 2 */

/**
  * @brief  DCMIPP Clock Configuration for Camera Middleware
  * @param  hdcmipp: DCMIPP handle pointer
  * @retval HAL_StatusTypeDef
  */
HAL_StatusTypeDef MX_DCMIPP_ClockConfig(DCMIPP_HandleTypeDef *hdcmipp)
{
  UNUSED(hdcmipp);
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_DCMIPP;
  PeriphClkInitStruct.DcmippClockSelection = RCC_DCMIPPCLKSOURCE_IC17;
  PeriphClkInitStruct.ICSelection[RCC_IC17].ClockSelection = RCC_ICCLKSOURCE_PLL1;
  PeriphClkInitStruct.ICSelection[RCC_IC17].ClockDivider = 4;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    return HAL_ERROR;
  }

  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_CSI;
  PeriphClkInitStruct.ICSelection[RCC_IC18].ClockSelection = RCC_ICCLKSOURCE_PLL1;
  PeriphClkInitStruct.ICSelection[RCC_IC18].ClockDivider = 60;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return HAL_OK;
}

/* USER CODE END 2 */

