/**
 ******************************************************************************
 * @file    app_azure_rtos.c
 * @author  MCD Application Team
 * @brief   app_azure_rtos application implementation file
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#include "app_azure_rtos.h"

#include "app_error.h"
#include "main.h"

#if (USE_STATIC_ALLOCATION == 1)

__ALIGN_BEGIN static UCHAR tx_byte_pool_buffer[TX_APP_MEM_POOL_SIZE] __ALIGN_END;
static TX_BYTE_POOL tx_app_byte_pool;

#endif

/**
 * @brief  Define the initial system.
 * @param  first_unused_memory : Pointer to the first unused memory
 * @retval None
 */
__attribute__((used)) VOID tx_application_define(VOID *first_unused_memory) {
#if (USE_STATIC_ALLOCATION == 1)
  UINT status = TX_SUCCESS;
  VOID *memory_ptr;

  status = tx_byte_pool_create(&tx_app_byte_pool, "tx_app_memory_pool", tx_byte_pool_buffer, TX_APP_MEM_POOL_SIZE);
  APP_REQUIRE(status == TX_SUCCESS);

  memory_ptr = (VOID *)&tx_app_byte_pool;
  status = ThreadX_Start(memory_ptr);
  APP_REQUIRE(status == TX_SUCCESS);
#else
  (void)first_unused_memory;
#endif
}
