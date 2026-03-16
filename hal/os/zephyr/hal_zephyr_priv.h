/*
 * Copyright 2025-2026 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _HAL_ZEPHYR_PRIV_H
#define _HAL_ZEPHYR_PRIV_H

#include "hal_os_config.h"

#if MPP_OS_ZEPHYR

#include <zephyr/kernel.h>
#include <stdbool.h>
#include <stdint.h>
#include "mpp_api_types.h"
#include "mpp_config.h"

/* Zephyr-specific type definitions for HAL */

/* Task wrapper - needed to track stack allocation for cleanup */
typedef struct hal_zephyr_task_s {
    struct k_thread thread;         /*!< k_thread descriptor */
    k_tid_t tid;                    /*!< Thread ID */
    k_thread_stack_t *stack;        /*!< Pointer to allocated stack memory */
    size_t stack_size;              /*!< Size of allocated stack in bytes */
} hal_zephyr_task_t;

#endif /* MPP_OS_ZEPHYR */
#endif /* _HAL_ZEPHYR_PRIV_H */
