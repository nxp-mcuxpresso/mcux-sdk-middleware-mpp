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

#endif /* HAL_ENABLE_STORAGE */