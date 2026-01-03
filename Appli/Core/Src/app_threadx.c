/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_threadx.c
  * @author  MCD Application Team
  * @brief   ThreadX applicative file
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
#include "app_threadx.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_buffers.h"
#include "app_cam.h"
#include "app_config.h"
#include "app_lcd.h"
#include "cmw_camera.h"
#include "main.h"
#include "stm32n6570_discovery_xspi.h"
#include "stm32n6xx_hal.h"
#include "stm32n6xx_hal_rif.h"
#include "utils.h"
#include <assert.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static void Security_Config(void);
/* USER CODE END PFP */

/**
  * @brief  Application ThreadX Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT App_ThreadX_Init(VOID *memory_ptr)
{
  UINT ret = TX_SUCCESS;
  /* USER CODE BEGIN App_ThreadX_MEM_POOL */

  /* USER CODE END App_ThreadX_MEM_POOL */
  /* USER CODE BEGIN App_ThreadX_Init */

  ret = BSP_XSPI_RAM_Init(0);
  if (ret != BSP_ERROR_NONE) {
    return TX_NOT_AVAILABLE;
  }

  ret = BSP_XSPI_RAM_EnableMemoryMappedMode(0);
  if (ret != BSP_ERROR_NONE) {
    return ret;
  }

  Security_Config();

  Buffer_Init();

  ret = LCD_Init();
  if (ret != 0) {
    return ret;
  }

  ret = CAM_InitIspSemaphore();
  if (ret != TX_SUCCESS) {
    return ret;
  }

  ret = CAM_Init();
  if (ret != 0) {
    return ret;
  }

  ret = Thread_IspUpdate_Init(memory_ptr);
  if (ret != TX_SUCCESS) {
    return ret;
  }

  CAM_DisplayPipe_Start(CMW_MODE_CONTINUOUS);

  /* USER CODE END App_ThreadX_Init */

  return ret;
}

/**
  * @brief  Function that implements the kernel's initialization.
  * @param  None
  * @retval None
  */
void MX_ThreadX_Init(void)
{
  /* USER CODE BEGIN Before_Kernel_Start */

  /* USER CODE END Before_Kernel_Start */

  tx_kernel_enter();

  /* USER CODE BEGIN Kernel_Start_Error */

  /* USER CODE END Kernel_Start_Error */
}

/* USER CODE BEGIN 1 */

/**
 * @brief  Configure RIF security for peripheral DMA masters
 * @retval None
 */
static void Security_Config() {
  __HAL_RCC_RIFSC_CLK_ENABLE();
  RIMC_MasterConfig_t RIMC_master = {0};
  RIMC_master.MasterCID = RIF_CID_1;
  RIMC_master.SecPriv = RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV;

  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_NPU, &RIMC_master);
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_DMA2D, &RIMC_master);
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_DCMIPP, &RIMC_master);
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_LTDC1, &RIMC_master);
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_LTDC2, &RIMC_master);
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_OTG1, &RIMC_master);

  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_NPU, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_DMA2D, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_CSI, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_DCMIPP, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_LTDC, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_LTDCL1, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_LTDCL2, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_OTG1HS, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_SPI5, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
}

uint32_t HAL_GetTick(void)
{
  return (tx_time_get() * 1000) / TX_TIMER_TICKS_PER_SECOND;
}

void HAL_Delay(uint32_t Delay)
{
  assert(!IS_IRQ_MODE());

  uint32_t ticks = (Delay * TX_TIMER_TICKS_PER_SECOND) / 1000;
  tx_thread_sleep(ticks);
}

HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
  UNUSED(TickPriority);
  return HAL_OK;
}
/* USER CODE END 1 */
