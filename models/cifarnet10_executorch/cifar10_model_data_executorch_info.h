/*
 * Copyright 2026 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _CIFAR_MODEL_TFLITE_NPU64_INFO_H_
#define _CIFAR_MODEL_TFLITE_NPU64_INFO_H_

/* This file provides information about the ExecuTorch model cifar10_model_data_executorch.h, such as width, heigth, etc.
   Other parameters include input mean, output zero point, output scale, grid height and width,
   the number of channels, and the maximum number of boxes.
*/

#include "mpp_api_types.h"

#define CIFAR_NAME "cifar_model_executorch"
/* mean and std will be used to get input values in the model input data range.
 * for CIFAR-10 data should be between -1 and 1. */
#define CIFAR_INPUT_MEAN  128
#define CIFAR_INPUT_STD   128
#define CIFAR_WIDTH       32
#define CIFAR_HEIGHT      32
/* output tensor properties */
#define CIFAR_TENSOR_SIZE 10
#define CIFAR_TENSOR_TYPE MPP_TENSOR_TYPE_FLOAT32

#endif /* _CIFAR10_MODEL_TFLITE_NPU64_INFO_H_ */

