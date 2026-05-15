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
#include "hal_filesink.h"
#include "mpp_config.h"

#include "FreeRTOS.h"

#ifdef HAL_ENABLE_FILE_SINK

#include "ff.h"
#include "fsl_common.h"

/*******************************************************************************
  * Definitions
  ******************************************************************************/

/* File sink context */
typedef struct {
    filesink_t *dev;
    bool file_opened;           /*!< Flag indicating file is already opened */
    uint64_t available_space;   /*!< Available size in bytes */
} filesink_context_t;

/*******************************************************************************
  * Variables
  ******************************************************************************/

AT_NONCACHEABLE_SECTION(static FIL fil);
static filesink_context_t s_filesink_ctx = {0};

/*******************************************************************************
  * Code
  ******************************************************************************/

/**
  * @brief Open the file for writing.
  *
  * @param dev  File sink device
  * @param ctx  File sink context
  *
  * @return MPP_kStatus_HAL_FileSinkSuccess on success, MPP_kStatus_HAL_FileSinkError on failure
  */
static hal_filesink_status_t filesink_open_file(const filesink_t *dev, filesink_context_t *ctx)
{
    if (dev == NULL || ctx == NULL)
    {
        HAL_LOGE("Invalid parameters\r\n");
        return MPP_kStatus_HAL_FileSinkError;
    }

    BYTE mode = FA_WRITE;
    if (dev->config.append_mode)
    {
        mode |= FA_OPEN_APPEND;
    }
    else
    {
        mode |= FA_CREATE_ALWAYS;
    }

    FRESULT error = f_open(&fil, dev->config.filepath, mode);
    if (error != FR_OK)
    {
        HAL_LOGE("Could not open %s for writing\r\n", dev->config.filepath);
        return MPP_kStatus_HAL_FileSinkError;
    }

    ctx->file_opened = true;
    HAL_LOGI("Opened file %s for writing.\r\n", dev->config.filepath);

    return MPP_kStatus_HAL_FileSinkSuccess;
}

/**
  * @brief Initialize the file sink device
  */
hal_filesink_status_t HAL_FileSink_Init(filesink_t *dev, mpp_filesink_params_t *config, void *param)
{
    HAL_LOGD("++HAL_FileSink_Init\n");

    if (config->filepath == NULL || strlen(config->filepath) == 0)
    {
        HAL_LOGE("File path not provided in configuration\r\n");
        return MPP_kStatus_HAL_FileSinkError;
    }

    s_filesink_ctx.file_opened = false;

    strncpy(dev->config.filepath, config->filepath, FILE_SINK_FILEPATH_MAX_LEN - 1);
    dev->config.filepath[FILE_SINK_FILEPATH_MAX_LEN - 1] = '\0';
    dev->config.append_mode = config->append_mode;

    if (config->max_file_size == 0)
    {
        HAL_LOGE("Max file size must be greater than 0\r\n");
        return MPP_kStatus_HAL_FileSinkError;
    }
    s_filesink_ctx.available_space = config->max_file_size;

    s_filesink_ctx.dev = dev;

    HAL_LOGD("--HAL_FileSink_Init\n");
    return MPP_kStatus_HAL_FileSinkSuccess;
}

/**
  * @brief Deinitialize the file sink device
  */
hal_filesink_status_t HAL_FileSink_Deinit(filesink_t *dev)
{
    HAL_LOGD("++HAL_FileSink_Deinit\n");

    if (s_filesink_ctx.file_opened)
    {
        f_close(&fil);
        s_filesink_ctx.file_opened = false;
    }

    memset(&s_filesink_ctx, 0, sizeof(s_filesink_ctx));

    HAL_LOGD("--HAL_FileSink_Deinit\n");
    return MPP_kStatus_HAL_FileSinkSuccess;
}

/**
  * @brief Start the file sink
  */
hal_filesink_status_t HAL_FileSink_Start(const filesink_t *dev)
{
    HAL_LOGD("++HAL_FileSink_Start\n");

    filesink_context_t *ctx = &s_filesink_ctx;

    if (!ctx->file_opened)
    {
        if (filesink_open_file(dev, ctx) != MPP_kStatus_HAL_FileSinkSuccess)
        {
            return MPP_kStatus_HAL_FileSinkError;
        }
    }

    HAL_LOGD("--HAL_FileSink_Start\n");
    return MPP_kStatus_HAL_FileSinkSuccess;
}

/**
  * @brief Stop the file sink
  */
hal_filesink_status_t HAL_FileSink_Stop(const filesink_t *dev)
{
    HAL_LOGD("++HAL_FileSink_Stop\n");

    if (s_filesink_ctx.file_opened)
    {
        f_close(&fil);
        s_filesink_ctx.file_opened = false;
    }

    HAL_LOGD("--HAL_FileSink_Stop\n");
    return MPP_kStatus_HAL_FileSinkSuccess;
}

/**
  * @brief Enqueue a buffer for writing to file.
  */
hal_filesink_status_t HAL_FileSink_Enqueue(const filesink_t *dev, void *data, uint32_t size)
{
    filesink_context_t *ctx = &s_filesink_ctx;
    FRESULT error;
    UINT bytes_written = 0;

    HAL_LOGD("++HAL_FileSink_Enqueue (size=%u)\n", size);

    if (dev == NULL || data == NULL || size == 0)
    {
        HAL_LOGE("Invalid parameters\r\n");
        return MPP_kStatus_HAL_FileSinkError;
    }

    if (!ctx->file_opened)
    {
        HAL_LOGE("File not opened\r\n");
        return MPP_kStatus_HAL_FileSinkError;
    }

    /* Check the available space */
    if (ctx->available_space < (uint64_t) size)
    {
        HAL_LOGE("Max file size exceeded\r\n");
        return MPP_kStatus_HAL_FileSinkError;
    }

    error = f_write(&fil, data, size, &bytes_written);
    if (error != FR_OK || bytes_written != size)
    {
        HAL_LOGE("File write error (error=%d, written=%u/%u)\r\n", error, bytes_written, size);
        return MPP_kStatus_HAL_FileSinkError;
    }

    /* Sync to ensure data is written to disk */
    error = f_sync(&fil);
    if (error != FR_OK)
    {
        HAL_LOGE("File sync error\r\n");
        return MPP_kStatus_HAL_FileSinkError;
    }

    ctx->available_space -= size;

    HAL_LOGD("--HAL_FileSink_Enqueue (written=%u)\n", bytes_written);
    return MPP_kStatus_HAL_FileSinkSuccess;
}

/**
  * @brief Communicate buffer requirements
  */
hal_filesink_status_t HAL_FileSink_Getbufdesc(const filesink_t *dev, hw_buf_desc_t *in_buf, mpp_memory_policy_t *policy)
{
    hal_filesink_status_t ret = MPP_kStatus_HAL_FileSinkSuccess;
    HAL_LOGD("++HAL_FileSink_Getbufdesc(in_buf=[%p])\r\n", in_buf);

    if ((in_buf == NULL) || (policy == NULL))
    {
        HAL_LOGE("NULL pointer to buffer descriptor\r\n");
        return MPP_kStatus_HAL_FileSinkError;
    }

    /* set memory policy - user provides input buffer */
    *policy = HAL_MEM_ALLOC_INPUT;
    in_buf->alignment = 0;
    in_buf->cacheable = false;
    in_buf->stride = 0;
    in_buf->addr = NULL;

    HAL_LOGD("--HAL_FileSink_Getbufdesc\r\n");
    return ret;
}

const static filesink_operator_t filesink_ops = {
    .init         = HAL_FileSink_Init,
    .deinit       = HAL_FileSink_Deinit,
    .start        = HAL_FileSink_Start,
    .stop         = HAL_FileSink_Stop,
    .enqueue      = HAL_FileSink_Enqueue,
    .get_buf_desc = HAL_FileSink_Getbufdesc,
};

int setup_filesink(filesink_t *dev)
{
    dev->ops = &filesink_ops;
    return 0;
}

#else /* HAL_ENABLE_FILE_SINK */
int setup_filesink(filesink_t *dev)
{
    HAL_LOGE("File Sink not enabled\r\n");
    return -1;
}
#endif /* HAL_ENABLE_FILE_SINK */
