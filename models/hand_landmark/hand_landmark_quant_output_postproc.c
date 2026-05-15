/*
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * The function that processes the tensor output of model hand_landmark
 */

#include <stdio.h>
#include <math.h>

#include "fsl_debug_console.h"

#include "mpp_config.h"
#include "mpp_api.h"

#include APP_TFLITE_HAND_LANDMARK_INFO
#include "hand_landmark_quant_output_postproc.h"
#include "utils.h"

#define EOL "\r\n"

int32_t HandLandmark_ProcessOutput(const mpp_inference_cb_param_t *inf_out, hand_data* hand_output)
{
    if (inf_out == NULL) {
        PRINTF("ERROR: HandLandmark_ProcessOutput parameter 'inf_out' is null pointer" EOL);
        return -1;
    }
    if (hand_output == NULL) {
        PRINTF("ERROR: HandLandmark_ProcessOutput parameter 'hand_output' is null pointer" EOL);
        return -1;
    }

    /* clear old data */
    memset(hand_output, 0, sizeof(hand_data));

    // get the 4 output tensors
    int8_t* has_hand_int;
    int8_t* lef_hand_int;
    int8_t* relative_keypoint_int;
    int8_t* global_keypoint_int;

    if(inf_out->inference_type == MPP_INFERENCE_TYPE_TFLITE)
    {
        has_hand_int = (int8_t *) inf_out->out_tensors[HAND_LANDMARK_HAS_HAND_OUTPUT_IDX]->data; /* [1, 1]  matrix */
        lef_hand_int = (int8_t *) inf_out->out_tensors[HAND_LANDMARK_LEFT_HAND_OUTPUT_IDX]->data; /* [1, 1]  matrix */
        relative_keypoint_int = (int8_t *) inf_out->out_tensors[HAND_LANDMARK_RELATIVE_KP_OUTPUT_IDX]->data; /* [1, 63]  matrix */
        global_keypoint_int = (int8_t *) inf_out->out_tensors[HAND_LANDMARK_GLOBAL_KP_OUTPUT_IDX]->data; /* [1, 63]  matrix */

        if (has_hand_int == NULL)
        {
            PRINTF("ERROR: HandLandmark_ProcessOutput: has_hand_int NULL pointer" EOL);
            return -1;
        }
        if (lef_hand_int == NULL)
        {
            PRINTF("ERROR: HandLandmark_ProcessOutput: lef_hand_int NULL pointer" EOL);
            return -1;
        }
        if (relative_keypoint_int == NULL)
        {
            PRINTF("ERROR: HandLandmark_ProcessOutput: relative_keypoint_int NULL pointer" EOL);
            return -1;
        }
        if (global_keypoint_int == NULL)
        {
            PRINTF("ERROR: HandLandmark_ProcessOutput: global_keypoint_int NULL pointer" EOL);
            return -1;
        }
    }
    else
    {
        PRINTF("ERROR: HandLandmark_ProcessOutput: Undefined Inference Engine" EOL);
        return -1;
    }

    float has_hand = (float)(has_hand_int[0] - HAND_LANDMARK_HAS_HAND_ZERO_POINT) * HAND_LANDMARK_HAS_HAND_SCALE;
    hand_output->score = has_hand;
    if (has_hand > HAND_LANDMARK_HAS_HAND_THRESHOLD)
        hand_output->has_hand = true;
    float is_left_hand = (float)(lef_hand_int[0] - HAND_LANDMARK_LEFT_HAND_ZERO_POINT) * HAND_LANDMARK_LEFT_HAND_SCALE;
    if (is_left_hand > HAND_LANDMARK_LEFT_HAND_THRESHOLD)
        hand_output->left_hand = true;

    /* Dequantize the outputs */
    for (int i = 0; i < HAND_LANDMARK_NUM_LANDMARKS; i++)
    {
        hand_output->landmarks[i].x = (float)(global_keypoint_int[i*3]   - HAND_LANDMARK_GLOBAL_KP_ZERO_POINT) * HAND_LANDMARK_GLOBAL_KP_SCALE;
        hand_output->landmarks[i].y = (float)(global_keypoint_int[i*3+1] - HAND_LANDMARK_GLOBAL_KP_ZERO_POINT) * HAND_LANDMARK_GLOBAL_KP_SCALE;
        hand_output->landmarks[i].z = (float)(global_keypoint_int[i*3+2] - HAND_LANDMARK_GLOBAL_KP_ZERO_POINT) * HAND_LANDMARK_GLOBAL_KP_SCALE;
    }

    return 0;
}
