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

#ifndef _HAL_FILESRC_H_
#define _HAL_FILESRC_H_

/**
 * @file hal_filesrc.h
 * @brief HAL File Source API for reading files from SD card
 * 
 * This module provides a file source abstraction for the MPP framework,
 * enabling reading of files from SD card.
 */

/*! \addtogroup HAL_TYPES
 *  @{
 */

#include "mpp_api_types.h"
#include "hal_types.h"

#define FILE_SOURCE_FILEPATH_MAX_LEN 128

/**
 * @brief Search function for H.264 NALU boundaries
 * 
 * This function searches for H.264 Network Abstraction Layer Unit (NALU)
 * start codes in the provided data buffer. It looks for:
 * - 4-byte start code: 0x00 0x00 0x00 0x01
 * - 3-byte start code: 0x00 0x00 0x01
 * 
 * @param[in] data Pointer to buffer containing H.264 bitstream data
 * @param[in] len Length of data in buffer (bytes)
 * 
 * @return Offset to next NALU start code, or -1 if not found
 * 
 * @note This function is provided as a reference implementation.
 *       Users can provide custom slice search functions for other formats.
 */
int search_h264_nalu(const uint8_t *data, int32_t len);

typedef struct _filesrc filesrc_t;

/** @brief File source return status. */
typedef enum _hal_filesrc_status
{
    MPP_kStatus_HAL_FileSrcSuccess = 0,  /*!< Successfully */
	MPP_kStatus_HAL_FileSrcError,        /*!< Error occurs on HAL filesrc */
    MPP_kStatus_HAL_FileSrcEOF           /*!< End of file reached */

} hal_filesrc_status_t;

/*! \addtogroup HAL_OPERATIONS
 *  @{
 */

/** @brief Operation that needs to be implemented by a file source element */
typedef struct _filesrc_operator
{
    hal_filesrc_status_t (*init)(filesrc_t *dev, mpp_filesrc_params_t *config, void *param); /*!< initialize the dev */
    hal_filesrc_status_t (*deinit)(filesrc_t *dev); /*!< deinitialize the dev */
    hal_filesrc_status_t (*start)(const filesrc_t *dev); /*!< start the dev */
    hal_filesrc_status_t (*stop)(const filesrc_t *dev);  /*!< stop the dev */
    hal_filesrc_status_t (*dequeue)(const filesrc_t *dev, void **data, uint32_t *size); /*!< dequeue a buffer */
    hal_filesrc_status_t (*get_buf_desc)(const filesrc_t *dev, hw_buf_desc_t *out_buf, mpp_memory_policy_t *policy); /*!< get buffer descriptors and policy */
} filesrc_operator_t;

/** @} */

/*! \addtogroup HAL_TYPES
 *  @{
 */

/** @brief Structure that characterize the file source element. */
typedef struct
{
    char filepath[FILE_SOURCE_FILEPATH_MAX_LEN];    /*!< File name to read */
    bool loop_enabled;                              /*!< Flag to enable looping the file */
    int file_buffer_size;                           /*!< File buffer size in bytes */
    slice_search_func_t slice_search_func;          /*!< Optional slice search function */
} filesrc_config_t;

/** @brief Attributes of a file source element. */
struct _filesrc
{
    const filesrc_operator_t *ops;                  /*!< operations */
    filesrc_config_t config;                        /*!< file source configs */
    uint8_t *buffer;                                /*!< Pointer to file read buffer */
    uint32_t buffer_size;                           /*!< File read buffer size */
};

/** @} */

#endif /*_HAL_FILESRC_H_*/
