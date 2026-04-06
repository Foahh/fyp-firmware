/**
 ******************************************************************************
 * @file    model_config.h
 * @author  Long Liangmao
 * @brief   Build-time model selection and generic NN API macros
 ******************************************************************************
 */
#ifndef MODEL_CONFIG_H
#define MODEL_CONFIG_H

/* Model selection */
#if defined(MODEL_YOLOX_NANO_480_D100_W025_PERSON)
#include "models/model_yolox_nano_480_coco_person.h"
#elif defined(MODEL_YOLOV8N)
#include "models/model_yolov8n.h"
#elif defined(MODEL_YOLOV11N)
#include "models/model_yolov11n.h"
#elif defined(MODEL_ST_YOLODV2MILLI_256_PERSON)
#include "models/model_st_yolodv2milli_256_coco_person.h"
#elif defined(MODEL_YOLO26_320_PERSON)
#include "models/model_yolo26_320_coco_person.h"
#elif defined(MODEL_YOLO26_320_FYP_MERGED)
#include "models/model_yolo26_320_fyp_merged.h"
#else
#error "No model selected."
#endif

/* Token-pasting helpers for generated stai API */
#define _MDL_CONCAT3(a, b, c) a##b##c
#define MDL_CONCAT3(a, b, c)  _MDL_CONCAT3(a, b, c)

/* Context declaration */
#define MDL_CONTEXT_DECLARE(name) \
  STAI_NETWORK_CONTEXT_DECLARE(name, MDL_NN_CTX_SIZE)

/* stai API wrappers */
#define mdl_init(ctx)                MDL_CONCAT3(stai_, MDL_NN_NETWORK_NAME, _init)(ctx)
#define mdl_get_info(ctx, info)      MDL_CONCAT3(stai_, MDL_NN_NETWORK_NAME, _get_info)(ctx, info)
#define mdl_set_inputs(ctx, in, n)   MDL_CONCAT3(stai_, MDL_NN_NETWORK_NAME, _set_inputs)(ctx, in, n)
#define mdl_set_outputs(ctx, out, n) MDL_CONCAT3(stai_, MDL_NN_NETWORK_NAME, _set_outputs)(ctx, out, n)
#define mdl_run(ctx, mode)           MDL_CONCAT3(stai_, MDL_NN_NETWORK_NAME, _run)(ctx, mode)
#define mdl_new_inference(ctx)       MDL_CONCAT3(stai_ext_, MDL_NN_NETWORK_NAME, _new_inference)(ctx)

#endif /* MODEL_CONFIG_H */
