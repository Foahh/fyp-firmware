/**
  ******************************************************************************
  * @file    yolo26_320_fyp_merged.h
  * @author  STEdgeAI
  * @date    2026-04-08 14:53:33
  * @brief   Minimal description of the generated c-implemention of the network
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  ******************************************************************************
  */
#ifndef LL_ATON_YOLO26_320_FYP_MERGED_H
#define LL_ATON_YOLO26_320_FYP_MERGED_H

/******************************************************************************/
#define LL_ATON_YOLO26_320_FYP_MERGED_C_MODEL_NAME        "yolo26_320_fyp_merged"
#define LL_ATON_YOLO26_320_FYP_MERGED_ORIGIN_MODEL_NAME   "yolo26_320_quant_pc_ui_fyp_merged"

/************************** USER ALLOCATED IOs ********************************/
#define LL_ATON_YOLO26_320_FYP_MERGED_USER_ALLOCATED_INPUTS   (1)  // Number of input buffers not allocated by the compiler
#define LL_ATON_YOLO26_320_FYP_MERGED_USER_ALLOCATED_OUTPUTS  (1)  // Number of output buffers not allocated by the compiler

/************************** INPUTS ********************************************/
#define LL_ATON_YOLO26_320_FYP_MERGED_IN_NUM        (1)    // Total number of input buffers
// Input buffer 1 -- Input_4_out_0
#define LL_ATON_YOLO26_320_FYP_MERGED_IN_1_ALIGNMENT   (32)
#define LL_ATON_YOLO26_320_FYP_MERGED_IN_1_SIZE_BYTES  (307200)

/************************** OUTPUTS *******************************************/
#define LL_ATON_YOLO26_320_FYP_MERGED_OUT_NUM        (1)    // Total number of output buffers
// Output buffer 1 -- Transpose_936_out_0
#define LL_ATON_YOLO26_320_FYP_MERGED_OUT_1_ALIGNMENT   (32)
#define LL_ATON_YOLO26_320_FYP_MERGED_OUT_1_SIZE_BYTES  (12600)

#endif /* LL_ATON_YOLO26_320_FYP_MERGED_H */
