/*
 * Copyright 2025-2026 NXP.
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

#include <stdio.h>
#include <string.h>

#include "app.h"
#include "mpp_debug.h"
#include "hal_camera_shared.h"
#include "mpp_config.h"

#ifdef RPMSG_USED
#include "rpmsg_lite.h"
#include "rpmsg_queue.h"
#include "rpmsg_ns.h"
#endif /* RPMSG_USED */

#ifdef MCMGR_USED
#include "mcmgr.h"
#endif /* MCMGR_USED */

#if (defined HAL_ENABLE_MULTICORE) && (HAL_ENABLE_MULTICORE == 1)
#include "hal_mc.h"
#endif /*(defined HAL_ENABLE_MULTICORE) && (HAL_ENABLE_MULTICORE == 1)*/

/* Multicore manager (MCMGR) and RPMSG configurations */
#define RPMSG_READY_EVENT_DATA (1U)
#define SH_MEM_TOTAL_SIZE          (6144U)

#ifdef MCMGR_USED
static void RPMsgRemoteReadyEventHandler(mcmgr_core_t coreNum, uint16_t eventData, void *context);
static volatile uint16_t RPMsgRemoteReadyEventData = 0U;
#if defined(__ICCARM__) /* IAR Workbench */
#pragma location = "rpmsg_sh_mem_section"
static char rpmsg_lite_base[SH_MEM_TOTAL_SIZE];
#elif defined(__CC_ARM) || defined(__ARMCC_VERSION) /* Keil MDK */
static char rpmsg_lite_base[SH_MEM_TOTAL_SIZE] __attribute__((section("rpmsg_sh_mem_section")));
#elif defined(__GNUC__)
static char rpmsg_lite_base[SH_MEM_TOTAL_SIZE] __attribute__((section(".noinit.$rpmsg_sh_mem")));
#else
#error "RPMsg: Please provide your definition of rpmsg_lite_base[]!"
#endif
#endif /* MCMGR_USED */

#ifdef BOOT_SECONDARY_CORE
#ifdef MCMGR_USED
static void RPMsgRemoteReadyEventHandler(mcmgr_core_t coreNum, uint16_t eventData, void *context)
{
    uint16_t *data = (uint16_t *)context;

    *data = eventData;
}

#endif /* MCMGR_USED */

volatile uint16_t *mpp_boot_secondary_core(void)
{
    /* Boot CORE 1 */
#ifdef CORE1_IMAGE_COPY_TO_RAM
    /* This section ensures the secondary core image is copied from flash location to the target RAM memory.
       It consists of several steps: image size calculation and image copying.
       These steps are not required on MCUXpresso IDE which copies the secondary core image to the target memory during
       startup automatically. */
    uint32_t core1_image_size;
    core1_image_size = get_core1_image_size();
    MPP_LOGI("Copy CORE1 image to address: 0x%x, size: %d from address: 0x%x\r\n", (void *)(char *)CORE1_BOOT_ADDRESS,
                 core1_image_size, CORE1_IMAGE_START);

    /* Copy application from FLASH to RAM */
    (void)memcpy((void *)(char *)CORE1_BOOT_ADDRESS, (void *)CORE1_IMAGE_START, core1_image_size);

#ifdef APP_INVALIDATE_CACHE_FOR_SECONDARY_CORE_IMAGE_MEMORY
    invalidate_cache_for_core1_image_memory(CORE1_BOOT_ADDRESS, core1_image_size);
#endif /* APP_INVALIDATE_CACHE_FOR_SECONDARY_CORE_IMAGE_MEMORY*/
#endif /* CORE1_IMAGE_COPY_TO_RAM */

#ifdef MCMGR_USED
    /* Initialize MCMGR before calling its API */
    (void)MCMGR_Init();

    /* Register the application event before starting the secondary core */
    (void)MCMGR_RegisterEvent(kMCMGR_RemoteApplicationEvent, RPMsgRemoteReadyEventHandler,
                              (void *)&RPMsgRemoteReadyEventData);

#if (defined HAL_ENABLE_MULTICORE) && (HAL_ENABLE_MULTICORE == 1)
#if defined(MCMGR_REMOTE_APP_EVENT_COUNT) && (MCMGR_REMOTE_APP_EVENT_COUNT >= 2)
    /* Register the application event for communication between elements on different cores 
     * We need to register the event before booting the second core to ensure proper synchronization */
    (void)MCMGR_RegisterEvent(kMCMGR_RemoteApplicationEvent1, hal_mc_rpmsg_remote_ev_handler, NULL);
#else
    #error "MCMGR_REMOTE_APP_EVENT_COUNT is less than 2. Cannot initialize multicore resources."
#endif
#endif /*(defined HAL_ENABLE_MULTICORE) && (HAL_ENABLE_MULTICORE == 1)*/

    /* Boot Secondary core application */
    (void)MCMGR_StartCore(kMCMGR_Core1, (void *)(char *)CORE1_BOOT_ADDRESS, (uint32_t)rpmsg_lite_base,
                          kMCMGR_Start_Synchronous);

    /* Wait until the secondary core application signals the rpmsg remote has been initialized and is ready to
     * communicate. */
     while (RPMSG_READY_EVENT_DATA != RPMsgRemoteReadyEventData){};
     MPP_LOGI("Secondary core is ready...\r\n");

     return &RPMsgRemoteReadyEventData;
#else
    return NULL;
#endif /* MCMGR_USED */
}

struct rpmsg_lite_instance *mpp_secondary_core_rpmsg_init(void)
{
    struct rpmsg_lite_instance *rpmsg_inst = NULL;
#ifdef MCMGR_USED
    uint32_t startupData;
    volatile mcmgr_status_t status;

    /* Get the startup data */
    do
    {
        status = MCMGR_GetStartupData(kMCMGR_Core0, &startupData);
    } while (status != kStatus_MCMGR_Success);

    rpmsg_inst = rpmsg_lite_remote_init((void *)(char *)(platform_patova(startupData)), RPMSG_LITE_LINK_ID, RL_NO_FLAGS);
    if (rpmsg_inst == NULL)
    {
        (void)MPP_LOGE("Failed to initialize rpmsg...\r\n");
        return NULL;
    }

#if (defined HAL_ENABLE_MULTICORE) && (HAL_ENABLE_MULTICORE == 1)
#if defined(MCMGR_REMOTE_APP_EVENT_COUNT) && (MCMGR_REMOTE_APP_EVENT_COUNT >= 2)
    /* Register the application event for communication between elements on different cores 
     * We need to register the event before booting the second core to ensure proper synchronization */
    (void)MCMGR_RegisterEvent(kMCMGR_RemoteApplicationEvent1, hal_mc_rpmsg_remote_ev_handler, NULL);
#else
    #error "MCMGR_REMOTE_APP_EVENT_COUNT is less than 2. Cannot initialize multicore resources."
#endif
#endif /*(defined HAL_ENABLE_MULTICORE) && (HAL_ENABLE_MULTICORE == 1)*/

    /* Signal the other core we are ready by triggering the event and passing the RPMSG_READY_EVENT_DATA */
    (void)MCMGR_TriggerEvent(kMCMGR_Core0, kMCMGR_RemoteApplicationEvent, RPMSG_READY_EVENT_DATA);
#else
    MPP_LOGI("RPMSG Share Base Addr is 0x%x\r\n", RPMSG_LITE_SHMEM_BASE);
    rpmsg_inst = rpmsg_lite_remote_init((void *)RPMSG_LITE_SHMEM_BASE, RPMSG_LITE_LINK_ID, RL_NO_FLAGS);
    if (rpmsg_inst == NULL)
    {
        MPP_LOGE("Failed to initialize rpmsg...\r\n");
        return NULL;
    }
#endif /* MCMGR_USED */
    return rpmsg_inst;
}
#endif /* BOOT_SECONDARY_CORE */

#ifdef RPMSG_USED
void *mpp_init_rpmsg(void)
{
    void *rpmsg_inst;
#ifdef MCMGR_USED
    rpmsg_inst = rpmsg_lite_master_init(rpmsg_lite_base,
                                   SH_MEM_TOTAL_SIZE,
                                   RPMSG_LITE_LINK_ID,
                                   RL_NO_FLAGS);
#else
    rpmsg_inst = rpmsg_lite_master_init((void *)RPMSG_LITE_SHMEM_BASE,
                                           SH_MEM_TOTAL_SIZE,
                                           RPMSG_LITE_LINK_ID,
                                           RL_NO_FLAGS);
#endif

    return rpmsg_inst;
}
#endif /* RPMSG_USED */
