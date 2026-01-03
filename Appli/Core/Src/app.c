/**
 ******************************************************************************
 * @file    app.c
 * @author  Long Liangmao
 * @brief   Centralized buffer management for display and camera pipelines
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

#include "app.h"
#include "app_buffers.h"
#include "app_cam.h"
#include "app_config.h"
#include "app_lcd.h"
#include "cmw_camera.h"
#include "main.h"
#include "stm32n6570_discovery_errno.h"
#include "stm32n6570_discovery_xspi.h"
#include "stm32n6xx_hal.h"
#include "stm32n6xx_hal_rif.h"
#include "utils.h"
#include <assert.h>


static void Security_Config(void);
static void XSPI_Config(void);
static void IAC_Config(void);

static void IAC_Config(void) {
  /* Configure IAC to trap illegal access events */
  __HAL_RCC_IAC_CLK_ENABLE();
  __HAL_RCC_IAC_FORCE_RESET();
  __HAL_RCC_IAC_RELEASE_RESET();
}

static void XSPI_Config(void) {
  int32_t ret = BSP_ERROR_NONE;

  ret = BSP_XSPI_RAM_Init(0);
  assert(ret == BSP_ERROR_NONE);

  ret = BSP_XSPI_RAM_EnableMemoryMappedMode(0);
  assert(ret == BSP_ERROR_NONE);

  // BSP_XSPI_NOR_Init_t NOR_Init;
  // NOR_Init.InterfaceMode = BSP_XSPI_NOR_OPI_MODE;
  // NOR_Init.TransferRate = BSP_XSPI_NOR_DTR_TRANSFER;

  // ret = BSP_XSPI_NOR_Init(0, &NOR_Init);
  // assert(ret == BSP_ERROR_NONE);

  // ret = BSP_XSPI_NOR_EnableMemoryMappedMode(0);
  // assert(ret == BSP_ERROR_NONE);
}

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

static void LED_Config(void) {
  BSP_LED_Init(LED_GREEN);
  BSP_LED_Init(LED_RED);
  BSP_LED_Off(LED_GREEN);
  BSP_LED_Off(LED_RED);
}

void App_Init(VOID *memory_ptr) {
  LED_Config();

  XSPI_Config();
  Security_Config();
  IAC_Config();
  Buffer_Init();

  LCD_Init();
  CAM_InitIspSemaphore();
  CAM_Init();
  Thread_IspUpdate_Init(memory_ptr);
  CAM_DisplayPipe_Start(CMW_MODE_CONTINUOUS);
}
