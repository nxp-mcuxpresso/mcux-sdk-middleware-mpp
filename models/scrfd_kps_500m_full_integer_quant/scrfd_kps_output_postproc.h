/*
 * Copyright 2025-2026 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SCRFD_KPS_OUTPUT_POSTPROCESS_H_
#define _SCRFD_KPS_OUTPUT_POSTPROCESS_H_

#include "mpp_api_types.h"
#include "utils.h"
#include "mpp_config.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/**
 * Process the SCRFD_KPS output tensors
 *
 * @param [in] inf_out: inference output (tensor data and description)
 * @param [out] final_boxes: array of object bounding boxes
 * @param [in] nb_box_max: nb of elements in array final_boxes
 *
 * @return: 0 if succeeded, else failed.
 */
int32_t SCRFDKPS_ProcessOutput(const mpp_inference_cb_param_t *inf_out, box_data* final_boxes, int nb_box_max, bool return_biggest_face);

#endif /* _SCRFD_KPS_OUTPUT_POSTPROCESS_H_ */
