/*
 * Copyright 2024-2025 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "app_constants.h"
#include "models/utils.h"
#include "string.h"
#include "math.h"
#include "stdbool.h"
#include "stdio.h"

#ifdef APP_ULTRAFACE_ULTRASLIM
static const mpp_color_t sc_green = {.rgb.R = 0x0, .rgb.G = 0xff, .rgb.B = 0x0 };
static const mpp_color_t sc_red = {.rgb.R = 0xff, .rgb.G = 0x0, .rgb.B = 0x0 };
#ifndef APP_DYNAMIC_RECO_ZONE
static const mpp_color_t sc_blue = {.rgb.R = 0x0, .rgb.G = 0x0, .rgb.B = 0xff };
#endif
#else
static const mpp_color_t sc_blue = {.rgb.R = 0x0, .rgb.G = 0x0, .rgb.B = 0xff };
#endif

#ifdef APP_ULTRAFACE_ULTRASLIM
int get_face_crop_area_ultraface(box_data *box, mpp_area_t *area)
{
    int err = 0;
    /*** create a square box containing the detected face box ***/
    /* X coord of center of detected box (in scrfd coordinates) */
    int x_center_face = (box->left + box->right)/2;
    int y_center_face = (box->top + box->bottom)/2;
    int square_box_dim = (box->bottom - box->top) * CROP_SIZE_TOP * FACE_BOX_RATIO / ULTRAFACE_HEIGHT / 100;
    int face_box_left = CROP_LEFT + (x_center_face * CROP_SIZE_LEFT / ULTRAFACE_WIDTH) - (square_box_dim / 2);
    
    area->top = CROP_TOP + (y_center_face * CROP_SIZE_TOP / ULTRAFACE_HEIGHT) - (square_box_dim / 2);
    area->bottom = area->top + square_box_dim;
    area->left = face_box_left;
    area->right = face_box_left + square_box_dim;

    /* if box crosses limit, fix box and return failure */
    if (area->left < 0)
    {
        area->left = 0;
        err = -1;
    }
    if (area->right >= INF_SRC_WIDTH)
    {
        area->right = INF_SRC_WIDTH - 1;
        err = -1;
    }
    if (area->bottom >= INF_SRC_HEIGHT)
    {
        area->bottom = INF_SRC_HEIGHT - 1;
        err = -1;
    }
    if (area->top < 0)
    {
        area->top = 0;
        err = -1;
    }
    
    return err;
}
#else
/* get a centered square crop area for SCRFD */
int get_face_crop_area_scrfd(box_data *box, mpp_area_t *area)
{
    int err = 0;
    /*** create a square box containing the detected face box ***/
    /* X coord of center of detected box (in scrfd coordinates) */
    int x_center_face = (box->left + box->right)/2;
    int y_center_face = (box->top + box->bottom)/2;
    int square_box_dim = (box->bottom - box->top) * CROP_SIZE_TOP * FACE_BOX_RATIO / SCRFD_KPS_HEIGHT / 100;
    int face_box_left = CROP_LEFT + (x_center_face * CROP_SIZE_LEFT / SCRFD_KPS_WIDTH) - (square_box_dim / 2);
    
    area->top = CROP_TOP + (y_center_face * CROP_SIZE_TOP / SCRFD_KPS_HEIGHT) - (square_box_dim / 2);
    area->bottom = area->top + square_box_dim;
    area->left = face_box_left;
    area->right = face_box_left + square_box_dim;

        /* if box crosses limit, fix box and return failure */
    if (area->left < 0)
    {
        area->left = 0;
        err = -1;
    }
    if (area->right >= INF_SRC_WIDTH)
    {
        area->right = INF_SRC_WIDTH - 1;
        err = -1;
    }
    if (area->bottom >= INF_SRC_HEIGHT)
    {
        area->bottom = INF_SRC_HEIGHT - 1;
        err = -1;
    }
    if (area->top < 0)
    {
        area->top = 0;
        err = -1;
    }
    
    return err;
}

/* process the face alignement based on the landmarks.
 * @param [in] box pointer to box_data
 * @returns true if face aligned and false if not aligned
 */
bool process_face_orientation(box_data *box)
{
    /* Calculate face tilt and yaw using landmarks */
    face_data_t face_data = {0};
    float tilt = 0.0f, yaw = 0.0f;
    if (SCRFD_NUM_LANDMARKS >= 3) 
    {
        /* Map landmarks from box_data to face_data_t format */
        /* Assuming landmarks order: left_eye, right_eye, nose, mouth_left, mouth_right */
        face_data.leye = box->landmarks[0]; /* left eye */
        face_data.reye = box->landmarks[1]; /* right eye */
        face_data.nose = box->landmarks[2]; /* nose */
        int tilt_yaw_ret = get_face_tilt_yaw(&face_data, &tilt, &yaw);
        if (tilt_yaw_ret != FACE_TILT_YAW_SUCCESS) {
            printf("Face tilt/yaw calculation failed\n");
            return false;
        }
        else{
            tilt = tilt * 180.0f / M_PI;  
            yaw =  yaw * 180.0f / M_PI;
            if ((tilt < 5 && tilt > -5) && (yaw < 8 &&  yaw > -8))
            {
                return true;
            }
            else 
            {
                return false;
            }
        } 
    }
    else {
        printf("Not enough landmarks for face tilt/yaw calculation\r\n");
        return false;
    }
}
#endif

int get_face_crop_area(box_data *box, mpp_area_t *area)
{
#ifdef APP_ULTRAFACE_ULTRASLIM
    return get_face_crop_area_ultraface(box, area);
#else
    return get_face_crop_area_scrfd(box, area);
#endif
}

/* returns true if 'rect' is inside recognition zone */
bool check_face_inside(mpp_labeled_rect_t *rect)
{
    bool ret = false;
    if ( (rect->left > RECO_ZONE_RECT_LEFT) && (rect->right < RECO_ZONE_RECT_RIGHT)
            && (rect->top > RECO_ZONE_RECT_TOP) && (rect->bottom < RECO_ZONE_RECT_BOTTOM) )
        ret = true;
    else
        ret = false;
    return ret;
}

/* returns true if 'rect' center is close enough from recognition zone center */
bool check_face_center(mpp_labeled_rect_t *rect)
{
    bool ret = false;
    /* zone center will not change */
    static const int zone_center_x = (RECO_ZONE_RECT_LEFT + RECO_ZONE_RECT_RIGHT) / 2;
    static const int zone_center_y = (RECO_ZONE_RECT_BOTTOM + RECO_ZONE_RECT_TOP) / 2;
    int rect_center_x = (rect->left + rect->right) / 2;
    int rect_center_y = (rect->bottom + rect->top) / 2;

    if ( (ABS(rect_center_x - zone_center_x) < MAX_CENTER_DIST)
        && (ABS(rect_center_y - zone_center_y) < MAX_CENTER_DIST) )
        ret = true;
    else
        ret = false;
    return ret;
}

/* returns 'true' if 'rect' area matches recognition zone area
 * else return 'false' and sets boolean 'far':
 * 'true' means too far, 'false' means too close */
bool check_face_size(mpp_labeled_rect_t *rect, bool *is_far)
{
    bool ret = false;
    if ( ((rect->bottom - rect->top) * (rect->right - rect->left)) < MIN_FACE_AREA )
    {
        ret = false;
        *is_far = true;
    }
    else if ( ((rect->bottom - rect->top) * (rect->right - rect->left)) > MAX_FACE_AREA )
    {
        ret = false;
        *is_far = false;
    }
    else
        ret = true;
    return ret;
}

#ifdef APP_ULTRAFACE_ULTRASLIM
/* UltraFace postprocessing */
bool boxes_to_rects_ultraface(box_data final_boxes[], uint32_t num_boxes, uint32_t max_boxes, mpp_labeled_rect_t *rects)
{
    uint32_t box_counter = 1;
    bool ret = false;
    
    for (uint32_t i = 0; i < num_boxes && box_counter < max_boxes; i++) {
        if (final_boxes[i].area == 0) continue;
        
        char *label = FACE_LABEL_CENTER;
        
#ifdef APP_DYNAMIC_RECO_ZONE
        mpp_area_t area;
        int err = get_face_crop_area_ultraface(&final_boxes[i], &area);
        
        rects[box_counter].left = area.left * VIEW_WIDTH / INF_SRC_WIDTH;
        rects[box_counter].right = area.right * VIEW_WIDTH / INF_SRC_WIDTH;
        rects[box_counter].bottom = area.bottom * VIEW_HEIGHT / INF_SRC_HEIGHT;
        rects[box_counter].top = area.top * VIEW_HEIGHT / INF_SRC_HEIGHT;
        rects[box_counter].line_width = RECT_LINE_WIDTH;
        
        if (((final_boxes[i].score * 100) > FACE_DET_THRESHOLD) && (err == 0)) {
            label = FACE_LABEL_OK;
            rects[box_counter].line_color = sc_green;
            ret = true;
        } else {
            label = FACE_LABEL_UNCLEAR;
            rects[box_counter].line_color = sc_red;
        }
#else
        bool is_far = false;
        rects[box_counter].left = (int)((final_boxes[i].left * DETECTION_ZONE_RECT_WIDTH) / ULTRAFACE_WIDTH) + BOXES_OFFSET_LEFT;
        rects[box_counter].right = (int)((final_boxes[i].right * DETECTION_ZONE_RECT_WIDTH) / ULTRAFACE_WIDTH) + BOXES_OFFSET_LEFT;
        rects[box_counter].bottom = (int)((final_boxes[i].bottom * DETECTION_ZONE_RECT_HEIGHT) / ULTRAFACE_HEIGHT) + BOXES_OFFSET_TOP;
        rects[box_counter].top = (int)((final_boxes[i].top * DETECTION_ZONE_RECT_HEIGHT) / ULTRAFACE_HEIGHT) + BOXES_OFFSET_TOP;
        rects[box_counter].line_width = RECT_LINE_WIDTH;
        rects[box_counter].line_color = sc_blue;
        
        if (check_face_inside(&rects[box_counter])) {
            if (check_face_center(&rects[box_counter])) {
                if (check_face_size(&rects[box_counter], &is_far)) {
                    label = FACE_LABEL_OK;
                    ret = true;
                } else if (is_far) {
                    label = FACE_LABEL_FAR;
                } else {
                    label = FACE_LABEL_CLOSE;
                }
            } else {
                label = FACE_LABEL_CENTER;
            }
        } else {
            label = FACE_LABEL_OUTSIDE;
        }
#endif
        strncpy((char *)rects[box_counter].label, label, sizeof(rects[box_counter].label));
        box_counter++;
    }
    return ret;
}
#else
/* SCRFD postprocessing */
 bool boxes_to_rects_scrfd(box_data final_boxes[], uint32_t num_boxes, uint32_t max_boxes, mpp_labeled_rect_t *rects)
{
    uint32_t box_counter = 1;
    bool ret = false;
    
    for (uint32_t i = 0; i < num_boxes && box_counter < max_boxes; i++) {
        if (final_boxes[i].area == 0) continue;
        
        char *label = FACE_LABEL_CENTER;
        
        bool is_far = false;

        mpp_area_t area;
        get_face_crop_area(&final_boxes[i], &area);

        /* scale the area to view */
        rects[box_counter].left = area.left * VIEW_WIDTH / INF_SRC_WIDTH;
        rects[box_counter].right = area.right * VIEW_WIDTH / INF_SRC_WIDTH;
        rects[box_counter].bottom = area.bottom * VIEW_HEIGHT / INF_SRC_HEIGHT;
        rects[box_counter].top = area.top * VIEW_HEIGHT / INF_SRC_HEIGHT;
        rects[box_counter].line_width = RECT_LINE_WIDTH;
        rects[box_counter].line_color = sc_blue;

        /* check face position to set indication in label */
        if (check_face_inside(&rects[box_counter]))
            if (check_face_size(&rects[box_counter], &is_far))
            {
                if(process_face_orientation(&final_boxes[i]))
                {
                    label = FACE_LABEL_OK;
                    ret = true;
                }
                else
                    label = FACE_LABEL_NOT_ALIGNED;
            }
            else if (is_far)
                label = FACE_LABEL_FAR;
            else
                label = FACE_LABEL_CLOSE;
        else
            label = FACE_LABEL_OUTSIDE;

        strncpy((char *)rects[box_counter].label, label, sizeof(rects[box_counter].label));
        box_counter++;
    }
    return ret;
}
#endif

/* Main public function - calls appropriate postprocessing */
bool boxes_to_rects(box_data final_boxes[], uint32_t num_boxes, uint32_t max_boxes, mpp_labeled_rect_t *rects)
{
#ifdef APP_ULTRAFACE_ULTRASLIM
    return boxes_to_rects_ultraface(final_boxes, num_boxes, max_boxes, rects);
#else
    return boxes_to_rects_scrfd(final_boxes, num_boxes, max_boxes, rects);
#endif
}
