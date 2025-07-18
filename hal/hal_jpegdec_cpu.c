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
 * This is the abstraction layer for the libjpeg image decoder running on CPU
 */

#include "mpp_config.h"
#include "hal_vdec_dev.h"
#include "hal_debug.h"
#if (HAL_ENABLE_JPEG_CPU == 1)
#include "fsl_common.h"
#include "hal_utils.h"
#include "hal_os.h"
#include "jpeglib.h"
#include "setjmp.h"

/* scaling factor of libjpeg: 8 = no resize */
#define HAL_LIBJPEG_SCALE 8

/* libjpeg context and error handling structures */
struct my_error_mgr
{
    struct jpeg_error_mgr pub;    /* "public" fields */
    jmp_buf setjmp_buffer;        /* for return to caller */
};

typedef struct _libjpeg_ctx
{
    struct jpeg_decompress_struct cinfo;
    struct my_error_mgr jerr;
} libjpeg_ctx;

typedef struct my_error_mgr *my_error_ptr;
char buffer[JMSG_LENGTH_MAX];

/* libjpeg error handler */
void my_error_exit(j_common_ptr cinfo)
{
    /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
    my_error_ptr myerr = (my_error_ptr)cinfo->err;

    /* Always display the message. */
    /* We could postpone this until after returning, if we chose. */
    /* Create the message */
    (*cinfo->err->format_message) (cinfo, buffer);
    HAL_LOGE("JPEG error message: %s\n\r", buffer);

    /* Return control to the setjmp point */
    longjmp(myerr->setjmp_buffer, 1);
}

int HAL_JPEG_Cpu_Init(vdec_dev_t *dev, void *param)
{
    libjpeg_ctx *ctx = NULL;

    /* check output color format */
    if (param == NULL)
    {
        HAL_LOGE("JPEG param NULL pointer \n\r");
        return -1;
    }
    mpp_element_params_t *elem_param = (mpp_element_params_t *)param;
    if (elem_param->decode.out_format != MPP_PIXEL_BGR)
    {
        HAL_LOGE("SW JPEG suppports only BGR888 (MPP_PIXEL_BGR) \n\r");
        return -1;
    }

    /* allocate decoder context */
    ctx = hal_malloc(sizeof(libjpeg_ctx));
    if (ctx == NULL)
    {
        HAL_LOGE("Malloc jpeg context failed \n\r");
        return -1;
    }
    dev->user_data = (void *)ctx;

    /* Step 1: allocate and initialize JPEG decompression object */
    ctx->cinfo.err = jpeg_std_error(&ctx->jerr.pub);
    /* override the exit() method */
    ctx->jerr.pub.error_exit = my_error_exit;

    /* Establish the setjmp return context for my_error_exit to use. */
    if (setjmp(ctx->jerr.setjmp_buffer))
    {
        HAL_LOGE("JPEG decoder creation failed\n\r");
        /* If we get here, the JPEG code has signaled an error.
        * We need to clean up the JPEG object, free memory, and return.
        */
        jpeg_destroy_decompress(&ctx->cinfo);
        if (ctx != NULL) hal_free(ctx);
        ctx = NULL;
        return -1;
    }

    /* Step 2: Initialize the JPEG decompression object */
    jpeg_create_decompress(&ctx->cinfo);

    return 0;
}

int HAL_JPEG_Cpu_Deinit(const vdec_dev_t *dev)
{
    int ret = 0;
    libjpeg_ctx *ctx = (libjpeg_ctx *) dev->user_data;

    /* Step 7: Release JPEG decompression object */
    jpeg_destroy_decompress(&ctx->cinfo);

    if (ctx != NULL) hal_free(ctx);
    ctx = NULL;

    return ret;
}

int HAL_JPEG_Cpu_Getbufdesc(const vdec_dev_t *dev, hw_buf_desc_t *in_buf, hw_buf_desc_t *out_buf, mpp_memory_policy_t *policy)
{
    int error = 0;
    do {
        if ((in_buf == NULL) || (out_buf == NULL) || (policy == NULL))
        {
            HAL_LOGE("NULL pointer to buffer descriptor\n");
            error =  -1;
            break;
        }

        /* set memory policy */
        *policy = HAL_MEM_ALLOC_NONE;
        /* set hw requirement */
        in_buf->alignment = 0;
        in_buf->nb_lines = 0;
        in_buf->cacheable = true;
        in_buf->stride = 0;
        out_buf->alignment = 0;
        out_buf->cacheable = true;
        out_buf->stride = 0;
    } while(false);

    return error;
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
int HAL_JPEG_Cpu_Decode(const vdec_dev_t *dev, uint8_t *pSrc, uint8_t *pDst, int32_t jpg_size, uint32_t row_stride)
{
    libjpeg_ctx *ctx = (libjpeg_ctx *) dev->user_data;
    JSAMPROW row_pointer[1] = {0}; /* Output row buffer */

    /* Establish the setjmp return context for my_error_exit to use. */
    if (setjmp(ctx->jerr.setjmp_buffer))
    {
        HAL_LOGE("JPEG decoding failed\n\r");
        /* If we get here, the JPEG code has signaled an error.
        * We need to clean up the JPEG object, free memory, and return.
        */
        jpeg_destroy_decompress(&ctx->cinfo);
        if (ctx != NULL) hal_free(ctx);
        ctx = NULL;
        return -1;
    }

    /* setup input buffer */
    jpeg_mem_src(&ctx->cinfo, pSrc, jpg_size);

    /* Step 3: read image parameters with jpeg_read_header() */
    jpeg_read_header(&ctx->cinfo, true);

    /* Step 4: set parameters for decompression */
    ctx->cinfo.dct_method = JDCT_FLOAT;

    /* scaling factor is cinfo.scale_num / 8, no resize = 8 */
    ctx->cinfo.scale_num = HAL_LIBJPEG_SCALE;

    /* Step 5: start decompressor */
    jpeg_start_decompress(&ctx->cinfo);

    while (ctx->cinfo.output_scanline < ctx->cinfo.output_height)
    {
        row_pointer[0] = &pDst[ctx->cinfo.output_scanline * row_stride];
        jpeg_read_scanlines(&ctx->cinfo, row_pointer, 1);
    }

    /* Step 6: Finish decompression */
    jpeg_finish_decompress(&ctx->cinfo);

    return 0;
}

const static vdec_dev_operator_t s_JpegCpuOps =
{
    .init         = HAL_JPEG_Cpu_Init,
    .deinit       = HAL_JPEG_Cpu_Deinit,
    .decode       = HAL_JPEG_Cpu_Decode,
    .get_buf_desc = HAL_JPEG_Cpu_Getbufdesc,
};

int HAL_JPEG_CPU_Register(vdec_dev_t *dev)
{
    dev->id = 0;
    dev->ops = &s_JpegCpuOps;

    return 0;
}
#else  /* (HAL_ENABLE_JPEG_CPU == 1) */
int HAL_JPEG_CPU_Register(vdec_dev_t *dev)
{
    HAL_LOGE("JPEG CPU decoder not enabled\n");
    return -1;
}
#endif  /* (HAL_ENABLE_JPEG_CPU == 1) */
