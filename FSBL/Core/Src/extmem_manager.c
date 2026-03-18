/**
 ******************************************************************************
 * @file           : extmem_manager.c
 * @version        : 1.0.0
 * @brief          : This file implements the extmem configuration
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

/* Includes ------------------------------------------------------------------*/
#include "extmem_manager.h"
#include "memory.h"

#define HEADER_V2_3_IMG_SIZE_OFFSET 108
#define HEADER_V2_3_SIZE            1024

uint32_t BOOT_GetApplicationSize(uint32_t img_addr) {
  uint32_t img_size;

  img_size = (*(uint32_t *)(img_addr + HEADER_V2_3_IMG_SIZE_OFFSET));
  img_size += HEADER_V2_3_SIZE;

  return img_size;
}

/**
 * Init External memory manager
 * @retval None
 */
void EXTMEM_MANAGER_Init(void) {

  /* Initialization of the memory parameters */
  memset(extmem_list_config, 0x0, sizeof(extmem_list_config));

  /* EXTMEMORY_1 */
  extmem_list_config[0].MemType = EXTMEM_NOR_SFDP;
  extmem_list_config[0].Handle = (void *)&hxspi2;
  extmem_list_config[0].ConfigType = EXTMEM_LINK_CONFIG_8LINES;

  EXTMEM_Init(EXTMEMORY_1, HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_XSPI2));
}
