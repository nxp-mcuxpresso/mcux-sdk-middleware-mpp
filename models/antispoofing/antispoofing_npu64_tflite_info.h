/*
 * Copyright 2025 NXP
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef _ANTISPOOFING_NPU64_TFLITE_INFO_H
#define _ANTISPOOFING_NPU64_TFLITE_INFO_H

#define ANTISPOOFING_NAME "MiniXception_int8_tflite"
/* mean and std will be used to get input values in the model input data range.
 * Antispoofing data should be between -1 and 1. */
#define ANTISPOOFING_INPUT_MEAN   128
#define ANTISPOOFING_INPUT_STD    127
#define ANTISPOOFING_WIDTH        96
#define ANTISPOOFING_HEIGHT       96

/* The spoofing threshold is found based on evaluating the model on a test dataset. It's the one that 
minimizes the spoofing attacks. */
#define SPOOFING_THRESHOLD        70

/* ANTISPOOFING_OUTPUT_SCALE and ANTISPOOFING_OUTPUT_ZERO_POINT are the model quantization parameters
 * and can be found by visualizing the model using netron */
#define ANTISPOOFING_OUTPUT_SCALE      0.00390625f
#define ANTISPOOFING_OUTPUT_ZERO_POINT 0.0f

#endif /* _ANTISPOOFING_NPU64_TFLITE_INFO_H_ */


