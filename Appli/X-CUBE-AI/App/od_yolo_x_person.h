/**
  ******************************************************************************
  * @file    od_yolo_x_person.h
  * @author  STEdgeAI
  * @date    2026-01-04 22:22:45
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
// No user allocated inputs
// No user allocated outputs

/************************** INPUTS ********************************************/
#define LL_ATON_OD_YOLO_X_PERSON_IN_NUM        (1)    // Total number of input buffers
// Input buffer 1 -- Input_0_out_0
#define LL_ATON_OD_YOLO_X_PERSON_IN_1_ALIGNMENT   (32)
#define LL_ATON_OD_YOLO_X_PERSON_IN_1_SIZE_BYTES  (691200)

/************************** OUTPUTS *******************************************/
#define LL_ATON_OD_YOLO_X_PERSON_OUT_NUM        (3)    // Total number of output buffers
// Output buffer 1 -- Transpose_783_out_0
#define LL_ATON_OD_YOLO_X_PERSON_OUT_1_ALIGNMENT   (32)
#define LL_ATON_OD_YOLO_X_PERSON_OUT_1_SIZE_BYTES  (16200)
// Output buffer 2 -- Transpose_889_out_0
#define LL_ATON_OD_YOLO_X_PERSON_OUT_2_ALIGNMENT   (32)
#define LL_ATON_OD_YOLO_X_PERSON_OUT_2_SIZE_BYTES  (259200)
// Output buffer 3 -- Transpose_836_out_0
#define LL_ATON_OD_YOLO_X_PERSON_OUT_3_ALIGNMENT   (32)
#define LL_ATON_OD_YOLO_X_PERSON_OUT_3_SIZE_BYTES  (64800)

#endif /* LL_ATON_OD_YOLO_X_PERSON_H */
