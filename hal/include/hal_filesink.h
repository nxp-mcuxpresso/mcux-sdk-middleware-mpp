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

#ifndef _HAL_FILESINK_H_
#define _HAL_FILESINK_H_

/**
 * @file hal_filesink.h
 * @brief HAL File Sink API for writing files.
 * 
 * This module provides a file sink abstraction for the MPP framework,
 * enabling writing of data to files.
 */

/*! \addtogroup HAL_TYPES
 *  @{
 */

#include "mpp_api_types.h"
#include "hal_types.h"

#define FILE_SINK_FILEPATH_MAX_LEN 128

typedef struct _filesink filesink_t;

/** @brief File sink return status. */
typedef enum _hal_filesink_status
{
    MPP_kStatus_HAL_FileSinkSuccess = 0,  /*!< Successfully */
    MPP_kStatus_HAL_FileSinkError         /*!< Error occurs on HAL filesink */
} hal_filesink_status_t;

/*! \addtogroup HAL_OPERATIONS
 *  @{
 */

/** @brief Operation that needs to be implemented by a file sink element */
typedef struct _filesink_operator
{
    hal_filesink_status_t (*init)(filesink_t *dev, mpp_filesink_params_t *config, void *param); /*!< initialize the dev */
    hal_filesink_status_t (*deinit)(filesink_t *dev); /*!< deinitialize the dev */
    hal_filesink_status_t (*start)(const filesink_t *dev); /*!< start the dev */
    hal_filesink_status_t (*stop)(const filesink_t *dev);  /*!< stop the dev */
    hal_filesink_status_t (*enqueue)(const filesink_t *dev, void *data, uint32_t size); /*!< enqueue a buffer */
    hal_filesink_status_t (*get_buf_desc)(const filesink_t *dev, hw_buf_desc_t *in_buf, mpp_memory_policy_t *policy); /*!< get buffer descriptors and policy */
} filesink_operator_t;

/** @} */

/*! \addtogroup HAL_TYPES
 *  @{
 */

/** @brief Structure that characterize the file sink element. */
typedef struct
{
    char filepath[FILE_SINK_FILEPATH_MAX_LEN];    /*!< File name to write */
    bool append_mode;                             /*!< Flag to enable append mode */
} filesink_config_t;

/** @brief Attributes of a file sink element. */
struct _filesink
{
    const filesink_operator_t *ops;                /*!< operations */
    filesink_config_t config;                      /*!< file sink configs */
};

/** @} */

int setup_filesink(filesink_t *dev);

#endif /*_HAL_FILESINK_H_*/