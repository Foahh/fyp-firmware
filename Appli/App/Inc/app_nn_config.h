/**
 ******************************************************************************
 * @file    app_nn_config.h
 * @author  Long Liangmao
 * @brief   Neural Network pipeline configuration for STM32N6570-DK
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 Long Liangmao.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
#ifndef APP_NN_CONFIG_H
#define APP_NN_CONFIG_H

/* Neural Network pipeline configuration */
#define NN_WIDTH 480
#define NN_HEIGHT 480
#define NN_FORMAT DCMIPP_PIXEL_PACKER_FORMAT_RGB888_YUV444_1
#define NN_BPP 3

#endif /* APP_NN_CONFIG_H */
