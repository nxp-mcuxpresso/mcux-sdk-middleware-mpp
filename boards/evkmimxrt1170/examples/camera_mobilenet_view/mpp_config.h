/*
 * Copyright 2021-2024 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _MPP_CONFIG_H
#define _MPP_CONFIG_H

/* This header configures the MPP HAL and the application according to the board model */

/*******************************************************************************
 * HAL configuration (Mandatory)
 ******************************************************************************/
/* Set here all the static configuration of the Media Processing Pipeline HAL */

/**
 * This is the evkmimxrt1170 board configuration
 * Disabling HAL of unused/missing devices saves memory
 */

#define HAL_ENABLE_CAMERA
#define HAL_ENABLE_CAMERA_DEV_MipiOv5640      1
#define HAL_ENABLE_DISPLAY
#define HAL_ENABLE_DISPLAY_DEV_Lcdifv2Rk055   1
#define HAL_ENABLE_2D_IMGPROC
#define HAL_ENABLE_GFX_DEV_Pxp                1
#define HAL_ENABLE_GFX_DEV_Cpu                0
#define HAL_ENABLE_GFX_DEV_GPU                0

/**
 * This is the inference HAL configuration
 */

/* enable TFlite by default */
#define HAL_ENABLE_INFERENCE_TFLITE           1

/* The size of Tensor Arena buffer for TensorFlowLite-Micro */
/* minimum required arena size for MobileNetv1 */
#define HAL_TFLM_TENSOR_ARENA_SIZE_KB         512

/**
 * This is the PXP HAL configuration
 */

/* Workaround for the PXP bug where BGR888 is output instead of RGB888 [MPP-97] */
#define HAL_PXP_WORKAROUND_OUT_RGB            1

/**
 * This is the display HAL configuration
 */

/* The display max byte per pixel */
#define HAL_DISPLAY_MAX_BPP                   2


/* Log level configuration
 * ERR:   0
 * INFO:  1
 * DEBUG: 2
 */
#ifndef HAL_LOG_LEVEL
#define HAL_LOG_LEVEL 0
#endif

/**
 *  Mutex lock timeout definition
 *  An arbitrary default value is defined to 5 seconds
 *  value unit should be milliseconds
 * */
#define HAL_MUTEX_TIMEOUT_MS   (5000)

/*******************************************************************************
 * Application configuration (Optional)
 ******************************************************************************/

/* Set here all the static configuration of the Application */

/* camera parameters */
#define APP_CAMERA_NAME    "MipiOv5640"
#define APP_CAMERA_WIDTH   1280
#define APP_CAMERA_HEIGHT  720
#define APP_CAMERA_FORMAT  MPP_PIXEL_YUV1P444

/* display parameters */
#define APP_DISPLAY_NAME   "Lcdifv2Rk055"
#define APP_DISPLAY_WIDTH  720
#define APP_DISPLAY_HEIGHT 1280
#define APP_DISPLAY_FORMAT MPP_PIXEL_RGB565

/* other parameters */
/* rotation is needed to display in landscape because display RK055 is portrait */
#define APP_DISPLAY_LANDSCAPE_ROTATE ROTATE_90


/* Tensorflow lite Model data */
#ifdef APP_USE_NEUTRON64_MODEL
#define APP_TFLITE_MOBILENET_DATA "models/mobilenet_v1_0.25_128_quant_int8/mobilenetv1_model_data_tflite_npu64.h"
#define APP_TFLITE_MOBILENET_INFO "models/mobilenet_v1_0.25_128_quant_int8/mobilenetv1_model_data_tflite_npu64_info.h"
#elif defined(APP_USE_NEUTRON16_MODEL)
#define APP_TFLITE_MOBILENET_DATA "models/mobilenet_v1_0.25_128_quant_int8/mobilenetv1_model_data_tflite_npu16.h"
#define APP_TFLITE_MOBILENET_INFO "models/mobilenet_v1_0.25_128_quant_int8/mobilenetv1_model_data_tflite_npu16_info.h"
#else
#define APP_TFLITE_MOBILENET_DATA "models/mobilenet_v1_0.25_128_quant_int8/mobilenetv1_model_data_tflite.h"
#define APP_TFLITE_MOBILENET_INFO "models/mobilenet_v1_0.25_128_quant_int8/mobilenetv1_model_data_tflite_info.h"
#endif  // APP_USE_NEUTRON16_MODEL

#endif /* _MPP_CONFIG_H */
