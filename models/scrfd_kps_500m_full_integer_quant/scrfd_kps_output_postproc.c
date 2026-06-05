/*
 * Copyright 2025-2026 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The function that processes the tensor output of model SCRFD_KPS_500M
 */

#include <stdio.h>
#include <math.h>
#include <float.h>

#include "get_top_n.h"
#include "fsl_debug_console.h"
#include "mpp_config.h"
#include APP_TFLITE_SCRFD_KPS_INFO
#include "scrfd_kps_output_postproc.h"

#include "mpp_api.h"

#define EOL "\r\n"

/* Check that the number of landmarks has been correctly defined at compile-time */
_Static_assert(MODEL_NUM_LANDMARKS == 5, "SCRFD KPS model requires 5 landmarks");
_Static_assert(sizeof(((box_data*)0)->landmarks) == 5 * sizeof(coord_t), "Landmarks array size mismatch");

static const float DETECTION_TRESHOLD = 0.5f;
static const float NMS_THRESH = 0.4f;

typedef struct {
    int x;
    int y;
} center_prior;

center_prior center_priors_small[SCRFD_KPS_OUTPUT_NUM_BOXES_SMALL]   = {0};
center_prior center_priors_medium[SCRFD_KPS_OUTPUT_NUM_BOXES_MEDIUM] = {0};
center_prior center_priors_large[SCRFD_KPS_OUTPUT_NUM_BOXES_LARGE]   = {0};

/**
 * @brief Parameters for processing detections at a specific stride level
 *
 * This structure contains all the necessary parameters for processing face detection
 * outputs at a particular stride level in the SCRFD model, including quantized
 * predictions and their corresponding dequantization parameters.
 */
typedef struct {
    const uint8_t* box_preds;      /**< Quantized bounding box predictions (4 values per box: left, top, right, bottom offsets) */
    const uint8_t* score_preds;    /**< Quantized confidence scores for each detection */
    const uint8_t* kps_preds;      /**< Quantized keypoint predictions (10 values per detection: 5 landmarks with x,y coordinates) */
    center_prior* center_priors;   /**< Array of center points for anchor generation at this stride level */
    int num_boxes;                 /**< Number of detection boxes to process at this stride level */
    float score_scale;             /**< Scale factor for dequantizing score predictions */
    float box_scale;               /**< Scale factor for dequantizing bounding box predictions */
    float kps_scale;               /**< Scale factor for dequantizing keypoint predictions */
    int score_zero_point;          /**< Zero point offset for dequantizing score predictions */
    int box_zero_point;            /**< Zero point offset for dequantizing bounding box predictions */
    int kps_zero_point;            /**< Zero point offset for dequantizing keypoint predictions */
    int stride;                    /**< Stride value for this detection level (e.g., 8, 16, 32) */
} stride_params_t;

static void generate_center_priors(const int input_height, const int input_width, int stride, center_prior *centers, int center_size)
{
    int num_points = 0;
    int feat_w = ceil((float)input_width / stride);
    int feat_h = ceil((float)input_height / stride);
    for (int y = 0; y < feat_h; y++)
    {
        for (int x = 0; x < feat_w; x++)
        {
            if (num_points >= center_size-1){
                PRINTF("Invalid number of points" EOL);
                break;
            }
            centers[num_points].x = x * stride;
            centers[num_points].y = y * stride;
            centers[num_points+1].x = x * stride;
            centers[num_points+1].y = y * stride;
            num_points = num_points + 2;
        }
    }
    return;
}

static int process_stride_detections(const stride_params_t* params, 
                                   int16_t* box_index, 
                                   box_data* final_boxes,
                                   int n_inserted,
                                   int nb_box_max)
{
    if (params == NULL || box_index == NULL || final_boxes == NULL) {
        PRINTF("ERROR: process_stride_detections: NULL parameter\n");
        return -1;
    }

    box_data curr_box = {0};
    
    for (int i = 0; i < params->num_boxes; i++) {
        curr_box.label = 0;

        // Dequantize score
        curr_box.score = (((float) params->score_preds[i] - params->score_zero_point)) * params->score_scale;

        if (curr_box.score > DETECTION_TRESHOLD) {

            // Dequantize box coordinates
            curr_box.left   = params->center_priors[i].x - (float)(params->box_preds[i * 4]     - params->box_zero_point) * params->box_scale * (float) params->stride;
            curr_box.top    = params->center_priors[i].y - (float)(params->box_preds[i * 4 + 1] - params->box_zero_point) * params->box_scale * (float) params->stride;
            curr_box.right  = params->center_priors[i].x + (float)(params->box_preds[i * 4 + 2] - params->box_zero_point) * params->box_scale * (float) params->stride;
            curr_box.bottom = params->center_priors[i].y + (float)(params->box_preds[i * 4 + 3] - params->box_zero_point) * params->box_scale * (float) params->stride;

            // Dequantize landmarks
            for (int j = 0; j < MODEL_NUM_LANDMARKS; j++) {
                // X coordinate
                int16_t res =  params->center_priors[i].x + (float)(params->kps_preds[i * MODEL_NUM_LANDMARKS * 2 + j * 2] - params->kps_zero_point) * params->kps_scale * (float) params->stride;
                curr_box.landmarks[j].x = res;
                
                // Y coordinate
                res = params->center_priors[i].y + (float)(params->kps_preds[i * MODEL_NUM_LANDMARKS * 2 + j * 2 + 1] - params->kps_zero_point) * params->kps_scale * (float) params->stride;
                curr_box.landmarks[j].y = res;
            }

            // Insert box with NMS
            int result = nms_insert_box(final_boxes, curr_box, n_inserted, NMS_THRESH, nb_box_max);
            if (result < 0) {
                PRINTF("ERROR: NMS insertion failed for box %d\n", *box_index);
                // Continue processing other boxes
            } else {
                n_inserted = result;
            }
        }

        (*box_index)++;
    }

    return n_inserted;
}


static int decode_output_boxes(const uint8_t* box_preds_small_int, const uint8_t* box_preds_medium_int, const uint8_t* box_preds_large_int, 
                              const uint8_t* score_preds_small_int, const uint8_t* score_preds_medium_int, const uint8_t* score_preds_large_int,
                              const uint8_t* kps_preds_small_int, const uint8_t* kps_preds_medium_int, const uint8_t* kps_preds_large_int,
                              int nb_box_max, box_data* final_boxes, bool return_biggest_face)
{
    int16_t box_index = 0;
    int n_inserted = 0;

    // Generate center priors for each stride level
    generate_center_priors(SCRFD_KPS_HEIGHT, SCRFD_KPS_WIDTH, SCRFD_KPS_STRIDE_SMALL, center_priors_small, SCRFD_KPS_OUTPUT_NUM_BOXES_SMALL);
    generate_center_priors(SCRFD_KPS_HEIGHT, SCRFD_KPS_WIDTH, SCRFD_KPS_STRIDE_MEDIUM, center_priors_medium, SCRFD_KPS_OUTPUT_NUM_BOXES_MEDIUM);
    generate_center_priors(SCRFD_KPS_HEIGHT, SCRFD_KPS_WIDTH, SCRFD_KPS_STRIDE_LARGE, center_priors_large, SCRFD_KPS_OUTPUT_NUM_BOXES_LARGE);

    // Process small stride
    stride_params_t small_params = {
        .box_preds = box_preds_small_int,
        .score_preds = score_preds_small_int,
        .kps_preds = kps_preds_small_int,
        .center_priors = center_priors_small,
        .num_boxes = SCRFD_KPS_OUTPUT_NUM_BOXES_SMALL,
        .score_scale = SCRFD_KPS_OUTPUT_SCORE_SMALL_SCALE,
        .box_scale = SCRFD_KPS_OUTPUT_BOXES_SMALL_SCALE,
        .kps_scale = SCRFD_KPS_OUTPUT_KPS_SMALL_SCALE,
        .score_zero_point = SCRFD_KPS_OUTPUT_SCORE_SMALL_ZERO_POINT,
        .box_zero_point = SCRFD_KPS_OUTPUT_BOXES_SMALL_ZERO_POINT,
        .kps_zero_point = SCRFD_KPS_OUTPUT_KPS_SMALL_ZERO_POINT,
        .stride = SCRFD_KPS_STRIDE_SMALL
    };
    n_inserted = process_stride_detections(&small_params, &box_index, final_boxes, n_inserted, nb_box_max);
    if (n_inserted < 0) return -1;

    // Process medium stride
    stride_params_t medium_params = {
        .box_preds = box_preds_medium_int,
        .score_preds = score_preds_medium_int,
        .kps_preds = kps_preds_medium_int,
        .center_priors = center_priors_medium,
        .num_boxes = SCRFD_KPS_OUTPUT_NUM_BOXES_MEDIUM,
        .score_scale = SCRFD_KPS_OUTPUT_SCORE_MEDIUM_SCALE,
        .box_scale = SCRFD_KPS_OUTPUT_BOXES_MEDIUM_SCALE,
        .kps_scale = SCRFD_KPS_OUTPUT_KPS_MEDIUM_SCALE,
        .score_zero_point = SCRFD_KPS_OUTPUT_SCORE_MEDIUM_ZERO_POINT,
        .box_zero_point = SCRFD_KPS_OUTPUT_BOXES_MEDIUM_ZERO_POINT,
        .kps_zero_point = SCRFD_KPS_OUTPUT_KPS_MEDIUM_ZERO_POINT,
        .stride = SCRFD_KPS_STRIDE_MEDIUM
    };
    n_inserted = process_stride_detections(&medium_params, &box_index, final_boxes, n_inserted, nb_box_max);
    if (n_inserted < 0) return -1;

    // Process large stride
    stride_params_t large_params = {
        .box_preds = box_preds_large_int,
        .score_preds = score_preds_large_int,
        .kps_preds = kps_preds_large_int,
        .center_priors = center_priors_large,
        .num_boxes = SCRFD_KPS_OUTPUT_NUM_BOXES_LARGE,
        .score_scale = SCRFD_KPS_OUTPUT_SCORE_LARGE_SCALE,
        .box_scale = SCRFD_KPS_OUTPUT_BOXES_LARGE_SCALE,
        .kps_scale = SCRFD_KPS_OUTPUT_KPS_LARGE_SCALE,
        .score_zero_point = SCRFD_KPS_OUTPUT_SCORE_LARGE_ZERO_POINT,
        .box_zero_point = SCRFD_KPS_OUTPUT_BOXES_LARGE_ZERO_POINT,
        .kps_zero_point = SCRFD_KPS_OUTPUT_KPS_LARGE_ZERO_POINT,
        .stride = SCRFD_KPS_STRIDE_LARGE
    };
    n_inserted = process_stride_detections(&large_params, &box_index, final_boxes, n_inserted, nb_box_max);
    if (n_inserted < 0) return -1;

    // If multiple faces detected after NMS, keep only the one with largest area
    if ((n_inserted > 1) && return_biggest_face) {
        int largest_idx = 0;
        float largest_area = area(&final_boxes[0]);

        for (int i = 1; i < n_inserted; i++) {
            float current_area = area(&final_boxes[i]);
            if (current_area > largest_area) {
                largest_area = current_area;
                largest_idx = i;
           }
        }

        // Move the largest face to index 0 if it's not already there
		if (largest_idx != 0) {
			final_boxes[0] = final_boxes[largest_idx];
		}

		// Clear the rest of the array
		for (int i = 1; i < n_inserted; i++) {
			memset(&final_boxes[i], 0, sizeof(box_data));
		}

		return 1; // Return 1 since we only keep one face
    }

    return n_inserted;
}

int32_t SCRFDKPS_ProcessOutput(const mpp_inference_cb_param_t *inf_out, box_data* final_boxes, int nb_box_max, bool return_biggest_face)
{
    if (inf_out == NULL) {
        PRINTF("ERROR: SCRFDKPS_ProcessOutput parameter 'inf_out' is null pointer" EOL);
        return -1;
    }
    if (final_boxes == NULL) {
        PRINTF("ERROR: SCRFDKPS_ProcessOutput parameter 'final_boxes' is null pointer" EOL);
        return -1;
    }

    /* clear old data */
    memset(final_boxes, 0, nb_box_max*sizeof(box_data));

    // get all 9 output tensors
    uint8_t* score_preds_small_int;
    uint8_t* score_preds_medium_int;
    uint8_t* score_preds_large_int;

    uint8_t* box_preds_small_int;
    uint8_t* box_preds_medium_int;
    uint8_t* box_preds_large_int;

    uint8_t* kps_preds_small_int;
    uint8_t* kps_preds_medium_int;
    uint8_t* kps_preds_large_int;


    if(inf_out->inference_type == MPP_INFERENCE_TYPE_TFLITE)
    {
        score_preds_small_int  = (uint8_t *) inf_out->out_tensors[SCRFD_KPS_TENSOR_IDX_SCORE_SMALL]->data;  /* [1, 512, 1]  matrix */
        score_preds_medium_int = (uint8_t *) inf_out->out_tensors[SCRFD_KPS_TENSOR_IDX_SCORE_MEDIUM]->data; /* [1, 128, 1]  matrix */
        score_preds_large_int  = (uint8_t *) inf_out->out_tensors[SCRFD_KPS_TENSOR_IDX_SCORE_LARGE]->data;  /* [1, 32,  1]  matrix */
        box_preds_small_int    = (uint8_t *) inf_out->out_tensors[SCRFD_KPS_TENSOR_IDX_BOX_SMALL]->data;    /* [1, 512, 4]  matrix */
        box_preds_medium_int   = (uint8_t *) inf_out->out_tensors[SCRFD_KPS_TENSOR_IDX_BOX_MEDIUM]->data;   /* [1, 128, 4]  matrix */
        box_preds_large_int    = (uint8_t *) inf_out->out_tensors[SCRFD_KPS_TENSOR_IDX_BOX_LARGE]->data;    /* [1, 32,  4]  matrix */
        kps_preds_small_int    = (uint8_t *) inf_out->out_tensors[SCRFD_KPS_TENSOR_IDX_KPS_SMALL]->data;    /* [1, 512, 10] matrix */
        kps_preds_medium_int   = (uint8_t *) inf_out->out_tensors[SCRFD_KPS_TENSOR_IDX_KPS_MEDIUM]->data;   /* [1, 128, 10] matrix */
        kps_preds_large_int    = (uint8_t *) inf_out->out_tensors[SCRFD_KPS_TENSOR_IDX_KPS_LARGE]->data;    /* [1, 32,  10] matrix */
        if (score_preds_small_int == NULL)
        {
            PRINTF("ERROR: SCRFDKPS_ProcessOutput: score_preds_small_int NULL pointer" EOL);
            return -1;
        }
        if (score_preds_medium_int == NULL)
        {
            PRINTF("ERROR: SCRFDKPS_ProcessOutput: score_preds_medium_int NULL pointer" EOL);
            return -1;
        }
        if (score_preds_large_int == NULL)
        {
            PRINTF("ERROR: SCRFDKPS_ProcessOutput: score_preds_large_int NULL pointer" EOL);
            return -1;
        }
        if (box_preds_small_int == NULL)
        {
            PRINTF("ERROR: SCRFDKPS_ProcessOutput: box_preds_small_int NULL pointer" EOL);
            return -1;
        }
        if (box_preds_medium_int == NULL)
        {
            PRINTF("ERROR: SCRFDKPS_ProcessOutput: box_preds_medium_int NULL pointer" EOL);
            return -1;
        }
        if (box_preds_large_int == NULL)
        {
            PRINTF("ERROR: SCRFDKPS_ProcessOutput: box_preds_large_int NULL pointer" EOL);
            return -1;
        }
        if (kps_preds_small_int == NULL)
        {
            PRINTF("ERROR: SCRFDKPS_ProcessOutput: kps_preds_small_int NULL pointer" EOL);
            return -1;
        }
        if (kps_preds_medium_int == NULL)
        {
            PRINTF("ERROR: SCRFDKPS_ProcessOutput: kps_preds_medium_int NULL pointer" EOL);
            return -1;
        }
        if (kps_preds_large_int == NULL)
        {
            PRINTF("ERROR: SCRFDKPS_ProcessOutput: kps_preds_large_int NULL pointer" EOL);
            return -1;
        }
    }
    else
    {
        PRINTF("ERROR: SCRFDKPS_ProcessOutput: Undefined Inference Engine" EOL);
        return -1;
    }
    

    decode_output_boxes(box_preds_small_int,
                        box_preds_medium_int,
                        box_preds_large_int,
                        score_preds_small_int,
                        score_preds_medium_int,
                        score_preds_large_int,
                        kps_preds_small_int,
                        kps_preds_medium_int,
                        kps_preds_large_int,
                        nb_box_max, final_boxes,
                        return_biggest_face);

    return 0;
}
