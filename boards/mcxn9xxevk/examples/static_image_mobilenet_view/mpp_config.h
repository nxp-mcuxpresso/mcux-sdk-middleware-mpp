/*
 * Copyright 2023-2024 NXP
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
#define HAL_ENABLE_DISPLAY
#define HAL_ENABLE_DISPLAY_DEV_SpiIli9341   1
#define HAL_ENABLE_GFX_DEV_Pxp 0
#define HAL_ENABLE_2D_IMGPROC

/* Only TFlite is supported for this application */
#define HAL_ENABLE_INFERENCE_TFLITE 1

/**
 * This is the inference HAL configuration
 */

/* The size of Tensor Arena buffer for TensorFlowLite-Micro */
/* minimum required arena size for MobileNetv1 converted for NPU */
#define HAL_TFLM_TENSOR_ARENA_SIZE_KB 256

/**
 * This is HAL debug configuration
 */

/*
 * Log level configuration
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

/* select inference model converted for NPU */
#define APP_USE_NEUTRON16_MODEL

/* display parameters (default values) */
#define APP_DISPLAY_NAME   "SpiIli9341"
#define APP_DISPLAY_WIDTH  128 /* this value can be adjusted depending on the application, max 320 */
#define APP_DISPLAY_HEIGHT 128 /* this value can be adjusted depending on the application, max 240 */
#define APP_DISPLAY_FORMAT MPP_PIXEL_RGB565
#define APP_DISPLAY_LANDSCAPE_ROTATE ROTATE_0

/*
 * when display has remote FB, partial refresh is possible,
 * thus application may define top&left position of image on display.
 * 0: display has its own frame buffer
 * 1: display has remote frame buffer
 */
#define APP_DISPLAY_REMOTE_FB 1


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
