/*
 * Copyright 2022-2025 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _UTILS_H_
#define _UTILS_H_

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus*/

/* error codes for get_face_tilt_yaw function */
#define FACE_TILT_YAW_SUCCESS 0
#define FACE_TILT_YAW_ERROR_NULL_POINTER -1
#define FACE_TILT_YAW_ERROR_COLOCATED_LANDMARKS -2

#include <stdint.h>

#include "mpp_config.h"

/* structure for landmark (screen) coordinate */
typedef struct {
    int16_t x;
    int16_t y;
} coord_t;

/* structure for bounding boxes */
typedef struct {
    int16_t left;
    int16_t top;
    int16_t right;
    int16_t bottom;
    int16_t label;
    int32_t area;
    float score;
#ifdef MODEL_NUM_LANDMARKS
    coord_t landmarks[MODEL_NUM_LANDMARKS];
#endif
} box_data;

/* structure for face landmark (right/left eye, nose) */
/* note: left and right is from watcher perspective */
typedef struct {
    coord_t leye;
    coord_t reye;
    coord_t nose;
} face_data_t;

/* computes box area
 */
int32_t area(box_data *box);

/* computes IoU of box 1 and 2.
 * box->area must be pre-computed
 */
float iou(box_data* box1, box_data* box2);

/* Performs non-maximum suppression on an array of boxes.
 * box->area will be computed.
 * Process the result of NMS in-place.
 * Boxes where score < score_thr will be zeroed.
 * Boxes removed by NMS will be zeroed.
 */
void nms(box_data boxes[], int32_t num_boxes, float nms_thr, float score_thr);

/* Inserts a box into an array of boxes sorted by score
 * and performs non-maximum suppression in place on the fly.
 * This function does NOT filter bounding boxes with low scores;
 * Score filtering must be done before calling this function.
 * Array boxes[] should be larger than number of inserted boxes,
 * else valid boxes may be lost (check console for error log).
 * Returns the number of boxes inserted (and kept) in the array.
 */
int32_t nms_insert_box(box_data boxes[], box_data curr_box, int32_t n_inserted, float nms_thr, int32_t max_boxes);

/* Computes the tilt and yaw based on face landmarks
 * tilt is an angle in Radians (0 = vertical),
 * yaw is an angle delta in Radians between left eye and right eye angle wrt nose,
 * 0: centered, >0: turned left, <0: turned right
 * returns non-zero in case of error (landmarks co-located).
 */
int get_face_tilt_yaw(face_data_t *face, float *tilt, float *yaw);

#if defined(__cplusplus)
}
#endif /* __cplusplus*/

#endif /* _UTILS_H_ */
