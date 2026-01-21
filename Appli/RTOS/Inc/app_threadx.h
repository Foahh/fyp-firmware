/**
  ******************************************************************************
  * @file    app_threadx.h
  * @author  MCD Application Team
  * @brief   ThreadX applicative header file
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
  
#ifndef __APP_THREADX_H
#define __APP_THREADX_H
#ifdef __cplusplus
 extern "C" {
#endif

#include "tx_api.h"

UINT ThreadX_Start(VOID *memory_ptr);
void ThreadX_Init(void);

#ifdef __cplusplus
}
#endif
#endif /* __APP_THREADX_H */
