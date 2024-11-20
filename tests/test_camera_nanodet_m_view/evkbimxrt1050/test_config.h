/*
 * Copyright 2022-2023 NXP
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
#define IMG_skigirl_COCO 0

/* Set to the image type used for testing. */
#ifndef IMAGE_TYPE
#define IMAGE_TYPE IMG_skigirl_COCO
#endif

#ifndef APP_CONFIG
#error "ERROR: test configuration APP_CONFIG is not defined"
#elif (APP_CONFIG==1)
/* TFlite is enabled by default */
#include "images/skigirl_COCO_320_320_bgra.h"
#define EXPECTED_CONFIDENCE_MIN         79
#define EXPECTED_NUM_DETECTED_OBJECTS   1
const char* expected_labels[] =         {"person"};
#define EXPECTED_CHECKSUM 0xa7d4a04a
#elif (APP_CONFIG==3)
#include "images/skigirl_COCO_320_320_bgra.h"
#define EXPECTED_CONFIDENCE_MIN         79
#define EXPECTED_NUM_DETECTED_OBJECTS   2
const char* expected_labels[] =         {"person", "person"};
#define EXPECTED_CHECKSUM 0xf67fc1a2
#else
#pragma message "configuration APP_CONFIG value is not supported by test"
#endif

#endif /* _TEST_CONFIG_H */
