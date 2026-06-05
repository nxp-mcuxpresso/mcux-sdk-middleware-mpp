/*
 * Copyright 2025 NXP
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * The functions that process the tensor output of the antispoofing model
 */

#include <math.h>
#include "antispoofing_output_postproc_quantized.h"
#include "fsl_debug_console.h"

extern "C" {
#include "mpp_api.h"
}

#define EOL              	    "\r\n"

void  ANTISPOOFING_ProcessOutput(const mpp_inference_cb_param_t *inf_out, antispoofing_result *antispoofing_res) {
	int score_fake = 0;
	int score_real = 0;
	const uint8_t * preds_int = NULL;

	if (inf_out == NULL) {
		PRINTF("ERROR:  ANTISPOOFING_ProcessOutput parameter 'inf_out' is null pointer" EOL);
		return;
	}
	if (inf_out->inference_type == MPP_INFERENCE_TYPE_TFLITE) {
		/*convert the data output tensor type to match the output type of the model */
		preds_int = reinterpret_cast<const uint8_t*>(inf_out-> out_tensors[0]->data);
		if (preds_int == NULL) {
			PRINTF("ERROR:  ANTISPOOFING_ProcessOutput: preds_int NULL pointer" EOL);
		}
	} else {
		PRINTF("ERROR:  ANTISPOOFING_ProcessOutput: Undefined Inference Engine" EOL);
	}
	/* Computing the percentages of fake and real scores*/
	score_fake = (int)((float)preds_int[0] * ANTISPOOFING_OUTPUT_SCALE * 100.0f);
	score_real = (int)((float)preds_int[1] * ANTISPOOFING_OUTPUT_SCALE * 100.0f);
	antispoofing_res->result[0] = score_fake;
	antispoofing_res->result[1] = score_real;

}


