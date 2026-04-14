/**
 ******************************************************************************
 * @file    app_cam_config.h
 * @author  Long Liangmao
 * @brief   Camera configuration for STM32N6570-DK
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
#ifndef CAM_CONFIG_H
#define CAM_CONFIG_H

/* Camera FPS: overridden by CMake (-DCAMERA_FPS=...) when using project.py build */
#ifndef CAMERA_FPS
#define CAMERA_FPS 15
#endif

/* Define sensor orientation */
#define CAMERA_FLIP CMW_MIRRORFLIP_MIRROR

#endif /* CAM_CONFIG_H */
