/**
 ******************************************************************************
 * @file    model_yolox_nano_480_coco_person.h
 * @author  Long Liangmao
 * @brief   Model config: YOLO-X Nano (480×480, INT8, COCO person detection)
 ******************************************************************************
 */
#ifndef MODEL_YOLOX_NANO_480_COCO_PERSON_H
#define MODEL_YOLOX_NANO_480_COCO_PERSON_H

#include "arm_math.h"
#include "st_yolo_x_480_person.h"
#include "stai_st_yolo_x_480_person.h"
#include "utils.h"

/* Network binding */
#define MDL_NN_NETWORK_NAME st_yolo_x_480_person
#define MDL_NN_IN_WIDTH     STAI_ST_YOLO_X_480_PERSON_IN_1_WIDTH
#define MDL_NN_IN_HEIGHT    STAI_ST_YOLO_X_480_PERSON_IN_1_HEIGHT
#define MDL_NN_IN_CHANNELS  STAI_ST_YOLO_X_480_PERSON_IN_1_CHANNEL
#define MDL_NN_OUT_NUM      STAI_ST_YOLO_X_480_PERSON_OUT_NUM
#define MDL_NN_CTX_SIZE     STAI_ST_YOLO_X_480_PERSON_CONTEXT_SIZE

/* Output buffer sizes */
#define MDL_NN_OUT_1_SIZE      STAI_ST_YOLO_X_480_PERSON_OUT_1_SIZE_BYTES
#define MDL_NN_OUT_1_ALIGNMENT STAI_ST_YOLO_X_480_PERSON_OUT_1_ALIGNMENT
#define MDL_NN_OUT_2_SIZE      STAI_ST_YOLO_X_480_PERSON_OUT_2_SIZE_BYTES
#define MDL_NN_OUT_2_ALIGNMENT STAI_ST_YOLO_X_480_PERSON_OUT_2_ALIGNMENT
#define MDL_NN_OUT_3_SIZE      STAI_ST_YOLO_X_480_PERSON_OUT_3_SIZE_BYTES
#define MDL_NN_OUT_3_ALIGNMENT STAI_ST_YOLO_X_480_PERSON_OUT_3_ALIGNMENT

/* Output buffer layout (model-owned, consumed by nn_thread.c) */
#define MDL_NN_OUT_BUFFER_SIZE                              \
  (ALIGN_VALUE(MDL_NN_OUT_1_SIZE, MDL_NN_OUT_1_ALIGNMENT) + \
   ALIGN_VALUE(MDL_NN_OUT_2_SIZE, MDL_NN_OUT_2_ALIGNMENT) + \
   ALIGN_VALUE(MDL_NN_OUT_3_SIZE, MDL_NN_OUT_3_ALIGNMENT))
#define MDL_NN_OUT_SIZES {MDL_NN_OUT_1_SIZE, MDL_NN_OUT_2_SIZE, MDL_NN_OUT_3_SIZE}

/* Post-processing */
#define MDL_PP_TYPE           POSTPROCESS_OD_ST_YOLOX_UI
#define MDL_PP_NB_CLASSES     1
#define MDL_PP_CONF_THRESHOLD 0.5f
#define MDL_PP_IOU_THRESHOLD  0.5f
#define MDL_PP_NB_ANCHORS     3

/* Grid sizes (480/8, 480/16, 480/32) */
#define MDL_PP_L_GRID_W 60
#define MDL_PP_L_GRID_H 60
#define MDL_PP_M_GRID_W 30
#define MDL_PP_M_GRID_H 30
#define MDL_PP_S_GRID_W 15
#define MDL_PP_S_GRID_H 15

/* Anchors (library references these names directly) */
static const float32_t AI_OD_ST_YOLOX_PP_L_ANCHORS[2 * MDL_PP_NB_ANCHORS] = {30.0f, 30.0f, 4.2f, 15.0f, 13.8f, 42.0f};
static const float32_t AI_OD_ST_YOLOX_PP_M_ANCHORS[2 * MDL_PP_NB_ANCHORS] = {15.0f, 15.0f, 2.1f, 7.5f, 6.9f, 21.0f};
static const float32_t AI_OD_ST_YOLOX_PP_S_ANCHORS[2 * MDL_PP_NB_ANCHORS] = {7.5f, 7.5f, 1.05f, 3.75f, 3.45f, 10.5f};

/* Display name */
#define MDL_DISPLAY_NAME LL_ATON_ST_YOLO_X_480_PERSON_ORIGIN_MODEL_NAME

/* Class labels */
#define MDL_PP_CLASS_LABEL_COUNT 1
#define MDL_PP_CLASS_LABEL_0     "person"

static const char *MDL_PP_CLASS_LABELS[] __attribute__((unused)) = {MDL_PP_CLASS_LABEL_0};

#endif /* MODEL_YOLOX_NANO_480_COCO_PERSON_H */
