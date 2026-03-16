/*
 * Copyright 2022-2026 NXP
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
 *  */

/*
 * HAL debug functions public header
 */

#ifndef _HAL_DEBUG_H
#define _HAL_DEBUG_H

#include "hal_os.h"

#if MPP_OS_FREERTOS

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_LVL_ERR       0
#define LOG_LVL_INFO      1
#define LOG_LVL_DEBUG     2

#define LOG_DEFAULT_LEVEL LOG_LVL_ERR

/*
 * Implementation of LOG() should be external since
 * this is depending on SDK configuration, especially
 * regarding flag SDK_DEBUGCONSOLE.
 */
extern void LOGE(const char* module, const char* func, int line, const char* format, ...);
extern void LOGI(const char* module, const char* func, int line, const char* format, ...);
extern void LOGD(const char* module, const char* func, int line, const char* format, ...);

#define HAL_LOGE(format, ...) \
    LOGE("HAL", __func__, __LINE__, format, ##__VA_ARGS__)
#define HAL_LOGI(format, ...) \
    LOGI("HAL", __func__, __LINE__, format, ##__VA_ARGS__)
#define HAL_LOGD(format, ...) \
    LOGD("HAL", __func__, __LINE__, format, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#elif MPP_OS_ZEPHYR

/* Map MPP logging macros to Zephyr's logging infrastructure */
#define HAL_LOGE(format, ...) \
    LOG_ERR("[%s:%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define HAL_LOGI(format, ...) \
    LOG_INF("[%s:%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define HAL_LOGD(format, ...) \
    LOG_DBG("[%s:%d] " format, __func__, __LINE__, ##__VA_ARGS__)

#ifndef PRINTF
#define PRINTF printk
#endif

#else

#error "Logging macros need to be defined for the current OS"

#endif

#ifdef __cplusplus
extern "C" {
#endif

// Error handling macros
#define RETURN_ON_ERROR(condition, error_code, message, ...) \
    do { \
        if (condition) { \
            HAL_LOGE(message, ##__VA_ARGS__); \
            return error_code; \
        } \
    } while(0)

#define GOTO_ON_ERROR(condition, label, message, ...) \
    do { \
        if (condition) { \
            HAL_LOGE(message, ##__VA_ARGS__); \
            goto label; \
        } \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif /* _HAL_DEBUG_H */
