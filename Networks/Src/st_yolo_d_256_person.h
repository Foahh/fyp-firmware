/**
  ******************************************************************************
  * @file    st_yolo_d_256_person.h
  * @author  STEdgeAI
  * @date    2026-04-06 23:10:16
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
#ifndef LL_ATON_ST_YOLO_D_256_PERSON_H
#define LL_ATON_ST_YOLO_D_256_PERSON_H

/******************************************************************************/
#define LL_ATON_ST_YOLO_D_256_PERSON_C_MODEL_NAME        "st_yolo_d_256_person"
#define LL_ATON_ST_YOLO_D_256_PERSON_ORIGIN_MODEL_NAME   "st_yolodv2milli_actrelu_pt_coco_person_256_qdq_int8"

/************************** USER ALLOCATED IOs ********************************/
#define LL_ATON_ST_YOLO_D_256_PERSON_USER_ALLOCATED_INPUTS   (1)  // Number of input buffers not allocated by the compiler
#define LL_ATON_ST_YOLO_D_256_PERSON_USER_ALLOCATED_OUTPUTS  (1)  // Number of output buffers not allocated by the compiler

/************************** INPUTS ********************************************/
#define LL_ATON_ST_YOLO_D_256_PERSON_IN_NUM        (1)    // Total number of input buffers
// Input buffer 1 -- Input_0_out_0
#define LL_ATON_ST_YOLO_D_256_PERSON_IN_1_ALIGNMENT   (32)
#define LL_ATON_ST_YOLO_D_256_PERSON_IN_1_SIZE_BYTES  (196608)

/************************** OUTPUTS *******************************************/
#define LL_ATON_ST_YOLO_D_256_PERSON_OUT_NUM        (1)    // Total number of output buffers
// Output buffer 1 -- Transpose_659_out_0
#define LL_ATON_ST_YOLO_D_256_PERSON_OUT_1_ALIGNMENT   (32)
#define LL_ATON_ST_YOLO_D_256_PERSON_OUT_1_SIZE_BYTES  (8064)

#endif /* LL_ATON_ST_YOLO_D_256_PERSON_H */
