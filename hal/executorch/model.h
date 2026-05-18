  /* 
 * Copyright 2026 NXP
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#ifndef _MODEL_EXECUTORCH_H_
#define _MODEL_EXECUTORCH_H_

#include <stdint.h>
#include "fsl_common.h"
#include "hal_valgo_dev.h"
#include "hal.h"
#include "mpp_config.h"

#if defined(__cplusplus)
extern "C" {
#endif

#ifndef HAL_EXECUTORCH_BUFFER_ALIGN
#define HAL_EXECUTORCH_BUFFER_ALIGN 16
#endif

/**
 * Initialize ExecuTorch model from a PTE file buffer.
 *
 * @param pte_data      pointer to PTE model binary
 * @param pte_size      size of PTE model binary
 * @param inputTensor   [out] input tensor metadata (data pointer, type, dims)
 * @param outputTensor  [out] array of output tensor metadata pointers
 * @param mean          model input normalization mean
 * @param std           model input normalization standard deviation
 * @param nb_out_tensor number of output tensors to retrieve
 * @return kStatus_Success on success, kStatus_Fail on error
 */
status_t MODEL_EXECUTORCH_Init(
    const void *pte_data,
    size_t pte_size,
    mpp_inference_tensor_params_t *inputTensor,
    mpp_inference_tensor_params_t *outputTensor[],
    int mean,
    int std,
    int nb_out_tensor);

/**
 * Deinitialize ExecuTorch model and free all runtime resources.
 */
status_t MODEL_EXECUTORCH_DeInit(void);

/**
 * Run ExecuTorch model inference.
 */
status_t MODEL_EXECUTORCH_RunInference(void);

/**
 * Convert unsigned 8-bit image data to model input format in-place.
 *
 * For ExecuTorch models, this performs basic normalization.
 * The model handles quantization/dequantization internally via
 * quantized_decomposed operators embedded in the PTE file.
 *
 * @param data  image buffer (uint8) to convert in-place
 * @param dims  tensor dimensions (NCHW order)
 * @param type  target tensor element type
 * @param mean  normalization mean
 * @param std   normalization standard deviation
 */
void MODEL_EXECUTORCH_ConvertInput(
    uint8_t *data,
    mpp_tensor_dims_t *dims,
    mpp_tensor_type_t type,
    int mean,
    int std);

#if defined(__cplusplus)
}
#endif

#endif /* _MODEL_EXECUTORCH_H_ */