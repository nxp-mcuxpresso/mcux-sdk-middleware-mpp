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
 * @brief This is the abstraction layer for the openh264 video encoder running on CPU
 */

#include "mpp_config.h"
#include "hal_venc_dev.h"
#include "hal_debug.h"

#ifdef HAL_ENABLE_H264_ENCODER

#undef HAL_ENABLE_H264_ENCODER_STATISTICS_AND_DEBUG

#include "codec_api.h"

typedef struct {
    ISVCEncoder *s_pEncoder;
} h264_encoder_context_t;

#ifdef HAL_ENABLE_H264_ENCODER_STATISTICS_AND_DEBUG
/*
 * @brief Dump encoder statistics and frame information for debugging
 *
 * @param ctx [in] Encoder context
 * @param info [in] Frame bitstream info
 * @param frame_info [in] Frame information
 */
static void dump_encoder_stats(h264_encoder_context_t *ctx, SFrameBSInfo *info, venc_h264_frame_info_t *frame_info)
{
    /* Log frame type and size */
    if (info->eFrameType == videoFrameTypeI)
        HAL_LOGD("I Frame encoded, frame dst = %p, frame size = %d\r\n", frame_info->dst, frame_info->dst_size);
    else if (info->eFrameType == videoFrameTypeIDR)
        HAL_LOGD("IDR Frame encoded, frame dst = %p, frame size = %d\r\n", frame_info->dst, frame_info->dst_size);
    else if (info->eFrameType == videoFrameTypeP)
        HAL_LOGD("P Frame encoded, frame dst = %p, frame size = %d\r\n", frame_info->dst, frame_info->dst_size);

    /* Get and log encoder statistics */
    SEncoderStatistics sEncoderStats;
    (*ctx->s_pEncoder)->GetOption(ctx->s_pEncoder, ENCODER_OPTION_GET_STATISTICS, &sEncoderStats);

    HAL_LOGD("Frame number: %u\r\n", sEncoderStats.uiInputFrameCount);
    HAL_LOGD("Skipped frames: %u\r\n", sEncoderStats.uiSkippedFrameCount);
    HAL_LOGD("Actual bitrate: %u bps\r\n", sEncoderStats.uiBitRate);
    HAL_LOGD("Average QP: %u\r\n", sEncoderStats.uiAverageFrameQP);

    /* Log layer information */
    for (int layer = 0; layer < info->iLayerNum; layer++)
    {
        SLayerBSInfo* pLayer = &info->sLayerInfo[layer];
        
        int layerSize = 0;
        for (int nal = 0; nal < pLayer->iNalCount; nal++)
        {
            layerSize += pLayer->pNalLengthInByte[nal];
        }
        
        HAL_LOGD("Layer %d:\r\n", layer);
        HAL_LOGD("  Layer type: %d (0=non-video, 1=video)\r\n", pLayer->uiLayerType);
        HAL_LOGD("  Frame type: %d\r\n", pLayer->eFrameType);
        HAL_LOGD("  NAL count: %d\r\n", pLayer->iNalCount);
        HAL_LOGD("  Size: %d bytes\r\n", layerSize);
        HAL_LOGD("  Spatial ID: %d\r\n", pLayer->uiSpatialId);
        HAL_LOGD("  Temporal ID: %d\r\n", pLayer->uiTemporalId);
    }
}
#endif /* HAL_ENABLE_H264_ENCODER_STATISTICS_AND_DEBUG */

hal_venc_status_t HAL_VencDev_H264_Init(venc_h264_dev_t *dev, void *param)
{
    SEncParamExt sEncParam = {0};
    long ret;
    mpp_element_params_t *elem_param = (mpp_element_params_t *)param;

    HAL_LOGD("++HAL_VencDev_H264_Init\r\n");

    if (dev == NULL || param == NULL)
    {
        HAL_LOGE("Invalid parameters\r\n");
        return MPP_kStatus_HAL_VEncError;
    }

    /* Validate pixel format early to avoid unnecessary initialization */
    if (elem_param->encode.format != MPP_PIXEL_YUV420P)
    {
        HAL_LOGE("Unsupported pixel format %d, H.264 encoder only supports YUV420P\r\n", 
                 elem_param->encode.format);
        return MPP_kStatus_HAL_VEncError;
    }

    h264_encoder_context_t *ctx = hal_malloc(sizeof(h264_encoder_context_t));
    if (ctx == NULL)
    {
        HAL_LOGE("Failed to allocate encoder context\r\n");
        return MPP_kStatus_HAL_VEncError;
    }
    memset(ctx, 0, sizeof(h264_encoder_context_t));

    ret = WelsCreateSVCEncoder(&ctx->s_pEncoder);
    if (ret != 0 || ctx->s_pEncoder == NULL)
    {
        hal_free(ctx);
        return MPP_kStatus_HAL_VEncError;
    }

    (*ctx->s_pEncoder)->GetDefaultParams(ctx->s_pEncoder, &sEncParam);

    // Basic settings
    sEncParam.iUsageType = CAMERA_VIDEO_REAL_TIME;
    sEncParam.fMaxFrameRate = elem_param->encode.fps;
    sEncParam.iPicWidth = elem_param->encode.width;
    sEncParam.iPicHeight = elem_param->encode.height;

    // GOP settings
    sEncParam.uiIntraPeriod = elem_param->encode.intra_period;
    sEncParam.iNumRefFrame = elem_param->encode.num_ref_frame;

    // Rate control
    sEncParam.iRCMode = elem_param->encode.rc_mode;
    sEncParam.iTargetBitrate = elem_param->encode.target_bitrate;
    sEncParam.iMaxBitrate = elem_param->encode.max_bitrate;
    sEncParam.bEnableFrameSkip = elem_param->encode.enable_frame_skip;

    // Profile settings
    sEncParam.iTemporalLayerNum = elem_param->encode.temporal_layer_num;
    sEncParam.sSpatialLayers[0].uiProfileIdc = elem_param->encode.profile_idc;

    // Configure spatial layer 0:
    sEncParam.sSpatialLayers[0].uiLevelIdc = elem_param->encode.level_idc;
    sEncParam.sSpatialLayers[0].iVideoWidth = elem_param->encode.width;
    sEncParam.sSpatialLayers[0].iVideoHeight = elem_param->encode.height;
    sEncParam.sSpatialLayers[0].fFrameRate = elem_param->encode.fps;
    sEncParam.sSpatialLayers[0].iSpatialBitrate = elem_param->encode.spatial_bitrate;
    sEncParam.sSpatialLayers[0].iMaxSpatialBitrate = elem_param->encode.max_spatial_bitrate;

    // Entropy coding mode
    sEncParam.iEntropyCodingModeFlag = elem_param->encode.entropy_coding_mode;

    ret = (*ctx->s_pEncoder)->InitializeExt(ctx->s_pEncoder, &sEncParam);
    if (ret != 0)
    {
        HAL_LOGE("Failed to initialize H.264 encoder, ret=%d\r\n", ret);
        (*ctx->s_pEncoder)->Uninitialize(ctx->s_pEncoder);
        WelsDestroySVCEncoder(ctx->s_pEncoder);
        hal_free(ctx);
        return MPP_kStatus_HAL_VEncError;
    }

    /* Set the encoder data format to I420 (YUV420P) */
    int videoFormat = videoFormatI420;
    (*ctx->s_pEncoder)->SetOption(ctx->s_pEncoder, ENCODER_OPTION_DATAFORMAT, &videoFormat);

    dev->user_data = (void *)ctx;

    HAL_LOGD("--HAL_VencDev_H264_Init\r\n");

    return MPP_kStatus_HAL_VEncSuccess;
}

hal_venc_status_t HAL_VencDev_H264_Deinit(const venc_h264_dev_t *dev)
{
    HAL_LOGD("++HAL_VencDev_H264_Deinit\r\n");

    if (dev == NULL)
    {
        HAL_LOGE("Invalid parameters\r\n");
        return MPP_kStatus_HAL_VEncError;
    }

    if (dev->user_data == NULL) {
        HAL_LOGE("Encoder context is NULL\r\n");
        return MPP_kStatus_HAL_VEncError;
    }

    h264_encoder_context_t *ctx = (h264_encoder_context_t *)dev->user_data;

    if (ctx->s_pEncoder != NULL) {
        (*ctx->s_pEncoder)->Uninitialize(ctx->s_pEncoder);
        WelsDestroySVCEncoder(ctx->s_pEncoder);
    }

    hal_free(ctx);

    HAL_LOGD("--HAL_VencDev_H264_Deinit\r\n");

    return MPP_kStatus_HAL_VEncSuccess;
}

hal_venc_status_t HAL_VencDev_H264_Getbufdesc(const venc_h264_dev_t *dev, hw_buf_desc_t *in_buf, hw_buf_desc_t *out_buf, mpp_memory_policy_t *policy)
{
    int ret = MPP_kStatus_HAL_VEncSuccess;
    do
    {
        if ((in_buf == NULL) || (out_buf == NULL) || (policy == NULL))
        {
            HAL_LOGE("\nNULL pointer to buffer descriptor\n");
            ret = MPP_kStatus_HAL_VEncError;
            break;
        }
        /* set memory policy */
        *policy = HAL_MEM_ALLOC_OUTPUT;
    } while(false);

    return ret;
}

hal_venc_status_t HAL_VencDev_H264_Encode(const venc_h264_dev_t *dev, venc_h264_frame_info_t *frame_info)
{
    HAL_LOGD("++HAL_VencDev_H264_Encode\r\n");

    if (dev == NULL || frame_info == NULL)
    {
        HAL_LOGE("Invalid parameters\r\n");
        return MPP_kStatus_HAL_VEncError;
    }

    if (dev->user_data == NULL)
    {
        HAL_LOGE("Encoder context is NULL\r\n");
        return MPP_kStatus_HAL_VEncError;
    }

    h264_encoder_context_t *ctx = (h264_encoder_context_t *)dev->user_data;

    if (frame_info->format != MPP_PIXEL_YUV420P)
    {
        HAL_LOGE("Unsupported pixel format, H.264 expects YUV420P\r\n");
        return MPP_kStatus_HAL_VEncError;
    }
    else
    {
        /* Convert to the format value expected by the encoder */
        frame_info->format = videoFormatI420;
    }

    SSourcePicture pic;
    memset (&pic, 0, sizeof(SSourcePicture));

    pic.iPicWidth = frame_info->width;
    pic.iPicHeight = frame_info->height;
    pic.iColorFormat = frame_info->format;
    pic.iStride[0] = frame_info->y_stride;
    pic.iStride[1] = frame_info->uv_stride;
    pic.iStride[2] = frame_info->uv_stride;
    pic.pData[0] = frame_info->src_y;
    pic.pData[1] = frame_info->src_u;
    pic.pData[2] = frame_info->src_v;

    SFrameBSInfo info;
    memset(&info, 0, sizeof(SFrameBSInfo));

    // Check if it need to be only IDR (keyframe)
    if (frame_info->intraframe)
        (*ctx->s_pEncoder)->ForceIntraFrame(ctx->s_pEncoder, true);

    // Encode the frame
    int rv = (*ctx->s_pEncoder)->EncodeFrame(ctx->s_pEncoder, &pic, &info);

    if (rv == cmResultSuccess && info.eFrameType != videoFrameTypeSkip)
    {
        HAL_LOGD("SUCCESS: Encoded as frame type %d\r\n", info.eFrameType);

        frame_info->dst = info.sLayerInfo[0].pBsBuf;

        int total_size = 0;
        for (int iLayer = 0; iLayer < info.iLayerNum; ++iLayer)
        {
            SLayerBSInfo* pLayerBsInfo = &info.sLayerInfo[iLayer];
            for (int iNal = 0; iNal < pLayerBsInfo->iNalCount; ++iNal)
            {
                total_size += pLayerBsInfo->pNalLengthInByte[iNal];
            }
        }
        frame_info->dst_size = total_size;

#ifdef HAL_ENABLE_H264_ENCODER_STATISTICS_AND_DEBUG
        dump_encoder_stats(ctx, &info, frame_info);
#endif /* HAL_ENABLE_H264_ENCODER_STATISTICS_AND_DEBUG */
    }
    else
    {
        HAL_LOGD("FAIL: Frame cannot be encoded\r\n");
        return MPP_kStatus_HAL_VEncError;
    }

    HAL_LOGD("--HAL_VencDev_H264_Encode\r\n");

    return MPP_kStatus_HAL_VEncSuccess;
}

const static venc_h264_dev_operator_t venc_dev_h264_ops = {
    .init = HAL_VencDev_H264_Init,
    .deinit = HAL_VencDev_H264_Deinit,
    .get_buf_desc = HAL_VencDev_H264_Getbufdesc,
    .encode = HAL_VencDev_H264_Encode,
};

hal_venc_status_t HAL_VencDev_H264_CPU_Register(venc_h264_dev_t *dev)
{
    if (dev == NULL)
    {
        HAL_LOGE("Invalid parameters\r\n");
        return MPP_kStatus_HAL_VEncError;
    }

    dev->id = 0;    /* TODO set unique id */
    dev->ops = &venc_dev_h264_ops;

    return MPP_kStatus_HAL_VEncSuccess;
}
#else /* HAL_ENABLE_H264_ENCODER */
hal_venc_status_t HAL_VencDev_H264_CPU_Register(venc_h264_dev_t *dev)
{
    HAL_LOGE("H.264 Encoder not enabled\r\n");
    return MPP_kStatus_HAL_VEncError;
}
#endif /* HAL_ENABLE_H264_ENCODER */