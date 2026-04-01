/**
 ******************************************************************************
 * @file    model_st_yolodv2milli_256_coco_person.h
 * @brief   Model config: ST YOLODv2Milli (256×256, INT8, COCO person detection)
 ******************************************************************************
 */
#ifndef MODEL_ST_YOLODV2MILLI_256_COCO_PERSON_H
#define MODEL_ST_YOLODV2MILLI_256_COCO_PERSON_H

#include "st_yolo_d_256_person.h"
#include "stai_st_yolo_d_256_person.h"
#include "utils.h"

/* Network binding */
#define MDL_NN_NETWORK_NAME st_yolo_d_256_person
#define MDL_NN_IN_WIDTH     STAI_ST_YOLO_D_256_PERSON_IN_1_WIDTH
#define MDL_NN_IN_HEIGHT    STAI_ST_YOLO_D_256_PERSON_IN_1_HEIGHT
#define MDL_NN_IN_CHANNELS  STAI_ST_YOLO_D_256_PERSON_IN_1_CHANNEL
#define MDL_NN_OUT_NUM      STAI_ST_YOLO_D_256_PERSON_OUT_NUM
#define MDL_NN_CTX_SIZE     STAI_ST_YOLO_D_256_PERSON_CONTEXT_SIZE

/* Single output tensor */
#define MDL_NN_OUT_1_SIZE      STAI_ST_YOLO_D_256_PERSON_OUT_1_SIZE_BYTES
#define MDL_NN_OUT_1_ALIGNMENT STAI_ST_YOLO_D_256_PERSON_OUT_1_ALIGNMENT

/* Output buffer layout (model-owned, consumed by nn_thread.c) */
#define MDL_NN_OUT_BUFFER_SIZE ALIGN_VALUE(MDL_NN_OUT_1_SIZE, MDL_NN_OUT_1_ALIGNMENT)
#define MDL_NN_OUT_SIZES       {MDL_NN_OUT_1_SIZE}

/* Post-processing */
#define MDL_PP_TYPE           POSTPROCESS_OD_ST_YOLOD_UI
#define MDL_PP_NB_CLASSES     1
#define MDL_PP_MAX_BOXES      10
#define MDL_PP_CONF_THRESHOLD 0.25f
#define MDL_PP_IOU_THRESHOLD  0.5f
/* Strides 8/16/32 → 32×32 + 16×16 + 8×8 = 1344 rows in raw output */
#define MDL_PP_STRIDE_0 8
#define MDL_PP_STRIDE_1 16
#define MDL_PP_STRIDE_2 32

/* Display name */
#define MDL_DISPLAY_NAME LL_ATON_ST_YOLO_D_256_PERSON_ORIGIN_MODEL_NAME

/* Class labels */
static const char *MDL_PP_CLASS_LABELS[] __attribute__((unused)) = {"person"};

#endif /* MODEL_ST_YOLODV2MILLI_256_COCO_PERSON_H */
