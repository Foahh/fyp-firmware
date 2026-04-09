/**
  ******************************************************************************
  * @file    tinyv8_320_person.h
  * @author  STEdgeAI
  * @date    2026-04-09 21:57:44
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
#ifndef LL_ATON_TINYV8_320_PERSON_H
#define LL_ATON_TINYV8_320_PERSON_H

/******************************************************************************/
#define LL_ATON_TINYV8_320_PERSON_C_MODEL_NAME        "tinyv8_320_person"
#define LL_ATON_TINYV8_320_PERSON_ORIGIN_MODEL_NAME   "tinyissimoyolo_v8_320_quant_pc_ui_coco_person"

/************************** USER ALLOCATED IOs ********************************/
#define LL_ATON_TINYV8_320_PERSON_USER_ALLOCATED_INPUTS   (1)  // Number of input buffers not allocated by the compiler
#define LL_ATON_TINYV8_320_PERSON_USER_ALLOCATED_OUTPUTS  (1)  // Number of output buffers not allocated by the compiler

/************************** INPUTS ********************************************/
#define LL_ATON_TINYV8_320_PERSON_IN_NUM        (1)    // Total number of input buffers
// Input buffer 1 -- Input_3_out_0
#define LL_ATON_TINYV8_320_PERSON_IN_1_ALIGNMENT   (32)
#define LL_ATON_TINYV8_320_PERSON_IN_1_SIZE_BYTES  (307200)

/************************** OUTPUTS *******************************************/
#define LL_ATON_TINYV8_320_PERSON_OUT_NUM        (1)    // Total number of output buffers
// Output buffer 1 -- Transpose_576_out_0
#define LL_ATON_TINYV8_320_PERSON_OUT_1_ALIGNMENT   (32)
#define LL_ATON_TINYV8_320_PERSON_OUT_1_SIZE_BYTES  (10500)

#endif /* LL_ATON_TINYV8_320_PERSON_H */
