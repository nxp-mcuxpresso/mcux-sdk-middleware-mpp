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

#ifndef _HAL_STORAGE_H_
#define _HAL_STORAGE_H_

#include <stdbool.h>

/**
 * @brief Storage operation status codes
 */
typedef enum _hal_storage_status
{
    kStatus_HAL_StorageSuccess = 0,      /*!< Operation successful */
    kStatus_HAL_StorageNotMounted = -1,  /*!< Storage not mounted */
    kStatus_HAL_StorageError = -2,       /*!< General storage error */
    kStatus_HAL_StorageFSError = -3,     /*!< Filesystem operation error */
} hal_storage_status_t;

/**
 * @brief Initialize the storage backend (SD card + FAT filesystem).
 *
 * This function handles SD card detection, power cycling,
 * and FAT filesystem mounting. Must be called before using
 * any file source element.
 *
 * @return 0 on success, negative error code on failure
 */
int hal_storage_init(void);

/**
 * @brief Check if storage is mounted and ready.
 *
 * @return true if storage is mounted, false otherwise
 */
bool hal_storage_is_mounted(void);

/**
 * @brief Get free and total space on mounted SD card
 *
 * @param free_bytes Pointer to store free space in bytes (can be NULL)
 * @param total_bytes Pointer to store total space in bytes (can be NULL)
 * @return hal_storage_status_t
 *         kStatus_HAL_StorageSuccess: Operation successful
 *         kStatus_HAL_StorageNotMounted: Storage not mounted
 *         kStatus_HAL_StorageFSError: Failed to get free space
 */
hal_storage_status_t hal_storage_get_free_space(uint64_t *free_bytes, uint64_t *total_bytes);

#endif /* _HAL_STORAGE_H_ */
