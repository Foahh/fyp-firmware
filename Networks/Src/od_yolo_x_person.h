/**
  ******************************************************************************
  * @file    od_yolo_x_person.h
  * @author  STEdgeAI
  * @date    2026-01-23 23:12:11
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
#ifndef LL_ATON_OD_YOLO_X_PERSON_H
#define LL_ATON_OD_YOLO_X_PERSON_H

/******************************************************************************/
#define LL_ATON_OD_YOLO_X_PERSON_C_MODEL_NAME        "od_yolo_x_person"
#define LL_ATON_OD_YOLO_X_PERSON_ORIGIN_MODEL_NAME   "st_yolo_x_nano_480_1_0_0_25_3_int8"

/************************** USER ALLOCATED IOs ********************************/
#define LL_ATON_OD_YOLO_X_PERSON_USER_ALLOCATED_INPUTS   (1)  // Number of input buffers not allocated by the compiler
#define LL_ATON_OD_YOLO_X_PERSON_USER_ALLOCATED_OUTPUTS  (3)  // Number of output buffers not allocated by the compiler

/************************** INPUTS ********************************************/
#define LL_ATON_OD_YOLO_X_PERSON_IN_NUM        (1)    // Total number of input buffers
// Input buffer 1 -- Input_0_out_0
#define LL_ATON_OD_YOLO_X_PERSON_IN_1_ALIGNMENT   (32)
#define LL_ATON_OD_YOLO_X_PERSON_IN_1_SIZE_BYTES  (691200)

/************************** OUTPUTS *******************************************/
#define LL_ATON_OD_YOLO_X_PERSON_OUT_NUM        (3)    // Total number of output buffers
// Output buffer 1 -- Transpose_782_out_0
#define LL_ATON_OD_YOLO_X_PERSON_OUT_1_ALIGNMENT   (32)
#define LL_ATON_OD_YOLO_X_PERSON_OUT_1_SIZE_BYTES  (4050)
// Output buffer 2 -- Transpose_886_out_0
#define LL_ATON_OD_YOLO_X_PERSON_OUT_2_ALIGNMENT   (32)
#define LL_ATON_OD_YOLO_X_PERSON_OUT_2_SIZE_BYTES  (64800)
// Output buffer 3 -- Transpose_834_out_0
#define LL_ATON_OD_YOLO_X_PERSON_OUT_3_ALIGNMENT   (32)
#define LL_ATON_OD_YOLO_X_PERSON_OUT_3_SIZE_BYTES  (16200)

#endif /* LL_ATON_OD_YOLO_X_PERSON_H */
