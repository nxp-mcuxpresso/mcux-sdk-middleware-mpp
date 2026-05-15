/*
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * The function that processes the tensor output of model blaze_detector_ptq
 */

#include <stdio.h>
#include <math.h>

#include "fsl_debug_console.h"

#include "mpp_config.h"
#include "mpp_api.h"

#include APP_TFLITE_BLAZE_DETECTOR_PTQ_INFO
#include "blaze_detector_ptq_output_postproc.h"
#include "utils.h"

#define EOL "\r\n"

static const float NMS_THRESH = 0.3f;

typedef struct anchor_s {
    float x_center;
    float y_center;
    float w;
    float h;
} anchor_t;

static anchor_t s_anchors[BLAZE_DETECTOR_NUM_BOXES];

typedef struct ssd_anchors_calculator_options_s {
    int input_size_width;
    int input_size_height;
    float anchor_offset_x;
    float anchor_offset_y;

    int num_layers;
    int* anchor_per_pixel;
    int* steps, steps_size;

    int* min_sizes;
    int* min_sizes_length_each;
    int min_sizes_cnt;

    int clip;
} ssd_anchors_calculator_options_t;

int generate_anchors(anchor_t *anchors, const ssd_anchors_calculator_options_t *options)
{
    int feature_maps[2][2];
    int *min_sizes = options->min_sizes;
    int *steps = options->steps;
    int cnt = 0;

    for (int i = 0; i < options->steps_size; i++) {
        feature_maps[i][0] = ceilf(options->input_size_width / options->steps[i]);
        feature_maps[i][1] = ceilf(options->input_size_height / options->steps[i]);
    }

    for (int k = 0; k < options->steps_size; k++) {
        int *f = feature_maps[k];

        for (int i = 0; i < f[0]; i++) {
            for (int j = 0; j < f[1]; j++) {
                for (int min_size_cnt = 0; min_size_cnt < options->min_sizes_length_each[k]; min_size_cnt++) {
                    float s_kx = 1.0f * min_sizes[min_size_cnt] / options->input_size_width;
                    float s_ky = 1.0f * min_sizes[min_size_cnt] / options->input_size_height;

                    float cx = (j + 0.5) * options->steps[k] / options->input_size_width;
                    float cy = (i + 0.5) * options->steps[k] / options->input_size_height;
                    anchors[cnt].x_center = cx;
                    anchors[cnt].y_center = cy;
                    anchors[cnt].w = s_kx;
                    anchors[cnt++].h = s_ky;
                }
            }
        }
        min_sizes += options->min_sizes_length_each[k];
    }
    return 0;
}

int generate_ssd_anchors(void)
{
    /* each strides has 2 anchor per piexel per aspect_ratios;
     * the same strides will have extra * 2, that means for {16, 16, 16}, will have 2 * 3 */
    int strides[2] = { 8, 16 };
    int min_sizes_cnt = 2;
    int min_sizes_length_each[] = { 2, 6 };
    int min_sizes[] = { 8, 11, 14, 19, 26, 38, 64, 149 };

    ssd_anchors_calculator_options_t anchor_options;
    anchor_options.input_size_height = BLAZE_DETECTOR_HEIGHT;
    anchor_options.input_size_width = BLAZE_DETECTOR_WIDTH;
    anchor_options.anchor_offset_x = 0.5f;
    anchor_options.anchor_offset_y = 0.5f;

    anchor_options.steps = strides;
    anchor_options.steps_size = sizeof(strides) / sizeof(strides[0]);

    anchor_options.min_sizes = min_sizes;
    anchor_options.min_sizes_length_each = min_sizes_length_each;
    anchor_options.min_sizes_cnt = min_sizes_cnt;

    anchor_options.clip = 1;

    generate_anchors(s_anchors, &anchor_options);

    return 0;
}

static void compute_rotation(box_data *box)
{
    float x0 = box->landmarks[BLAZE_DETECTOR_WRIST_LANDMARK_IDX].x;
    float y0 = box->landmarks[BLAZE_DETECTOR_WRIST_LANDMARK_IDX].y;
    float x1 = box->landmarks[BLAZE_DETECTOR_MIDDLE_FINGER_IDX].x;
    float y1 = box->landmarks[BLAZE_DETECTOR_MIDDLE_FINGER_IDX].y;

    float theta = atan2f((y1 - y0), (x1 - x0));
    float angle_y = fabsf(theta);

    angle_y = 90.0f - (angle_y * (180.f/M_PI));

    if( angle_y < 0.0f )
        angle_y = fabsf(angle_y);
    else
        box->rotation = 0 - angle_y;

    if (fabsf(angle_y) < 45)
        box->rotation = 0;
    else
        box->rotation = angle_y;

    int w = (box->right - box->left), h = (box->bottom - box->top);

    int new_w = MIN((int)(w * 2.6f), BLAZE_DETECTOR_WIDTH);
    int new_h = MIN((int)(h * 2.6f), BLAZE_DETECTOR_HEIGHT);

    float offset_x = 0;
    float offset_y = 0;

    if ((box->rotation > 45.0f) && (box->rotation < 135.0f))
    {
        offset_x = -0.6;
        offset_y = 0;
    }
    else if ((box->rotation < -45.0f ) && (box->rotation > -135.0f))
    {
        offset_x = 0.6;
        offset_y = 0;
    }
    else
    {
        offset_x = 0;
        offset_y = -0.6;
    }

    int x_begin = MAX(box->left + (int)(offset_x * w) - (new_w - w) / 2.0f, 0);
    int y_begin = MAX(box->top + (int)(offset_y * h) - (new_h - h) / 2.0f, 0);

    box->left = (x_begin);
    box->top = (y_begin);
    box->right = MIN(((x_begin + new_w)),BLAZE_DETECTOR_WIDTH);
    box->bottom = MIN(((y_begin + new_h)),BLAZE_DETECTOR_HEIGHT);
}

static int decode_output_boxes(const int8_t* boxes_int, const int8_t* score_int, int nb_box_max, box_data* final_boxes, bool return_biggest_hand)
{
    int n_inserted = 0;

    // Generate center priors for each stride level
    generate_ssd_anchors();

    // Process keypoints and bounding boxes from quantized output
    for (int i=0; i<BLAZE_DETECTOR_NUM_BOXES; i++)
    {
        anchor_t anchor = s_anchors[i];
        box_data curr_box = {0};
        float score;

        score = (float)(score_int[i] - BLAZE_DETECTOR_SCORE_ZERO_POINT) * BLAZE_DETECTOR_SCORE_SCALE;

        if (score <= BLAZE_DETECTOR_PALM_SCORE_THRES)
            continue;

        curr_box.score = score;

        float p[BLAZE_DETECTOR_NUM_POINTS_PER_PALM];

        for (int j=0;j<BLAZE_DETECTOR_NUM_POINTS_PER_PALM;j++)
            p[j] = (float) (boxes_int[i * BLAZE_DETECTOR_NUM_POINTS_PER_PALM + j] - BLAZE_DETECTOR_BOXES_ZERO_POINT) * BLAZE_DETECTOR_BOXES_SCALE;

        /* boundary box */
        float sx = p[0];
        float sy = p[1];
        float w = p[2];
        float h = p[3];

        float cx = sx * BLAZE_DETECTOR_VAR_0 * anchor.w + anchor.x_center;
        float cy = sy * BLAZE_DETECTOR_VAR_0 * anchor.h + anchor.y_center;
        w = expf(w * BLAZE_DETECTOR_VAR_1) * anchor.w;
        h = expf(h * BLAZE_DETECTOR_VAR_1) * anchor.h;

        curr_box.left = (cx - w * 0.5f) * BLAZE_DETECTOR_WIDTH;
        curr_box.top = (cy - h * 0.5f) * BLAZE_DETECTOR_HEIGHT;
        curr_box.right = (cx + w * 0.5f) * BLAZE_DETECTOR_WIDTH;
        curr_box.bottom = (cy + h * 0.5f) * BLAZE_DETECTOR_HEIGHT;

        /* landmark positions */
        for (int j = 0; j < MODEL_NUM_LANDMARKS; j++)
        {
            float lx = p[4 + (2 * j) + 0];
            float ly = p[4 + (2 * j) + 1];
            lx = lx * BLAZE_DETECTOR_VAR_0 * anchor.w + anchor.x_center;
            ly = ly * BLAZE_DETECTOR_VAR_0 * anchor.h + anchor.y_center;

            curr_box.landmarks[j].x = lx*BLAZE_DETECTOR_WIDTH;
            curr_box.landmarks[j].y = ly*BLAZE_DETECTOR_HEIGHT;
        }

        // Insert box with NMS
        int result = nms_insert_box(final_boxes, curr_box, n_inserted, NMS_THRESH, nb_box_max);
        if (result < 0) {
            PRINTF("ERROR: NMS insertion failed for box %d\n", n_inserted);
        } else {
            n_inserted = result;
        }
    }

    // If multiple hands detected after NMS, keep only the one with largest area
    if ((n_inserted > 1) && return_biggest_hand) {
        int largest_idx = 0;
        float largest_area = area(&final_boxes[0]);

        for (int i = 1; i < n_inserted; i++) {
            float current_area = area(&final_boxes[i]);
            if (current_area > largest_area) {
                largest_area = current_area;
                largest_idx = i;
           }
        }

        // Move the largest hand to index 0 if it's not already there
        if (largest_idx != 0) {
            final_boxes[0] = final_boxes[largest_idx];
        }

        // Clear the rest of the array
        for (int i = 1; i < n_inserted; i++) {
            memset(&final_boxes[i], 0, sizeof(box_data));
        }

        compute_rotation(&final_boxes[0]);

        return 1; // Return 1 since we only keep one hand
    }

    // Compute rotation for all detected hands
    if (n_inserted > 0) {
        for (int i = 0; i < n_inserted; i++) {
            compute_rotation(&final_boxes[i]);
        }
    }

    return n_inserted;
}

int32_t BlazeDetectorPtq_ProcessOutput(const mpp_inference_cb_param_t *inf_out, box_data* final_boxes, int nb_box_max, bool return_biggest_hand)
{
    if (inf_out == NULL) {
        PRINTF("ERROR: BlazeDetectorPtq_ProcessOutput parameter 'inf_out' is null pointer" EOL);
        return -1;
    }
    if (final_boxes == NULL) {
        PRINTF("ERROR: BlazeDetectorPtq_ProcessOutput parameter 'final_boxes' is null pointer" EOL);
        return -1;
    }

    /* clear old data */
    memset(final_boxes, 0, nb_box_max*sizeof(box_data));

    // get the 2 output tensors
    int8_t* boxes_int;
    int8_t* score_int;

    if(inf_out->inference_type == MPP_INFERENCE_TYPE_TFLITE)
    {
        boxes_int = (int8_t *) inf_out->out_tensors[BLAZE_DETECTOR_BOXES_OUTPUT_IDX]->data; /* [1, 2016, 18]  matrix */
        score_int = (int8_t *) inf_out->out_tensors[BLAZE_DETECTOR_SCORE_OUTPUT_IDX]->data; /* [1, 2016, 1]  matrix */

        if (boxes_int == NULL)
        {
            PRINTF("ERROR: BlazeDetectorPtq_ProcessOutput: boxes_int NULL pointer" EOL);
            return -1;
        }
        if (score_int == NULL)
        {
            PRINTF("ERROR: BlazeDetectorPtq_ProcessOutput: score_int NULL pointer" EOL);
            return -1;
        }
    }
    else
    {
        PRINTF("ERROR: BlazeDetectorPtq_ProcessOutput: Undefined Inference Engine" EOL);
        return -1;
    }

    decode_output_boxes(boxes_int, score_int, nb_box_max, final_boxes, return_biggest_hand);

    return 0;
}
