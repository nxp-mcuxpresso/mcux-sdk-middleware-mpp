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
 * This is the evkbimxrt1050 board configuration
 * Disabling HAL of unused/missing devices saves memory
 */

#define HAL_ENABLE_DISPLAY
#define HAL_ENABLE_DISPLAY_DEV_LcdifRk043fn   1
#define HAL_ENABLE_2D_IMGPROC
#define HAL_ENABLE_GFX_DEV_Pxp                1

/**
 * This is the inference HAL configuration
 */

/* enable TFlite by default */
#define HAL_ENABLE_INFERENCE_TFLITE              1

/* The size of Tensor Arena buffer for TensorFlowLite-Micro */
/* minimum required arena size for Nanodet-M */
#define HAL_TFLM_TENSOR_ARENA_SIZE_KB             2048

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

/**
 * This is HAL debug configuration
 */

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

/* display parameters */
#define APP_DISPLAY_NAME   "LcdifRk043fn"
#define APP_DISPLAY_WIDTH  480
#define APP_DISPLAY_HEIGHT 272
#define APP_DISPLAY_FORMAT MPP_PIXEL_RGB565

/* other parameters */
/* no rotation is needed to display in landscape because display Rk043 is already landscape */
#define APP_DISPLAY_LANDSCAPE_ROTATE ROTATE_0

/* detection boxes params */
/* maximum number of boxes stored in RAM by APP (1box ~= 16B) */
#define APP_MAX_BOXES 10000

/* Tensorflow lite Model data */
#define APP_TFLITE_NANODET_DATA "models/nanodet_m_320_quant_int8/nanodet_m_0.5x_nhwc_tflite.h"
#define APP_TFLITE_NANODET_INFO "models/nanodet_m_320_quant_int8/nanodet_m_0.5x_nhwc_tflite_info.h"


/* Tensorflow lite Model data */
#ifdef APP_USE_NEUTRON64_MODEL
#define APP_TFLITE_NANODET_DATA "models/nanodet_m_320_quant_int8/nanodet_m_0.5x_nhwc_nopermute_npu64_tflite.h"
#define APP_TFLITE_NANODET_INFO "models/nanodet_m_320_quant_int8/nanodet_m_0.5x_nhwc_nopermute_npu64_tflite_info.h"
#else
#define APP_TFLITE_NANODET_DATA "models/nanodet_m_320_quant_int8/nanodet_m_0.5x_nhwc_tflite.h"
#define APP_TFLITE_NANODET_INFO "models/nanodet_m_320_quant_int8/nanodet_m_0.5x_nhwc_tflite_info.h"
#endif  // APP_USE_NEUTRON64_MODEL

#endif /* _MPP_CONFIG_H */
