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
#define HAL_ENABLE_DISPLAY_DEV_SpiIli9341   1
#define HAL_ENABLE_2D_IMGPROC
#define HAL_ENABLE_GFX_DEV_Pxp 0

/**
 * This is the inference HAL configuration
 */
#define HAL_ENABLE_INFERENCE_TFLITE 0
#define HAL_ENABLE_INFERENCE_GLOW 0
#define HAL_ENABLE_INFERENCE_DVRT 0

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

/* display params (default values) */
#define APP_DISPLAY_NAME   "SpiIli9341"
#define APP_DISPLAY_WIDTH  320 /* this value can be adjusted depending on the application, max 320 */
#define APP_DISPLAY_HEIGHT 240 /* this value can be adjusted depending on the application, max 240 */
#define APP_DISPLAY_FORMAT MPP_PIXEL_RGB565
#define APP_DISPLAY_LANDSCAPE_ROTATE ROTATE_0

/*
 * when display has remote FB, partial refresh is possible,
 * thus application may define top&left position of image on display.
 * 0: display has its own frame buffer
 * 1: display has remote frame buffer
 */
#define APP_DISPLAY_REMOTE_FB 1

#endif /* _MPP_CONFIG_H */

