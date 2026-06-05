/*
 * Copyright 2024-2025 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FACE_BOX_UTILS_H_
#define FACE_BOX_UTILS_H_

#include "models/utils.h"

/* Translate boxes into labeled rectangles using display characteristics */
bool boxes_to_rects(box_data final_boxes[], uint32_t num_boxes, uint32_t max_boxes, mpp_labeled_rect_t *rects);
bool boxes_to_rects_ultraface(box_data final_boxes[], uint32_t num_boxes, uint32_t max_boxes, mpp_labeled_rect_t *rects);
bool boxes_to_rects_scrfd(box_data final_boxes[], uint32_t num_boxes, uint32_t max_boxes, mpp_labeled_rect_t *rects);

/* get a centered square crop area, based on face detection box */
int get_face_crop_area(box_data *box, mpp_area_t *area);
int get_face_crop_area_scrfd(box_data *box, mpp_area_t *area);
int get_face_crop_area_ultraface(box_data *box, mpp_area_t *area);

#endif /* FACE_BOX_UTILS_H_ */
