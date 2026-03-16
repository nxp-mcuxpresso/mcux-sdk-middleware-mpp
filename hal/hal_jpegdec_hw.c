/*
 * Copyright 2025 NXP.
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
 * This is the abstraction layer for the JPEG HW image decoder
 */

#include "mpp_config.h"
#include "hal_vdec_dev.h"
#include "hal_debug.h"

#if (HAL_ENABLE_JPEG_HW == 1)
#include "fsl_common.h"
#include "hal_utils.h"
#include "hal_os.h"
#include "fsl_jpegdec.h"

static hal_mutex_t s_mutex; /* prevent stopping JPEG decode during conversion */

JPEG_DECODER_Type g_jpegdec = {
    .core    = JPEGDEC,
    .wrapper = JPGDECWRP,
};

AT_NONCACHEABLE_SECTION(static jpegdec_descpt_t s_decoderDespt);

#define HAL_JPEGDEC                    (&g_jpegdec)
#define HAL_JPEG_DEC_BUFF_ALIGN         16      /* JPEG decoder input/output buffers address should be 16B aligned */
#define HAL_JPEG_DEC_STRIDE_ALIGN       64      /* JPEG decoder output buffer stride should be 64B aligned */
#define HAL_JPEG_DECODE_TIMEOUT_MS      10      /*  JPEG decode complete timeout in ms */
#define HAL_JPEG_WIDTH_ALIGNMENT_MASK   0xFU    /* Width must be multiple of 16 */
#define HAL_JPEG_HEIGHT_ALIGNMENT_MASK  0x7U    /* Height must be multiple of 8 */

/**
 * This function returns the aligned stride.
 * */
static inline int hal_jpegdec_get_aligned_stride(int width, uint32_t pixel_fmt)
{
	int byte_pp = 0;
	int aligned_stride = 0;

	switch (pixel_fmt)
	{
	case kJPEGDEC_PixelFormatYUV422:
		byte_pp = get_bitpp(MPP_PIXEL_YUYV) / 8;
		break;
	default:
		return -1;
	}

    aligned_stride = (width * byte_pp + HAL_JPEG_DEC_STRIDE_ALIGN - 1) & ~(HAL_JPEG_DEC_STRIDE_ALIGN - 1);

    return aligned_stride;
}

int HAL_JPEG_Hw_Init(vdec_dev_t *dev, void *param)
{
	status_t ret = MPP_SUCCESS;
	static jpegdec_config_t config;
	mpp_element_params_t *elem_param = (mpp_element_params_t *)param;

	if (elem_param->decode.out_format != MPP_PIXEL_YUYV)
	{
		HAL_LOGE("HW JPEG supports only YUYV (MPP_PIXEL_YUYV) \n\r");
		return kStatus_Fail;
	}

	if (((elem_param->decode.height & HAL_JPEG_HEIGHT_ALIGNMENT_MASK) != 0U) ||
	    ((elem_param->decode.width & HAL_JPEG_WIDTH_ALIGNMENT_MASK) != 0U))
	{
		HAL_LOGE("HAL_JPEG_Hw_Init: image width should be a multiple of 16 and image height should be a multiple of 8.\n");
		return kStatus_Fail;
	}

	/* Step 1: Init JPEG decoder module. */
	JPEGDEC_GetDefaultConfig(&config);

	/* Enable only one slot. */
	config.slots = kJPEGDEC_Slot0;

	JPEGDEC_Init(HAL_JPEGDEC, &config);

	/* Create mutex */
	if (hal_mutex_create(&s_mutex) != MPP_SUCCESS)
	{
		HAL_LOGE("Failed to create JPEG HW decode mutex\n");
		JPEGDEC_Deinit(HAL_JPEGDEC);
		return kStatus_Fail;
	}

    return ret;
}

int HAL_JPEG_Hw_Deinit(const vdec_dev_t *dev)
{
    int ret = kStatus_Success;

    JPEGDEC_Deinit(HAL_JPEGDEC);
    hal_mutex_remove(s_mutex);

    return ret;
}

int HAL_JPEG_Hw_Getbufdesc(const vdec_dev_t *dev, hw_buf_desc_t *in_buf, hw_buf_desc_t *out_buf, mpp_memory_policy_t *policy)
{
    int ret = kStatus_Success;

    do {
        if ((in_buf == NULL) || (out_buf == NULL) || (policy == NULL))
        {
            HAL_LOGE("NULL pointer to buffer descriptor\n");
            return kStatus_Fail;
        }

        /* set memory policy */
        *policy = HAL_MEM_ALLOC_NONE;
        /* set hw requirement */
        in_buf->alignment = HAL_JPEG_DEC_BUFF_ALIGN;
        in_buf->nb_lines = 0;
        in_buf->cacheable = true;
        in_buf->stride = 0;
        in_buf->max_image_size = 0;
        out_buf->alignment = HAL_JPEG_DEC_BUFF_ALIGN;
        out_buf->cacheable = true;
        out_buf->stride = 0;
        out_buf->max_image_size = 0;
    } while(false);

    return ret;
}

/*
 * @brief decode the source buffer to the destination buffer.
 *
 * @param *dev [in] Pointer to jpeg decoder device.
 * @param *pSrc [in] Pointer to source buffer.
 * @param *pDst [in] Pointer to destination buffer.
 *
 * @returns 0 for the success.
 */
int HAL_JPEG_Hw_Decode(const vdec_dev_t *dev, uint8_t *pSrc, uint8_t *pDst, int32_t jpg_size, uint32_t row_stride)
{
	status_t ret = kStatus_Success;
	uint32_t status = 0;
	int aligned_stride = 0;
	int decode_get_status_start = 0;
	int decode_get_status_end   = 0;

	if (((unsigned int)pSrc % HAL_JPEG_DEC_BUFF_ALIGN) != 0)
	{
		HAL_LOGE("Input buffer at addr=0x%x is not %d bytes aligned\n", (unsigned int)pSrc, HAL_JPEG_DEC_BUFF_ALIGN);
		return kStatus_Fail;
	}

	if (((unsigned int)pDst % HAL_JPEG_DEC_BUFF_ALIGN) != 0)
	{
		HAL_LOGE("Output buffer at addr=0x%x is not %d bytes aligned\n", (unsigned int)pDst, HAL_JPEG_DEC_BUFF_ALIGN);
		return kStatus_Fail;
	}

	if (hal_mutex_lock(s_mutex) != kStatus_Success)
	{
		HAL_LOGE("Failed to lock JPEG decode mutex\n");
		return kStatus_Fail;
	}

	memset(&s_decoderDespt, 0U, sizeof(jpegdec_descpt_t));

	/* Step 2: Reset the JPEG decoder to ensure clean state before use */
	JPEGDEC_Reset(HAL_JPEGDEC);

	/* Step 3: Set source buffer, buffer size. */
	JPEGDEC_SetJpegBuffer(&s_decoderDespt.config, pSrc, jpg_size);

	/* Step 4: Set buffer of generated image for JPEG decoder. */
	JPEGDEC_SetOutputBuffer(&s_decoderDespt.config, pDst, NULL);

	/* Step 5: Parse header. */
	ret = JPEGDEC_ParseHeader(&s_decoderDespt.config);

	if (ret != kStatus_Success)
	{
		HAL_LOGE("HAL_JPEG_Hw_Decode: Error JPEG header parser %d\r\n", ret);
	    hal_mutex_unlock(s_mutex);
		return kStatus_Fail;
	}

	/* Get width and pixel format from descriptor */
    int output_width = s_decoderDespt.config.width;
    uint32_t output_pixel_fmt = s_decoderDespt.config.pixelFormat;

	/* Get JPEG decode output aligned stride */
	aligned_stride = hal_jpegdec_get_aligned_stride(output_width, output_pixel_fmt);
	if (aligned_stride == -1)
	{
		HAL_LOGE("HAL_JPEG_Hw_Decode: unsupported JPEG DEC pixel fmt %d\r\n", output_pixel_fmt);
		hal_mutex_unlock(s_mutex);
		return kStatus_Fail;
	}

	/* Step 6: Set decoder option. */
	JPEGDEC_SetDecodeOption(&s_decoderDespt.config, aligned_stride, false, true);

	/* Step 7: Set slot descriptor. */
	JPEGDEC_SetSlotNextDescpt(HAL_JPEGDEC, 0U, &s_decoderDespt);

	/* Step 8: Enable the descriptor to start the decoding. */
	JPEGDEC_EnableSlotNextDescpt(HAL_JPEGDEC, 0U);

	/* Step 9: Wait for decoding complete with timeout.*/
	decode_get_status_start = hal_get_exec_time();
	status = JPEGDEC_GetStatusFlags(HAL_JPEGDEC, 0U);

    while (((status & (kJPEGDEC_DecodeCompleteFlag | kJPEGDEC_ErrorFlags)) == 0U))
    {
        decode_get_status_end = hal_get_exec_time();
        if (decode_get_status_end >= decode_get_status_start + HAL_JPEG_DECODE_TIMEOUT_MS)
        {
            HAL_LOGE("HAL_JPEG_Hw_Decode: Timeout waiting for JPEG decode completion!\r\n");
            hal_mutex_unlock(s_mutex);
            return kStatus_Fail;
        }

        hal_task_delay(1); /* Add small delay to reduce CPU usage */
        status = JPEGDEC_GetStatusFlags(HAL_JPEGDEC, 0U);
    }

	if (status & kJPEGDEC_ErrorFlags)
	{
		JPEGDEC_ClearStatusFlags(HAL_JPEGDEC, 0U, status);
		HAL_LOGE("HAL_JPEG_Hw_Decode: Error occured during JPEG decoding, status:0x%x\r\n", status);
		hal_mutex_unlock(s_mutex);
		return kStatus_Fail;
	}

	if (hal_mutex_unlock(s_mutex) != kStatus_Success)
	{
		HAL_LOGE("HAL_JPEG_Hw_Decode: Failed to unlock JPEG decode mutex\n");
		return kStatus_Fail;
	}

	return ret;
}

const static vdec_dev_operator_t s_JpegHwOps =
{
    .init         = HAL_JPEG_Hw_Init,
    .deinit       = HAL_JPEG_Hw_Deinit,
    .decode       = HAL_JPEG_Hw_Decode,
    .get_buf_desc = HAL_JPEG_Hw_Getbufdesc,
};

int HAL_JPEG_HW_Register(vdec_dev_t *dev)
{
    dev->id = 0;
    dev->ops = &s_JpegHwOps;

    return 0;
}
#else  /* (HAL_ENABLE_JPEG_HW == 1) */
int HAL_JPEG_HW_Register(vdec_dev_t *dev)
{
    HAL_LOGE("JPEG HW decoder not enabled\n");
    return -1;
}
#endif  /* (HAL_ENABLE_JPEG_HW == 1) */
