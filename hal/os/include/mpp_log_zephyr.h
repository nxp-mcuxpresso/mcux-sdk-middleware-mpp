/*
 * Copyright 2026 NXP.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MPP_LOG_ZEPHYR_H
#define MPP_LOG_ZEPHYR_H

#include "hal_os_config.h"

#if MPP_OS_ZEPHYR
#include <zephyr/logging/log.h>

#ifdef CONFIG_MPP_LOG_LEVEL
#define MPP_LOG_LEVEL CONFIG_MPP_LOG_LEVEL
#else
#define MPP_LOG_LEVEL LOG_LEVEL_INF
#endif

/*
 * Logging module setup with protection against multiple declarations:
 * - ONE file (mpp_api.c) should define MPP_LOG_MODULE_REGISTER before including
 * this
 * - All other files get LOG_MODULE_DECLARE, but only ONCE per compilation unit
 */
#ifdef MPP_LOG_MODULE_REGISTER
/* This file registers the logging module (only mpp_api.c) */
#ifndef MPP_LOG_MODULE_REGISTERED
#define MPP_LOG_MODULE_REGISTERED
LOG_MODULE_REGISTER(mpp, MPP_LOG_LEVEL);
#endif
#else
/* All other files declare the logging module, but only once per .c file */
#ifndef MPP_LOG_MODULE_DECLARED
#define MPP_LOG_MODULE_DECLARED
LOG_MODULE_DECLARE(mpp, MPP_LOG_LEVEL);
#endif
#endif

#endif /* MPP_OS_ZEPHYR */
#endif /* MPP_LOG_ZEPHYR_H */
