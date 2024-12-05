/*
 * Copyright 2023 NXP
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
#define IMG_couple_COCO   0
#define IMG_skigirl_COCO  1

/* Set to the image type used for testing. */
#ifndef IMAGE_TYPE
#define IMAGE_TYPE IMG_couple_COCO
#endif

#ifndef APP_CONFIG
#error "ERROR: test configuration APP_CONFIG is not defined"
#elif (APP_CONFIG==1)
#include "images/couple_COCO_320_240_bgra.h"
#define EXPECTED_CONFIDENCE_MIN       99
#define EXPECTED_NUM_DETECTED_FACES   2
#define EXPECTED_CHECKSUM             0xe25a0831
#elif (APP_CONFIG==2)
#include "images/skigirl_COCO_427_284_bgra.h"
#define EXPECTED_CONFIDENCE_MIN       97
#define EXPECTED_NUM_DETECTED_FACES   1
#define EXPECTED_CHECKSUM             0x668a2401
#else
#pragma message "configuration APP_CONFIG value is not supported by test"
#endif

#endif /* _TEST_CONFIG_H */
