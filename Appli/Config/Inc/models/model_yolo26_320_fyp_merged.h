/**
 ******************************************************************************
 * @file    model_yolo26_320_fyp_merged.h
 * @brief   Model config: YOLO26 (320×320, INT8, FYP merged: hand + tool OD)
 * @note    Raw head matches YOLOv8: single tensor [1, 4+nb_classes, nb_anchors].
 ******************************************************************************
 */
#ifndef MODEL_YOLO26_320_FYP_MERGED_H
#define MODEL_YOLO26_320_FYP_MERGED_H

#include "stai_yolo26_320_fyp_merged.h"
#include "utils.h"
#include "yolo26_320_fyp_merged.h"

/* Network binding */
#define MDL_NN_NETWORK_NAME yolo26_320_fyp_merged
#define MDL_NN_IN_WIDTH     STAI_YOLO26_320_FYP_MERGED_IN_1_WIDTH
#define MDL_NN_IN_HEIGHT    STAI_YOLO26_320_FYP_MERGED_IN_1_HEIGHT
#define MDL_NN_IN_CHANNELS  STAI_YOLO26_320_FYP_MERGED_IN_1_CHANNEL
#define MDL_NN_OUT_NUM      STAI_YOLO26_320_FYP_MERGED_OUT_NUM
#define MDL_NN_CTX_SIZE     STAI_YOLO26_320_FYP_MERGED_CONTEXT_SIZE

#define MDL_NN_OUT_1_SIZE      STAI_YOLO26_320_FYP_MERGED_OUT_1_SIZE_BYTES
#define MDL_NN_OUT_1_ALIGNMENT STAI_YOLO26_320_FYP_MERGED_OUT_1_ALIGNMENT

#define MDL_NN_OUT_BUFFER_SIZE ALIGN_VALUE(MDL_NN_OUT_1_SIZE, MDL_NN_OUT_1_ALIGNMENT)
#define MDL_NN_OUT_SIZES       {MDL_NN_OUT_1_SIZE}

/* Post-processing: same family as YOLOv8 (od_yolov8_pp_*) */
#define MDL_PP_TYPE           POSTPROCESS_OD_YOLO_V8_UI
#define MDL_PP_NB_CLASSES     2
#define MDL_PP_TOTAL_BOXES    2100
#define MDL_PP_MAX_BOXES      20
#define MDL_PP_CONF_THRESHOLD 0.5f
#define MDL_PP_IOU_THRESHOLD  0.5f

#define MDL_DISPLAY_NAME LL_ATON_YOLO26_320_FYP_MERGED_ORIGIN_MODEL_NAME

static const char *MDL_PP_CLASS_LABELS[] __attribute__((unused)) = {"hand", "tool"};

#endif /* MODEL_YOLO26_320_FYP_MERGED_H */
