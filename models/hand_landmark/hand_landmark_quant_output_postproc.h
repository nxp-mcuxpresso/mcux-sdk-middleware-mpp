/*
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __HAND_LANDMARK_OUTPUT_POSTPROC_H__
#define __HAND_LANDMARK_OUTPUT_POSTPROC_H__

#include <stdint.h>

#include "mpp_api_types.h"
#include "utils.h"
#include "mpp_config.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

typedef struct coord_3d_s {
    float x;
    float y;
    float z;
} coord_3d_t;

/* structure for hand landmark data */
typedef struct {
#ifdef MODEL_NUM_3D_LANDMARKS
    coord_3d_t landmarks[MODEL_NUM_3D_LANDMARKS];
#endif
    float score;
    bool has_hand;
    bool left_hand;
} hand_data;

/**
 * Process the HAND_LANDMARK output tensors
 *
 * @param [in] inf_out: inference output (tensor data and description)
 * @param [out] hand_output: hand landmark data (landmarks, hand presence, hand side)
 *
 * @return: 0 if succeeded, else failed.
 */

int32_t HandLandmark_ProcessOutput(const mpp_inference_cb_param_t *inf_out, hand_data* hand_output);

#endif /* __HAND_LANDMARK_OUTPUT_POSTPROC_H__ */