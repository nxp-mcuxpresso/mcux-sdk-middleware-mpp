/*
 * Copyright 2024-2026 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The functions that process the tensor output of the Mobilefacenet model
 */

#include "../../../models/mobilefacenet/mobilefacenet_output_postproc_quantized.h"
#include <math.h>
#include "fsl_debug_console.h"

extern "C" {
#include "mpp_api.h"
}

#define EOL              	    "\r\n"

/* Normalize the embeddings vector to unit length (L2 norm = 1), to ensure that embeddings lie in a consistent range.  
 * It takes a non-normalized float embeddings vector and returns a normalized embeddings vector where all values are in [-1, 1].
 */
void normalize(float* emb) {
    float norm = 0.0f;
    for (int i = 0; i < SIZE_EMBEDDING; i++) {
        norm += emb[i] * emb[i];
    }
    norm = sqrtf(norm);
 
    if (norm > 0.0f) {
        for (int i = 0; i < SIZE_EMBEDDING; i++) {
            emb[i] /= norm;
        }
    }
}

/* Calculate the squared euclidean distance between two embeddings.
 * Embeddings should be normalized, since the model has been validated on normalized embeddings.
 * When the embeddings are L2-normalized (|a| = 1), the squared Euclidean distance between two embeddings a and b:
 * ||a - b||² = ||a||² + ||b||² - 2 * (a • b)
 * becomes: 1 + 1 - 2 * (a • b) = 2 - 2 * cosine_similarity (a, b)
 * So in this case, squared Euclidean distance is directly related to cosine similarity.
 * A smaller distance means higher similarity, and vice versa.
 */
float squared_euclidean_distance( float* norm_pred_emb, const float* database_emb) {
	float dot_product = 0.0f;
	for (int i = 0; i < SIZE_EMBEDDING; i++) {
		dot_product += (norm_pred_emb[i] * database_emb[i]);
	}
	return 2.0f - (2.0f * dot_product);
}

/* Compare embeddings and recognize faces from a database with the maximum similarity st:
 * the smaller the distance is, the more similar the emebeddings are.
 */
static void recognize_face(const int8_t* out_emb, face_t *database, int max_faces, recognition_result* reco_res, float reco_thresh) {
	float match_thres;

	strcpy(reco_res->recognized_name, "\0");
    /* Initialize the distance to the maximum value the squared euclidean distance could be*/
	reco_res->similarity_percentage = 0;

	/* Dequantize the output of the model */
	for (int i = 0; i < SIZE_EMBEDDING; i++) {
		reco_res->embedding[i] = ((out_emb[i] - OUTPUT_ZERO_POINT) * OUTPUT_SCALE);
	}

	if (reco_thresh > 0.0f)
		match_thres = reco_thresh;
	else
		match_thres = MATCH_THRESHOLD;

	/* Normalize the output of the model */
    normalize(reco_res->embedding);
	for (int i = 0; i < max_faces; i++) {
		/* Calculate the similarity Euclidean distance */
		float squared_distance = squared_euclidean_distance(reco_res->embedding, database[i].embedding);
		int similarity = (int)((1.0f - (squared_distance / 4.0f)) * 100);
		/*
		* When the embeddings are normalized, the squared Euclidean distance between them will always fall in the range [0, 4].
		* 0 distance means the embeddings are identical (maximum similarity).
		* 4 distance means the embeddings are in completely opposite directions (minimum similarity).
		* The distance can be converted into a similarity percentage st percentage = (1 - (distance / 4)) * 100
		* Hence: distance = 0 → similarity = 100% and distance = 4 → similarity = 0%
		*/	
		if (squared_distance < match_thres && similarity > reco_res->similarity_percentage) {
			reco_res->similarity_percentage = similarity;
			strcpy(reco_res->recognized_name, database[i].name);
		}
	}
}


void  MOBILEFACENET_ProcessOutput(const mpp_inference_cb_param_t *inf_out, face_t *database, int num_faces, recognition_result* reco_res, float reco_thres) {
	const int8_t * preds_int = NULL;
	if (inf_out == NULL) {
		PRINTF("ERROR:  Mobilefacenet_ProcessOutput parameter 'inf_out' is null pointer" EOL);
	}
	if (inf_out->inference_type == MPP_INFERENCE_TYPE_TFLITE) {
		/*convert the data output tensor type to match the output type of the model */
		preds_int = reinterpret_cast<const int8_t*>(inf_out-> out_tensors[0]->data);

		if (preds_int == NULL) {
			PRINTF("ERROR:  Mobilefacenet_ProcessOutput: preds_int NULL pointer" EOL);
		}
	} else {
		PRINTF("ERROR:  Mobilefacenet_ProcessOutput: Undefined Inference Engine" EOL);
	}
	recognize_face(preds_int, database, num_faces, reco_res, reco_thres);

}
