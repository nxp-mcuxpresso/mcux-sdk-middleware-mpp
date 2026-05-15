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
#include "hal_filesrc.h"
#include "hal_debug.h"
#include "mpp_config.h"

#ifdef HAL_ENABLE_STORAGE

#include "hal_storage.h"
#include "sdmmc_config.h"
#include "fsl_sd_disk.h"

/*******************************************************************************
  * Definitions
  ******************************************************************************/

/*******************************************************************************
  * Variables
  ******************************************************************************/

AT_NONCACHEABLE_SECTION(static FATFS g_fileSystem); /* File system object */
static bool s_storage_mounted = false;

/*******************************************************************************
  * Code
  ******************************************************************************/

static status_t storage_wait_for_card(void)
{
    BOARD_SD_Config(&g_sd, NULL, BOARD_SDMMC_SD_HOST_IRQ_PRIORITY, NULL);

    if (SD_HostInit(&g_sd) != kStatus_Success)
    {
        return kStatus_Fail;
    }

    SD_SetCardPower(&g_sd, false);

    if (SD_PollingCardInsert(&g_sd, kSD_Inserted) == kStatus_Success)
    {
        SD_SetCardPower(&g_sd, true);
    }
    else
    {
        return kStatus_Fail;
    }

    return kStatus_Success;
}

int hal_storage_init(void)
{
    if (s_storage_mounted)
        return 0;

    const TCHAR volumePath[3U] = {SDDISK + '0', ':', '/'};

    memset((void *)&g_fileSystem, 0, sizeof(g_fileSystem));

    if (storage_wait_for_card() != kStatus_Success)
        return -1;

    if (f_mount(&g_fileSystem, volumePath, 1))
        return -2;

#if (FF_FS_RPATH >= 2U)
    if (f_chdrive((char const *)&volumePath[0U]))
        return -3;
#endif

    s_storage_mounted = true;
    return 0;
}

bool hal_storage_is_mounted(void)
{
    return s_storage_mounted;
}

hal_storage_status_t hal_storage_get_free_space(uint64_t *free_bytes, uint64_t *total_bytes)
{
    FATFS *fs;
    DWORD free_clust;
    FRESULT res;
    const TCHAR volumePath[3U] = {SDDISK + '0', ':', '/'};

    if (!s_storage_mounted)
    {
        HAL_LOGE("Storage not mounted\r\n");
        return kStatus_HAL_StorageNotMounted;
    }

    /* Get volume information and free clusters */
    res = f_getfree(volumePath, &free_clust, &fs);
    if (res != FR_OK)
    {
        HAL_LOGE("Failed to get free space, FRESULT=%d\r\n", res);
        return kStatus_HAL_StorageFSError;
    }

    /* Calculate free space in bytes */
    if (free_bytes != NULL)
    {
#if FF_MAX_SS != FF_MIN_SS
        *free_bytes = (uint64_t)free_clust * fs->csize * fs->ssize;
#else
        *free_bytes = (uint64_t)free_clust * fs->csize * FF_MAX_SS;
#endif
    }

    /* Calculate total space in bytes */
    if (total_bytes != NULL)
    {
#if FF_MAX_SS != FF_MIN_SS
        *total_bytes = (uint64_t)(fs->n_fatent - 2) * fs->csize * fs->ssize;
#else
        *total_bytes = (uint64_t)(fs->n_fatent - 2) * fs->csize * FF_MAX_SS;
#endif
    }

    return kStatus_HAL_StorageSuccess;
}

#endif /* HAL_ENABLE_STORAGE */