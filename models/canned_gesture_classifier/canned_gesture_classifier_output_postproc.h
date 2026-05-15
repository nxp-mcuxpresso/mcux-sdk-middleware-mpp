/*
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __GESTURE_CLASSIFIER_OUTPUT_POSTPROC_H__
#define __GESTURE_CLASSIFIER_OUTPUT_POSTPROC_H__

/* structure for gesture data */
typedef struct {
    float score;
    uint32_t gesture_id;
    char gesture[GESTURE_CLASSIFIER_MAX_LABEL_LENGTH];
} gesture_data;

/**
 * Process the GESTURE_CLASSIFIER output tensors
 *
 * @param [in] inf_out: inference output (tensor data and description)
 * @param [out] gesture_output: gesture data (gesture label and confidence score)
 *
 * @return: 0 if succeeded, else failed.
 */

int32_t GestureClassifier_ProcessOutput(const mpp_inference_cb_param_t *inf_out, gesture_data* gesture_output);

#endif /* __GESTURE_CLASSIFIER_OUTPUT_POSTPROC_H__ */