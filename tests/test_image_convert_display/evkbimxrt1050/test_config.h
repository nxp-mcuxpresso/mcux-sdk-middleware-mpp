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
#define IMG_90_160_rgb565le          0
#define IMG_stopwatch168_208_vuyx    1
#define IMG_dogs_COCO_320_320_bgra   2
#define IMG_stopwatch128_128_rgb     3
#define IMG_stopwatch168_208_rgb565  4
#define IMG_stopwatch168_208_uyvy422 5
#define IMG_stopwatch168_208_vyuy422 6

/* Set to the image type used for testing. */
#ifndef IMAGE_TYPE
#define IMAGE_TYPE IMG_90_160_rgb565le
#endif

#ifndef APP_CONFIG
#error "ERROR: test configuration APP_CONFIG is not defined"
#elif (APP_CONFIG==0) /* default app config */
#include "images/90_160_rgb565le.h"
#define IMAGE_NAME "90_160_rgb565le"
#define EXPECTED_CHECKSUM 0x370c54fa
#elif (APP_CONFIG==1)
#include "images/stopwatch168_208_vuyx.h"
#define IMAGE_NAME "stopwatch168_208_vuyx"
#define EXPECTED_CHECKSUM 0xdc95dcf8
#elif (APP_CONFIG==2)
#include "images/dogs_COCO_320_320_bgra.h"
#define IMAGE_NAME "dogs_COCO_320_320_bgra.h"
#define EXPECTED_CHECKSUM 0x0dbb65ce
#elif (APP_CONFIG==3)
#include "images/stopwatch128_128_rgb.h"
#define IMAGE_NAME "stopwatch_RGB888"
#define EXPECTED_CHECKSUM 0xce9328d6
#elif (APP_CONFIG==4)
#include "images/stopwatch168_208_rgb565.h"
#define IMAGE_NAME "stopwatch168_208_rgb565"
#define EXPECTED_CHECKSUM 0x5d279039
#elif (APP_CONFIG==5)
#include "images/stopwatch168_208_rgb565.h"
#define IMAGE_NAME "stopwatch168_208_rgb565"
#define EXPECTED_CHECKSUM 0x918821b2
#elif (APP_CONFIG==6)
#include "images/stopwatch168_208_rgb565.h"
#define IMAGE_NAME "stopwatch168_208_rgb565"
#define EXPECTED_CHECKSUM 0x0d39c753
#elif (APP_CONFIG==7)
#include "images/stopwatch168_208_vuyx.h"
#define IMAGE_NAME "stopwatch168_208_vuyx"
#define EXPECTED_CHECKSUM 0x57b1b872
#elif (APP_CONFIG==8)
#include "images/stopwatch168_208_uyvy422.h"
#define IMAGE_NAME "stopwatch_uyvy422"
#define EXPECTED_CHECKSUM 0x2a87ec50
#elif (APP_CONFIG==9)
#include "images/stopwatch168_208_vyuy422.h"
#define IMAGE_NAME "stopwatch_vyuy422"
#define EXPECTED_CHECKSUM 0x2a87ec50
#elif (APP_CONFIG==10)
#include "images/stopwatch168_208_rgb565.h"
#define IMAGE_NAME "stopwatch168_208_rgb565"
#define EXPECTED_CHECKSUM 0x12a832ac
#elif (APP_CONFIG==11)
#include "images/stopwatch168_208_vuyx.h"
#define IMAGE_NAME "stopwatch168_208_vuyx"
#define EXPECTED_CHECKSUM 0xd1da2d0f
#elif (APP_CONFIG==12)
#include "images/stopwatch168_208_rgb565.h"
#define IMAGE_NAME "stopwatch168_208_rgb565"
#define EXPECTED_CHECKSUM 0x9bbc3028
#elif (APP_CONFIG==13)
#include "images/90_160_rgb565le.h"
#define IMAGE_NAME "90_160_rgb565le"
#define EXPECTED_CHECKSUM 0xf07a9539
#elif (APP_CONFIG==14)
#include "images/stopwatch168_208_vuyx.h"
#define IMAGE_NAME "stopwatch168_208_vuyx"
#define EXPECTED_CHECKSUM 0xdc7b126e
#elif (APP_CONFIG==15)
#include "images/stopwatch168_208_uyvy422.h"
#define IMAGE_NAME "stopwatch_uyvy422"
#define EXPECTED_CHECKSUM 0x661823a3
#elif (APP_CONFIG==16)
#include "images/stopwatch128_128_rgb.h"
#define IMAGE_NAME "stopwatch_RGB888"
#define EXPECTED_CHECKSUM 0x21aa9044
#elif (APP_CONFIG==17)
#include "images/stopwatch168_208_rgb565.h"
#define IMAGE_NAME "stopwatch168_208_rgb565"
#define EXPECTED_CHECKSUM 0x5322d577
#elif (APP_CONFIG==18)
#include "images/stopwatch168_208_vuyx.h"
#define IMAGE_NAME "stopwatch168_208_vuyx"
#define EXPECTED_CHECKSUM 0x28e07e42
#elif (APP_CONFIG==19)
#include "images/stopwatch168_208_vuyx.h"
#define IMAGE_NAME "stopwatch168_208_vuyx"
#define EXPECTED_CHECKSUM 0x2990324e
#elif (APP_CONFIG==20)
#include "images/stopwatch168_208_rgb565.h"
#define IMAGE_NAME "stopwatch168_208_rgb565"
#define EXPECTED_CHECKSUM 0xc5d41214
#elif (APP_CONFIG==21)
#include "images/stopwatch168_208_vuyx.h"
#define IMAGE_NAME "stopwatch168_208_vuyx"
#define EXPECTED_CHECKSUM 0xe71f1a8b
#elif (APP_CONFIG==22)
#include "images/stopwatch168_208_vuyx.h"
#define IMAGE_NAME "stopwatch168_208_vuyx"
#define EXPECTED_CHECKSUM 0x238d1319
#elif (APP_CONFIG==23)
#include "images/stopwatch168_208_uyvy422.h"
#define IMAGE_NAME "stopwatch_uyvy422"
#define EXPECTED_CHECKSUM 0x661823a3
#else
#pragma message "configuration APP_CONFIG value is not supported by test"
#endif

#endif /* _TEST_CONFIG_H */
