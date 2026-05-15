/*
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * The function that processes the tensor output of model gesture classifier
 */

#include <stdint.h>

#include "fsl_debug_console.h"

#include "mpp_config.h"
#include "mpp_api.h"

#include APP_TFLITE_GESTURE_CLASSIFIER_INFO
#include "canned_gesture_classifier_output_postproc.h"
#include "utils.h"

#define EOL "\r\n"

const char* gesture_labels[] = {
    "None",
    "Closed_Fist",
    "Open_Palm",
    "Pointing_Up",
    "Thumb_Down",
    "Thumb_Up",
    "Victory",
    "ILoveYou"
};

int32_t GestureClassifier_ProcessOutput(const mpp_inference_cb_param_t *inf_out, gesture_data* gesture_output)
{
    if (inf_out == NULL) {
        PRINTF("ERROR: GestureClassifier_ProcessOutput parameter 'inf_out' is null pointer" EOL);
        return -1;
    }
    if (gesture_output == NULL) {
        PRINTF("ERROR: GestureClassifier_ProcessOutput parameter 'gesture_output' is null pointer" EOL);
        return -1;
    }

    /* clear old data */
    memset(gesture_output, 0, sizeof(gesture_data));

    // get the output tensor
    float *gest_cls_output;

    if(inf_out->inference_type == MPP_INFERENCE_TYPE_TFLITE)
    {
        gest_cls_output = (float *) inf_out->out_tensors[GESTURE_CLASSIFIER_SCORE_OUTPUT_IDX]->data; /* [1, 8]  matrix */

        if (gest_cls_output == NULL)
        {
            PRINTF("ERROR: GestureClassifier_ProcessOutput: gest_cls_output NULL pointer" EOL);
            return -1;
        }
    }
    else
    {
        PRINTF("ERROR: GestureClassifier_ProcessOutput: Undefined Inference Engine" EOL);
        return -1;
    }

    float score = 0.0f;
    int32_t gesture_id = 0;

    for (int i = 0; i < GESTURE_CLASSIFIER_OUTPUT_SIZE; i++) {
        if (gest_cls_output[i] > score) {
            score = gest_cls_output[i];
            gesture_id = i;
        }
    }

    gesture_output->score = score;
    gesture_output->gesture_id = gesture_id;
    /* copy gesture label string */
    strncpy(gesture_output->gesture, gesture_labels[gesture_id], sizeof(gesture_output->gesture) - 1);
    gesture_output->gesture[sizeof(gesture_output->gesture) - 1] = '\0';

    return 0;
}
