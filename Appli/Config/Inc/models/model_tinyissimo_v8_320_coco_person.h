/**
 ******************************************************************************
 * @file    model_tinyissimo_v8_320_coco_person.h
 * @brief   Model config: TinyIssimo YOLOv8 (320×320, INT8, COCO person OD)
 * @note    Raw head matches YOLOv8: single tensor [1, 4+nb_classes, nb_anchors].
 ******************************************************************************
 */
#ifndef MODEL_TINYISSIMO_V8_320_COCO_PERSON_H
#define MODEL_TINYISSIMO_V8_320_COCO_PERSON_H

#include "stai_tinyv8_320_person.h"
#include "utils.h"
#include "tinyv8_320_person.h"

/* Network binding */
#define MDL_NN_NETWORK_NAME tinyv8_320_person
#define MDL_NN_IN_WIDTH     STAI_TINYV8_320_PERSON_IN_1_WIDTH
#define MDL_NN_IN_HEIGHT    STAI_TINYV8_320_PERSON_IN_1_HEIGHT
#define MDL_NN_IN_CHANNELS  STAI_TINYV8_320_PERSON_IN_1_CHANNEL
#define MDL_NN_OUT_NUM      STAI_TINYV8_320_PERSON_OUT_NUM
#define MDL_NN_CTX_SIZE     STAI_TINYV8_320_PERSON_CONTEXT_SIZE

#define MDL_NN_OUT_1_SIZE      STAI_TINYV8_320_PERSON_OUT_1_SIZE_BYTES
#define MDL_NN_OUT_1_ALIGNMENT STAI_TINYV8_320_PERSON_OUT_1_ALIGNMENT

#define MDL_NN_OUT_BUFFER_SIZE ALIGN_VALUE(MDL_NN_OUT_1_SIZE, MDL_NN_OUT_1_ALIGNMENT)
#define MDL_NN_OUT_SIZES       {MDL_NN_OUT_1_SIZE}

/* Post-processing: YOLOv8 (od_yolov8_pp_*) */
#define MDL_PP_TYPE           POSTPROCESS_OD_YOLO_V8_UI
#define MDL_PP_NB_CLASSES     1
#define MDL_PP_TOTAL_BOXES    2100
#define MDL_PP_CONF_THRESHOLD 0.5f
#define MDL_PP_IOU_THRESHOLD  0.5f

#define MDL_DISPLAY_NAME LL_ATON_TINYV8_320_PERSON_ORIGIN_MODEL_NAME

#define MDL_PP_CLASS_LABEL_COUNT 1
#define MDL_PP_CLASS_LABEL_0     "person"

static const char *MDL_PP_CLASS_LABELS[] __attribute__((unused)) = {MDL_PP_CLASS_LABEL_0};

#endif /* MODEL_TINYISSIMO_V8_320_COCO_PERSON_H */
