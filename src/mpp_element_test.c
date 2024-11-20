/*
 * Copyright 2020-2024 NXP.
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

#ifdef EMULATOR

#include "mpp_api.h"
#include "mpp_api_types_internal.h"
#include "mpp_heap.h"
#include "hal_os.h"
#include "string.h"
#include "hal_utils.h"
#include "mpp_debug.h"

int cat_frames(uint8_t *frame1, uint8_t *frame2,
                int width, int height, mpp_pixel_format_t format,
                uint8_t *out_frame);


/* element processing */
static int test_func(_elem_t *elem)
{
    cat_frames(elem->io.in_buf[0]->hw->addr, elem->io.in_buf[1]->hw->addr,
            elem->io.in_buf[0]->width, elem->io.in_buf[0]->height,
            elem->io.in_buf[0]->format, elem->io.out_buf[0]->hw->addr);
    return MPP_SUCCESS;
}

/* element setup function */
unsigned int elem_test_setup(_elem_t *elem)
{
    _mpp_t *mpp1 = elem->io_mpp[0];
    _mpp_t *mpp2 = elem->io_mpp[1];

    if (!mpp1 || !mpp2)
        return MPP_INVALID_PARAM;
    else {
        /* check that buffer parms from both input mpps are the same */
        if ( (mpp1->buf_params->format != mpp2->buf_params->format)
            || (mpp1->buf_params->width != mpp2->buf_params->width)
            || (mpp1->buf_params->height != mpp2->buf_params->height) )
            return MPP_ERROR;
    }

   /* assign element entry/function */
    elem->entry = test_func;

    /* set output buffer parameters */
    elem->io.mem_policy = HAL_MEM_ALLOC_NONE;
    elem->io.inplace = false;
    elem->io.nb_in_buf = 2;
    elem->io.nb_out_buf = 1;
    /* allocate output buffer descriptor */
    elem->io.out_buf[0] = hal_malloc(sizeof(buf_desc_t));
    if (elem->io.out_buf[0] == NULL)
    {
        MPP_LOGE("\nAllocation failed\n");
        return MPP_MALLOC_ERROR;
    }
    /* set buffer descriptor */
    memset(elem->io.out_buf[0], 0, sizeof(buf_desc_t));
    elem->io.out_buf[0]->format = elem->params.test.format;
    elem->io.out_buf[0]->width = elem->params.test.width;
    elem->io.out_buf[0]->height = elem->params.test.height;

    /* save input parameters for next element */
    _mpp_t *mpp = elem->io_mpp[2];

    return MPP_SUCCESS;
}

#endif /* EMULATOR */
