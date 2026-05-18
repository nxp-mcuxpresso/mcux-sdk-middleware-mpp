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

#include "hal_vdec_dev.h"
#include "hal_utils.h"

/* element process function */
static int elem_video_decode_process(_elem_t *elem)
{
    vdec_h264_dev_t *vdec_h264_dev = elem->dev.vdec_h264;
    buf_desc_t *in_buf = elem->io.in_buf[0];
    buf_desc_t *out_buf = elem->io.out_buf[0];

    MPP_LOGD("++elem_video_decode_process\r\n");

    if (vdec_h264_dev == NULL || vdec_h264_dev->ops == NULL || vdec_h264_dev->ops->decode == NULL) {
        MPP_LOGE("Video decoder not properly initialized\r\n");
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
    vdec_h264_frame_info_t frame_info = {0};
    frame_info.src_data = in_buf->hw->addr;
    frame_info.src_size = in_buf->compressed_size;

    MPP_LOGD("Decoding frame: src=%p, size=%d\r\n", 
             frame_info.src_data, frame_info.src_size);

    hal_vdec_status_t decode_ret = vdec_h264_dev->ops->decode(vdec_h264_dev, &frame_info);

    if (decode_ret == MPP_kStatus_HAL_VDecSuccess) {
        /* Decode successful - check if we have output */
        out_buf->status = MPP_BUFFER_READY;
        out_buf->frame_id = in_buf->frame_id;
        MPP_LOGD("Decode successful, frame_id=%d\r\n", out_buf->frame_id);
        if (frame_info.width && frame_info.height) {
            out_buf->hw->addr = frame_info.dst_y;
            out_buf->hw->addr_u = frame_info.dst_u;
            out_buf->hw->addr_v = frame_info.dst_v;
            out_buf->width = frame_info.width;
            out_buf->height = frame_info.height;
            out_buf->hw->stride = frame_info.y_stride;
            out_buf->hw->stride_uv = frame_info.uv_stride;
            MPP_LOGD("Frame dimensions: width=%d, height=%d, Y stride=%d, UV stride=%d\r\n", 
                     frame_info.width, frame_info.height, frame_info.y_stride, frame_info.uv_stride);
        }
    } else if (decode_ret == MPP_kStatus_HAL_VDecSkipped) {
        /* Decode skipped so far */
        out_buf->status = MPP_BUFFER_WRITTING;
    } else {
        MPP_LOGE("Decode failed\r\n");
        out_buf->status = MPP_BUFFER_EMPTY;
        return MPP_ERROR;
    }

    MPP_LOGD("--elem_video_decode_process\r\n");

    return MPP_SUCCESS;
}

/* element setup function */
unsigned int elem_video_decode_setup(_elem_t *elem)
{
    MPP_LOGD("++elem_video_decode_setup\r\n");
    int ret = MPP_SUCCESS;
    vdec_h264_dev_t *vdec_h264_dev = NULL;
    buf_desc_t *prev_buf = NULL;

    do {
        /* Sanity checks */
        if (elem == NULL)
        {
            MPP_LOGE("Input elem pointer is NULL\r\n");
            ret = MPP_INVALID_PARAM;
            break;
        }
        if ((elem->type != MPP_TYPE_PROC) || (elem->proc_typ != MPP_ELEMENT_VIDEO_DECODE))
        {
            MPP_LOGE("invalid element %s (expected element VIDEO_DECODE)\r\n", elem_name(elem));
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

        vdec_h264_dev = hal_malloc(sizeof(vdec_h264_dev_t));
        if (!vdec_h264_dev)
        {
            MPP_LOGE("Failed to allocate video decoder device\r\n");
            ret = MPP_MALLOC_ERROR;
            break;
        }
        memset(vdec_h264_dev, 0, sizeof(vdec_h264_dev_t));
        elem->dev.vdec_h264 = vdec_h264_dev;

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
            MPP_LOGE("Video Decode: buffer descriptors allocation failed\r\n");
            ret = MPP_MALLOC_ERROR;
            break;
        }
        /* set buffer descriptor */
        memset(elem->io.out_buf[0], 0, sizeof(buf_desc_t));

        elem->io.out_buf[0]->format = elem->params.decode.out_format;
        elem->io.out_buf[0]->width = elem->params.decode.width;
        elem->io.out_buf[0]->height = elem->params.decode.height;

        /* init stripes: none */
        elem->io.out_buf[0]->stripe_num = 0;

        /* Set callback for MPP events */
        vdec_h264_dev->callback = mpp->params.evt_callback_f;
        /* Note: user_data will be used by HAL for decoder context after init */

        /* CPU registration for decode device */

        hal_vdec_status_t hal_ret = HAL_VdecDev_H264_CPU_Register(vdec_h264_dev);
        if (hal_ret != MPP_kStatus_HAL_VDecSuccess)
        {
            MPP_LOGE("Video Decode: Failed to register device\r\n");
            ret = MPP_ERROR;
            break;
        }

        if (vdec_h264_dev->ops == NULL)
        {
            MPP_LOGE("Setup HAL Video Decode fails: vdec_h264_dev->ops is NULL\r\n");
            ret = MPP_ERROR;
            break;
        }

        /* init HAL function - this will set user_data to decoder context */
        if (vdec_h264_dev->ops->init != NULL)
            hal_ret = vdec_h264_dev->ops->init(vdec_h264_dev, &elem->params);
        else
        {

            MPP_LOGE("Setup HAL Video Decode fails: vdec_h264_dev->ops->init is NULL\r\n");
            ret = MPP_ERROR;
            break;
        }

        if (hal_ret != MPP_kStatus_HAL_VDecSuccess) {
            MPP_LOGE("Video Decode: HAL init failed\r\n");
            ret = MPP_ERROR;
            break;
        }

        /* retrieve buffer requirements from HAL */
        hal_ret = vdec_h264_dev->ops->get_buf_desc(vdec_h264_dev,
                                          &elem->io.in_buf[0]->hw_req_cons, 
                                          &elem->io.out_buf[0]->hw_req_prod, 
                                          &elem->io.mem_policy);

        if (hal_ret != MPP_kStatus_HAL_VDecSuccess) {
            MPP_LOGE("Video Decode: get_buf_desc failed\r\n");
            ret = MPP_ERROR;
            break;
        }

        /* Set the process function */
        elem->entry = elem_video_decode_process;

    } while (false);

    if (ret != MPP_SUCCESS && vdec_h264_dev != NULL) {
        hal_free(vdec_h264_dev);
        elem->dev.vdec_h264 = NULL;
    }

    MPP_LOGD("--elem_video_decode_setup (ret=%d)\r\n", ret);
    return ret;
}