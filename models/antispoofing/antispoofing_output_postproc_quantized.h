/*
 * Copyright 2025 NXP
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef MPP_EXAMPLES_MODELS_ANTISPOOFING_OUTPUT_POSTPROC_H_
#define MPP_EXAMPLES_MODELS_ANTISPOOFING_OUTPUT_POSTPROC_H_

#include "mpp_api_types.h"
#include "mpp_config.h"

/* include model infos  */
#include APP_TFLITE_ANTISPOOFING_INFO

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus*/

// The antispoofing result is a 2D array, that represents the probablities of a face being fake(array[0]) or real( array[1]).
typedef struct _antispoofing_result
{
	unsigned int result[2];

} antispoofing_result;

void ANTISPOOFING_ProcessOutput(const mpp_inference_cb_param_t *inf_out, antispoofing_result* antispoofing_res);

#if defined(__cplusplus)
}
#endif /* __cplusplus*/

#endif /* MPP_EXAMPLES_MODELS_ANTISPOOFING_OUTPUT_POSTPROC_H_ */
