/*
 * Copyright 2020-2026 NXP.
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

#include "mpp_api.h"
#include "mpp_api_types_internal.h"
#include "mpp_heap.h"
#include "hal_os.h"
#include "string.h"

#include "mpp_debug.h"
#include "hal_graphics_dev.h"
#include "hal.h"

/* fill-in the gfx source and destination structures */
static void set_gfx_dev(gfx_dev_t *gfx, _elem_t * elem)
{
    /* set source buffer */
    gfx->src.format = elem->io.in_buf[0]->format;
    gfx->src.width = elem->io.in_buf[0]->width;
    if (elem->io.in_buf[0]->stripe_num > 0)
        gfx->src.height = elem->io.in_buf[0]->height / MPP_STRIPE_NUM;
    else
        gfx->src.height = elem->io.in_buf[0]->height;
    gfx->src.swapByte = 0;

    /* set destination buffer */
    gfx->dst.format = elem->params.convert.pixel_format;
    gfx->dst.width = elem->params.convert.out_buf.width;
    if (elem->io.in_buf[0]->stripe_num > 0)
    {
        /* even if output is full frame, deal with stripe */
        float v_scaling = (float) elem->params.convert.scale.height / elem->io.in_buf[0]->height;
        gfx->dst.height = gfx->src.height * v_scaling;  /* output stripe height is input x scale */
    }
    else
        gfx->dst.height = elem->params.convert.out_buf.height;
    gfx->dst.swapByte = 0;

    /* crop source window */
    gfx->src.top = elem->params.convert.crop.top;
    gfx->src.bottom = elem->params.convert.crop.bottom;
    gfx->src.left = elem->params.convert.crop.left;
    gfx->src.right = elem->params.convert.crop.right;

    /* output destination window */
    gfx->dst.left = elem->params.convert.out_window.left;
    gfx->dst.top = elem->params.convert.out_window.top;
    gfx->dst.right = elem->params.convert.out_window.left + elem->params.convert.scale.width - 1;
    gfx->dst.bottom = elem->params.convert.out_window.top + elem->params.convert.scale.height - 1;

    return;
}

/* element processing function */
static int convert_func(_elem_t *elem)
{
    int ret;
    gfx_dev_t *gfx = elem->dev.gfx;
    buf_desc_t *obuf = elem->io.out_buf[0];
    buf_desc_t *ibuf = elem->io.in_buf[0];

    /* apply current params (already validated at setup/update) to the gfx structures */
    set_gfx_dev(gfx, elem);

    /* set source buffer address (only available at runtime) */
    gfx->src.buf = ibuf->hw->addr;
    gfx->src.pitch = ibuf->hw->stride;
    if (ibuf->format == MPP_PIXEL_YUV420P)
    {
        gfx->src.buf_u = ibuf->hw->addr_u;
        gfx->src.buf_v = ibuf->hw->addr_v;
        gfx->src.pitch_uv = ibuf->hw->stride_uv;
    }

    /* in stripe mode: */
    /* update crop source, destination window & address */
    if (ibuf->stripe_num > 0)
    {
        int stripe_in_height = gfx->src.height;   /* input stripe height */
        int stripe_out_height = gfx->dst.height;   /* output stripe height */
        int stripe_in_start = (ibuf->stripe_num - 1) * stripe_in_height;    /* input stripe y-position */
        int stripe_out_start = (ibuf->stripe_num - 1) * stripe_out_height;  /* output stripe y-position */

        /* set input 'top' */
        if ((elem->params.convert.crop.top > stripe_in_start) && (elem->params.convert.crop.top < stripe_in_start + stripe_in_height))
        {   /* set top in stripe coords */
            gfx->src.top = elem->params.convert.crop.top - stripe_in_start;
        }
        else if (elem->params.convert.crop.top > stripe_in_start + stripe_in_height)
        {   /* nothing to do, skip this stripe */
            return MPP_SUCCESS;
        }
        else
        {   /* ignore top for now */
            gfx->src.top = 0;
        }

        /* set input 'bottom' */
        if ((elem->params.convert.crop.bottom > stripe_in_start) && (elem->params.convert.crop.bottom < stripe_in_start + stripe_in_height))
        {   /* set top in stripe coords */
            gfx->src.bottom = elem->params.convert.crop.bottom - stripe_in_start;
        }
        else if (elem->params.convert.crop.bottom < stripe_in_start)
        {   /* nothing to do, skip this stripe */
            return MPP_SUCCESS;
        }
        else
        {   /* ignore top for now */
            gfx->src.bottom = stripe_in_height - 1;
        }

        /* set output 'top' */
        if ((elem->params.convert.out_window.top > stripe_out_start) && (elem->params.convert.out_window.top < stripe_out_start + stripe_out_height))
        {   /* set top in stripe coords */
            gfx->dst.top = elem->params.convert.out_window.top - stripe_out_start;
        }
        else if (elem->params.convert.out_window.top > stripe_out_start + stripe_out_height)
        {   /* nothing to do, skip this stripe */
            return MPP_SUCCESS;
        }
        else
        {   /* ignore top for now */
            gfx->dst.top = 0;
        }

        /* set output 'bottom' */
        int outbot = elem->params.convert.out_window.top + elem->params.convert.scale.height - 1;
        if ((outbot > stripe_out_start) && (outbot < stripe_out_start + stripe_out_height))
        {   /* set top in stripe coords */
            gfx->dst.bottom = outbot - stripe_out_start;
        }
        else if (outbot < stripe_out_start)
        {   /* nothing to do, skip this stripe */
            return MPP_SUCCESS;
        }
        else
        {   /* ignore top for now */
            gfx->dst.bottom = stripe_out_height - 1;
        }

        /* update destination address */
        if (!elem->params.convert.stripe_out)
        {
            /* blit from stripe src to full frame destination */
            /* need to point to stripe in destination buffer */
            int offset = (ibuf->stripe_num - 1) * obuf->hw->stride * stripe_out_height;
            gfx->dst.buf = obuf->hw->addr + offset;
        } else {
            /* blit src stripe to dest stripe */
            gfx->dst.buf = obuf->hw->addr;
            /* transmit stripe number for the destination buffer */
            obuf->stripe_num = ibuf->stripe_num;
        }
    } else {
        /* full frame mode */
        gfx->dst.buf = obuf->hw->addr;
    }
    gfx->dst.pitch = obuf->hw->stride;

    if (obuf->format == MPP_PIXEL_YUV420P)
    {
        gfx->dst.buf_u = obuf->hw->addr_u;
        gfx->dst.buf_v = obuf->hw->addr_v;
        gfx->dst.pitch_uv = obuf->hw->stride_uv;
    }

    gfx_rotate_config_t rot = { 
        .degree = elem->params.convert.angle, 
        .target = kGFXRotate_DSTSurface,
        .custom_angle = elem->params.convert.custom_angle
    };

    ret = gfx->ops->blit(gfx, &gfx->src, &gfx->dst, &rot, elem->params.convert.flip);
    if (ret != 0) {
        MPP_LOGE("Blit operation failed\n");
        return MPP_ERROR;
    }

    ret = gfx->ops->finish(gfx);
    if (ret != 0) {
        MPP_LOGE("Finish operation failed\n");
        return MPP_ERROR;
    }
    return MPP_SUCCESS;
}

static inline bool is_custom_rotation(mpp_rotate_degree_t angle, float custom_angle)
{
    return (angle == ROTATE_CUSTOM && custom_angle != 0.0f && custom_angle != 90.0f
                                   && custom_angle != 180.0f && custom_angle != 270.0f);
}

/* check and complete the given convert params entry.
 * params is modified in-place: default crop, scale and pixel_format are filled in
 * when the corresponding ops flags are not set.
 * elem is used only to obtain the previous element's buffer descriptor.
 */
static int check_convert_params(_elem_t *elem, mpp_element_params_t *params)
{
    int ret = MPP_SUCCESS;
    unsigned int input_width = 0, input_height = 0;

    /* get input parameters from previous element */
    buf_desc_t *prev_buf = get_in_buff_from_prev_elem(elem);
    if (prev_buf == NULL) {
        MPP_LOGE("No input buffer found from previous element\n");
        return MPP_ERROR;
    }

    /* check output buffer dimensions */
    if ((params->convert.out_buf.width == 0) ||
            (params->convert.out_buf.height == 0))
    {
        MPP_LOGE("Invalid output buffer parameter\n");
        return MPP_INVALID_PARAM;
    }

    if(!(params->convert.ops & MPP_CONVERT_CROP))
    {
        /* keep full image area */
        params->convert.crop.top = 0;
        params->convert.crop.left = 0;
        params->convert.crop.bottom = prev_buf->height - 1;
        params->convert.crop.right = prev_buf->width - 1;
    }

    /* check crop params */
    if ( (params->convert.crop.top > params->convert.crop.bottom)
            || (params->convert.crop.left > params->convert.crop.right)
            || (params->convert.crop.right > prev_buf->width)
            || (params->convert.crop.bottom > prev_buf->height) )
    {
        MPP_LOGE("Invalid crop parameter\n\r");
        MPP_LOGE("top = %d, left = %d, right = %d, bott = %d\n\r",
                params->convert.crop.top, params->convert.crop.left,
                params->convert.crop.right, params->convert.crop.bottom);
        return MPP_INVALID_PARAM;
    }

    /* compute the input area dimensions */
    input_width = params->convert.crop.right - params->convert.crop.left + 1;
    input_height = params->convert.crop.bottom - params->convert.crop.top + 1;

    /* complete convert params for hal */
    if(!(params->convert.ops & MPP_CONVERT_COLOR))
    {   /* keep source color format */
        params->convert.pixel_format = prev_buf->format;
    }

    /* In case of output window, top and left coordinates from struct out_window
     * should be defined by user.
     * Otherwise, it will be set to:
     * - top/left coordinates of the output buffer
     */
    if(!(params->convert.ops & MPP_CONVERT_OUT_WINDOW))
    {   /* keep source position */
        params->convert.out_window.top = 0;
        params->convert.out_window.left = 0;
    }

    /* In case of scaling, dimensions from struct scale
     * should be defined by user.
     * Otherwise, it will be set to:
     * - dims of input window
     */
    if(!(params->convert.ops & MPP_CONVERT_SCALE))
    {
        /* keep source dimensions */
        if(params->convert.ops & MPP_CONVERT_ROTATE)
        {
            /* rotate swaps dimensions */
            /* Check for custom rotation */
            if (is_custom_rotation(params->convert.angle, params->convert.custom_angle))
            {
                /* For custom angles, calculate bounding box */
                float angle_rad = params->convert.custom_angle * 3.14159265f / 180.0f;
                /* CID 54622912, CID 54622917: MISRA D4.11 - validate sinf/cosf argument is finite
                 * (not NaN or infinity) before passing to library functions */
                if (!isfinite(angle_rad))
                {
                    HAL_LOGE("Custom rotation angle is not finite, skipping\n");
                    return -1;
                }
                float cos_val = fabsf(cosf(angle_rad));
                float sin_val = fabsf(sinf(angle_rad));

                params->convert.scale.width = (unsigned int)((input_width * cos_val) + (input_height * sin_val));
                params->convert.scale.height = (unsigned int)((input_height * cos_val) + (input_width * sin_val));
            }
            else
            {
                /* Fixed angle rotation */
                switch(params->convert.angle)
                {
                case ROTATE_90:
                case ROTATE_270:
                    params->convert.scale.width = input_height;
                    params->convert.scale.height = input_width;
                    break;
                case ROTATE_CUSTOM:
                    if (params->convert.custom_angle == 0.0f || params->convert.custom_angle == 180.0f)
                    {
                        params->convert.scale.width = input_width;
                        params->convert.scale.height = input_height;
                    }
                    else
                    {
                        params->convert.scale.width = input_height;
                        params->convert.scale.height = input_width;
                    }
                    break;
                case ROTATE_0:
                case ROTATE_180:
                default:
                    params->convert.scale.width = input_width;
                    params->convert.scale.height = input_height;
                    break;
                }
            }
        } else {
            params->convert.scale.width = input_width;
            params->convert.scale.height = input_height;
        }
    }

    if(!(params->convert.ops & MPP_CONVERT_ROTATE))
    {   /* keep source orientation */
        params->convert.angle = ROTATE_0;
        params->convert.flip = FLIP_NONE;
    }

    /* check output window params */
    if ((params->convert.out_window.top + params->convert.scale.height > params->convert.out_buf.height)
            || (params->convert.out_window.left + params->convert.scale.width > params->convert.out_buf.width))
    {
        MPP_LOGE("Invalid output window parameter\n");
        MPP_LOGE("Window %dx%d at pos %d,%d cannot fit in %dx%d buffer\n",
            params->convert.scale.width, params->convert.scale.height,
            params->convert.out_window.top, params->convert.out_window.left,
            params->convert.out_buf.width, params->convert.out_buf.height);
        return MPP_INVALID_PARAM;
    }
    return ret;
}

/* element setup function */
unsigned int elem_convert_setup(_elem_t *elem)
{
    int ret = MPP_SUCCESS;
    gfx_dev_t *gfx = NULL;

    do {
        /* sanity checks */
        if (elem == NULL) {
            MPP_LOGE("invalid input buffer - elem (0x%x)\n", ret);
            ret = MPP_INVALID_PARAM;
            break;
        }
        if ((elem->type != MPP_TYPE_PROC) || (elem->proc_typ != MPP_ELEMENT_CONVERT))
        {
            MPP_LOGE("invalid element %s (expected element CONVERT)\n", elem_name(elem));
            ret = MPP_INVALID_PARAM;
            break;
        }
        /* setup the device */
        gfx = hal_malloc(sizeof(gfx_dev_t));
        if (!gfx)
        {
            MPP_LOGE("\nAllocation failed\n");
            ret = MPP_MALLOC_ERROR;
            break;
        }
        memset(gfx, 0, sizeof(gfx_dev_t));
        elem->dev.gfx = gfx;

        _mpp_t *mpp = elem->mpp;
        if (!mpp)
        {
            ret = MPP_INVALID_PARAM;
            break;
        }

        ret = check_convert_params(elem, &elem->params);
        if (ret != MPP_SUCCESS) {
            MPP_LOGE("invalid parameters for Image Convert\r\n");
            ret = MPP_INVALID_PARAM;
            break;
        }

        /* set operating mode */
        elem->io.inplace = false;
        /* input buffer points to previous element buffer */
        elem->io.nb_in_buf = 1;
        elem->io.in_buf[0] = get_in_buff_from_prev_elem(elem);
        if (elem->io.in_buf[0] == NULL) {
            MPP_LOGE("No input buffer found from previous element\n");
            return MPP_ERROR;
        }
        /* create output buffer parameters to be passed to next element */
        elem->io.nb_out_buf = 1;
        elem->io.out_buf[0] = hal_malloc(sizeof(buf_desc_t));
        if (elem->io.out_buf[0] == NULL)
        {
            MPP_LOGE("\nAllocation failed\n");
            ret = MPP_MALLOC_ERROR;
            break;
        }
        /* set buffer descriptor */
        memset(elem->io.out_buf[0], 0, sizeof(buf_desc_t));
        elem->io.out_buf[0]->format = elem->params.convert.pixel_format;
        elem->io.out_buf[0]->width = elem->params.convert.out_buf.width;
        elem->io.out_buf[0]->height = elem->params.convert.out_buf.height;

        /* init stripes */
        if (elem->params.convert.stripe_out)
            elem->io.out_buf[0]->stripe_num = 1;
        else
            elem->io.out_buf[0]->stripe_num = 0;

        if (   ((elem->io.in_buf[0]->stripe_num > 0) && !elem->params.convert.stripe_in)
            || ((elem->io.in_buf[0]->stripe_num == 0) && elem->params.convert.stripe_in) )
        {
            MPP_LOGE("\nStripe mode not consistent between Convert element and previous element.\n");
            ret = MPP_INVALID_PARAM;
            break;
        }

        if (((elem->io.in_buf[0]->stripe_num > 0) || (elem->io.out_buf[0]->stripe_num > 0))
            && (elem->params.convert.angle != ROTATE_0))
        {
            MPP_LOGE("\nStripe mode not supported with rotation.\n");
            ret = MPP_INVALID_PARAM;
            break;
        }

        if (elem->params.convert.stripe_in && (elem->io.in_buf[0]->stripe_num == 0))
        {
            MPP_LOGE("\nStripe mode not enabled in previous element.\n");
            ret = MPP_INVALID_PARAM;
            break;
        }

        gfx->callback  = mpp->params.evt_callback_f;
        gfx->cb_user_data = mpp->params.cb_userdata;
        gfx->mpp = (mpp_t) mpp;
        gfx->id = elem->params.convert.elem_id;

        /* setup HAL gfx structure */
        ret  = hal_gfx_setup(elem->params.convert.dev_name, gfx);
        if (ret != MPP_SUCCESS) {
            MPP_LOGE ("Setup HAL graphics fails with ret=%d\n", ret);
            break;
        }
        if (gfx->ops == NULL) {
            MPP_LOGE ("Setup HAL graphics fails: gfx->ops is NULL \n");
            ret = MPP_ERROR;
            break;
        }

        /* init HAL function */
        if (gfx->ops->init != NULL)
            gfx->ops->init(gfx, &elem->params);

        /* set source and destination in gfx */
        set_gfx_dev(gfx, elem);

        /* retrieve buffer requirements from HAL */
        ret = gfx->ops->get_buf_desc(gfx, &elem->io.in_buf[0]->hw_req_cons, &elem->io.out_buf[0]->hw_req_prod, &elem->io.mem_policy);
        if (ret != MPP_SUCCESS) break;

        /* assign element entry/function */
        elem->entry = convert_func;

    } while (false);

    if (ret != MPP_SUCCESS)
    {
        if (elem != NULL && elem->io.out_buf[0] != NULL)
            hal_free(elem->io.out_buf[0]);
        if (gfx != NULL)
            hal_free(gfx);
    }

    return ret;
}

/* multipass setup for convert element:
 * - validates that all entries in pass_params share the same output dimensions
 *   and pixel format as the first entry (buffer descriptors are fixed at setup time)
 * - then delegates to the regular elem_convert_setup using pass_params[0] as params
 */
unsigned int elem_convert_multipass_setup(_elem_t *elem)
{
    int ret = MPP_SUCCESS;

    if (elem == NULL || elem->pass_params == NULL || elem->n_pass_params == 0)
        return MPP_INVALID_PARAM;

    /* check output dimensions are identical across all pass entries */
    mpp_element_params_t *base = &elem->pass_params[0];
    for (uint32_t i = 1; i < elem->n_pass_params; i++) {
        mpp_element_params_t *p = &elem->pass_params[i];
        if (p->convert.out_buf.width  != base->convert.out_buf.width  ||
            p->convert.out_buf.height != base->convert.out_buf.height ||
            p->convert.pixel_format   != base->convert.pixel_format   ||
            p->convert.stripe_in      != base->convert.stripe_in      ||
            p->convert.stripe_out     != base->convert.stripe_out)
        {
            MPP_LOGE("pass entry %d has incompatible output dimensions or format\n", i);
            return MPP_INVALID_PARAM;
        }
    }

    /* validate and complete all pass entries (including entry 0) via
     * check_convert_params. check_convert_params fills in defaults (crop,
     * scale, pixel_format) in-place so that at runtime the injected
     * pass_params[i] are already fully specified. Entry 0 must also be
     * completed here because elem_convert_setup works on elem->params (a
     * copy), not on pass_params[0] directly. */
    for (uint32_t i = 0; i < elem->n_pass_params; i++) {
        ret = check_convert_params(elem, &elem->pass_params[i]);
        if (ret != MPP_SUCCESS) {
            MPP_LOGE("pass entry %d has invalid convert parameters\n", i);
            return ret;
        }
    }

    /* use the first (completed) entry as active params, then run regular setup */
    elem->params = elem->pass_params[0];
    ret = elem_convert_setup(elem);
    return ret;
}

unsigned int mpp_convert_update(_elem_t *elem, mpp_element_params_t *params)
{
    int ret = MPP_SUCCESS;
    do {
        /* sanity checks */
        if (params == NULL) {
            MPP_LOGE("Failed to perform conversion: params buffer is invalid\n");
            ret = MPP_INVALID_PARAM;
            break;
        }
        if (params->convert.stripe_in != elem->params.convert.stripe_in)
        {
            MPP_LOGE("Cannot update input stripe format\n");
            ret = MPP_INVALID_PARAM;
            break;
        }
        if (params->convert.stripe_out != elem->params.convert.stripe_out)
        {
            MPP_LOGE("Cannot update output stripe format\n");
            ret = MPP_INVALID_PARAM;
            break;
        }

        /* save new parameters in element context */
        memcpy(&elem->params, params, sizeof(mpp_element_params_t));

        ret = check_convert_params(elem, &elem->params);
        if (ret != MPP_SUCCESS) {
            MPP_LOGE("invalid parameters for Image Convert\r\n");
            ret = MPP_INVALID_PARAM;
            break;
        }

        /* do not change resolution in buffer descriptor */
        if (((elem->io.in_buf[0]->stripe_num > 0) || (elem->io.out_buf[0]->stripe_num > 0))
            && (elem->params.convert.angle != ROTATE_0))
        {   
            MPP_LOGE("\nStripe mode not supported with rotation.\n");
            ret = MPP_INVALID_PARAM;
            break;
        }

    } while (false);

    return ret;
}

/* multipass update for convert element:
 * validates stripe constraints and output dimensions consistency,
 * then completes all pass entries via check_convert_params and
 * writes back the completed params to params_array.
 * Finally updates the gfx structure for pass entry 0 (current active params).
 */
unsigned int mpp_convert_multipass_update(_elem_t *elem,
                                      mpp_element_params_t *params_array,
                                      uint32_t n_params)
{
    int ret = MPP_SUCCESS;

    if (params_array == NULL || n_params == 0)
        return MPP_INVALID_PARAM;

    /* validate fixed constraints across all entries */
    for (uint32_t i = 0; i < n_params; i++) {
        if (params_array[i].convert.stripe_in != elem->params.convert.stripe_in) {
            MPP_LOGE("pass entry %d: cannot update input stripe format\n", i);
            return MPP_INVALID_PARAM;
        }
        if (params_array[i].convert.stripe_out != elem->params.convert.stripe_out) {
            MPP_LOGE("pass entry %d: cannot update output stripe format\n", i);
            return MPP_INVALID_PARAM;
        }
        /* all pass entries must share the same output buffer dimensions and format */
        if (params_array[i].convert.out_buf.width  != params_array[0].convert.out_buf.width  ||
            params_array[i].convert.out_buf.height != params_array[0].convert.out_buf.height ||
            params_array[i].convert.pixel_format   != params_array[0].convert.pixel_format)
        {
            MPP_LOGE("pass entry %d has incompatible output dimensions or format\n", i);
            return MPP_INVALID_PARAM;
        }
    }

    /* complete and validate all entries via check_convert_params */
    for (uint32_t i = 0; i < n_params; i++) {
        ret = check_convert_params(elem, &params_array[i]);
        if (ret != MPP_SUCCESS) {
            MPP_LOGE("pass entry %d has invalid convert parameters\n", i);
            return ret;
        }

        /* check stripe + rotation constraint */
        if (((elem->io.in_buf[0]->stripe_num > 0) || (elem->io.out_buf[0]->stripe_num > 0))
            && (params_array[i].convert.angle != ROTATE_0))
        {
            MPP_LOGE("pass entry %d: stripe mode not supported with rotation\n", i);
            return MPP_INVALID_PARAM;
        }
    }

    /* set entry 0 as the current active params */
    elem->params = params_array[0];

    return ret;
}
