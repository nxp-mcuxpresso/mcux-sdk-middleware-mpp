/*
 * Copyright 2026 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>

#include "cifar10_output_postproc.h"
#include "get_top_n.h"
#include "cifar10_labels.h"
#include "mpp_config.h"
#include APP_EXECUTORCH_CIFAR_INFO

extern "C" {
#include "mpp_api.h"
}

#define NUM_RESULTS        1
static const int DETECTION_TRESHOLD = 60;

int32_t CIFAR10_ProcessOutput(const mpp_inference_cb_param_t *inf_out, void *mpp,
        mpp_elem_handle_t elem, mpp_labeled_rect_t *rects, cifar_post_proc_data_t* out_data)
{
    const float threshold = (float)DETECTION_TRESHOLD / 100;
    result_t topResults[NUM_RESULTS];
    const char* label = "No label detected";
    uint32_t tensor_size;
    mpp_tensor_type_t tensor_type;

    /* Some inference types do not provide model output size and type. */
    if (inf_out->out_tensors[0]->dims.size == 0) {
        tensor_size = CIFAR_TENSOR_SIZE;
        tensor_type = CIFAR_TENSOR_TYPE;
    } else {
        tensor_size = inf_out->out_tensors[0]->dims.data[inf_out->out_tensors[0]->dims.size - 1];
        tensor_type = inf_out->out_tensors[0]->type;
    }

    /* Find best label candidates. */
    MODEL_GetTopN(inf_out->out_tensors[0]->data, tensor_size, tensor_type,
                  NUM_RESULTS, threshold, topResults);

    float confidence = 0;
    if (topResults[0].index >= 0)
    {
        confidence = topResults[0].score;
        int index = topResults[0].index;
        if (confidence * 100 > DETECTION_TRESHOLD)
        {
            label = labels[index];
        }
    }

    int score = (int)(confidence * 100);

    if (out_data)
    {
        out_data->score = score;
        out_data->label = label;
    }

    if ((mpp != NULL) && (elem != 0) && (rects != NULL))
    {
        mpp_element_params_t params;
        memset(&params, 0, sizeof(params));
        uint8_t label_size = sizeof(params.labels.rectangles[0].label);
        params.labels.detected_rect = 1;
        params.labels.max_rect = 1;
        params.labels.rectangles = rects;
        strncpy((char *)params.labels.rectangles[0].label, label, label_size);
        params.labels.rectangles[0].label[label_size - 1] = '\0';
        mpp_element_update(mpp, elem, &params, true);
    }

    return 0;
}