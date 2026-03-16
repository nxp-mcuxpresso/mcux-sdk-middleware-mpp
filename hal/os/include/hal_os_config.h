/*
 * Copyright 2022-2026 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _HAL_OS_CONFIG_H
#define _HAL_OS_CONFIG_H

/* OS Selection check */
#if defined(CONFIG_MPP_OS_FREERTOS)
/* FreeRTOS */
#ifndef MPP_OS_FREERTOS
#define MPP_OS_FREERTOS 1
#endif
#elif defined(CONFIG_MPP_OS_ZEPHYR)
/* Zephyr OS */
#ifndef MPP_OS_ZEPHYR
#define MPP_OS_ZEPHYR 1
#endif
#else
#error Build error, choose one supported OS (CONFIG_MPP_OS_ZEPHYR or CONFIG_MPP_OS_FREERTOS)
#endif

#ifdef MPP_OS_FREERTOS
/* FreeRTOS specific includes */
#include <FreeRTOS.h>
#include <atomic.h>
#include <event_groups.h>
#include <queue.h>
#include <semphr.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <task.h>

/** precomputation of the OS tick period with no precision loss */
#define TICK_PERIOD_MS   ((1000*128 / configTICK_RATE_HZ) / 128)

/* period (us) of the high precision RunTime counter (= /10 OS tick) */
#ifndef HAL_TIMER_PRECISION_1_US
#define HAL_EXEC_TIMER_US (TICK_PERIOD_MS * 1000 / 10) /* precision 100us */
#else
#define HAL_EXEC_TIMER_US 1 /* precision 1us */
#endif

/* max number of tasks expected in the system */
#define HAL_MAX_TASKS 50

#endif /* MPP_OS_FREERTOS */

#ifdef MPP_OS_ZEPHYR
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/mutex.h>
#include <zephyr/sys/printk.h>

#define TICK_PERIOD_MS 1000
#define HAL_EXEC_TIMER_US 1 /* precision 1us */

/* max number of tasks expected in the system */
#define HAL_MAX_TASKS 50

#endif /* MPP_OS_ZEPHYR */

#endif /* _HAL_OS_CONFIG_H */
