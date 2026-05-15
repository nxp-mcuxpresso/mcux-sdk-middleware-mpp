/*
 * Copyright 2025-2026 NXP.
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

#if (!defined(HAL_JPEG_HW_DISABLE_VALIDATION)) || (HAL_JPEG_HW_DISABLE_VALIDATION == 0)

#include "fsl_debug_console.h"

#define MAX_TABLES 4
#define MAX_COMPONENTS 4

/* =============================
   Context
   ============================= */
typedef struct {
    uint8_t dqt_valid[MAX_TABLES];
    uint8_t dht_valid[2][MAX_TABLES]; // [DC/AC]

    uint8_t comp_qt[MAX_COMPONENTS];

    uint16_t width, height;
    uint8_t components;

    int has_soi;
    int has_sof;
    int has_sos;

    int dqt_count;
    int dht_count;
} jpeg_context_t;

/* =============================
   Reader
   ============================= */
typedef struct {
    const uint8_t *ptr;
    const uint8_t *end;
} jpeg_reader_t;

 /* =============================
   Validation stats
   ============================= */
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
    uint32_t err_valid_ent_prefix;   /* Invalid entropy prefix */
    uint32_t err_eoi_before_sos;     /* EOI marker found before SOS marker */
    uint32_t err_dupl_sos;           /* Duplicate SOS marker found */
    uint32_t err_invalid_dqt;        /* Invalid DQT segment */
    uint32_t err_invalid_dht;        /* Invalid DHT segment */
    uint32_t err_no_dqt_before_sos;  /* No DQT tables before SOS */
    uint32_t err_no_dht_before_sos;  /* No DHT tables before SOS */
    uint32_t err_invalid_sos_parse;  /* Failed to parse SOS segment */
    uint32_t err_invalid_sof_parse;  /* Failed to parse SOF marker */

    /* Warnings (don't cause validation failure) */
    uint32_t warn_eoi_not_at_end;    /* EOI found but not at end */

    /* Additional statistics */
    uint32_t max_validation_time_ms;  /* Maximum validation time in milliseconds */
    uint32_t max_eoi_offset_from_end; /* Maximum offset from end where EOI was found */
} jpeg_validation_stats_t;

static jpeg_validation_stats_t s_jpeg_stats = {0};
static int read_byte(jpeg_reader_t *r, uint8_t *out) {
    if (r->ptr >= r->end) return 0;
    *out = *r->ptr++;
    return 1;
}

static int read_word(jpeg_reader_t *r, uint16_t *out) {
    if (r->end - r->ptr < 2) return 0;
    *out = (r->ptr[0] << 8) | r->ptr[1];
    r->ptr += 2;
    return 1;
}

static int skip_bytes(jpeg_reader_t *r, size_t n) {
    if ((size_t)(r->end - r->ptr) < n) return 0;
    r->ptr += n;
    return 1;
}

/* =============================
   Marker Reading
   ============================= */
static int read_marker(jpeg_reader_t *r, uint8_t *marker) {
    uint8_t b;

    do {
        if (!read_byte(r, &b)) return 0;
    } while (b != 0xFF);

    do {
        if (!read_byte(r, marker)) return 0;
    } while (*marker == 0xFF);

    return 1;
}

/* =============================
   Segment Parsers
   ============================= */
static int parse_dqt(jpeg_reader_t *r, jpeg_context_t *ctx) {
    uint16_t length;
    if (!read_word(r, &length) || length < 2) return 0;

    size_t rem = length - 2;

    while (rem > 0) {
        uint8_t info;
        if (!read_byte(r, &info)) return 0;

        uint8_t precision = info >> 4;
        uint8_t id = info & 0x0F;

        if (id >= MAX_TABLES) return 0;
        if (precision != 0 && precision != 1) return 0;

        size_t size = (precision == 0) ? 64 : 128;
        if (rem < 1 + size) return 0;

        // Skip table data (do NOT copy)
        if (!skip_bytes(r, size)) return 0;

        ctx->dqt_valid[id] = 1;
        ctx->dqt_count++;

        rem -= (1 + size);
    }

    return 1;
}

static int parse_dht(jpeg_reader_t *r, jpeg_context_t *ctx) {
    uint16_t length;
    if (!read_word(r, &length) || length < 2) return 0;

    size_t rem = length - 2;

    while (rem > 0) {
        uint8_t info;
        if (!read_byte(r, &info)) return 0;

        uint8_t type = info >> 4;
        uint8_t id = info & 0x0F;

        if (type > 1 || id >= MAX_TABLES) return 0;

        if (rem < 1 + 16) return 0;

        uint8_t counts[16];
        int total = 0;

        for (int i = 0; i < 16; i++) {
            if (!read_byte(r, &counts[i])) return 0;
            total += counts[i];
        }

        if (total > 256) return 0;
        if (rem < 1 + 16 + total) return 0;

        if (!skip_bytes(r, total)) return 0;

        ctx->dht_valid[type][id] = 1;
        ctx->dht_count++;

        rem -= (1 + 16 + total);
    }

    return 1;
}

static int parse_sof(jpeg_reader_t *r, jpeg_context_t *ctx) {
    uint16_t length;
    if (!read_word(r, &length) || length < 8) return 0;

    uint8_t precision;
    if (!read_byte(r, &precision) || precision != 8) return 0;

    if (!read_word(r, &ctx->height)) return 0;
    if (!read_word(r, &ctx->width)) return 0;

    if (ctx->width == 0 || ctx->height == 0) return 0;

    if (!read_byte(r, &ctx->components)) return 0;
    if (ctx->components == 0 || ctx->components > MAX_COMPONENTS) return 0;

    for (int i = 0; i < ctx->components; i++) {
        uint8_t id, sampling, qt;

        if (!read_byte(r, &id)) return 0;
        if (!read_byte(r, &sampling)) return 0;
        if (!read_byte(r, &qt)) return 0;

        uint8_t h = sampling >> 4;
        uint8_t v = sampling & 0x0F;

        if (h == 0 || v == 0 || h > 4 || v > 4) return 0;

        if (qt >= MAX_TABLES || !ctx->dqt_valid[qt]) return 0;

        ctx->comp_qt[i] = qt;
    }

    ctx->has_sof = 1;
    return 1;
}

static int parse_sos(jpeg_reader_t *r, jpeg_context_t *ctx, const uint8_t **entropy_start) {
    uint16_t length;
    if (!read_word(r, &length) || length < 2) return 0;

    uint8_t comps;
    if (!read_byte(r, &comps)) return 0;

    if (comps != ctx->components) return 0;

    for (int i = 0; i < comps; i++) {
        uint8_t id, sel;
        if (!read_byte(r, &id)) return 0;
        if (!read_byte(r, &sel)) return 0;

        uint8_t dc = sel >> 4;
        uint8_t ac = sel & 0x0F;

        if (!ctx->dht_valid[0][dc] || !ctx->dht_valid[1][ac])
            return 0;
    }

    if (!skip_bytes(r, 3)) return 0;

    *entropy_start = r->ptr;

    ctx->has_sos = 1;
    return 1;
}

static int skip_segment(jpeg_reader_t *r) {
    uint16_t length;
    if (!read_word(r, &length) || length < 2) return 0;
    return skip_bytes(r, length - 2);
}

/* =============================
   Entropy Prefix Check
   ============================= */

static int validate_entropy_prefix(const uint8_t *ptr, const uint8_t *end) {
    size_t len = (size_t)(end - ptr);
    size_t limit = len > 128 ? 128 : len;

    for (size_t i = 0; i + 1 < limit; i++) {
        if (ptr[i] == 0xFF) {
            uint8_t b = ptr[i + 1];

            if (b == 0x00) continue;
            if (b >= 0xD0 && b <= 0xD7) continue;
            if (b == 0xD9) return 1;

            return 0;
        }
    }
    return 1;
}

static int find_eoi_backward(const uint8_t *data, size_t size) {
    if (size < 2) return 0;

    size_t window = 1024;
    if (window > size) window = size;

    const uint8_t *start = data + size - window;
    size_t offset_from_end = 0;

    for (const uint8_t *p = data + size - 2; p >= start; p--) {

        if (p[0] == 0xFF && p[1] == 0xD9) {
            if (offset_from_end)
            {
                HAL_LOGD("EOI found at offset %u from end\r\n", offset_from_end);
                if (offset_from_end > s_jpeg_stats.max_eoi_offset_from_end)
                    s_jpeg_stats.max_eoi_offset_from_end = offset_from_end;
                s_jpeg_stats.warn_eoi_not_at_end++;
            }
            return 1;
        }

        offset_from_end++;

        if (p == start) break; // prevent underflow
    }

    return 0;
}

/* =============================
   Main Validation Entry
   ============================= */
static bool is_valid_jpeg_frame(const uint8_t *data, size_t size) {
    s_jpeg_stats.total_frames++;

    if (!data || size < 512)
    {
        HAL_LOGI("JPEG frame too small: %u bytes\r\n", size);
        s_jpeg_stats.err_too_small++;
        s_jpeg_stats.invalid_frames++;
        return 0;
    }

    jpeg_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    jpeg_reader_t r = { data, data + size };

    // SOI
    if (r.ptr[0] != 0xFF || r.ptr[1] != 0xD8)
    {
        HAL_LOGI("JPEG SOI marker missing\r\n");
        s_jpeg_stats.err_no_soi++;
        s_jpeg_stats.invalid_frames++;
        return 0;
    }

    r.ptr += 2;
    ctx.has_soi = 1;

    uint8_t marker;
    const uint8_t *entropy_start = NULL;

    while (read_marker(&r, &marker)) {

        switch (marker) {

        case 0xD9: // EOI before SOS → invalid
            HAL_LOGI("JPEG EOI marker found before SOS\r\n");
            s_jpeg_stats.err_eoi_before_sos++;
            s_jpeg_stats.invalid_frames++;
            return 0;

        case 0xC0: // SOF0
            if (ctx.has_sof) 
            {
                HAL_LOGI("JPEG found duplicate SOF marker\r\n");
                s_jpeg_stats.err_dupl_sos++;
                s_jpeg_stats.invalid_frames++;
                return 0;
            }
            if (!parse_sof(&r, &ctx)) 
            {
                HAL_LOGI("JPEG invalid SOF marker\r\n");
                s_jpeg_stats.err_invalid_sof_parse++;
                s_jpeg_stats.invalid_frames++;
                return 0;
            }
            break;

        case 0xDB:
            if (!parse_dqt(&r, &ctx))
            {
                HAL_LOGI("JPEG invalid DQT segment\r\n");
                s_jpeg_stats.err_invalid_dqt++;
                s_jpeg_stats.invalid_frames++;
                return 0;
            }
            break;

        case 0xC4:
            if (!parse_dht(&r, &ctx))
            {
                HAL_LOGI("JPEG invalid DHT segment\r\n");
                s_jpeg_stats.err_invalid_dht++;
                s_jpeg_stats.invalid_frames++;
                return 0;
            }
            break;

        case 0xDA: // SOS
            if (!ctx.has_sof)
            {
                HAL_LOGI("JPEG SOS found before SOF\r\n");
                s_jpeg_stats.err_no_sof++;
                s_jpeg_stats.invalid_frames++;
                return 0;
            }
            if (ctx.dqt_count == 0 || ctx.dht_count == 0)
            {
                if (ctx.dqt_count == 0)
                {
                    HAL_LOGI("JPEG no DQT tables before SOS\r\n");
                    s_jpeg_stats.err_no_dqt_before_sos++;
                }
                if (ctx.dht_count == 0)
                {
                    HAL_LOGI("JPEG no DHT tables before SOS\r\n");
                    s_jpeg_stats.err_no_dht_before_sos++;
                }
                s_jpeg_stats.invalid_frames++;
                return 0;
            }

            if (!parse_sos(&r, &ctx, &entropy_start))
            {
                HAL_LOGI("JPEG failed to parse SOS segment\r\n");
                s_jpeg_stats.err_invalid_sos_parse++;
                s_jpeg_stats.invalid_frames++;
                return 0;
            }

            if (!validate_entropy_prefix(entropy_start, r.end))
            {
                HAL_LOGI("JPEG invalid entropy data prefix\r\n");
                s_jpeg_stats.err_valid_ent_prefix++;
                s_jpeg_stats.invalid_frames++;
                return 0;
            }

            if (!find_eoi_backward(data, size))
            {
                HAL_LOGI("JPEG EOI marker not found\r\n");
                s_jpeg_stats.err_no_eoi++;
                s_jpeg_stats.invalid_frames++;
                return 0;
            }

            s_jpeg_stats.valid_frames++;
            return 1;

        default:
            if (!skip_segment(&r))
            {
                HAL_LOGI("JPEG invalid segment length\r\n");
                s_jpeg_stats.err_invalid_segment++;
                s_jpeg_stats.invalid_frames++;
                return 0;
            }
            break;
        }
    }

    HAL_LOGI("JPEG unexpected end of data\r\n");
    s_jpeg_stats.err_no_sos++;
    s_jpeg_stats.invalid_frames++;
    return 0;
}

/* Function to print JPEG validation statistics */
void HAL_JPEG_PrintValidationStats(void)
{
    PRINTF("=== JPEG Validation Statistics ===\r\n");
    PRINTF("Total frames:            %u\r\n", s_jpeg_stats.total_frames);
    PRINTF("Valid frames:            %u\r\n", s_jpeg_stats.valid_frames);
    PRINTF("Invalid frames:          %u\r\n", s_jpeg_stats.invalid_frames);

    PRINTF("\n--- Critical Errors ---\r\n");
    PRINTF("Too small:               %u\r\n", s_jpeg_stats.err_too_small);
    PRINTF("No SOI:                  %u\r\n", s_jpeg_stats.err_no_soi);
    PRINTF("No SOF:                  %u\r\n", s_jpeg_stats.err_no_sof);
    PRINTF("No SOS:                  %u\r\n", s_jpeg_stats.err_no_sos);
    PRINTF("Invalid segment:         %u\r\n", s_jpeg_stats.err_invalid_segment);
    PRINTF("No EOI:                  %u\r\n", s_jpeg_stats.err_no_eoi);
    PRINTF("Invalid DQT:             %u\r\n", s_jpeg_stats.err_invalid_dqt);
    PRINTF("Invalid DHT:             %u\r\n", s_jpeg_stats.err_invalid_dht);
    PRINTF("No DQT before SOS:       %u\r\n", s_jpeg_stats.err_no_dqt_before_sos);
    PRINTF("No DHT before SOS:       %u\r\n", s_jpeg_stats.err_no_dht_before_sos);
    PRINTF("Invalid SOS parse:       %u\r\n", s_jpeg_stats.err_invalid_sos_parse);
    PRINTF("Invalid SOF parse:       %u\r\n", s_jpeg_stats.err_invalid_sof_parse);
    PRINTF("Invalid entropy prefix:  %u\r\n", s_jpeg_stats.err_valid_ent_prefix);
    PRINTF("EOI before SOS:          %u\r\n", s_jpeg_stats.err_eoi_before_sos);
    PRINTF("Duplicate SOF:           %u\r\n", s_jpeg_stats.err_dupl_sos);

    PRINTF("\n--- Warnings ---\r\n");
    PRINTF("EOI not at end:          %u\r\n", s_jpeg_stats.warn_eoi_not_at_end);

    PRINTF("\n--- Additional Info ---\r\n");
    PRINTF("Max validation time:     %u ms\r\n", s_jpeg_stats.max_validation_time_ms);
    PRINTF("Max EOI offset from end: %u bytes\r\n", s_jpeg_stats.max_eoi_offset_from_end);
    PRINTF("================================\r\n");
}
#else
/* Function to print JPEG validation statistics */
void HAL_JPEG_PrintValidationStats(void)
{
    HAL_LOGE("JPEG software validation is not enabled\r\n");
}
#endif

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
    uint32_t startTime = hal_get_exec_time();
    bool is_valid = is_valid_jpeg_frame((const uint8_t *)pSrc, jpg_size);
    uint32_t duration = hal_get_exec_time() - startTime;
    if (duration > s_jpeg_stats.max_validation_time_ms)
        s_jpeg_stats.max_validation_time_ms = duration;
    if (!is_valid)
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
