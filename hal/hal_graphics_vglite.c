/*
 * Copyright 2024-2025 NXP.
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
 * @brief GPU HAL gfx driver implementation using VGLite. Used to perform image data conversion
 * from different color formats, scaling, rotation and flip.
 */

#include "mpp_config.h"
#include "hal_graphics_dev.h"
#include "hal_debug.h"

#if (defined HAL_ENABLE_2D_IMGPROC) &&  (HAL_ENABLE_GFX_DEV_GPU == 1)
#include "fsl_common.h"
#include "hal.h"
#include "hal_utils.h"
#include "hal_os.h"

/* VGLite includes */
#include "vglite_support.h"
#include "vg_lite.h"

#ifndef HAL_GPU_CHIPID
#error "HAL:VGLite: GPU chip ID should be defined."
#endif

/* different GPUs chip ids */
#define HAL_GPU_GC355_CHIP_ID 0x355
#define HAL_GPU_GC555_CHIP_ID 0x555

/* Tessellation buffer is not needed for blit operations */
#define HAL_VGLITE_TESSELLATION_BUFF_WIDTH  0
#define HAL_VGLITE_TESSELLATION_BUFF_HEIGHT 0

#if defined(__cplusplus)
extern "C" {
#endif
int HAL_GfxDev_GPU_Register();
#if defined(__cplusplus)
}
#endif

static int s_vgliteInit;  /*<! Flag to track VGLite initialization status */

typedef struct _gfx_vglite_context
{
	bool draw_ongoing;                      /*!< commands are on-going for this context, no need to lock mutex again */
    vg_lite_buffer_t input_buffer_config;   /*!< Pointer to VGLite input buffer configuration */
    vg_lite_buffer_t output_buffer_config;  /*!< Pointer to VGLite output buffer configuration */
    vg_lite_matrix_t vglite_matrix;         /*!< transformation matrix */
} gfx_vglite_context_t;

/* CUSTOM_VGLITE_MEMORY_CONFIG flag should be set to allocate
 * custom vglite memory.
 */
#if defined(CUSTOM_VGLITE_MEMORY_CONFIG) && (CUSTOM_VGLITE_MEMORY_CONFIG == 1)
/* Allocate the heap */
AT_NONCACHEABLE_SECTION_ALIGN(uint8_t vglite_heap[HAL_VGLITE_HEAP_SZ], HAL_VGLITE_BUFFER_ALIGN);
void *vglite_heap_base        = &vglite_heap;
uint32_t vglite_heap_size     = HAL_VGLITE_HEAP_SZ;
#endif

static hal_mutex_t s_mutex; /* prevent stopping GPU during conversion */

int HAL_GfxDev_VGLite_Init(gfx_dev_t *dev, void *param)
{
    status_t status = kStatus_Success;
    int error = 0;
        
    /* initialize vglite controller */
    status = BOARD_PrepareVGLiteController();
    if (status != kStatus_Success)
    {
        HAL_LOGE("\nPrepare VGlite controller error\n");
        return -1;
    }

    /* initialize the draw
     * if vglite is already initialized, no need to reinitialize it.*/
    if (s_vgliteInit == 0)
    {
        status = vg_lite_init(HAL_VGLITE_TESSELLATION_BUFF_WIDTH, HAL_VGLITE_TESSELLATION_BUFF_HEIGHT);
        if (status != kStatus_Success)
        {
            HAL_LOGE("vg_lite engine init failed: vg_lite_init() returned error %d\r\n", error);
            vg_lite_close();
            error = -1;
        }

        if (hal_mutex_create(&s_mutex) != kStatus_Success)
        {
            HAL_LOGE("Failed to create GPU mutex\n");
            return -1;
        }

        s_vgliteInit = 1;
    }

    /* allocate context for VGLite operations in this task */
    gfx_vglite_context_t *vglite_ctx = hal_malloc(sizeof(gfx_vglite_context_t));
    if (vglite_ctx == NULL)
    {
        HAL_LOGE("Failed to allocate VGLite context\r\n");
        return -1;
    }
    /* Zero-initialize the VGLite context */
    memset(vglite_ctx, 0, sizeof(gfx_vglite_context_t));

    /* attach context in user data */
    dev->user_data = vglite_ctx;

    return error;
}

int HAL_GfxDev_VGLite_Deinit(gfx_dev_t *dev)
{
    int error = 0;
    vg_lite_close();
    hal_mutex_remove(s_mutex);
    /* free the context */
    gfx_vglite_context_t *vglite_ctx = (gfx_vglite_context_t *)dev->user_data;
    if (vglite_ctx != NULL) {
        hal_free(vglite_ctx);
        dev->user_data = NULL;
    }
        
    return error;
}

/**
 * This function returns the src buffer alignment required in bytes.
 * */
static inline int hal_vglite_get_src_buffer_alignment(mpp_pixel_format_t type)
{
    int ret = 0;

#if (HAL_GPU_CHIPID == HAL_GPU_GC555_CHIP_ID) /* GC555 */
    switch(type)
    {
    case MPP_PIXEL_ARGB:
    case MPP_PIXEL_BGRA:
    case MPP_PIXEL_RGBA:
    case MPP_PIXEL_BGRX:
    case MPP_PIXEL_RGBX:
    case MPP_PIXEL_RGB:
    case MPP_PIXEL_BGR:
        ret = 64;
        break;
    case MPP_PIXEL_RGB565:
    case MPP_PIXEL_YUYV:
        ret = 32;
        break;
    case MPP_PIXEL_GRAY:
        ret = 16;
        break;
    default:
        HAL_LOGE("hal_vglite_get_src_buffer_alignment() Color format %d is not supported\n", type);
        return -1;
    }
#elif (HAL_GPU_CHIPID == HAL_GPU_GC355_CHIP_ID) /* GC355 */
    switch(type)
    {
    case MPP_PIXEL_ARGB:
    case MPP_PIXEL_BGRA:
    case MPP_PIXEL_RGBA:
    case MPP_PIXEL_BGRX:
    case MPP_PIXEL_RGBX:
    case MPP_PIXEL_RGB565:
    case MPP_PIXEL_YUYV:
        ret = 16 * get_bitpp(type) / 8; /* GC355 requires 16 Pixels alignment */
        break;
    default:
        HAL_LOGE("hal_vglite_get_src_buffer_alignment() Color format %d is not supported\n", type);
        return -1;
    }
#endif

    return ret;
}

/**
 * This function returns the dst buffer alignment required in bytes.
 * */
static inline int hal_vglite_get_dst_buffer_alignment(mpp_pixel_format_t type)
{
    int ret = 0;

    switch(type)
    {
    case MPP_PIXEL_ARGB:
    case MPP_PIXEL_BGRA:
    case MPP_PIXEL_RGBA:
    case MPP_PIXEL_BGRX:
    case MPP_PIXEL_RGBX:
    case MPP_PIXEL_RGB:
    case MPP_PIXEL_BGR:
    case MPP_PIXEL_RGB565:
    case MPP_PIXEL_YUYV:
    case MPP_PIXEL_GRAY:
        ret = 64;
        break;
    default:
        HAL_LOGE("hal_vglite_get_dst_buffer_alignment() Color format %d is not supported\n", type);
        return -1;
    }
    return ret;
}

/**
 * This function returns the aligned stride.
 * */
static inline int hal_vglite_get_aligned_stride(int width, mpp_pixel_format_t type)
{
    int alignment = 0;
    int stride = 0;

#if (HAL_GPU_CHIPID == HAL_GPU_GC555_CHIP_ID) /* GC555 */
    /* get GPU stride alignment requirement */
    switch(type)
    {
    case MPP_PIXEL_ARGB:
    case MPP_PIXEL_BGRA:
    case MPP_PIXEL_RGBA:
    case MPP_PIXEL_BGRX:
    case MPP_PIXEL_RGBX:
    case MPP_PIXEL_YUYV:
        alignment = 64;
        break;
    case MPP_PIXEL_RGB:
    case MPP_PIXEL_BGR:
        alignment = 48;
        break;
    case MPP_PIXEL_RGB565:
        alignment = 32;
        break;
    case MPP_PIXEL_GRAY:
        alignment = 16;
        break;
    default:
        HAL_LOGE("hal_vglite_get_aligned_stride() Color format %d is not supported\n", type);
        return -1;
    }
#elif (HAL_GPU_CHIPID == HAL_GPU_GC355_CHIP_ID) /* GC355 */
    switch(type)
    {
    case MPP_PIXEL_ARGB:
    case MPP_PIXEL_BGRA:
    case MPP_PIXEL_RGBA:
    case MPP_PIXEL_BGRX:
    case MPP_PIXEL_RGBX:
    case MPP_PIXEL_RGB565:
    case MPP_PIXEL_YUYV:
    	alignment = 16 * get_bitpp(type) / 8; /* GC355 requires 16 Pixels alignment */
        break;
    default:
        HAL_LOGE("hal_vglite_get_src_buffer_alignment() Color format %d is not supported\n", type);
        return -1;
    }
#endif

    int non_aligned_stride = width * get_bitpp(type) / 8;

    if ((alignment != 0) && ((non_aligned_stride % alignment)) != 0)
    {
        /* if stride is not aligned take the multiple of alignment
           that's greater than the non aligned stride */
        stride = (non_aligned_stride + alignment) - (non_aligned_stride % alignment);
    }
    else
    {
        /* no alignment is required or stride is already aligned */
        stride = non_aligned_stride;
    }

    return stride;
}

int HAL_GfxDev_VGLite_Getbufdesc(const gfx_dev_t *dev, hw_buf_desc_t *in_buf, hw_buf_desc_t *out_buf, mpp_memory_policy_t *policy)
{
    int error = 0;
    do
    {
        if ((in_buf == NULL) || (out_buf == NULL) || (policy == NULL))
        {
            HAL_LOGE("\nNULL pointer to buffer descriptor\n");
            error =  -1;
            break;
        }
        /* set memory policy */
        *policy = HAL_MEM_ALLOC_NONE;

        /* set input buffer hw requirement */
        /* VGLITE requires input buffer and stride to be aligned */
        in_buf->alignment = hal_vglite_get_src_buffer_alignment(dev->src.format);

        if (in_buf->alignment == -1)
        {
            HAL_LOGE("\nHAL_GfxDev_VGLite_Getbufdesc(): Invalid input buffer alignment\n");
            return -1;
        }

        in_buf->nb_lines = 0;
        in_buf->cacheable = false;
        in_buf->stride = hal_vglite_get_aligned_stride(dev->src.width, dev->src.format);

        if (in_buf->stride == -1)
        {
            HAL_LOGE("\nHAL_GfxDev_VGLite_Getbufdesc(): Invalid input stride alignment\n");
            return -1;
        }

        in_buf->max_image_size = 0;

        /* set output buffer hw requirement */
        /* Alignment is required for the output buffer address*/
        out_buf->alignment = hal_vglite_get_dst_buffer_alignment(dev->dst.format);

        if (out_buf->alignment == -1)
        {
            HAL_LOGE("\nHAL_GfxDev_VGLite_Getbufdesc(): Invalid output buffer alignment\n");
            return -1;
        }

        out_buf->cacheable = false;
        out_buf->stride = 0;
        out_buf->max_image_size = 0;

    } while(false);

    return error;
}

/**
 * This function sets the input buffer color format. It returns:
 *    -  0 : Success
 *    - -1:  Fail
 */
static int hal_vglite_set_input_buff_format(vg_lite_buffer_t *input_buffer, gfx_surface_t *src)
{
    int error = 0;

    /* setup input buffer color format */
    switch (src->format)
    {
    case MPP_PIXEL_ARGB:
        input_buffer->format = VG_LITE_ARGB8888; /* This is a temporary workaround */
        break;

    case MPP_PIXEL_BGRA:
        input_buffer->format = VG_LITE_BGRA8888;
        break;

    case MPP_PIXEL_BGRX:
        input_buffer->format = VG_LITE_BGRX8888;
        break;

    case MPP_PIXEL_RGBA:
        input_buffer->format = VG_LITE_RGBA8888;
        break;

    case MPP_PIXEL_RGBX:
        input_buffer->format = VG_LITE_RGBX8888;
        break;

    /* GRAY format is not supported on GC355 */
    case MPP_PIXEL_GRAY:
#if (HAL_GPU_CHIPID == HAL_GPU_GC555_CHIP_ID)    /* GC555 */
        input_buffer->format = VG_LITE_L8;
#elif (HAL_GPU_CHIPID == HAL_GPU_GC355_CHIP_ID) /* GC355 */
        HAL_LOGE("GRAY format is not supported on GPU GC355\r\n");
        error = -1;
#else
        HAL_LOGE("GPU GC%x not supported\r\n", HAL_GPU_CHIPID);
        error = -1;
#endif
        break;

    case MPP_PIXEL_RGB565:
        input_buffer->format = VG_LITE_BGR565;
        break;

    case MPP_PIXEL_YUYV:
        input_buffer->format = VG_LITE_YUYV;
        break;

    case MPP_PIXEL_RGB:
    {
        /* RGB888 format is still not supported on GC355 */
#if (HAL_GPU_CHIPID == HAL_GPU_GC555_CHIP_ID)    /* GC555 */
        input_buffer->format = VG_LITE_RGB888;  /* This is a temporary workaround */
#elif (HAL_GPU_CHIPID == HAL_GPU_GC355_CHIP_ID) /* GC355 */
        HAL_LOGE("RGB888 format is not supported on GPU GC355\r\n");
        error = -1;
#else
        HAL_LOGE("GPU GC%x not supported\r\n", HAL_GPU_CHIPID);
        error = -1;
#endif
        break;
    }

    case MPP_PIXEL_BGR:
    {
        /* BGR888 format is still not supported on GC355 */
#if (HAL_GPU_CHIPID == HAL_GPU_GC555_CHIP_ID)    /* GC555 */
        input_buffer->format = VG_LITE_BGR888;  /* This is a temporary workaround */
#elif (HAL_GPU_CHIPID == HAL_GPU_GC355_CHIP_ID) /* GC355 */
        HAL_LOGE("BGR888 format is not supported on GPU GC355\r\n");
        error = -1;
#else
        HAL_LOGE("GPU GC%x not supported\r\n", HAL_GPU_CHIPID);
        error = -1;
#endif
        break;
    }

    default:
    {
        HAL_LOGE("Unsupported source surface pixel format: %d\n", src->format);
        error = -1;
    }
    break;
    }

    return error;
}

/**
 * This function initializes the vg_lite input buffer structure. It returns:
 *    -  0 : Success
 *    - -1:  Fail
 */
static int hal_vglite_init_input_buffer(vg_lite_buffer_t *vg_input_buffer, gfx_surface_t *src)
{
    int error = 0;
    uint32_t input_fbuf;
    int input_buff_alignment = 0;

    error = hal_vglite_set_input_buff_format(vg_input_buffer, src);
    if (error == -1)
    {
        HAL_LOGE("hal_vglite_set_input_buff_format() returned error %d\n", error);
        return error;
    }

    input_buff_alignment = hal_vglite_get_src_buffer_alignment(src->format);
    if (input_buff_alignment == -1)
    {
        HAL_LOGE("hal_vglite_get_src_buffer_alignment(): color format %d is not supported.\n", src->format);
        return -1;
    }

    int bpp = get_bitpp(src->format);

    /* use src buffer boundaries to calculate the vglite buffer width/height
     * to adapt resolution with crop parameters.
     */
    vg_input_buffer->height    = src->bottom -src->top +1;
    vg_input_buffer->width     = src->right -src->left + 1;
    vg_input_buffer->stride    = hal_vglite_get_aligned_stride(src->width, src->format);

    input_fbuf = (uint32_t)src->buf + (src->left * bpp / 8) + (src->top * src->pitch);

    /* align cropped input buffer */
    if ((input_buff_alignment != 0) && ((input_fbuf % input_buff_alignment) != 0))
        input_fbuf = input_fbuf + input_buff_alignment - (input_fbuf % input_buff_alignment);

    vg_input_buffer->memory    = (void *)input_fbuf;
    vg_input_buffer->address   = input_fbuf;
    vg_input_buffer->tiled = VG_LITE_LINEAR;
    vg_input_buffer->image_mode = VG_LITE_NORMAL_IMAGE_MODE;
    vg_input_buffer->transparency_mode = VG_LITE_IMAGE_OPAQUE;

    return error;
}

/**
 * This function returns the rotation degree.
 */
static vg_lite_float_t hal_vglite_set_surface_rotate(gfx_rotate_config_t *gRotate)
{
    vg_lite_float_t rotateDegree = 0.0;

    if (gRotate != NULL)
    {
        switch (gRotate->degree)
        {
        case ROTATE_90:
            rotateDegree = 90.0;
            break;

        case ROTATE_180:
            rotateDegree = 180.0;
            break;

        case ROTATE_270:
            rotateDegree = 270.0;
            break;

        default:
            rotateDegree = 0.0;
            break;
        }
    }

    return rotateDegree;
}

static int hal_vglite_scale(vg_lite_buffer_t *input_buffer,
        vg_lite_matrix_t *vglite_matrix, int output_width, int output_height)
{

    int status = 0;
    vg_lite_float_t width_scaling_f = 1.0f, height_scaling_f = 1.0f;

    /* get scaling width/height factors */
    if ((input_buffer->height != 0) && (input_buffer->width != 0))
    {
        width_scaling_f = (vg_lite_float_t)output_width / (vg_lite_float_t)input_buffer->width;
        height_scaling_f = (vg_lite_float_t)output_height / (vg_lite_float_t)input_buffer->height;
    }
    else
    {
        HAL_LOGE("Invalid input resolution parameters: %dx%d\n", input_buffer->width, input_buffer->height);
        return -1;
    }

    if ((width_scaling_f != 1.0f) || (height_scaling_f != 1.0f))
        vg_lite_scale(width_scaling_f, height_scaling_f, vglite_matrix);

    return status;
}

/**
 * This function returns the product of two matrices matrix_1 and matrix2.
 */
static vg_lite_matrix_t hal_vglite_multiply_matrix(vg_lite_matrix_t *matrix_1, vg_lite_matrix_t *matrix2)
{
    /* matrix that will store the product of matrix_1 & matrix_2 */
    vg_lite_matrix_t mult_matrix = {0};
    /* vg_lite_matrix_t is a 3x3 matrix */
    const int vg_nb_row = 3;
    const int vg_nb_coloumn = 3;

    /* browse all rows */
    for (int row = 0; row < vg_nb_row; row++)
    {
        /* browse all columns */
        for (int column = 0; column < vg_nb_coloumn; column++)
        {
            mult_matrix.m[row][column] = (matrix_1->m[row][0] * matrix2->m[0][column])
                                                                    + (matrix_1->m[row][1] * matrix2->m[1][column])
                                                                    + (matrix_1->m[row][2] * matrix2->m[2][column]);

        }
    }

    return mult_matrix;
}

/**
 * This function sets the flip transformation matrix
 * */
static int hal_vglite_flip(vg_lite_matrix_t *vglite_matrix, mpp_flip_mode_t flip_mode,
        int output_width, int output_height)
{
    int status = 0;
    vg_lite_float_t translate_x = 0.0, translate_y = 0.0;
    vg_lite_matrix_t mult_matrix = {0};

    /* Set flip matrix. */
    vg_lite_matrix_t vg_reflection_matrix = {0};
    vg_lite_identity(&vg_reflection_matrix);

    switch (flip_mode)
    {
    case FLIP_HORIZONTAL:
    {
        vg_reflection_matrix.m[0][0] = -1.0f;
        translate_x = -1.0f * output_width;
    }
    break;

    case FLIP_VERTICAL:
    {
        vg_reflection_matrix.m[1][1] = -1.0f;
        translate_y = -1.0f * output_height;
    }
    break;

    case FLIP_BOTH:
    {
        vg_reflection_matrix.m[0][0] = -1.0f;
        vg_reflection_matrix.m[1][1] = -1.0f;
        translate_x = -1.0f * output_width;
        translate_y = -1.0f * output_height;
    }
    break;

    case FLIP_NONE:
        break;

    default:
    {
        HAL_LOGE("Unsupported flip mode %d\n", flip_mode);
        return -1;
    }
    break;
    }

    /* Multiply with current matrix. */
    mult_matrix = hal_vglite_multiply_matrix(vglite_matrix, &vg_reflection_matrix);
    /* copy the result of the multiplication of vglite_matrix and vg_reflection_matrix
     * in vglite_matrix.
     */
    memcpy(vglite_matrix, &mult_matrix, sizeof(mult_matrix));

    vg_lite_translate(translate_x, translate_y, vglite_matrix);

    return status;
}

/**
 *  This function sets the rotation/scaling/flip transformation matrix
 *  */
static int hal_vglite_init_surface(gfx_rotate_config_t rotate, vg_lite_matrix_t *vglite_matrix,
        vg_lite_buffer_t *output_buffer, vg_lite_buffer_t *input_buffer, gfx_surface_t *gDst,
        mpp_flip_mode_t flip_mode)
{
    int status = 0;

    /* get scaled area width/height before rotation */
    int image_scaled_width = 0;
    int image_scaled_height = 0;

    /* setup rotation configuration */
    vg_lite_float_t rotate_degree     = 0.0;
    vg_lite_float_t translate_x, translate_y = 0.0;

    /* initialize transformation matrix */
    vg_lite_identity(vglite_matrix);

    /* set output position */
    if (rotate.target == kGFXRotate_DSTSurface)
    {
        if (rotate.degree == ROTATE_90)
        {
            /* swap output dims */
            image_scaled_width = gDst->bottom - gDst->top + 1;
            image_scaled_height = gDst->right - gDst->left + 1;
            /* translate on x-axix in order to rotate the Left point by 90 degrees.*/
            translate_x = gDst->right;
            translate_y = gDst->top;
        }
        else if (rotate.degree == ROTATE_270)
        {
            /* swap output dims */
            image_scaled_width = gDst->bottom - gDst->top + 1;
            image_scaled_height = gDst->right - gDst->left + 1;
            /* translate on x,y-axis in order to rotate the top,Left point by 270 degrees.*/
            translate_x = gDst->left;
            translate_y = gDst->bottom;
        }
        else if (rotate.degree == ROTATE_180)
        {
            image_scaled_width = gDst->right - gDst->left + 1;
            image_scaled_height = gDst->bottom - gDst->top + 1;
            /* translate on x,y-axis in order to rotate the top,Left point by 180 degrees.*/
            translate_x = gDst->right;
            translate_y = gDst->bottom;
        }
        else
        { /* 0 degree */
            image_scaled_width = gDst->right - gDst->left + 1;
            image_scaled_height = gDst->bottom - gDst->top + 1;
            translate_x = gDst->left;
            translate_y = gDst->top;
        }
    }
    else
    { /* 0 degree */
        image_scaled_width = gDst->right - gDst->left + 1;
        image_scaled_height = gDst->bottom - gDst->top + 1;
        translate_x = gDst->left;
        translate_y = gDst->top;
    }

    /* apply translate for output window position */
    vg_lite_translate(translate_x, translate_y, vglite_matrix);
    /* apply rotate */
    if (rotate.degree != ROTATE_0)
    {
        rotate_degree = hal_vglite_set_surface_rotate(&rotate);
        vg_lite_rotate(rotate_degree, vglite_matrix);
    }

    /* flip if needed */
    if (flip_mode != FLIP_NONE)
    {
        status = hal_vglite_flip(vglite_matrix, flip_mode, image_scaled_width,
                image_scaled_height);
        if (status != 0)
        {
            HAL_LOGE("hal_vglite_flip() failed\r\n");
            return status;
        }
    }

    /* setup scaler configuration */
    status = hal_vglite_scale(input_buffer, vglite_matrix, image_scaled_width,
            image_scaled_height);
    if (status != 0)
    {
        HAL_LOGE("hal_vglite_scale() failed.\r\n");
        return status;
    }

    return status;
}

/*
 * This function sets the output buffer color format. It returns:
 *    -  0 : Success
 *    - -1:  Fail
 */
static int hal_vglite_set_output_buff_format(vg_lite_buffer_t *output_buffer, const gfx_surface_t *dst)
{
    int error = 0;

    /* setup input buffer color format */
    switch (dst->format)
    {
    case MPP_PIXEL_RGB565:
        output_buffer->format = VG_LITE_BGR565;
        break;

    /* GRAY format is not supported on GC355 */
    case MPP_PIXEL_GRAY:
#if (HAL_GPU_CHIPID == HAL_GPU_GC555_CHIP_ID)    /* GC555 */
        output_buffer->format = VG_LITE_L8;
#elif (HAL_GPU_CHIPID == HAL_GPU_GC355_CHIP_ID) /* GC355 */
        HAL_LOGE("GRAY format is not supported on GPU GC355\r\n");
        return -1;
#else
        HAL_LOGE("GPU GC%x not supported\r\n", HAL_GPU_CHIPID);
        return -1;
#endif
        break;

    case MPP_PIXEL_YUYV:
        output_buffer->format = VG_LITE_YUYV;
        break;

    case MPP_PIXEL_ARGB:
        output_buffer->format = VG_LITE_ARGB8888;
        break;

    case MPP_PIXEL_RGB:
    {
        /* RGB888 format is still not supported on GC355 */
#if (HAL_GPU_CHIPID == HAL_GPU_GC555_CHIP_ID)    /* GC555 */
        output_buffer->format = VG_LITE_RGB888;  /* This is a temporary workaround */
#elif (HAL_GPU_CHIPID == HAL_GPU_GC355_CHIP_ID) /* GC355 */
        HAL_LOGE("RGB888 format is not supported on GPU GC355\r\n");
        return -1;
#else
        HAL_LOGE("GPU GC%x not supported\r\n", HAL_GPU_CHIPID);
        return -1;
#endif
        break;
    }

    case MPP_PIXEL_BGR:
    {
        /* BGR888 format is still not supported on GC355 */
#if (HAL_GPU_CHIPID == HAL_GPU_GC555_CHIP_ID)    /* GC555 */
        output_buffer->format = VG_LITE_BGR888;  /* This is a temporary workaround */
#elif (HAL_GPU_CHIPID == HAL_GPU_GC355_CHIP_ID) /* GC355 */
        HAL_LOGE("BGR888 format is not supported on GPU GC355\r\n");
        return -1;
#else
        HAL_LOGE("GPU GC%x not supported\r\n", HAL_GPU_CHIPID);
        return -1;
#endif
        break;
    }
    default:
    {
        HAL_LOGE("Unsupported output surface pixel format: %d\n", dst->format);
        return -1;
    }
    break;
    }

    return error;
}

/*
 * This function initializes the vg_lite output buffer structure. It returns:
 *    -  0 : Success
 *    - -1:  Fail
 */
static int hal_vglite_init_output_buffer(vg_lite_buffer_t *vg_output_buffer, const gfx_surface_t *dst)
{
    int error = 0;
    uint32_t output_fbuf;

    error = hal_vglite_set_output_buff_format(vg_output_buffer, dst);
    if (error == -1)
    {
        HAL_LOGE("hal_vglite_set_output_buff_format() returned error %d\n", error);
        return error;
    }

    vg_output_buffer->height    = dst->height;
    vg_output_buffer->width     = dst->width;
    vg_output_buffer->stride    = dst->pitch;

    output_fbuf = (uint32_t)dst->buf;
    vg_output_buffer->memory    = (void *)output_fbuf;
    vg_output_buffer->address   = output_fbuf;
    vg_output_buffer->tiled = VG_LITE_LINEAR;
    vg_output_buffer->image_mode = VG_LITE_NORMAL_IMAGE_MODE;
    vg_output_buffer->transparency_mode = VG_LITE_IMAGE_OPAQUE;

    return error;
}

/*
 * @brief blit the source surface to the destination surface.
 *
 * @param *dev      [in] Pointer to VGLite device.
 * @param *pSrc     [in] Pointer to source surface.
 * @param *pDst     [in] Pointer to destination surface (after rotation/scale/flip).
 * @param *pRotate  [in] Pointer to the rotation config.
 * @param flip_mode [in] Flip mode.
 *
 * @returns 0 for the success.
 */
int HAL_GfxDev_VGLite_Blit(const gfx_dev_t *dev, const gfx_surface_t *gfx_src,
        const gfx_surface_t *gfx_dst, const gfx_rotate_config_t *gfx_rotate,
        mpp_flip_mode_t flip_mode)
{
    int error                                = 0;
    gfx_vglite_context_t *vglite_ctx = (gfx_vglite_context_t *)dev->user_data;
    vg_lite_buffer_t *input_buffer_config    = &vglite_ctx->input_buffer_config;
    vg_lite_buffer_t *output_buffer_config   = &vglite_ctx->output_buffer_config;
    gfx_surface_t src = {0}, dst = {0};
    gfx_rotate_config_t rotate = {0};
    int input_buff_alignment = 0;
    

    if ( (gfx_src->buf == NULL) || (gfx_dst->buf == NULL) )
    {
        HAL_LOGE("GPU accessing buffer at address NULL\n");
        return -1;
    }

    input_buff_alignment = hal_vglite_get_src_buffer_alignment(gfx_src->format);
    if ((input_buff_alignment != 0) && (((unsigned int)(gfx_src->buf) % input_buff_alignment) != 0))
    {
        HAL_LOGE("Input buffer at addr=0x%x is not %d bytes aligned\n", (unsigned int)gfx_src->buf, input_buff_alignment);
    }

    memcpy(&src, gfx_src, sizeof(gfx_surface_t));
    memcpy(&dst, gfx_dst, sizeof(gfx_surface_t));
    memcpy(&rotate, gfx_rotate, sizeof(gfx_rotate_config_t));

    /* lock the mutex only once in this context */
    if (!vglite_ctx->draw_ongoing)
    {
        // Acquire GPU context mutex to ensure thread-safe GPU operations
        if (hal_mutex_lock(s_mutex) != kStatus_Success)
        {
            HAL_LOGE("Failed to lock GPU mutex\n");
            vg_lite_close();
            return -1;
        }
        vglite_ctx->draw_ongoing = true;
    }

    /* setup the input buffer configuration */
    error = hal_vglite_init_input_buffer(input_buffer_config, &src);
    if (error == -1)
    {
        HAL_LOGE("hal_vglite_init_input_buffer() returned error %d\n", error);
        vg_lite_close();
        hal_mutex_unlock(s_mutex);
        return -1;
    }

    /* setup the output buffer configuration */
    error = hal_vglite_init_output_buffer(output_buffer_config, &dst);
    if (error == -1)
    {
        HAL_LOGE("hal_vglite_init_output_buffer() returned error %d\n", error);
        vg_lite_close();
        hal_mutex_unlock(s_mutex);
        return error;
    }

    /* scale/rotate/flip */
    error = hal_vglite_init_surface(rotate, &vglite_ctx->vglite_matrix,
            output_buffer_config, input_buffer_config, &dst, flip_mode);
    if (error == -1)
    {
        HAL_LOGE("hal_vglite_init_surface() returned error %d\r\n", error);
        vg_lite_close();
        hal_mutex_unlock(s_mutex);
        return error;
    }

    error = vg_lite_blit(output_buffer_config, input_buffer_config, &vglite_ctx->vglite_matrix,
            VG_LITE_BLEND_NONE, 0, VG_LITE_FILTER_BI_LINEAR);
    if (error != kStatus_Success)
    {
        HAL_LOGE("vg_lite_blit() returned error %d\r\n", error);
        vg_lite_close();
        hal_mutex_unlock(s_mutex);
        return -1;
    }

    /* submit command buffer to GPU */
    error = vg_lite_finish();
    if (error != kStatus_Success)
    {
        HAL_LOGE("vg_lite_finish() returned error %d\r\n", error);
        vg_lite_close();
        hal_mutex_unlock(s_mutex);
        return -1;
    }

    return error;
}

int HAL_GfxDev_VGLite_Finish(gfx_dev_t *dev)
{
    gfx_vglite_context_t *vglite_ctx = (gfx_vglite_context_t *)dev->user_data;

    vg_lite_finish();
    vglite_ctx->draw_ongoing = false;
    /* release mutex for other tasks */
    if (hal_mutex_unlock(s_mutex) != kStatus_Success)
    {
        HAL_LOGE("Failed to unlock GPU mutex\n");
        vg_lite_close();
        return -1;
    }

    return 0;
}

const static gfx_dev_operator_t s_GfxDevVGLiteOps = {
        .init        = HAL_GfxDev_VGLite_Init,
        .deinit      = HAL_GfxDev_VGLite_Deinit,
        .blit        = HAL_GfxDev_VGLite_Blit,
        .finish      = HAL_GfxDev_VGLite_Finish,
        .get_buf_desc = HAL_GfxDev_VGLite_Getbufdesc,
};

int HAL_GfxDev_GPU_Register(gfx_dev_t *dev)
{
    dev->id = 0;    /* TODO set unique id */
    dev->ops = &s_GfxDevVGLiteOps;

    return 0;
}
#else  /* (defined HAL_ENABLE_2D_IMGPROC) && (HAL_ENABLE_GFX_DEV_GPU == 1) */
int HAL_GfxDev_GPU_Register(gfx_dev_t *dev)
{
    HAL_LOGE("GPU not enabled\n");
    return -1;
}
#endif /* (defined HAL_ENABLE_2D_IMGPROC) && (HAL_ENABLE_GFX_DEV_GPU == 1) */
