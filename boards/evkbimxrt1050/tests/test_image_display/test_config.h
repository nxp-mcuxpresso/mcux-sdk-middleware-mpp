/*
 * Copyright 2022, 2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _TEST_CONFIG_H
#define _TEST_CONFIG_H

/*
 * This is the test configuration for evkbimxrt1050
 */

/*******************************************************************************
 * TEST configuration
 ******************************************************************************/
#include "images/banana_256_192_rgb565le.h"
#define SRC_IMAGE_FORMAT SRC_IMAGE_BANANA_256_192_RGB565LE_FORMAT
#define SRC_IMAGE_CHANNELS_NUMBER SRC_IMAGE_BANANA_256_192_RGB565LE_CHANNELS_NUMBER
#define SRC_IMAGE_HEIGHT SRC_IMAGE_BANANA_256_192_RGB565LE_HEIGHT
#define SRC_IMAGE_WIDTH SRC_IMAGE_BANANA_256_192_RGB565LE_WIDTH
void *image_data = (void *)banana_256_192_rgb565le_data;
#define EXPECTED_CHECKSUM 0x152bc29b

#endif /* _TEST_CONFIG_H */
