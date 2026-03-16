/*
 * Copyright 2022-2026 NXP
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
#include "mpp_double_buffer.h"
#include "mpp_api_types_internal.h"
#include "mpp_heap.h"
#include "hal_os.h"
#include "string.h"

#include "mpp_debug.h"
#include "hal.h"

#include <stdbool.h>

/* Declare type-safe wrappers */
MPP_DBUF_DECLARE_TYPED(lbl_rect, mpp_labeled_rect_t)
MPP_DBUF_DECLARE_TYPED(landmark, mpp_landmark_t)

/* Element context */
typedef struct {
    mpp_dbuf_t *rect_dbuf;
    mpp_dbuf_t *landmark_dbuf;
} lbl_rect_context_t;

/* element processing function */
static int label_rectangle_landmark_func (_elem_t *elem);

static uint32_t mpp_lbl_rectangle_validate(_elem_t *elem, mpp_element_params_t *params)
{
    volatile uint32_t ret = MPP_ERROR;

    do {
        /* sanity checks */
        if (elem == NULL) {
            ret = MPP_INVALID_PARAM;
            MPP_LOGE("ERR: invalid input buffer - elem (0x%x)\n",
                      (unsigned int)ret);
            break;
        }
        if ((elem->type != MPP_TYPE_PROC) || (elem->proc_typ != MPP_ELEMENT_LABELED_RECTANGLE))
        {
            ret = MPP_INVALID_ELEM;
            MPP_LOGE ("ERR: invalid element %s (expected element LABELED_RECTANGLE)\n", elem_name(elem));
            break;
        }

        /* params = NULL is used for setup intial validation */
        if (params == NULL)
        {
            params = &elem->params;

            /*  validations done only for setup */
            if (!elem->mpp) {
                ret = MPP_INVALID_PARAM;
                MPP_LOGE ("ERR: mpp is null\n");
                break;
            }

            if ((elem->params.labels.max_landmk == 0) && (elem->params.labels.max_rect == 0)) {
                ret = MPP_INVALID_PARAM;
                MPP_LOGE ("ERR: invalid elem params - max array size cannot be zero for both landmarks and rectangle \r\n");
                break;
            }
        }

        if (params->labels.max_rect > elem->params.labels.max_rect)
        {
            ret = MPP_INVALID_PARAM;
            MPP_LOGE ("ERR: invalid elem params - rectangle max count is bigger than setup %d < %d\n",
                      (int)elem->params.labels.max_rect,
                      (int)params->labels.max_rect);
            break;
        }
        if (params->labels.detected_rect > params->labels.max_rect)
        {
            ret = MPP_INVALID_PARAM;
            MPP_LOGE ("ERR: invalid elem params - rectangle detected count is bigger than max count %d < %d\n",
                      (int)params->labels.detected_rect,
                      (int)elem->params.labels.max_rect);
            break;
        }
        if (params->labels.max_landmk > elem->params.labels.max_landmk)
        {
            ret = MPP_INVALID_PARAM;
            MPP_LOGE ("ERR: invalid elem params - landmark max count is bigger than setup %d < %d\n",
                      (int)elem->params.labels.max_landmk,
                      (int)params->labels.max_landmk);
            break;
        }
        if (params->labels.detected_landmk > params->labels.max_landmk)
        {
            ret = MPP_INVALID_PARAM;
            MPP_LOGE ("ERR: invalid elem params - landmark detected count is bigger than max count %d < %d\n",
                      (int)params->labels.detected_landmk,
                      (int)params->labels.max_landmk);
            break;
        }

        ret = MPP_SUCCESS;
    } while (false);

    return ret;
}

/*
 * Setup function
 */
uint32_t elem_lbl_rct_setup(_elem_t *elem)
{
    volatile uint32_t ret = MPP_ERROR;
    mpp_dbuf_status_t status;
    lbl_rect_context_t *ctx = NULL;

    do {
        /* sanity checks */
        ret = mpp_lbl_rectangle_validate(elem, NULL);
        if (ret != MPP_SUCCESS)
            break;

        /* set operating mode */
        elem->io.inplace = true;
        /* input buffer points to previous element buffer */
        elem->io.nb_in_buf = 1;
        elem->io.in_buf[0] = get_in_buff_from_prev_elem(elem);
        if (elem->io.in_buf[0] == NULL) {
            MPP_LOGE("ERR: no input buffer found from previous element\n");
            return MPP_ERROR;
        }
        /* create output buffer parameters to be passed to next element */
        elem->io.nb_out_buf = 1;
        /* element process in-place: means input & output point to same buffer */
        elem->io.out_buf[0] = elem->io.in_buf[0];

        /* Allocate context */
        ctx = (lbl_rect_context_t *)hal_malloc(sizeof(lbl_rect_context_t));
        if (ctx == NULL) {
            ret = MPP_MALLOC_ERROR;
            MPP_LOGE ("ERR: malloc failed for context\n");
            break;
        }
        memset(ctx, 0, sizeof(lbl_rect_context_t));

        /* Create rectangle double buffer */
        if (elem->params.labels.max_rect > 0) {
            status = mpp_dbuf_lbl_rect_create(elem->params.labels.max_rect,
                                              &ctx->rect_dbuf);
            if (status != MPP_DBUF_OK) {
                ret = MPP_MALLOC_ERROR;
                MPP_LOGE ("ERR: failed to create rectangle double buffer\n");
                break;
            }

            /* Initialize with initial rectangles */
            if (elem->params.labels.detected_rect > 0) {
                mpp_labeled_rect_t *rects = NULL;
                uint32_t max_count = 0;

                mpp_dbuf_lbl_rect_producer_acquire(ctx->rect_dbuf, &rects, &max_count);
                memcpy(rects, elem->params.labels.rectangles,
                       sizeof(mpp_labeled_rect_t) * elem->params.labels.detected_rect);
                mpp_dbuf_lbl_rect_producer_release(ctx->rect_dbuf,
                                                   elem->params.labels.detected_rect);
            }
        }

        /* Create landmark double buffer */
        if (elem->params.labels.max_landmk > 0) {
            status = mpp_dbuf_landmark_create(elem->params.labels.max_landmk,
                                              &ctx->landmark_dbuf);
            if (status != MPP_DBUF_OK) {
                ret = MPP_MALLOC_ERROR;
                MPP_LOGE ("ERR: failed to create landmark double buffer\n");
                break;
            }

            /* Initialize with initial landmarks */
            if (elem->params.labels.detected_landmk > 0) {
                mpp_landmark_t *landmarks = NULL;
                uint32_t max_count = 0;

                mpp_dbuf_landmark_producer_acquire(ctx->landmark_dbuf,
                                                   &landmarks, &max_count);
                memcpy(landmarks, elem->params.labels.landmarks,
                       sizeof(mpp_landmark_t) * elem->params.labels.detected_landmk);
                mpp_dbuf_landmark_producer_release(ctx->landmark_dbuf,
                                                   elem->params.labels.detected_landmk);
            }
        }

        elem->priv = ctx;
        elem->entry = label_rectangle_landmark_func;

        ret = MPP_SUCCESS;
    } while (false);

    if (ret != MPP_SUCCESS) {
        if (ctx != NULL) {
            mpp_dbuf_lbl_rect_destroy(&ctx->rect_dbuf);
            mpp_dbuf_landmark_destroy(&ctx->landmark_dbuf);
            hal_free(ctx);
        }
    }

    return ret;
}

/*
 * Processing function (CONSUMER)
 */
static int label_rectangle_landmark_func(_elem_t *elem)
{
    lbl_rect_context_t *ctx = (lbl_rect_context_t *)elem->priv;
    int ret = MPP_SUCCESS;

    /* Draw rectangles */
    if (ctx->rect_dbuf != NULL) {
        mpp_labeled_rect_t *rects = NULL;
        uint32_t count = 0;

        mpp_dbuf_lbl_rect_consumer_acquire(ctx->rect_dbuf, &rects, &count);

        for (uint32_t i = 0; i < count && ret == MPP_SUCCESS; i++) {
            if (rects[i].clear == 0) {
                ret = hal_label_rectangle(
                        elem->io.in_buf[0]->hw->addr,
                        elem->io.in_buf[0]->width,
                        elem->io.in_buf[0]->height,
                        elem->io.in_buf[0]->format,
                        &rects[i],
                        elem->io.in_buf[0]->stripe_num,
                        MPP_STRIPE_NUM);
                if (ret != MPP_SUCCESS) {
                    HAL_LOGI("mpp_labeled_rectangle element num %x failed !\n", i);
                }
            }
        }

        mpp_dbuf_lbl_rect_consumer_release(ctx->rect_dbuf);
    }

    /* Draw landmarks */
    if (ctx->landmark_dbuf != NULL && ret == MPP_SUCCESS) {
        mpp_landmark_t *landmarks = NULL;
        uint32_t count = 0;

        mpp_dbuf_landmark_consumer_acquire(ctx->landmark_dbuf, &landmarks, &count);

        for (uint32_t i = 0; i < count && ret == MPP_SUCCESS; i++) {
            if (landmarks[i].clear == 0) {
                ret = hal_landmark(
                        elem->io.in_buf[0]->hw->addr,
                        elem->io.in_buf[0]->width,
                        elem->io.in_buf[0]->height,
                        elem->io.in_buf[0]->format,
                        &landmarks[i],
                        elem->io.in_buf[0]->stripe_num,
                        MPP_STRIPE_NUM);
                if (ret != MPP_SUCCESS) {
                    MPP_LOGE ("ERR: mpp_landmark element num %x failed !\n", i);
                }
            }
        }

        mpp_dbuf_landmark_consumer_release(ctx->landmark_dbuf);
    }

    if (ret != MPP_SUCCESS) {
        HAL_LOGI("%s: return error %d\n", __func__, ret);
    }

    return ret;
}

/*
 * Update function (PRODUCER)
 */
uint32_t mpp_lbl_rectangle_update(_elem_t *elem, mpp_element_params_t *params)
{
    volatile uint32_t ret = MPP_ERROR;
    lbl_rect_context_t *ctx;

    do {
        /* sanity checks */
        ret = mpp_lbl_rectangle_validate(elem, params);
        if (ret != MPP_SUCCESS)
            break;

        ctx = (lbl_rect_context_t *)elem->priv;

        /* Update rectangles */
        if (ctx->rect_dbuf != NULL && params->labels.detected_rect > 0) {
            mpp_labeled_rect_t *rects = NULL;
            uint32_t max_count = 0;

            mpp_dbuf_lbl_rect_producer_acquire(ctx->rect_dbuf, &rects, &max_count);

            if (params->labels.detected_rect > max_count) {
                mpp_dbuf_lbl_rect_producer_release(ctx->rect_dbuf, 0);
                ret = MPP_INVALID_PARAM;
                MPP_LOGE("ERR: rectangle count exceeds buffer capacity\n");
                break;
            }

            memcpy(rects, params->labels.rectangles,
                   sizeof(mpp_labeled_rect_t) * params->labels.detected_rect);

            mpp_dbuf_lbl_rect_producer_release(ctx->rect_dbuf,
                                               params->labels.detected_rect);
        }

        /* Update landmarks */
        if (ctx->landmark_dbuf != NULL && params->labels.detected_landmk > 0) {
            mpp_landmark_t *landmarks = NULL;
            uint32_t max_count = 0;

            mpp_dbuf_landmark_producer_acquire(ctx->landmark_dbuf,
                                               &landmarks, &max_count);

            if (params->labels.detected_landmk > max_count) {
                mpp_dbuf_landmark_producer_release(ctx->landmark_dbuf, 0);
                ret = MPP_INVALID_PARAM;
                MPP_LOGE("ERR: landmark count exceeds buffer capacity\n");
                break;
            }

            memcpy(landmarks, params->labels.landmarks,
                   sizeof(mpp_landmark_t) * params->labels.detected_landmk);

            mpp_dbuf_landmark_producer_release(ctx->landmark_dbuf,
                                               params->labels.detected_landmk);
        }

        /* update rectangles & landmarks count */
        elem->params.labels.detected_rect = params->labels.detected_rect;
        elem->params.labels.detected_landmk = params->labels.detected_landmk;

        ret = MPP_SUCCESS;
    } while (false);

    return ret;
}

/*
 * Cleanup function
 */
void elem_lbl_rct_close(_elem_t *elem)
{
    lbl_rect_context_t *ctx = (lbl_rect_context_t *)elem->priv;

    if (ctx != NULL) {
        mpp_dbuf_lbl_rect_destroy(&ctx->rect_dbuf);
        mpp_dbuf_landmark_destroy(&ctx->landmark_dbuf);
        hal_free(ctx);
        elem->priv = NULL;
    }
}
