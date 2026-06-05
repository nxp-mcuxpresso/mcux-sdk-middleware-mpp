/*
 * Copyright 2024-2026 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MPP_EXAMPLES_MODELS_MOBILEFACENET_OUTPUT_POSTPROC_H_
#define MPP_EXAMPLES_MODELS_MOBILEFACENET_OUTPUT_POSTPROC_H_

#include "mpp_api_types.h"
#include "mpp_config.h"

/* include model infos  */
#include APP_TFLITE_MOBILEFACENET_INFO

/* include database infos  */
#include APP_DATABASE_INFOS

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus*/

typedef struct _recognition_result
{
	char recognized_name[MAX_NAME_SIZE+1] ;
	int similarity_percentage;
	float embedding[SIZE_EMBEDDING];
} recognition_result;

/**
 * Process the Mobilefacenet output tensor
 *
 * @param [in] inf_out: inference output (tensor data and description)
 * @param [in] database: the database containing the faces embeddings
 * @param [in] num_faces: number of faces in the database
 * @param [out] reco_res: structure containing the result of the recognition
 *
 * @return: void;
 */
void MOBILEFACENET_ProcessOutput(const mpp_inference_cb_param_t *inf_out, face_t *database, int num_faces, recognition_result* reco_res, float reco_thres);

#if defined(__cplusplus)
}
#endif /* __cplusplus*/

#endif /* MPP_EXAMPLES_MODELS_MOBILEFACENET_OUTPUT_POSTPROC_H_ */
