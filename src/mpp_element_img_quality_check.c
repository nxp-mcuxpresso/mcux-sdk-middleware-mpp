/*
 * Copyright 2026 NXP.
 *
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include <math.h>
#include <assert.h>

#include "mpp_api.h"
#include "mpp_api_types_internal.h"
#include "mpp_debug.h"

#include "hal_utils.h"

/* Quality check max resolution */
#define QUAL_CHECK_MAX_WIDTH  1920
#define QUAL_CHECK_MAX_HEIGHT 1080

/* main processing function for image quality check */
static int quality_check_func(_elem_t *elem)
{
    int ret = MPP_SUCCESS;
    img_quality_metrics_t img_quality_metrics = {0};

    if (elem == NULL || elem->io.in_buf[0] == NULL || elem->io.in_buf[0]->hw == NULL) {
        MPP_LOGE("ERROR: Invalid input parameters\r\n");
        return MPP_ERROR;
    }

    /* Skip quality check if disabled  */
    if (elem->params.img_quality_check.disable) {
        MPP_LOGD("Image quality check is disabled.\r\n");
        return MPP_SUCCESS;
    }

    uint8_t *buffer = (uint8_t *)elem->io.in_buf[0]->hw->addr;
    int width = elem->io.in_buf[0]->width;
    int height = elem->io.in_buf[0]->height;
    int stride = elem->io.in_buf[0]->hw->stride;
    mpp_pixel_format_t format = elem->io.in_buf[0]->format;

    assert(width > 0 && width <= QUAL_CHECK_MAX_WIDTH);
    assert(height > 0 && height <= QUAL_CHECK_MAX_HEIGHT);
    assert(stride >= width);

    int bytes_per_pixel = get_bitpp(format) / 8;
    if (bytes_per_pixel == 0) {
        MPP_LOGE("ERROR: Invalid or unsupported pixel format\r\n");
        return MPP_ERROR;
    }

    int mean_luminance = 0;
    int contrast = 0;

    int32_t mean_scaled = 0;
    int64_t M2_scaled = 0; /* Sum of squared differences from current mean */
    int32_t delta_scaled, delta2_scaled, variance_scaled;
    int pixel_index, total_pixels;
    uint8_t r, g, b;

    assert((int64_t)width * height <= INT32_MAX);
    total_pixels = width * height;

    for (int y = 0; y < height; y++) {
        uint8_t *row_ptr = buffer + (y * stride);
        for (int x = 0; x < width; x++) {
            uint8_t *pixel_ptr = row_ptr + (x * bytes_per_pixel);
            pixel_index = y * width + x;
            switch (format) {
                case MPP_PIXEL_RGB565: {
                    uint16_t pixel = *(uint16_t *)pixel_ptr;
                    r = ((pixel >> 11) & 0x1F) << 3;
                    g = ((pixel >> 5) & 0x3F) << 2;
                    b = (pixel & 0x1F) << 3;
                    break;
                }
                case MPP_PIXEL_RGB:
                case MPP_PIXEL_RGBA:
                case MPP_PIXEL_RGBX: {
                    r = pixel_ptr[0];
                    g = pixel_ptr[1];
                    b = pixel_ptr[2];
                    break;
                }
                case MPP_PIXEL_BGR:
                case MPP_PIXEL_BGRA:
                case MPP_PIXEL_BGRX: {
                    b = pixel_ptr[0];
                    g = pixel_ptr[1];
                    r = pixel_ptr[2];
                    break;
                }
                case MPP_PIXEL_ARGB: {
                    r = pixel_ptr[1];
                    g = pixel_ptr[2];
                    b = pixel_ptr[3];
                    break;
                }
                case MPP_PIXEL_GRAY:
                case MPP_PIXEL_GRAY888:
                case MPP_PIXEL_GRAY888X: {
                    r = g = b = pixel_ptr[0];
                    break;
                }
                default:
                    MPP_LOGE("Unsupported pixel format: %d\n", elem->io.in_buf[0]->format);
                    return MPP_ERROR;
            }
            /* Calculate luminance using BT.601 coefficients */
            /* Y = 0.299*R + 0.587*G + 0.114*B */
            /* Coefficients scaled by 1024: R=306, G=601, B=117 */
            /* Luminance is scaled by 1024 for fixed-point arithmetic */
            uint32_t luminance_scaled = 306U * r + 601U * g + 117U * b;

            /* Welford's algorithm to compute mean and variance in the same pass */
            delta_scaled = (int32_t)luminance_scaled - mean_scaled;
            mean_scaled += delta_scaled / (pixel_index + 1);
            delta2_scaled = (int32_t)luminance_scaled - mean_scaled;

            int64_t product = (int64_t)delta_scaled * delta2_scaled;
            assert(product >= INT64_MIN && product <= INT64_MAX);
            int64_t delta_M2 = product >> 10;
            /* Check for overflows */
            assert(M2_scaled <= INT64_MAX - delta_M2);
            M2_scaled += delta_M2;
        }
    }

    /* Calculate final statistics */
    mean_luminance = mean_scaled >> 10;

    /* Calculate standard deviation (contrast) */
    if (total_pixels > 1)
    {
        variance_scaled = M2_scaled / total_pixels;
        contrast = (int)sqrt((double)(variance_scaled >> 10));
    }
    img_quality_metrics.brightness = mean_luminance;
    img_quality_metrics.contrast = contrast;

    if (elem->mpp->params.evt_callback_f != NULL)
        elem->mpp->params.evt_callback_f(NULL, MPP_EVENT_QUALITY_CHECK_READY,
                (void *)&img_quality_metrics, elem->mpp->params.cb_userdata);

    return ret;
}

/* element setup function */
unsigned int elem_img_quality_check_setup(_elem_t *elem)
{
    int ret = MPP_SUCCESS;
    do {
        /* Sanity checks */
        if (elem == NULL)
        {
            MPP_LOGE("Input elem pointer is NULL\n");
            ret = MPP_INVALID_PARAM;
            break;
        }
        if ((elem->type != MPP_TYPE_PROC) || (elem->proc_typ != MPP_ELEMENT_IMG_QUALITY_CHECK))
        {
            MPP_LOGE("invalid element %s (expected element IMG_QUALITY_CHECK)\n", elem_name(elem));
            ret = MPP_INVALID_PARAM;
            break;
        }

        _mpp_t *mpp = elem->mpp;
        if (!mpp)
        {
            MPP_LOGE("mpp is null\n");
            ret = MPP_INVALID_PARAM;
            break;
        }

        /* Set operating mode */
        elem->io.inplace = true;
        /* Input buffer points to previous element buffer */
        elem->io.nb_in_buf = 1;
        elem->io.in_buf[0] = get_in_buff_from_prev_elem(elem);
        if (elem->io.in_buf[0] == NULL) {
            MPP_LOGE("No input buffer found from previous element\n");
            return MPP_ERROR;
        }
        elem->io.nb_out_buf = 1;
        /* Element process in-place */
        elem->io.out_buf[0] = elem->io.in_buf[0];
        
        /* Set element entry point */
        elem->entry = quality_check_func;
    } while (false);

    if (ret != MPP_SUCCESS) {
        if ( (elem != NULL) && (elem->io.out_buf[0] != NULL) )
            hal_free(elem->io.out_buf[0]);
    }
    
    return ret;
}

uint32_t mpp_img_quality_check_update(_elem_t *elem, mpp_element_params_t *params)
{
    int ret = MPP_SUCCESS;
    do {
        /* sanity checks */
        if (params == NULL) {
            MPP_LOGE("Failed to perform update: params buffer is invalid\n");
            ret = MPP_INVALID_PARAM;
            break;
        }
        elem->params.img_quality_check.disable = params->img_quality_check.disable;
    } while (false);
    
    return ret;
}