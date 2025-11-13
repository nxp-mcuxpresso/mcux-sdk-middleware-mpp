/*
 * Copyright 2025 NXP.
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

#include "mpp_api.h"
#include "mpp_api_types_internal.h"
#include "mpp_heap.h"
#include "hal_os.h"
#include "string.h"

#include "mpp_debug.h"
#include "hal_graphics_dev.h"
#include "hal.h"
#include "hal_utils.h"

static int mpp_compose_create_surface(gfx_surface_t *surface, 
                                    int width, int height, int stride,
                                    mpp_pixel_format_t format,
                                    void *buffer,
                                    const mpp_area_t *area) {
    if (!surface || !buffer) {
        return MPP_INVALID_PARAM;
    }

    surface->width = width;
    surface->height = height;
    surface->pitch = stride;
    surface->format = format;
    surface->buf = buffer;
    surface->swapByte = 0;
    surface->lock = NULL;

    if (area) {
        surface->left = area->left;
        surface->top = area->top;
        surface->right = area->right;
        surface->bottom = area->bottom;
    } else {
        surface->left = 0;
        surface->top = 0;
        surface->right = width  - 1;
        surface->bottom = height - 1;
    }

    return MPP_SUCCESS;
}

/* element processing function */
static int compose_func(_elem_t *elem)
{
    int ret = MPP_SUCCESS;
    gfx_surface_t input_surface, output_surface;
    gfx_surface_t image_surface;
    bool can_compose = (elem->dev.gfx->ops && elem->dev.gfx->ops->compose);
    bool can_blit = (elem->dev.gfx->ops && elem->dev.gfx->ops->blit);
    
    if (!elem || !elem->dev.gfx) {
        MPP_LOGE("Compose: Invalid element or graphics device\r\n");
        return MPP_INVALID_PARAM;
    }

    const mpp_element_params_t *params = &elem->params;

    /* Create input surface */
    ret = mpp_compose_create_surface(&input_surface, 
                                   elem->io.in_buf[0]->width,
                                   elem->io.in_buf[0]->height,
                                   elem->io.in_buf[0]->hw->stride,
                                   elem->io.in_buf[0]->format,
                                   elem->io.in_buf[0]->hw->addr,
                                   NULL);   /* no crop area for input surface */
    if (ret != MPP_SUCCESS) {
        MPP_LOGE("Compose: Failed to create input surface\r\n");
        return ret;
    }

    /* Create output surface */
    ret = mpp_compose_create_surface(&output_surface,
                                   elem->io.out_buf[0]->width,
                                   elem->io.out_buf[0]->height,
                                   elem->io.out_buf[0]->hw->stride,
                                   elem->io.out_buf[0]->format,
                                   elem->io.out_buf[0]->hw->addr,
                                   &params->compose.input_area);
    if (ret != MPP_SUCCESS) {
        MPP_LOGE("Compose: Failed to create output surface\r\n");
        return ret;
    }

    /* configure rotation */
    gfx_rotate_config_t rot = { .degree = params->compose.out_angle, .target = kGFXRotate_DSTSurface};

    /* Copy input to output as base layer using blit operation */
    if (can_blit) {
        ret = elem->dev.gfx->ops->blit(elem->dev.gfx, 
                                     &input_surface, &output_surface, 
                                     &rot, params->compose.out_flip);
        if (ret != 0) {
            MPP_LOGE("Compose: Failed to blit input image\r\n");
            return MPP_ERROR;
        }
    } else {
        MPP_LOGE("Compose: Graphics device blit operation not available\r\n");
        return MPP_ERROR;
    }

    /* Compose all images in the array */
    for (int i = 0; i < params->compose.nb_images; i++) {
        const mpp_img_compose_param_t *img = &params->compose.image_list[i];
        
        /* Skip if no buffer provided */
        if (!img->buffer) {
            MPP_LOGD("Compose: Skipping image %d - no buffer provided\r\n", i);
            continue;
        }

        /* Validate image dimensions */
        if (img->width <= 0 || img->height <= 0) {
            MPP_LOGE("Compose: Invalid dimensions for image %d\r\n", i);
            continue;
        }

        /* Create surface for this image */
        ret = mpp_compose_create_surface(&image_surface, 
                                       img->width,
                                       img->height,
                                       img->width * get_bitpp(img->format) / 8,
                                       img->format,
                                       img->buffer,
                                       NULL);
        if (ret != MPP_SUCCESS) {
            MPP_LOGE("Compose: Failed to create surface for image %d\r\n", i);
            return ret;
        }

        /* Set image position on output surface */
        output_surface.left = img->dest_area.left;
        output_surface.top = img->dest_area.top;
        output_surface.right = img->dest_area.right;
        output_surface.bottom = img->dest_area.bottom;

        /* Validate area bounds */
        if (output_surface.left < 0 || output_surface.top < 0 ||
            output_surface.right >= elem->io.out_buf[0]->width ||
            output_surface.bottom >= elem->io.out_buf[0]->height ||
            output_surface.left >= output_surface.right ||
            output_surface.top >= output_surface.bottom) {
            MPP_LOGE("Compose: Invalid area for image %d\r\n", i);
            continue;
        }

        if (can_compose) {
            ret = elem->dev.gfx->ops->compose(elem->dev.gfx,
                                            &output_surface, &image_surface, 
                                            &output_surface, &rot, FLIP_NONE);
            if (ret != 0) {
                MPP_LOGE("Compose: Failed to compose image %d\r\n", i);
                return MPP_ERROR;
            }
        } else if (can_blit) {
            /* Fallback to blit if compose not available */
            ret = elem->dev.gfx->ops->blit(elem->dev.gfx,
                                         &image_surface, &output_surface,
                                         &rot, FLIP_NONE);
            if (ret != 0) {
                MPP_LOGE("Compose: Failed to blit image %d\r\n", i);
                return MPP_ERROR;
            }
        }
    }

    ret = elem->dev.gfx->ops->finish(elem->dev.gfx);
    if (ret != 0) {
        MPP_LOGE("Finish operation failed\n");
        return MPP_ERROR;
    }

    return MPP_SUCCESS;
}

/* element setup function */
unsigned int elem_img_compose_setup(_elem_t *elem)
{
    int ret = MPP_SUCCESS;
    gfx_dev_t *gfx = NULL;

    do {
        /* sanity checks */
        if (elem == NULL)
        {
            MPP_LOGE("invalid input buffer - elem (0x%x)\n", elem);
            ret = MPP_INVALID_PARAM;
            break;
        }
        if ((elem->type != MPP_TYPE_PROC) || (elem->proc_typ != MPP_ELEMENT_IMG_COMPOSE))
        {
            MPP_LOGE("invalid element %s (expected element COMPOSE)\n", elem_name(elem));
            ret = MPP_INVALID_PARAM;
            break;
        }

        _mpp_t *mpp = elem->mpp;
        if (!mpp)
        {
            ret = MPP_INVALID_PARAM;
            break;
        }

        /* validate compose parameters */
        const mpp_element_params_t *params = &elem->params;
        
        /* Validate number of images */
        if (params->compose.nb_images < 0) {
            MPP_LOGE("Compose: Invalid number of images (%d)\r\n", params->compose.nb_images);
            ret = MPP_INVALID_PARAM;
            break;
        }

        /* Validate each image parameters */
        for (int i = 0; i < params->compose.nb_images; i++) {
            const mpp_img_compose_param_t *img = &params->compose.image_list[i];
            
            if (img->buffer && 
                (img->width <= 0 || img->height <= 0)) {
                MPP_LOGE("Compose: Invalid dimensions for image %d\r\n", i);
                ret = MPP_INVALID_PARAM;
                break;
            }
        }
        
        if (ret != MPP_SUCCESS) break;

        /* setup the graphics device */
        gfx = hal_malloc(sizeof(gfx_dev_t));
        if (!gfx)
        {
            MPP_LOGE("\nImage Compose: element allocation failed\n");
            ret = MPP_MALLOC_ERROR;
            break;
        }
        memset(gfx, 0, sizeof(gfx_dev_t));
        elem->dev.gfx = gfx;

        /* set operating mode */
        elem->io.inplace = false;
        /* input buffer points to previous element buffer */
        elem->io.nb_in_buf = 1;
        elem->io.in_buf[0] = elem->prev->io.out_buf[0];
        /* create output buffer parameters to be passed to next element */
        elem->io.nb_out_buf = 1;
        elem->io.out_buf[0] = hal_malloc(sizeof(buf_desc_t));
        if (elem->io.out_buf[0] == NULL)
        {
            MPP_LOGE("\nImage Compose: buffer descriptors allocation failed\n");
            ret = MPP_MALLOC_ERROR;
            break;
        }
        /* set buffer descriptor - output same as input */
        memset(elem->io.out_buf[0], 0, sizeof(buf_desc_t));
        elem->io.out_buf[0]->format = params->compose.out_format;
        elem->io.out_buf[0]->width  = params->compose.out_width;
        elem->io.out_buf[0]->height = params->compose.out_height;
        /* init stripes: none */
        elem->io.out_buf[0]->stripe_num = 0;

        gfx->callback  = mpp->params.evt_callback_f;
        gfx->user_data = mpp->params.cb_userdata;

        /* Register graphics device with GPU operations (fallback) */
        ret = HAL_GfxDev_GPU_Register(gfx);
        if (ret != 0) {
            MPP_LOGE("Compose: Failed to register GPU graphics device, trying CPU\n");
            /* Try CPU registration as fallback */
            ret = HAL_GfxDev_CPU_Register(gfx);
            if (ret != 0) {
                MPP_LOGE("Compose: Failed to register graphics device\n");
                break;
            }
        }

        if (gfx->ops == NULL)
        {
            MPP_LOGE ("Setup HAL graphics compose fails: gfx->ops is NULL \n");
            ret = MPP_ERROR;
            break;
        }

        /* init HAL function */
        if (gfx->ops->init != NULL)
            gfx->ops->init(gfx, &elem->params);
        else
        {
            MPP_LOGE ("Setup HAL graphics compose fails: gfx->ops->init is NULL \n");
            ret = MPP_ERROR;
            break;
        }

        /* retrieve buffer requirements from HAL */
        ret = gfx->ops->get_buf_desc(gfx, 
                                   &elem->io.in_buf[0]->hw_req_cons, 
                                   &elem->io.out_buf[0]->hw_req_prod, 
                                   &elem->io.mem_policy);
        if (ret != MPP_SUCCESS) break;

        /* assign element entry/function */
        elem->entry = compose_func;

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

uint32_t mpp_compose_update(_elem_t *elem, mpp_element_params_t *params)
{
    int ret = MPP_SUCCESS;

    do {
        /* sanity checks */
        if (elem == NULL) {
            MPP_LOGE("invalid input buffer - elem (0x%x)\n", elem);
            ret = MPP_INVALID_PARAM;
            break;
        }
        if ((elem->type != MPP_TYPE_PROC) || (elem->proc_typ != MPP_ELEMENT_IMG_COMPOSE)) {
            MPP_LOGE("invalid element %s (expected element COMPOSE)\n", elem_name(elem));
            ret = MPP_INVALID_PARAM;
            break;
        }
        if (params == NULL) {
            MPP_LOGE("invalid input buffer - params (0x%x)\n", params);
            ret = MPP_INVALID_PARAM;
            break;
        }

        /* check if user is trying to update non-dynamic parameters */
        if (params->compose.out_format != elem->params.compose.out_format) {
            MPP_LOGE("Compose: Cannot update output format dynamically\r\n");
            ret = MPP_INVALID_PARAM;
            break;
        }
        if (params->compose.out_width != elem->params.compose.out_width) {
            MPP_LOGE("Compose: Cannot update output width dynamically\r\n");
            ret = MPP_INVALID_PARAM;
            break;
        }
        if (params->compose.out_height != elem->params.compose.out_height) {
            MPP_LOGE("Compose: Cannot update output height dynamically\r\n");
            ret = MPP_INVALID_PARAM;
            break;
        }

        /* validate compose parameters */
        /* Validate number of images */
        if (params->compose.nb_images < 0) {
            MPP_LOGE("Compose: Invalid number of images (%d)\r\n", params->compose.nb_images);
            ret = MPP_INVALID_PARAM;
            break;
        }

        /* Validate each image parameters */
        for (int i = 0; i < params->compose.nb_images; i++) {
            const mpp_img_compose_param_t *img = &params->compose.image_list[i];
            
            if (img->buffer && 
                (img->width <= 0 || img->height <= 0)) {
                MPP_LOGE("Compose: Invalid dimensions for image %d\r\n", i);
                ret = MPP_INVALID_PARAM;
                break;
            }
        }
        
        if (ret != MPP_SUCCESS) break;

        /* update element parameters */
        elem->params.compose.nb_images = params->compose.nb_images;
        
        /* Copy the image list - Note: this assumes the image_list array is properly allocated */
        for (int i = 0; i < params->compose.nb_images; i++) {
            elem->params.compose.image_list[i] = params->compose.image_list[i];
        }
        
        elem->params.compose.input_area = params->compose.input_area;
        elem->params.compose.out_angle = params->compose.out_angle;
        elem->params.compose.out_flip = params->compose.out_flip;

    } while (false);

    return ret;
}
