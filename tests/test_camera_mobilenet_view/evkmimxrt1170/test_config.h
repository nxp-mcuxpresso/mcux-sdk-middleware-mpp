/*
 * Copyright 2022-2023 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _TEST_CONFIG_H
#define _TEST_CONFIG_H

/*
 * This is the test configuration for evkmimxrt1070
 */

/*******************************************************************************
 * TEST configuration
 ******************************************************************************/
#define IMG_stopwatch 0

/* Set to the image type used for testing. */
#ifndef IMAGE_TYPE
#define IMAGE_TYPE IMG_stopwatch
#endif

#ifndef APP_CONFIG
#error "ERROR: test configuration APP_CONFIG is not defined"
#elif (APP_CONFIG==1) || (APP_CONFIG==3) || (APP_CONFIG==5)
#include "images/mechanical_stopwatch_128_128_vuyx.h"
#define EXPECTED_CONFIDENCE_MIN 62
#define EXPECTED_LABEL          "stopwatch"
#define EXPECTED_CHECKSUM       0xec15b997
#else
#pragma message "configuration APP_CONFIG value is not supported by test"
#endif

#endif /* _TEST_CONFIG_H */
