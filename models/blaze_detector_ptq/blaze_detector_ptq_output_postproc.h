/*
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __BLAZE_DETECTOR_OUTPUT_POSTPROC_H__
#define __BLAZE_DETECTOR_OUTPUT_POSTPROC_H__

#include <stdint.h>

#include "mpp_api_types.h"
#include "utils.h"
#include "mpp_config.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/**
 * Process the BLAZE_DETECTOR_PTQ output tensors
 *
 * @param [in] inf_out: inference output (tensor data and description)
 * @param [out] final_boxes: array of object bounding boxes
 * @param [in] nb_box_max: nb of elements in array final_boxes
 * @param [in] return_biggest_hand: if true, return only the detected hand with biggest area
 *
 * @return: 0 if succeeded, else failed.
 */

int32_t BlazeDetectorPtq_ProcessOutput(const mpp_inference_cb_param_t *inf_out, box_data* final_boxes, int nb_box_max, bool return_biggest_hand);

#endif /* __BLAZE_DETECTOR_OUTPUT_POSTPROC_H__ */