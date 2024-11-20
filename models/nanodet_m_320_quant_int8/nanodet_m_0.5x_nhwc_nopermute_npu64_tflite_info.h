/*
 * Copyright 2024 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _NANODET_M_0_5X_NHWC_NPU64_TFLITE_INFO_H_
#define _NANODET_M_0_5X_NHWC_NPU64_TFLITE_INFO_H_

/* This file provides information about the TFLite model nanodet_m_0.5x_nhwc_npu64_tflite.h such as width, heigth, etc.
   Other parameters include input mean, output zero point, output scale, grid height and width,
   the number of channels, and the maximum number of boxes.
*/

#define NANODET_NAME "nanodet_m_0_5x_nhwc_nopermute_tflite"
/* mean and std will be used to get input values in the model input data range.
 * for Nanodet data should be between -1 and 1. */
#define NANODET_INPUT_MEAN   -14.0f
// STD value should be multiplied by 127 to get input image between -1 and 1 before normalization
#define NANODET_INPUT_STD    (127.0f * 0.01865844801068306f)
#define NANODET_WIDTH       320
#define NANODET_HEIGHT      320
/* output tensor properties */
#define NANODET_STRIDE      32
#define NANODET_NUM_CLASS   80
#define NANODET_NUM_REGS    32
#define NANODET_MAX_POINTS  100 //maximum number of center priors

#endif /* _NANODET_M_0_5X_NHWC_NPU64_TFLITE_INFO_H_ */
