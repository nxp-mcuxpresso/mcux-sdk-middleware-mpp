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
#define IMG_stopwatch 0

/* Set to the image type used for testing. */
#ifndef IMAGE_TYPE
#define IMAGE_TYPE IMG_stopwatch
#endif

#ifndef APP_CONFIG
#error "ERROR: test configuration APP_CONFIG is not defined"
#elif (APP_CONFIG==1) || (APP_CONFIG==3) || (APP_CONFIG==5)
#include <images/stopwatch168_208_vuyx.h>
#define EXPECTED_CONFIDENCE_MIN 75
#define EXPECTED_LABEL          "stopwatch"
#define EXPECTED_CHECKSUM       0xbef2a19d
#else
#pragma message "configuration APP_CONFIG value is not supported by test"
#endif

#endif /* _TEST_CONFIG_H */
