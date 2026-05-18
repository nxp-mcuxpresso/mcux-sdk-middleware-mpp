/*
 * Copyright 2026 NXP
 * All rights reserved.
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

/*
 * @brief This is the abstraction layer for the openh264 video decoder running on CPU
 */

#include "mpp_config.h"
#include "hal_vdec_dev.h"
#include "hal_debug.h"

#ifdef HAL_ENABLE_H264_DECODER

#include "codec_api.h"

#include "hal_os.h"

/* Decode all SVC layers (no effect for non-SVC streams) */
#define OPENH264_DECODE_ALL_LAYERS 255

typedef struct {
    ISVCDecoder *s_pDecoder;
    SBufferInfo sDstBufInfo;
    uint8_t *dst[3];  /* YUV output buffers */
} h264_decoder_context_t;

hal_vdec_status_t HAL_VdecDev_H264_Init(vdec_h264_dev_t *dev, void *param)
{
    SDecodingParam sDecParam = {0};
    long ret;

    HAL_LOGD("++HAL_VdecDev_H264_Init\r\n");

    h264_decoder_context_t *ctx = hal_malloc(sizeof(h264_decoder_context_t));
    if (ctx == NULL) {
        HAL_LOGE("Failed to allocate decoder context\r\n");
        return MPP_kStatus_HAL_VDecError;
    }
    memset(ctx, 0, sizeof(h264_decoder_context_t));

    ret = WelsCreateDecoder(&ctx->s_pDecoder);
    if (ret != 0 || ctx->s_pDecoder == NULL) {
        HAL_LOGE("Failed to create H.264 decoder\r\n");
        hal_free(ctx);
        return MPP_kStatus_HAL_VDecError;
    }

    sDecParam.uiTargetDqLayer = OPENH264_DECODE_ALL_LAYERS;
    sDecParam.eEcActiveIdc = ERROR_CON_FRAME_COPY_CROSS_IDR;
    sDecParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;

    ret = (*ctx->s_pDecoder)->Initialize(ctx->s_pDecoder, &sDecParam);
    if (ret != 0) {
        HAL_LOGE("Failed to initialize H.264 decoder\r\n");
        (*ctx->s_pDecoder)->Uninitialize(ctx->s_pDecoder);
        WelsDestroyDecoder(ctx->s_pDecoder);
        hal_free(ctx);
        return MPP_kStatus_HAL_VDecError;
    }

    memset(&ctx->sDstBufInfo, 0, sizeof(SBufferInfo));

    dev->user_data = (void *)ctx;

    HAL_LOGD("--HAL_VdecDev_H264_Init\r\n");

    return MPP_kStatus_HAL_VDecSuccess;
}

hal_vdec_status_t HAL_VdecDev_H264_Deinit(const vdec_h264_dev_t *dev)
{
    HAL_LOGD("++HAL_VdecDev_H264_Deinit\r\n");
    
    if (dev->user_data == NULL) {
        HAL_LOGE("Decoder context is NULL\r\n");
        return MPP_kStatus_HAL_VDecError;
    }
    
    h264_decoder_context_t *ctx = (h264_decoder_context_t *)dev->user_data;
    
    if (ctx->s_pDecoder != NULL) {
        (*ctx->s_pDecoder)->Uninitialize(ctx->s_pDecoder);
        WelsDestroyDecoder(ctx->s_pDecoder);
    }
    
    hal_free(ctx);
    
    HAL_LOGD("--HAL_VdecDev_H264_Deinit\r\n");
    return MPP_kStatus_HAL_VDecSuccess;
}

hal_vdec_status_t HAL_VdecDev_H264_Decode(const vdec_h264_dev_t *dev, vdec_h264_frame_info_t *frame_info)
{
    SBufferInfo sDstBufInfo = {0};
    uint8_t *pData[3] = {NULL};  // Array for YUV planes
    DECODING_STATE decState;

    HAL_LOGD("++HAL_VdecDev_H264_Decode\r\n");

    if (dev == NULL || frame_info == NULL) {
        HAL_LOGE("Invalid parameters\r\n");
        return MPP_kStatus_HAL_VDecError;
    }

    if (dev->user_data == NULL) {
        HAL_LOGE("Decoder context is NULL\r\n");
        return MPP_kStatus_HAL_VDecError;
    }

    if (frame_info->src_data == NULL || frame_info->src_size <= 0) {
        HAL_LOGE("Invalid source data\r\n");
        return MPP_kStatus_HAL_VDecError;
    }

    h264_decoder_context_t *ctx = (h264_decoder_context_t *)dev->user_data;

    if (ctx->s_pDecoder == NULL) {
        HAL_LOGE("Decoder not initialized\r\n");
        return MPP_kStatus_HAL_VDecError;
    }

    decState = (*ctx->s_pDecoder)->DecodeFrameNoDelay(ctx->s_pDecoder, frame_info->src_data, frame_info->src_size, pData, &sDstBufInfo);
    if (decState == 0)
    {
       if (sDstBufInfo.iBufferStatus == 1)
        {
            frame_info->width = sDstBufInfo.UsrData.sSystemBuffer.iWidth;
            frame_info->height = sDstBufInfo.UsrData.sSystemBuffer.iHeight;
            frame_info->y_stride = sDstBufInfo.UsrData.sSystemBuffer.iStride[0];
            frame_info->uv_stride = sDstBufInfo.UsrData.sSystemBuffer.iStride[1];
            frame_info->dst_y = sDstBufInfo.pDst[0];
            frame_info->dst_u = sDstBufInfo.pDst[1];
            frame_info->dst_v = sDstBufInfo.pDst[2];
            HAL_LOGD("--HAL_VdecDev_H264_Decode (Success)\r\n");
            return MPP_kStatus_HAL_VDecSuccess;
        }
        else
        {
            HAL_LOGD("--HAL_VdecDev_H264_Decode (Skipped)\r\n");
            return MPP_kStatus_HAL_VDecSkipped;
        }
    }
    else
    {
        HAL_LOGE("--HAL_VdecDev_H264_Decode (Error)\r\n");
        return MPP_kStatus_HAL_VDecError;
    }
}

hal_vdec_status_t HAL_VdecDev_H264_Getbufdesc(const vdec_h264_dev_t *dev, hw_buf_desc_t *in_buf, hw_buf_desc_t *out_buf, mpp_memory_policy_t *policy)
{
    int error = MPP_kStatus_HAL_VDecSuccess;
    do
    {
        if ((in_buf == NULL) || (out_buf == NULL) || (policy == NULL))
        {
            HAL_LOGE("\nNULL pointer to buffer descriptor\n");
            error = MPP_kStatus_HAL_VDecError;
            break;
        }
        /* set memory policy */
        *policy = HAL_MEM_ALLOC_OUTPUT;
    } while(false);

    return error;
}

const static vdec_h264_dev_operator_t vdec_dev_h264_ops = {
    .init = HAL_VdecDev_H264_Init,
    .deinit = HAL_VdecDev_H264_Deinit,
    .get_buf_desc = HAL_VdecDev_H264_Getbufdesc,
    .decode = HAL_VdecDev_H264_Decode,
};

hal_vdec_status_t HAL_VdecDev_H264_CPU_Register(vdec_h264_dev_t *dev)
{
    dev->id = 0;    /* TODO set unique id */
    dev->ops = &vdec_dev_h264_ops;

    return MPP_kStatus_HAL_VDecSuccess;
}
#else /* HAL_ENABLE_H264_DECODER */
hal_vdec_status_t HAL_VdecDev_H264_CPU_Register(vdec_h264_dev_t *dev)
{
    HAL_LOGE("H.264 Decoder not enabled\r\n");
    return MPP_kStatus_HAL_VDecError;
}
#endif /* HAL_ENABLE_H264_DECODER */
