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
#define HAL_JPEG_DECODE_TIMEOUT_MS      10U     /*  JPEG decode complete timeout in ms */
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

/* JPEG frame validation statistics */
typedef struct {
    uint32_t total_frames;           /* Total frames validated */
    uint32_t valid_frames;           /* Frames that passed validation */
    uint32_t invalid_frames;         /* Frames that failed validation */

    /* Critical errors (cause validation failure) */
    uint32_t err_too_small;          /* Frame too small */
    uint32_t err_no_soi;             /* Missing SOI marker */
    uint32_t err_no_sof;             /* Missing SOF marker */
    uint32_t err_no_sos;             /* Missing SOS marker */
    uint32_t err_invalid_segment;    /* Invalid segment length */
    uint32_t err_no_eoi;             /* Missing EOI marker */

    /* Warnings (don't cause validation failure) */
    uint32_t warn_eoi_not_at_end;    /* EOI found but not at end */
    uint32_t warn_no_dqt;            /* Missing DQT marker */
    uint32_t warn_no_dht;            /* Missing DHT marker */

    /* Additional statistics */
    uint32_t max_eoi_offset_from_end; /* Maximum offset from end where EOI was found */
} jpeg_validation_stats_t;

static jpeg_validation_stats_t s_jpeg_stats = {0};

static bool is_valid_jpeg_frame(const uint8_t *data, uint32_t size)
{
    s_jpeg_stats.total_frames++;

    /* Minimum JPEG size: SOI (2) + minimal frame header + some data */
    if (size < 20)
    {
        HAL_LOGI("JPEG frame too small: %u bytes\r\n", size);
        s_jpeg_stats.err_too_small++;
        s_jpeg_stats.invalid_frames++;
        return false;
    }

    /* Check SOI (Start of Image) marker: 0xFF 0xD8 */
    if (data[0] != 0xFF || data[1] != 0xD8)
    {
        HAL_LOGI("JPEG SOI marker missing\r\n");
        s_jpeg_stats.err_no_soi++;
        s_jpeg_stats.invalid_frames++;
        return false;
    }

    /* Quick scan for essential markers and validate structure */
    bool has_sof = false;  /* Start of Frame */
    bool has_sos = false;  /* Start of Scan */
    bool has_dqt = false;  /* Define Quantization Table */
    bool has_dht = false;  /* Define Huffman Table */
    uint32_t pos = 2;      /* Skip SOI */
    uint32_t image_data_start = 0;

    while (pos < size - 2)
    {
        /* Find next marker (0xFF followed by non-zero, non-0xFF) */
        if (data[pos] != 0xFF)
        {
            pos++;
            continue;
        }

        uint8_t marker = data[pos + 1];

        /* Skip padding bytes (0xFF 0xFF) and standalone markers */
        if (marker == 0xFF || marker == 0x00)
        {
            pos++;
            continue;
        }

        /* Check for DQT marker (0xDB) - Quantization tables */
        if (marker == 0xDB)
        {
            has_dqt = true;
        }

        /* Check for DHT marker (0xC4) - Huffman tables */
        if (marker == 0xC4)
        {
            has_dht = true;
        }

        /* Check for SOF markers (0xC0-0xCF, excluding 0xC4, 0xC8, 0xCC) */
        if ((marker >= 0xC0 && marker <= 0xCF) &&
            marker != 0xC4 && marker != 0xC8 && marker != 0xCC)
        {
            has_sof = true;
        }

        /* Check for SOS marker (0xDA) */
        if (marker == 0xDA)
        {
            has_sos = true;

            /* Get segment length */
            if (pos + 3 < size)
            {
                uint16_t segment_len = (data[pos + 2] << 8) | data[pos + 3];
                image_data_start = pos + 2 + segment_len;
                HAL_LOGD("Image data starts at offset: %u\r\n", image_data_start);
            }
            break;  /* SOS is followed by image data, stop scanning */
        }

        /* Get segment length and skip to next marker */
        if (pos + 3 < size)
        {
            uint16_t segment_len = (data[pos + 2] << 8) | data[pos + 3];
            if (segment_len < 2)
            {
                HAL_LOGI("Invalid JPEG segment length\r\n");
                s_jpeg_stats.err_invalid_segment++;
                s_jpeg_stats.invalid_frames++;
                return false;
            }
            pos += 2 + segment_len;
        }
        else
        {
            break;
        }
    }

    /* Validate essential markers */
    if (!has_sof)
    {
        HAL_LOGI("JPEG SOF marker missing\r\n");
        s_jpeg_stats.err_no_sof++;
        s_jpeg_stats.invalid_frames++;
        return false;
    }

    if (!has_sos)
    {
        HAL_LOGI("JPEG SOS marker missing\r\n");
        s_jpeg_stats.err_no_sos++;
        s_jpeg_stats.invalid_frames++;
        return false;
    }

    /* For baseline JPEG, DQT and DHT should be present */
    if (!has_dqt)
    {
        HAL_LOGI("JPEG DQT marker missing (may cause decode issues)\r\n");
        s_jpeg_stats.warn_no_dqt++;
    }

    if (!has_dht)
    {
        HAL_LOGI("JPEG DHT marker missing (may cause decode issues)\r\n");
        s_jpeg_stats.warn_no_dht++;
    }

    /* Check for EOI marker (0xFF 0xD9) - CRITICAL */
    bool has_eoi = false;
    uint32_t eoi_position = 0;

    /* First check if EOI is at the expected position (end of frame) */
    if (size >= 2 && data[size - 2] == 0xFF && data[size - 1] == 0xD9)
    {
        has_eoi = true;
        eoi_position = size - 2;
        HAL_LOGD("EOI found at end of frame (offset %u)\r\n", eoi_position);
    }
    else
    {
        /* Search for EOI from the end backwards until we reach image data start */
        /* Determine search start position */
        uint32_t search_start = (image_data_start > 0) ? image_data_start : 2;

        /* Search backwards from end of buffer to start of image data */
        for (int i = (int)size - 2; i >= (int)search_start; i--)
        {
            if (data[i] == 0xFF && data[i + 1] == 0xD9)
            {
                has_eoi = true;
                eoi_position = i;
                uint32_t offset_from_end = size - eoi_position - 2;

                s_jpeg_stats.warn_eoi_not_at_end++;

                /* Update max offset from end */
                if (offset_from_end > s_jpeg_stats.max_eoi_offset_from_end)
                {
                    s_jpeg_stats.max_eoi_offset_from_end = offset_from_end;
                }

                HAL_LOGD("EOI found at offset %u (not at end, %u bytes after)\r\n",
                         eoi_position, offset_from_end);
                break;
            }
        }
    }

    if (!has_eoi)
    {
        HAL_LOGI("JPEG EOI marker missing\r\n");
        s_jpeg_stats.err_no_eoi++;
        s_jpeg_stats.invalid_frames++;
        return false;
    }

    /* Frame is valid */
    s_jpeg_stats.valid_frames++;
    return true;
}

/* Function to print JPEG validation statistics */
void HAL_JPEG_PrintValidationStats(void)
{
    HAL_LOGI("=== JPEG Validation Statistics ===\r\n");
    HAL_LOGI("Total frames:    %u\r\n", s_jpeg_stats.total_frames);
    HAL_LOGI("Valid frames:    %u\r\n", s_jpeg_stats.valid_frames);
    HAL_LOGI("Invalid frames:  %u\r\n", s_jpeg_stats.invalid_frames);

    HAL_LOGI("\n--- Critical Errors ---\r\n");
    HAL_LOGI("Too small:       %u\r\n", s_jpeg_stats.err_too_small);
    HAL_LOGI("No SOI:          %u\r\n", s_jpeg_stats.err_no_soi);
    HAL_LOGI("No SOF:          %u\r\n", s_jpeg_stats.err_no_sof);
    HAL_LOGI("No SOS:          %u\r\n", s_jpeg_stats.err_no_sos);
    HAL_LOGI("Invalid segment: %u\r\n", s_jpeg_stats.err_invalid_segment);
    HAL_LOGI("No EOI:          %u\r\n", s_jpeg_stats.err_no_eoi);

    HAL_LOGI("\n--- Warnings ---\r\n");
    HAL_LOGI("EOI not at end:  %u\r\n", s_jpeg_stats.warn_eoi_not_at_end);
    HAL_LOGI("No DQT:          %u\r\n", s_jpeg_stats.warn_no_dqt);
    HAL_LOGI("No DHT:          %u\r\n", s_jpeg_stats.warn_no_dht);

    HAL_LOGI("\n--- Additional Info ---\r\n");
    HAL_LOGI("Max EOI offset from end: %u bytes\r\n", s_jpeg_stats.max_eoi_offset_from_end);
    HAL_LOGI("================================\r\n");
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
	uint32_t decode_get_status_start = 0;
	uint32_t decode_get_status_end   = 0;

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

#if (!defined(HAL_JPEG_HW_DISABLE_VALIDATION)) || (HAL_JPEG_HW_DISABLE_VALIDATION == 0)
	if (!is_valid_jpeg_frame((const uint8_t *)pSrc, jpg_size))
	{
		HAL_LOGE("Invalid JPEG frame detected\r\n");
		return kStatus_Fail;
	}
#endif

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
void HAL_JPEG_PrintValidationStats(void)
{
    HAL_LOGE("JPEG HW decoder not enabled, validation statistics not available\n");
}

int HAL_JPEG_HW_Register(vdec_dev_t *dev)
{
    HAL_LOGE("JPEG HW decoder not enabled\n");
    return -1;
}
#endif  /* (HAL_ENABLE_JPEG_HW == 1) */
