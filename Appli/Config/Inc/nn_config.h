/**
 ******************************************************************************
 * @file    app_nn_config.h
 * @author  Long Liangmao
 * @brief   Neural Network pipeline configuration for STM32N6570-DK
 ******************************************************************************
 */
#ifndef NN_CONFIG_H
#define NN_CONFIG_H

#include "model_config.h"

#define NN_WIDTH  MDL_NN_IN_WIDTH
#define NN_HEIGHT MDL_NN_IN_HEIGHT
#define NN_FORMAT DCMIPP_PIXEL_PACKER_FORMAT_RGB888_YUV444_1
#define NN_BPP    MDL_NN_IN_CHANNELS
#define NN_OUT_NB MDL_NN_OUT_NUM

#endif /* NN_CONFIG_H */
