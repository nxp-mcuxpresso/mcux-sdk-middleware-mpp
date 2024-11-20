/*
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _TEST_CONFIG_H
#define _TEST_CONFIG_H

/*
 * This is the test configuration for mcxn9xxevk
 */

/*******************************************************************************
 * TEST configuration
 ******************************************************************************/
#if !defined(RTOS_HEAP_SIZE) || (defined(RTOS_HEAP_SIZE) && (RTOS_HEAP_SIZE < 175))
#error "APP requires at least 175k of freertos heap size"
#endif

/* checksum used for this test is PISANO */
#define CHECKSUM_TYPE_EXPECTED_PISANO 1

#define IMG_stopwatch 0
#define IMG_tiger     1

/* Set to the image type used for testing. */
#ifndef IMAGE_TYPE
#define IMAGE_TYPE IMG_stopwatch
#endif

#if (IMAGE_TYPE == IMG_stopwatch)
#include <images/stopwatch320_240_rgb565le.h>
/* define the output image position in the screen */
#define OUTPUT_WINDOW_TOP    0
#define OUTPUT_WINDOW_LEFT   0
#define EXPECTED_CHECKSUM    0x75f67cc6
#elif (IMAGE_TYPE == IMG_tiger)
#include "images/zebra_256_206_rgb565le.h"
/* define the output image position in the screen */
#define OUTPUT_WINDOW_TOP    17
#define OUTPUT_WINDOW_LEFT   32
#define EXPECTED_CHECKSUM    0x3ed09c40
#endif

#endif /* _TEST_CONFIG_H */
