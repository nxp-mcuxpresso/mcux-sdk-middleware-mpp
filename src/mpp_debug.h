/*
 * Copyright 2020-2025 NXP.
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

#ifndef _MPP_DEBUG_H
#define _MPP_DEBUG_H

#include "hal_os.h"
#include "hal_debug.h"
#include "mpp_api_types_internal.h"
#if MPP_OS_ZEPHYR
#include <zephyr/logging/log.h>
#endif

int tick_check_rate(uint32_t *last, int *curr, int max);

/* get string from element id */
char * elem_name(_elem_t *elem);

#if MPP_OS_FREERTOS
/* non-conditional logs */
#define MPP_LOGE(format, ...) \
    LOGE("MPP", __func__, __LINE__, format, ##__VA_ARGS__)
#define MPP_LOGI(format, ...) \
    LOGI("MPP", __func__, __LINE__, format, ##__VA_ARGS__)
#define MPP_LOGD(format, ...) \
    LOGD("MPP", __func__, __LINE__, format, ##__VA_ARGS__)
#else 
/* Map MPP logging macros to Zephyr's logging infrastructure */
#define MPP_LOGE(format, ...) \
    LOG_ERR("[%s:%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define MPP_LOGI(format, ...) \
    LOG_INF("[%s:%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define MPP_LOGD(format, ...) \
    LOG_DBG("[%s:%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#endif /* MPP_OS_FREERTOS */

/* conditional logs */
#define MPP_LOGE_IF(cond, format, ...)  \
    do { if(cond) MPP_LOGE(format, ##__VA_ARGS__); } while (0)
#define MPP_LOGI_IF(cond, format, ...)  \
    do { if(cond) MPP_LOGI(format, ##__VA_ARGS__); } while (0)
#define MPP_LOGD_IF(cond, format, ...)  \
    do { if(cond) MPP_LOGD(format, ##__VA_ARGS__); } while (0)

/* Rate limited routines */
#define RLMT_CHECK(mps)  ({				\
    static uint32_t last;                             \
    static int curr;                                    \
    bool enable = false;                                \
    if (tick_check_rate(&last, &curr, mps))               \
        enable = true;                                  \
    enable;                                             \
})

/* log depending on rate limitation */
#define RLMT_DEBUG(mps, format, ...)  do {                   \
    {                                                        \
        static uint32_t last;                              \
        static int curr;                                     \
        if (tick_check_rate(&last, &curr, mps))                \
            MPP_LOGD(format, ##__VA_ARGS__);                 \
    }                                                        \
} while (0)


#endif
