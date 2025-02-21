/*
 * Copyright 2023 NXP
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
 * This is the mcxn9xxevk board configuration
 * Disabling HAL of unused/missing devices saves memory
 */
#define HAL_ENABLE_DISPLAY
#define HAL_ENABLE_DISPLAY_DEV_McuLcdSsd1963 1
#define HAL_ENABLE_CAMERA
#define HAL_ENABLE_CAMERA_DEV_EzhOv7670 1
#define HAL_ENABLE_2D_IMGPROC
#define HAL_ENABLE_GFX_DEV_Pxp 0

/* enable TFlite by default */
#if !defined(INFERENCE_ENGINE_GLOW) && !defined(INFERENCE_ENGINE_DeepViewRT)
#define HAL_ENABLE_INFERENCE_TFLITE 1
#define HAL_ENABLE_INFERENCE_GLOW 0
#define HAL_ENABLE_INFERENCE_DVRT 0
#elif defined(INFERENCE_ENGINE_GLOW)
#error "Glow inference not supported in this app for this board"
#elif defined(INFERENCE_ENGINE_DeepViewRT)
#error "DeepViewRT inference not supported in this app for this board"
#endif

/**
 * This is the inference HAL configuration
 */

/* The size of Tensor Arena buffer for TensorFlowLite-Micro */
/* minimum required arena size for MobileNetv1 converted for NPU */
#define HAL_TFLM_TENSOR_ARENA_SIZE_KB 300


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

/* camera params */
#define APP_CAMERA_NAME "EzhOv7670"
#define APP_CAMERA_WIDTH  160
#define APP_CAMERA_HEIGHT 120
#define APP_CAMERA_FORMAT MPP_PIXEL_RGB565

/* display params (default values) */
#define APP_DISPLAY_NAME   "McuLcdSsd1963"
#define APP_DISPLAY_WIDTH  800
#define APP_DISPLAY_HEIGHT 480
#define APP_DISPLAY_FORMAT MPP_PIXEL_RGB565
#define APP_DISPLAY_LANDSCAPE_ROTATE ROTATE_0

/* detection boxes params */
/* maximum number of boxes stored in RAM by APP (1box ~= 16B) */
#define APP_MAX_BOXES 100

/*
 * when display has remote FB, partial refresh is possible,
 * thus application may define top&left position of image on display.
 * 0: display has its own frame buffer
 * 1: display has remote frame buffer
 */
#define APP_DISPLAY_REMOTE_FB 1


/* Tensorflow lite Model data */
#ifndef APP_ULTRAFACE_ULTRASLIM
#define APP_TFLITE_ULTRAFACE_DATA "models/ultraface_slim_quant_int8/ultraface_slim_tflite.h"
#define APP_TFLITE_ULTRAFACE_INFO "models/ultraface_slim_quant_int8/ultraface_slim_tflite_info.h"
#else
#ifdef APP_USE_NEUTRON16_MODEL
#define APP_TFLITE_ULTRAFACE_DATA "models/ultraface_slim_quant_int8/ultraface_slim_ultraslim_npu16_tflite.h"
#define APP_TFLITE_ULTRAFACE_INFO "models/ultraface_slim_quant_int8/ultraface_slim_ultraslim_npu16_tflite_info.h"
#elif defined(APP_USE_NEUTRON64_MODEL)
#define APP_TFLITE_ULTRAFACE_DATA "models/ultraface_slim_quant_int8/ultraface_slim_ultraslim_npu64_tflite.h"
#define APP_TFLITE_ULTRAFACE_INFO "models/ultraface_slim_quant_int8/ultraface_slim_ultraslim_npu64_tflite_info.h"
#else
#define APP_TFLITE_ULTRAFACE_DATA "models/ultraface_slim_quant_int8/ultraface_slim_ultraslim_tflite.h"
#define APP_TFLITE_ULTRAFACE_INFO "models/ultraface_slim_quant_int8/ultraface_slim_ultraslim_tflite_info.h"
#endif
#endif

#endif /* _MPP_CONFIG_H */

