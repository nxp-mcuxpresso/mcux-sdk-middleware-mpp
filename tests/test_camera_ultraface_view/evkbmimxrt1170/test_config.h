/*
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _TEST_CONFIG_H
#define _TEST_CONFIG_H

/*
 * This is the test configuration for evkbmimxrt1070
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

#pragma message "No test configuration is supported for this application and for this board""

#endif /* _TEST_CONFIG_H */
