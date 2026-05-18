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

#endif /* _HAL_STORAGE_H_ */
