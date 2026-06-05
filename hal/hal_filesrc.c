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

#include <string.h>
#include <limits.h>
#include <assert.h>
#include "hal.h"
#include "hal_utils.h"
#include "hal_filesrc.h"
#include "mpp_config.h"

#include "FreeRTOS.h"

#ifdef HAL_ENABLE_FILE_SOURCE

#include "ff.h"
#include "fsl_common.h"

/*******************************************************************************
  * Definitions
  ******************************************************************************/

/* File source context */
typedef struct {
    filesrc_t *dev;
    bool file_opened;           /*!< Flag indicating file is already opened */
    bool start_of_file;         /*!< Flag indicating start of file */
    bool end_of_file;           /*!< Flag indicating end of file */
    uint32_t remaining_bytes;   /*!< Bytes remaining from previous read */
    uint32_t slice_offset;      /*!< Current offset in buffer for slice search */
    uint32_t bytes_in_buffer;   /*!< Total valid bytes currently in the buffer */
} filesrc_context_t;

/*******************************************************************************
  * Variables
  ******************************************************************************/

AT_NONCACHEABLE_SECTION(static FIL fil);
static filesrc_context_t s_filesrc_ctx = {0};

/*******************************************************************************
  * Code
  ******************************************************************************/

int search_h264_nalu(const uint8_t *data, int32_t len)
{
    int i;

    if (len < 5)
    {
        return -1;
    }

    /* parse NALU 00 00 00 01 or 00 00 01 */
    for (i = 1; i < len - 4; i++)
    {
        if ((data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 0 && data[i + 3] == 1) ||
            (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1))
        {
            /* Skip slices too small to contain a start code + NALU header */
            if (i < 4)
            {
                continue;
            }
            return i;
        }
    }

    if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1)
    {

        if (i >= 4)
        {
            return i;
        }
        return -1;
    }

    return -1;
}

/**
  * @brief Try to find the next valid slice in the buffered data.
  *
  * @param dev  File source device
  * @param ctx  File source context
  * @param data [out] Pointer to the slice data
  * @param size [out] Size of the slice
  *
  * @return true if a valid slice was found, false otherwise
  */
static bool filesrc_find_next_slice(const filesrc_t *dev, filesrc_context_t *ctx,
                                    void **data, uint32_t *size)
{
    if (dev == NULL || ctx == NULL || data == NULL || size == NULL)
    {
        HAL_LOGE("Invalid parameters\r\n");
        return false;
    }

    while (ctx->slice_offset < ctx->bytes_in_buffer)
    {
        int32_t slice_size = dev->config.slice_search_func(
            dev->buffer + ctx->slice_offset,
            ctx->bytes_in_buffer - ctx->slice_offset);

        if (slice_size < 0)
        {
            /* No more slices found in current buffer */
            break;
        }
        else
        {
            *data = dev->buffer + ctx->slice_offset;
            *size = slice_size;
            ctx->slice_offset += slice_size;
            return true;
        }
    }

    /* Update remaining bytes for next read */
    ctx->remaining_bytes = ctx->bytes_in_buffer - ctx->slice_offset;
    return false;
}

/**
  * @brief Read the next chunk of data from the file into the buffer.
  *
  * @param dev  File source device
  * @param ctx  File source context
  *
  * @return MPP_kStatus_HAL_FileSrcSuccess on success, MPP_kStatus_HAL_FileSrcError on failure
  */
static hal_filesrc_status_t filesrc_read_next_chunk(const filesrc_t *dev, filesrc_context_t *ctx)
{
    FRESULT error;
    UINT bytes_read = 0;

    if (dev == NULL || ctx == NULL)
    {
        HAL_LOGE("Invalid parameters\r\n");
        return MPP_kStatus_HAL_FileSrcError;
    }

    bool use_slice_search = (dev->config.slice_search_func != NULL);

    /* Move remaining bytes from previous read to the beginning of the buffer */
    if (ctx->remaining_bytes > 0 && use_slice_search)
    {
        if (ctx->remaining_bytes > dev->buffer_size)
        {
            HAL_LOGE("Remaining bytes exceed buffer size\r\n");
            ctx->remaining_bytes = dev->buffer_size;
        }
        memmove(dev->buffer,
                dev->buffer + (ctx->bytes_in_buffer - ctx->remaining_bytes),
                ctx->remaining_bytes);
    }

    /* Read data into the buffer after any remaining bytes */
    uint32_t read_offset = use_slice_search ? ctx->remaining_bytes : 0;
    uint32_t read_size = dev->buffer_size - read_offset;

    error = f_read(&fil, dev->buffer + read_offset, read_size, &bytes_read);
    if (error != FR_OK)
    {
        HAL_LOGE("File read error\r\n");
        f_close(&fil);
        ctx->file_opened = false;
        return MPP_kStatus_HAL_FileSrcError;
    }

    if (use_slice_search)
    {
        ctx->bytes_in_buffer = bytes_read + ctx->remaining_bytes;
        ctx->end_of_file = (dev->buffer_size > ctx->bytes_in_buffer);
        ctx->slice_offset = 0;
    }
    else
    {
        ctx->bytes_in_buffer = bytes_read;
        ctx->end_of_file = (bytes_read < read_size);
    }

    ctx->remaining_bytes = 0;

    return MPP_kStatus_HAL_FileSrcSuccess;
}

/**
  * @brief Open the file and reset context state.
  *
  * @param dev  File source device
  * @param ctx  File source context
  *
  * @return MPP_kStatus_HAL_FileSrcSuccess on success, MPP_kStatus_HAL_FileSrcError on failure
  */
static hal_filesrc_status_t filesrc_open_file(const filesrc_t *dev, filesrc_context_t *ctx)
{
    if (dev == NULL || ctx == NULL)
    {
        HAL_LOGE("Invalid parameters\r\n");
        return MPP_kStatus_HAL_FileSrcError;
    }

    FRESULT error = f_open(&fil, dev->config.filepath, FA_READ | FA_OPEN_EXISTING);
    if (error != FR_OK)
    {
        HAL_LOGE("Could not open %s\r\n", dev->config.filepath);
        return MPP_kStatus_HAL_FileSrcError;
    }

    ctx->file_opened = true;
    ctx->start_of_file = true;
    ctx->end_of_file = false;
    ctx->remaining_bytes = 0;
    ctx->slice_offset = 0;
    ctx->bytes_in_buffer = 0;
    HAL_LOGI("Start to read file %s.\r\n", dev->config.filepath);

    return MPP_kStatus_HAL_FileSrcSuccess;
}

/**
  * @brief Initialize the file source device
  */
hal_filesrc_status_t HAL_FileSrc_Init(filesrc_t *dev, mpp_filesrc_params_t *config, void *param)
{
    HAL_LOGD("++HAL_FileSrc_Init\n");

    dev->buffer = param;
    dev->buffer_size = config->file_buffer_size;

    if (config->filepath == NULL || strlen(config->filepath) == 0)
    {
        HAL_LOGE("File path not provided in configuration\r\n");
        return MPP_kStatus_HAL_FileSrcError;
    }

    s_filesrc_ctx.file_opened = false;

    strncpy(dev->config.filepath, config->filepath, FILE_SOURCE_FILEPATH_MAX_LEN - 1);
    dev->config.filepath[FILE_SOURCE_FILEPATH_MAX_LEN - 1] = '\0';
    dev->config.loop_enabled = config->loop;
    dev->config.file_buffer_size = config->file_buffer_size;
    dev->config.slice_search_func = config->slice_search_func;

    s_filesrc_ctx.dev = dev;

    HAL_LOGD("--HAL_FileSrc_Init\n");
    return MPP_kStatus_HAL_FileSrcSuccess;
}

/**
  * @brief Deinitialize the file source device
  */
hal_filesrc_status_t HAL_FileSrc_Deinit(filesrc_t *dev)
{
    HAL_LOGD("++HAL_FileSrc_Deinit\n");

    if (s_filesrc_ctx.file_opened)
    {
        f_close(&fil);
        s_filesrc_ctx.file_opened = false;
    }

    memset(&s_filesrc_ctx, 0, sizeof(s_filesrc_ctx));

    HAL_LOGD("--HAL_FileSrc_Deinit\n");
    return MPP_kStatus_HAL_FileSrcSuccess;
}

/**
  * @brief Start the file source
  */
hal_filesrc_status_t HAL_FileSrc_Start(const filesrc_t *dev)
{
    HAL_LOGD("++HAL_FileSrc_Start\n");

    HAL_LOGD("--HAL_FileSrc_Start\n");
    return MPP_kStatus_HAL_FileSrcSuccess;
}

/**
  * @brief Stop the file source
  */
hal_filesrc_status_t HAL_FileSrc_Stop(const filesrc_t *dev)
{
    HAL_LOGD("++HAL_FileSrc_Stop\n");

    if (s_filesrc_ctx.file_opened)
    {
        f_close(&fil);
        s_filesrc_ctx.file_opened = false;
    }
    
    /* Reset EOF state to allow restart */
    s_filesrc_ctx.end_of_file = false;
    s_filesrc_ctx.bytes_in_buffer = 0;
    s_filesrc_ctx.remaining_bytes = 0;
    s_filesrc_ctx.slice_offset = 0;

    HAL_LOGD("--HAL_FileSrc_Stop\n");
    return MPP_kStatus_HAL_FileSrcSuccess;
}

/**
  * @brief Dequeue the next buffer/slice for processing.
  */
hal_filesrc_status_t HAL_FileSrc_Dequeue(const filesrc_t *dev, void **data, uint32_t *size)
{
    filesrc_context_t *ctx = &s_filesrc_ctx;

    if (dev == NULL)
    {
        HAL_LOGE("Invalid parameter\r\n");
        return MPP_kStatus_HAL_FileSrcError;
    }
    
    bool use_slice_search = (dev->config.slice_search_func != NULL);

    HAL_LOGD("++HAL_FileSrc_Dequeue\n");

    /* In slice mode, try to return the next slice from already buffered data */
    if (use_slice_search && ctx->bytes_in_buffer > 0)
    {
        if (filesrc_find_next_slice(dev, ctx, data, size))
        {
            HAL_LOGD("--HAL_FileSrc_Dequeue (buffered slice)\n");
            return MPP_kStatus_HAL_FileSrcSuccess;
        }
    }

    /* Main read loop — read from file until we have data to return */
    while (1)
    {
        /* Open file if needed */
        if (!ctx->file_opened)
        {
            if (ctx->end_of_file && !dev->config.loop_enabled)
            {
                HAL_LOGD("EOF already reached, returning EOF again\r\n");
                *data = NULL;
                *size = 0;
                return MPP_kStatus_HAL_FileSrcEOF;
            }

            if (filesrc_open_file(dev, ctx) != MPP_kStatus_HAL_FileSrcSuccess)
            {
                return MPP_kStatus_HAL_FileSrcError;
            }
        }

        /* Read next chunk from file */
        if (filesrc_read_next_chunk(dev, ctx) != MPP_kStatus_HAL_FileSrcSuccess)
        {
            return MPP_kStatus_HAL_FileSrcError;
        }

        if (use_slice_search)
        {
            /* Try to find a valid slice in the freshly read data */
            if (filesrc_find_next_slice(dev, ctx, data, size))
            {
                ctx->start_of_file = false;
                HAL_LOGD("--HAL_FileSrc_Dequeue (new slice)\n");
                return MPP_kStatus_HAL_FileSrcSuccess;
            }
        }
        else
        {
            /* Whole buffer mode — return entire read buffer */
            if (ctx->bytes_in_buffer > 0)
            {
                *data = dev->buffer;
                *size = ctx->bytes_in_buffer;
                ctx->start_of_file = false;
                HAL_LOGD("--HAL_FileSrc_Dequeue (whole buffer)\n");
                return MPP_kStatus_HAL_FileSrcSuccess;
            }
        }

        ctx->start_of_file = false;

        /* Handle end-of-file */
        if (ctx->end_of_file)
        {
            f_close(&fil);
            ctx->file_opened = false;

            if (dev->config.loop_enabled)
            {
                /* Clear buffer and restart from beginning */
                memset(dev->buffer, 0, dev->buffer_size);
                ctx->remaining_bytes = 0;
                ctx->bytes_in_buffer = 0;
                continue;
            }
            else
            {
                /* Signal end of file to caller */
                *data = NULL;
                *size = 0;
                HAL_LOGI("End of file reached\n");
                HAL_LOGD("--HAL_FileSrc_Dequeue (EOF)\n");
                return MPP_kStatus_HAL_FileSrcEOF;
            }
        }
    }
}

/**
  * @brief Communicate buffer requirements
  */
hal_filesrc_status_t HAL_FileSrc_Getbufdesc(const filesrc_t *dev, hw_buf_desc_t *out_buf, mpp_memory_policy_t *policy)
{
    hal_filesrc_status_t ret = MPP_kStatus_HAL_FileSrcSuccess;
    HAL_LOGD("++HAL_FileSrc_Getbufdesc(out_buf=[%p])\r\n", out_buf);

    if ((out_buf == NULL) || (policy == NULL))
    {
        HAL_LOGE("NULL pointer to buffer descriptor\r\n");
        return MPP_kStatus_HAL_FileSrcError;
    }

    /* set memory policy */
    *policy = HAL_MEM_ALLOC_NONE;
    out_buf->alignment = 0;
    out_buf->cacheable = false; /* TODO check cacheability */
    out_buf->stride = 0;
    out_buf->addr = NULL;

    HAL_LOGD("--HAL_FileSrc_Getbufdesc\r\n");
    return ret;
}

const static filesrc_operator_t filesrc_ops = {
    .init         = HAL_FileSrc_Init,
    .deinit       = HAL_FileSrc_Deinit,
    .start        = HAL_FileSrc_Start,
    .stop         = HAL_FileSrc_Stop,
    .dequeue      = HAL_FileSrc_Dequeue,
    .get_buf_desc = HAL_FileSrc_Getbufdesc,
};

int setup_filesrc(filesrc_t *dev)
{
    dev->ops = &filesrc_ops;
    return 0;
}

#else /* HAL_ENABLE_FILE_SOURCE */
int setup_filesrc(filesrc_t *dev)
{
    HAL_LOGE("File Source not enabled\r\n");
    return -1;
}
#endif /* HAL_ENABLE_FILE_SOURCE */