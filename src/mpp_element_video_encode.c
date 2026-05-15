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

#include <string.h>
#include "mpp_api.h"
#include "mpp_api_types_internal.h"
#include "mpp_debug.h"

#include "hal_venc_dev.h"
#include "hal_utils.h"

/* element process function */
static int elem_video_encode_process(_elem_t *elem)
{
    venc_h264_dev_t *venc_h264_dev = elem->dev.venc_h264;
    buf_desc_t *in_buf = elem->io.in_buf[0];
    buf_desc_t *out_buf = elem->io.out_buf[0];

    MPP_LOGD("++elem_video_encode_process\r\n");

    if (venc_h264_dev == NULL || venc_h264_dev->ops == NULL || venc_h264_dev->ops->encode == NULL) {
        MPP_LOGE("Video encoder not properly initialized\r\n");
        return MPP_ERROR;
    }

    if (in_buf == NULL || in_buf->hw == NULL || in_buf->hw->addr == NULL) {
        MPP_LOGE("Invalid input buffer\r\n");
        return MPP_ERROR;
    }

    if (out_buf == NULL || out_buf->hw == NULL) {
        MPP_LOGE("Invalid output buffer\r\n");
        return MPP_ERROR;
    }

    /* Check if input buffer has data */
    if (in_buf->status != MPP_BUFFER_READING) {
        MPP_LOGD("Input buffer not in reading state (status: %d)\r\n", in_buf->status);
        return MPP_ERROR;
    }

    /* Mark output buffer as being written */
    out_buf->status = MPP_BUFFER_WRITTING;

    /* Prepare frame info structure */
    venc_h264_frame_info_t frame_info = {0};
    frame_info.format = in_buf->format;
    frame_info.src_y = in_buf->hw->addr;
    frame_info.src_u = in_buf->hw->addr_u;
    frame_info.src_v = in_buf->hw->addr_v;
    frame_info.y_stride = in_buf->hw->stride;
    frame_info.uv_stride = in_buf->hw->stride_uv;
    frame_info.width = in_buf->width;
    frame_info.height = in_buf->height;
    frame_info.intraframe = elem->params.encode.intraframe;
    hal_venc_status_t decode_ret = venc_h264_dev->ops->encode(venc_h264_dev, &frame_info);

    if (decode_ret == MPP_kStatus_HAL_VEncSuccess) {
        if ((frame_info.dst == NULL) || (frame_info.dst_size == 0)) {
            MPP_LOGE("Invalid encoded output\r\n");
            out_buf->status = MPP_BUFFER_EMPTY;
            return MPP_ERROR;
        }

        /* Encode successful */
        out_buf->status = MPP_BUFFER_READY;
        out_buf->frame_id = in_buf->frame_id;

        out_buf->hw->addr = frame_info.dst;
        out_buf->compressed_size =  frame_info.dst_size;

    }
    else {
        MPP_LOGE("Encoding failed\r\n");
        out_buf->status = MPP_BUFFER_EMPTY;
        return MPP_ERROR;
    }

    MPP_LOGD("--elem_video_encode_process\r\n");

    return MPP_SUCCESS;
}

/* element setup function */
unsigned int elem_video_encode_setup(_elem_t *elem)
{
    MPP_LOGD("++elem_video_encode_setup\r\n");

    int ret = MPP_SUCCESS;
    venc_h264_dev_t *venc_h264_dev = NULL;
    buf_desc_t *prev_buf = NULL;

    do {
        /* Sanity checks */
        if (elem == NULL)
        {
            MPP_LOGE("Input elem pointer is NULL\r\n");
            ret = MPP_INVALID_PARAM;
            break;
        }

        if ((elem->type != MPP_TYPE_PROC) || (elem->proc_typ != MPP_ELEMENT_VIDEO_ENCODE))
        {
            MPP_LOGE("invalid element %s (expected element VIDEO_ENCODE)\r\n", elem_name(elem));
            ret = MPP_INVALID_PARAM;
            break;
        }

        _mpp_t *mpp = elem->mpp;
        if (!mpp)
        {
            MPP_LOGE("mpp is null\r\n");
            ret = MPP_INVALID_PARAM;
            break;
        }

        venc_h264_dev = hal_malloc(sizeof(venc_h264_dev_t));
        if (!venc_h264_dev)
        {
            MPP_LOGE("Failed to allocate video encoder device\r\n");
            ret = MPP_MALLOC_ERROR;
            break;
        }
        memset(venc_h264_dev, 0, sizeof(venc_h264_dev_t));
        elem->dev.venc_h264 = venc_h264_dev;

        prev_buf = get_in_buff_from_prev_elem(elem);
        if (prev_buf == NULL) {
            MPP_LOGE("No input buffer found from previous element\r\n");
            ret = MPP_ERROR;
            break;
        }

        /* set operating mode */
        elem->io.inplace = false;
        /* input buffer points to previous element buffer */
        elem->io.nb_in_buf = 1;
        elem->io.in_buf[0] = prev_buf;
        /* create output buffer parameters to be passed to next element */
        elem->io.nb_out_buf = 1;
        elem->io.out_buf[0] = hal_malloc(sizeof(buf_desc_t));
        if (elem->io.out_buf[0] == NULL)
        {
            MPP_LOGE("Video Encode: buffer descriptors allocation failed\r\n");
            ret = MPP_MALLOC_ERROR;
            break;
        }
        /* set buffer descriptor */
        memset(elem->io.out_buf[0], 0, sizeof(buf_desc_t));

        /* init stripes: none */
        elem->io.out_buf[0]->stripe_num = 0;

        /* Set callback for MPP events */
        venc_h264_dev->callback = mpp->params.evt_callback_f;
        /* Note: user_data will be used by HAL for encoder context after init */

        /* CPU registration for decode device */
        hal_venc_status_t hal_ret = HAL_VencDev_H264_CPU_Register(venc_h264_dev);
        if (hal_ret != MPP_kStatus_HAL_VEncSuccess)
        {
            MPP_LOGE("Video Encode: Failed to register device\r\n");
            ret = MPP_ERROR;
            break;
        }

        if (venc_h264_dev->ops == NULL)
        {
            MPP_LOGE("Setup HAL Video Encode fails: venc_h264_dev->ops is NULL\r\n");
            ret = MPP_ERROR;
            break;
        }

        /* init HAL function - this will set user_data to decoder context */
        if (venc_h264_dev->ops->init != NULL)
        {
            hal_ret = venc_h264_dev->ops->init(venc_h264_dev, &elem->params);
        }
        else
        {
            MPP_LOGE("Setup HAL Video Encode fails: venc_h264_dev->ops->init is NULL\r\n");
            ret = MPP_ERROR;
            break;
        }

        if (hal_ret != MPP_kStatus_HAL_VEncSuccess) {
            MPP_LOGE("Video Encode: HAL init failed\r\n");
            ret = MPP_ERROR;
            break;
        }

        /* retrieve buffer requirements from HAL */
        hal_ret = venc_h264_dev->ops->get_buf_desc(venc_h264_dev,
                                          &elem->io.in_buf[0]->hw_req_cons, 
                                          &elem->io.out_buf[0]->hw_req_prod, 
                                          &elem->io.mem_policy);

        if (hal_ret != MPP_kStatus_HAL_VEncSuccess) {
            MPP_LOGE("Video Encode: get_buf_desc failed\r\n");
            ret = MPP_ERROR;
            break;
        }

        /* Set the process function */
        elem->entry = elem_video_encode_process;

    } while (false);

    if (ret != MPP_SUCCESS && venc_h264_dev != NULL) {
        if (venc_h264_dev->ops && venc_h264_dev->ops->deinit)
            venc_h264_dev->ops->deinit(venc_h264_dev);
        hal_free(venc_h264_dev);
        elem->dev.venc_h264 = NULL;
    }

    MPP_LOGD("--elem_video_encode_setup (ret=%d)\r\n", ret);
    return ret;
}