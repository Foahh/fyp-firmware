/**
 ******************************************************************************
 * @file    init_peripherals.c
 * @brief   Peripheral initialization (GPIO, SMPS, IAC, XSPI, LED, COM, NPU,
 *          priority, system isolation, debug reset)
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

#include "stm32n6570_discovery.h"
#include "stm32n6570_discovery_errno.h"
#include "stm32n6570_discovery_xspi.h"
#include "stm32n6xx_hal_ramcfg.h"
#include "stm32n6xx_hal_rif.h"

#include "error.h"
#include "init_peripherals.h"
#include "npu_cache.h"
#include "power_mode.h"

/* GPDMA1 handle for USART1 TX DMA (referenced by GPDMA1_Channel0_IRQHandler) */
DMA_HandleTypeDef hdma_usart1_tx;

void SystemIsolation_Config(void) {
  /* set all required IPs as secure privileged */
  __HAL_RCC_RIFSC_CLK_ENABLE();

  /*RIMC configuration*/
  RIMC_MasterConfig_t RIMC_master = {0};
  RIMC_master.MasterCID = RIF_CID_1;
  RIMC_master.SecPriv = RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV;
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_DCMIPP, &RIMC_master);

  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_DMA2D, &RIMC_master);

  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_LTDC1, &RIMC_master);

  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_LTDC2, &RIMC_master);
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_NPU, &RIMC_master);

  /*RISUP configuration*/
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_CSI, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_DCMIPP, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_DMA2D, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_LTDC, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_LTDCL1, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_LTDCL2, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_NPU, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
}

void SMPS_Config(void) {
#if POWER_MODE == POWER_MODE_OVERDRIVE
  BSP_SMPS_Init(SMPS_VOLTAGE_OVERDRIVE);
#else
  BSP_SMPS_Init(SMPS_VOLTAGE_NOMINAL);
#endif
  HAL_Delay(1);
}

void IAC_Config(void) {
  /* Configure IAC to trap illegal access events */
  __HAL_RCC_IAC_CLK_ENABLE();
  __HAL_RCC_IAC_FORCE_RESET();
  __HAL_RCC_IAC_RELEASE_RESET();
}

void XSPI_Config(void) {
  int32_t ret = BSP_ERROR_NONE;

  ret = BSP_XSPI_RAM_Init(0);
  APP_REQUIRE(ret == BSP_ERROR_NONE);

  ret = BSP_XSPI_RAM_EnableMemoryMappedMode(0);
  APP_REQUIRE(ret == BSP_ERROR_NONE);

  BSP_XSPI_NOR_Init_t NOR_Init;
  NOR_Init.InterfaceMode = BSP_XSPI_NOR_OPI_MODE;
  NOR_Init.TransferRate = BSP_XSPI_NOR_DTR_TRANSFER;
  ret = BSP_XSPI_NOR_Init(0, &NOR_Init);
  APP_REQUIRE(ret == BSP_ERROR_NONE);

  ret = BSP_XSPI_NOR_EnableMemoryMappedMode(0);
  APP_REQUIRE(ret == BSP_ERROR_NONE);
}

void LED_Config(void) {
  BSP_LED_Init(LED_GREEN);
  BSP_LED_Init(LED_RED);
  BSP_LED_Off(LED_GREEN);
  BSP_LED_Off(LED_RED);
}

#if (USE_BSP_COM_FEATURE > 0)
void COM_Config(void) {
  COM_InitTypeDef COM_Init = {0};
  int32_t ret = BSP_ERROR_NONE;

  /* Configure COM1 (USART1) for logging */
  COM_Init.BaudRate = 921600U;
  COM_Init.WordLength = COM_WORDLENGTH_8B;
  COM_Init.StopBits = COM_STOPBITS_1;
  COM_Init.Parity = COM_PARITY_NONE;
  COM_Init.HwFlowCtl = COM_HWCONTROL_NONE;

  ret = BSP_COM_Init(COM1, &COM_Init);
  APP_REQUIRE(ret == BSP_ERROR_NONE);

  /* USART1 interrupt (needed for DMA completion callbacks) */
  HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);

  /* GPDMA1 Channel 0 → USART1 TX */
  __HAL_RCC_GPDMA1_CLK_ENABLE();

  hdma_usart1_tx.Instance = GPDMA1_Channel0;
  hdma_usart1_tx.Init.Request = GPDMA1_REQUEST_USART1_TX;
  hdma_usart1_tx.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
  hdma_usart1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
  hdma_usart1_tx.Init.SrcInc = DMA_SINC_INCREMENTED;
  hdma_usart1_tx.Init.DestInc = DMA_DINC_FIXED;
  hdma_usart1_tx.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_BYTE;
  hdma_usart1_tx.Init.DestDataWidth = DMA_DEST_DATAWIDTH_BYTE;
  hdma_usart1_tx.Init.Priority = DMA_LOW_PRIORITY_LOW_WEIGHT;
  hdma_usart1_tx.Init.SrcBurstLength = 1;
  hdma_usart1_tx.Init.DestBurstLength = 1;
  hdma_usart1_tx.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT1 | DMA_DEST_ALLOCATED_PORT1;
  hdma_usart1_tx.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
  hdma_usart1_tx.Init.Mode = DMA_NORMAL;
  APP_REQUIRE(HAL_DMA_Init(&hdma_usart1_tx) == HAL_OK);

  __HAL_LINKDMA(&hcom_uart[COM1], hdmatx, hdma_usart1_tx);

  HAL_NVIC_SetPriority(GPDMA1_Channel0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(GPDMA1_Channel0_IRQn);

#if (USE_COM_LOG > 0)
  /* Select COM1 as the logging port */
  ret = BSP_COM_SelectLogPort(COM1);
  APP_REQUIRE(ret == BSP_ERROR_NONE);
#endif /* USE_COM_LOG */
}
#endif /* USE_BSP_COM_FEATURE */

void NPU_Config(void) {
  __HAL_RCC_NPU_CLK_ENABLE();
  __HAL_RCC_NPU_FORCE_RESET();
  __HAL_RCC_NPU_RELEASE_RESET();

  /* Enable NPU RAMs (4x448KB) */
  __HAL_RCC_AXISRAM3_MEM_CLK_ENABLE();
  __HAL_RCC_AXISRAM4_MEM_CLK_ENABLE();
  __HAL_RCC_AXISRAM5_MEM_CLK_ENABLE();
  __HAL_RCC_AXISRAM6_MEM_CLK_ENABLE();
  __HAL_RCC_RAMCFG_CLK_ENABLE();
  RAMCFG_HandleTypeDef hramcfg = {0};
  hramcfg.Instance = RAMCFG_SRAM3_AXI;
  HAL_RAMCFG_EnableAXISRAM(&hramcfg);
  hramcfg.Instance = RAMCFG_SRAM4_AXI;
  HAL_RAMCFG_EnableAXISRAM(&hramcfg);
  hramcfg.Instance = RAMCFG_SRAM5_AXI;
  HAL_RAMCFG_EnableAXISRAM(&hramcfg);
  hramcfg.Instance = RAMCFG_SRAM6_AXI;
  HAL_RAMCFG_EnableAXISRAM(&hramcfg);
}

void npu_cache_enable_clocks_and_reset(void) {
  __HAL_RCC_CACHEAXIRAM_MEM_CLK_ENABLE();
  __HAL_RCC_CACHEAXI_CLK_ENABLE();
  __HAL_RCC_CACHEAXI_FORCE_RESET();
  __HAL_RCC_CACHEAXI_RELEASE_RESET();
}

void npu_cache_disable_clocks_and_reset(void) {
  __HAL_RCC_CACHEAXIRAM_MEM_CLK_DISABLE();
  __HAL_RCC_CACHEAXI_CLK_DISABLE();
  __HAL_RCC_CACHEAXI_FORCE_RESET();
}

void GPIO_Config(void) {
  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
}

void Button_Config(void) {
  BSP_PB_Init(BUTTON_USER1, BUTTON_MODE_EXTI);
}
