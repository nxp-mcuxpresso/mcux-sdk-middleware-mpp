/*
 * Copyright 2022-2026 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * HAL OS public header
 */

#ifndef _HAL_OS_H
#define _HAL_OS_H

#include <stdbool.h>
#include <stdint.h>
#include "hal_os_config.h"

#if MPP_OS_FREERTOS
/* FreeRTOS specific includes */

typedef SemaphoreHandle_t hal_mutex_t; /*!< mutex handle */
typedef SemaphoreHandle_t hal_sema_t; /*!< semaphore handle */

typedef void * hal_task_t;                  /*!< task handle */
typedef void * hal_event_group_t;           /*!< event group handle */

#elif MPP_OS_ZEPHYR
/* Zephyr specific includes */
#include "mpp_log_zephyr.h"

typedef struct k_mutex * hal_mutex_t; /*!< mutex handle */
typedef struct k_sem * hal_sema_t; /*!< semaphore handle */
struct hal_zephyr_task_s; /* opaque struct - forward declaration */
typedef struct hal_zephyr_task_s * hal_task_t; /*!< task handle */
typedef struct k_event *hal_event_group_t;           /*!< event group handle */

#endif /* MPP_OS_FREERTOS */

typedef void (* hal_task_fct_t)( void * );  /*!< task entry-point function */
typedef uint32_t hal_eventbits_t;           /*!< event bits */
/** @brief HAL context structure used to save and restore interrupt state. */
typedef struct hal_ctx
{
    unsigned int key;  /*!< saved interrupt key */
    bool active;       /*!< context is active */
    bool in_isr;       /*!< executing inside an ISR */
} hal_ctx_t;

#define HAL_MAX_TIMEOUT 0xffffffff          /*!< maximum timeout */

/*! @brief create mutex */
int hal_mutex_create (hal_mutex_t *mutex);

/*! @brief remove mutex */
void hal_mutex_remove (hal_mutex_t mutex);

/*! @brief lock mutex */
int hal_mutex_lock (hal_mutex_t mutex);

/*! @brief lock mutex without waiting */
int hal_mutex_lock_no_wait (hal_mutex_t mutex);

/*! @brief unlock mutex */
int hal_mutex_unlock (hal_mutex_t mutex);

/*! @brief allocate memory from heap */
void *hal_malloc(uint32_t size);

/*! @brief free memory from heap */
void hal_free(void *pointer);

/*! @brief create semaphore */
hal_sema_t hal_sema_create();

/*! @brief create binary semaphore */
hal_sema_t hal_sema_create_binary();

/*! @brief delete semaphore */
void hal_sema_delete(hal_sema_t handle);

/*! @brief give semaphore */
bool hal_sema_give(hal_sema_t handle);

/*! @brief take semaphore */
bool hal_sema_take(hal_sema_t handle, uint32_t timeout);

/*! @brief give semaphore from ISR */
bool hal_sema_give_isr(hal_sema_t handle, long int * const p_higher_prio);

/*! @brief take semaphore from ISR */
bool hal_sema_take_isr(hal_sema_t handle, long int * const p_higher_prio);

/*! @brief yield scheduler from ISR */
void hal_sched_yield(long HigherPriorityTaskWoken);

/*! @brief create task */
int hal_task_create(hal_task_fct_t fct,
                    const char * const name,
                    const uint16_t stackdepth,
                    void * const pparams,
                    uint32_t prio,
                    hal_task_t * const ptask );

/*! @brief suspend task */
void hal_task_suspend(hal_task_t task);

/*! @brief resume task */
void hal_task_resume(hal_task_t task);

/*! @brief delete task */
void hal_task_delete(hal_task_t task);

/*! @brief delay task */
void hal_task_delay(uint32_t ticks);

/*! @brief Delay task execution for specified milliseconds */
void hal_task_delay_ms(uint32_t ms);

/*! @brief create event group */
hal_event_group_t hal_eventgrp_create();

/*! @brief delete event group */
void hal_eventgrp_delete(hal_event_group_t eventgrp);

/*! @brief set bits of event group */
hal_eventbits_t hal_eventgrp_set_bits(  hal_event_group_t eventgrp,
                                        const hal_eventbits_t bitmask );

/*! @brief wait for bits of event group */
hal_eventbits_t hal_eventgrp_wait_bits( hal_event_group_t eventgrp,
                                        const hal_eventbits_t bitmask,
                                        const uint32_t bClearOnExit,
                                        const uint32_t bWaitForAllBits,
                                        uint32_t tickstowait );

/*! @brief get os tick value */
uint32_t hal_get_ostick();

/*! @brief get os tick period in milliseconds */
uint32_t hal_get_tick_period_ms();

/*! @brief get os tick rate in Hz */
uint32_t hal_get_tick_rate_hz();

/*! @brief Converts OS ticks to milliseconds. */
uint32_t hal_tick_to_ms(uint32_t os_tick);

/*! @brief Convert milliseconds to OS ticks */
uint32_t hal_ms_to_ticks(uint32_t ms);

/*! @brief Provides the exec time in ms of current task */
uint32_t hal_get_exec_time();

/*! @brief Provides the os crt time in us */
uint64_t hal_get_crt_time();

/*! @brief start suspending task switching */
void hal_atomic_enter(hal_ctx_t *ctx);

/*! @brief stop suspending task switching */
void hal_atomic_exit(hal_ctx_t *ctx);

/*! @brief get max NVIC priority of syscalls */
int hal_get_max_syscall_prio();

/*! @brief get os max priority */
int hal_get_os_max_prio();

/*! @brief get CPU percent used by idle task */
unsigned int hal_get_idle_percent();

/*! @brief create queue (returns opaque handle, NULL on failure) */
void *hal_queue_create(uint32_t length, uint32_t item_size);

/*! @brief delete queue */
void hal_queue_delete(void *queue);

/*! @brief receive item from queue; returns true on success */
bool hal_queue_receive(void *queue, void *item, uint32_t timeout);

/*! @brief create queue set */
void *hal_queueset_create(uint32_t length);

/*! @brief add queue to queue set */
int hal_queueset_add_queue(void *queue_set, void *queue);

/*! @brief receive from queue set */
void *hal_queueset_receive(void *queue_set, uint32_t timeout);

/*! @brief delete queue set */
void hal_queueset_delete(void *queue_set);

#if MPP_OS_FREERTOS

#define LOWER_PRIO(P) ((P) - 1) /* in FreeRTOS prio 3 < 4 */
#define HIGHER_PRIO(P) ((P) + 1) /* in FreeRTOS prio 5 > 4 */
#define IS_HIGHER_PRIO(P1, P2) ((P1) > (P2)) /* in FreeRTOS higher number = higher priority */

#elif MPP_OS_ZEPHYR

#define LOWER_PRIO(P) ((P) + 1) /* in Zephyr prio 5 < 4 */
#define HIGHER_PRIO(P) ((P) - 1) /* in Zephyr prio 3 > 4 */
#define IS_HIGHER_PRIO(P1, P2) ((P1) < (P2)) /* in Zephyr lower number = higher priority */

#else

#error "Priority macros need to be defined!"

#endif

#endif /* _HAL_OS_H */
