/*
 * Copyright 2025-2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hal_os.h"

#if MPP_OS_ZEPHYR

#include "hal_debug.h"
#include "hal_zephyr_priv.h"
#include <stdlib.h>
#include <zephyr/cache.h>
#include <zephyr/sys/mem_blocks.h>

#define HAL_ALIGN 64

#ifndef HAL_MUTEX_TIMEOUT_MS
/**
 *  Mutex lock timeout definition
 *  An arbitrary default value is defined to 5 seconds
 *  value unit should be milliseconds
 * */
#define HAL_MUTEX_TIMEOUT_MS   (5000)
#endif

static inline void *safe_malloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }

    void *ptr = k_aligned_alloc(HAL_ALIGN, size);
    if (ptr) {
        memset(ptr, 0, size);
    } else {
        printk("k_malloc(%zu) FAILED\n", size);
    }
    return ptr;
}

static inline void safe_free(void *ptr)
{
    if (ptr) {
        k_free(ptr);
    }
}

void *hal_malloc(uint32_t size)
{
    if (size == 0) {
        return NULL;
    }

    void *ptr = k_aligned_alloc(HAL_ALIGN, size);

    return ptr;
}

void hal_free(void *pointer)
{
    if (pointer) {
        k_free(pointer);
    }
}

/**
 * hal mutex handling:
 *  create, lock, unlock and remove
 */

int hal_mutex_create(hal_mutex_t *mutex)
{
    volatile int ret = MPP_ERROR;

    do {
        if (mutex == NULL) {
            HAL_LOGE("%s: Invalid mutex pointer", __func__);
            ret = MPP_INVALID_MUTEX;
            break;
        }

        struct k_mutex *kmutex = safe_malloc(sizeof(struct k_mutex));
        if (!kmutex) {
            HAL_LOGE("%s: Failed to allocate mutex", __func__);
            ret = MPP_ERR_ALLOC_MUTEX;
            break;
        }

        k_mutex_init(kmutex);
        *mutex = kmutex;
        ret = MPP_SUCCESS;

    } while (false);

    return ret;
}

void hal_mutex_remove(hal_mutex_t kmutex)
{
    if (kmutex) {
        safe_free(kmutex);
    }
}

int hal_mutex_lock(hal_mutex_t kmutex)
{
    volatile int ret = MPP_ERROR;

    do {
        if (kmutex == NULL) {
            HAL_LOGE("%s: Invalid mutex", __func__);
            ret = MPP_INVALID_MUTEX;
            break;
        }

        int zret = k_mutex_lock(kmutex, K_MSEC(HAL_MUTEX_TIMEOUT_MS));
        if (zret != 0) {
            HAL_LOGE("%s: Mutex timed out", __func__);
            ret = MPP_MUTEX_TIMEOUT;
            break;
        }
        ret = MPP_SUCCESS;
    } while (false);

    return ret;
}


int hal_mutex_lock_no_wait(hal_mutex_t kmutex)
{
    volatile int ret = MPP_ERROR;

    do {
        if (kmutex == NULL) {
            HAL_LOGE("%s: Invalid mutex", __func__);
            ret = MPP_INVALID_MUTEX;
            break;
        }

        int zret = k_mutex_lock(kmutex, K_NO_WAIT);
        if (zret != 0) {
            ret = MPP_MUTEX_TIMEOUT;
            break;
        }
        ret = MPP_SUCCESS;
    } while (false);

    return ret;
}

int hal_mutex_unlock(hal_mutex_t kmutex)
{
    volatile int ret = MPP_ERROR;

    do {
        if (kmutex == NULL) {
            HAL_LOGE("%s: Invalid mutex", __func__);
            ret = MPP_INVALID_MUTEX;
            break;
        }

        int zret = k_mutex_unlock(kmutex);
        if (zret != 0) {
            HAL_LOGE("%s: Mutex unlock error", __func__);
            ret = MPP_MUTEX_ERROR;
            break;
        }
        ret = MPP_SUCCESS;
    } while (false);

    return ret;
}

uint32_t hal_get_exec_time()
{
    /* TODO: Zephyr doesn't have built-in per-task runtime stats like FreeRTOS
     * This would need to be implemented using thread analyzer or custom
     * tracking */
    return hal_get_ostick();
}

uint64_t hal_get_crt_time()
{
    return hal_get_ostick();
}

hal_sema_t hal_sema_create()
{
    /* not implemented */
    return NULL;
}

hal_sema_t hal_sema_create_binary()
{
    struct k_sem *ksem = safe_malloc(sizeof(struct k_sem));
    if (!ksem) {
        HAL_LOGE("%s: Failed to allocate semaphore", __func__);
        return NULL;
    }

    k_sem_init(ksem, 0, 1);

    return ksem;
}

bool hal_sema_give(hal_sema_t handle)
{
    struct k_sem *ksem = (struct k_sem *) handle;
    if (!ksem) {
        HAL_LOGE("%s: Invalid semaphore handle", __func__);
        return false;
    }

    k_sem_give(ksem);
    return true;
}

bool hal_sema_take(hal_sema_t handle, uint32_t timeout)
{
    struct k_sem *ksem = (struct k_sem *) handle;
    if (!ksem) {
        HAL_LOGE("%s: Invalid semaphore handle", __func__);
        return false;
    }

    k_timeout_t ztimeout =
        (timeout == HAL_MAX_TIMEOUT) ? K_FOREVER : K_MSEC(timeout);
    int ret = k_sem_take(ksem, ztimeout);
    return (ret == 0);
}

bool hal_sema_give_isr(hal_sema_t handle, long int *const p_higher_prio)
{
    struct k_sem *ksem = (struct k_sem *) handle;
    if (!ksem) {
        return false;
    }

    /* Zephyr doesn't distinguish between ISR and non-ISR context for semaphores
     */
    k_sem_give(ksem);
    /* p_higher_prio is not used in Zephyr */
    return true;
}

void hal_sched_yield(long HigherPriorityTaskWoken)
{
    /* Zephyr handles this automatically, but we can yield if needed */
    k_yield();
}

uint32_t hal_get_ostick()
{
    return k_uptime_get_32();
}

uint32_t hal_get_tick_period_ms()
{
    return TICK_PERIOD_MS;
}

uint32_t hal_get_tick_rate_hz()
{
    // code assumes >= 1ms ticks
    return (CONFIG_SYS_CLOCK_TICKS_PER_SEC <= 1000)
               ? CONFIG_SYS_CLOCK_TICKS_PER_SEC
               : 1000;
}

void hal_atomic_enter(hal_ctx_t *ctx)
{
    __ASSERT_NO_MSG(ctx != NULL);
    ctx->in_isr = k_is_in_isr();
    ctx->key    = irq_lock();   // works in thread and ISR; disables local CPU interrupts
    ctx->active = true;
}

void hal_atomic_exit(hal_ctx_t *ctx)
{
    __ASSERT_NO_MSG(ctx != NULL && ctx->active);
    // Optional misuse detection: ensure we exit in the same context type
    __ASSERT_NO_MSG(k_is_in_isr() == ctx->in_isr);

    ctx->active = false;
    irq_unlock(ctx->key);
}

uint32_t hal_tick_to_ms(uint32_t os_tick)
{
    return k_ticks_to_ms_floor32(os_tick);
}

int hal_task_create(hal_task_fct_t fct, const char *const name,
                    const uint16_t stackdepth, void *const pparams,
                    uint32_t prio, hal_task_t *const ptask)
{
    hal_zephyr_task_t *ztask = safe_malloc(sizeof(hal_zephyr_task_t));
    if (!ztask) {
        HAL_LOGE("%s: Failed to allocate task structure", __func__);
        return MPP_ERROR;
    }

    /* Use K_THREAD_STACK_ALLOC for proper stack allocation with guards */
    ztask->stack_size = stackdepth;
    ztask->stack = k_thread_stack_alloc(stackdepth, 0);
    if (!ztask->stack) {
        HAL_LOGE("%s: Failed to allocate task stack", __func__);
        safe_free(ztask);
        return MPP_ERROR;
    }

    ztask->tid = k_thread_create(&ztask->thread, ztask->stack, stackdepth,
                                 (k_thread_entry_t) fct, pparams, NULL, NULL,
                                 prio, 0, K_NO_WAIT);

    if (!ztask->tid) {
        HAL_LOGE("%s: Failed to create thread", __func__);
        k_thread_stack_free(ztask->stack);
        safe_free(ztask);
        return MPP_ERROR;
    }

    HAL_LOGD("Thread [%s] was created: tid=%p, stack %#x@%#x\n", name,
             ztask->tid, stackdepth, (unsigned int) (void *) ztask->stack);

    if (name) {
        k_thread_name_set(ztask->tid, name);
    }

    *ptask = ztask;
    return MPP_SUCCESS;
}

void hal_task_suspend(hal_task_t task)
{
    hal_zephyr_task_t *ztask = (hal_zephyr_task_t *) task;
    if (ztask && ztask->tid) {
        k_thread_suspend(ztask->tid);
    } else {
        // Suspend current task
        k_thread_suspend(k_current_get());
    }
}

void hal_task_resume(hal_task_t task)
{
    hal_zephyr_task_t *ztask = (hal_zephyr_task_t *) task;
    if (ztask && ztask->tid) {
        k_thread_resume(ztask->tid);
    }
}

void hal_task_delay(uint32_t ms)
{
    k_msleep(ms);
}

hal_event_group_t hal_eventgrp_create()
{
    struct k_event *kevent = safe_malloc(sizeof(struct k_event));
    if (!kevent) {
        HAL_LOGE("%s: Failed to allocate event group", __func__);
        return NULL;
    }

    k_event_init(kevent);

    return kevent;
}

hal_eventbits_t hal_eventgrp_set_bits(hal_event_group_t eventgrp,
                                      const hal_eventbits_t bitmask)
{
    struct k_event *kevent = (struct k_event *) eventgrp;
    if (!kevent) {
        HAL_LOGE("%s: Invalid event group handle", __func__);
        return 0;
    }

    k_event_set(kevent, bitmask);
    return bitmask;
}

hal_eventbits_t hal_eventgrp_get_bits(hal_event_group_t eventgrp)
{
    struct k_event *kevent = (struct k_event *) eventgrp;
    if (!kevent) {
        HAL_LOGE("%s: Invalid event group handle", __func__);
        return 0;
    }

    hal_eventbits_t bitmask = k_event_test(kevent, 0xffffffff);
    return bitmask;
}

hal_eventbits_t hal_eventgrp_wait_bits(hal_event_group_t eventgrp,
                                       const hal_eventbits_t bitmask,
                                       const uint32_t bClearOnExit,
                                       const uint32_t bWaitForAllBits,
                                       uint32_t tickstowait)
{
    struct k_event *kevent = (struct k_event *) eventgrp;
    if (!kevent) {
        HAL_LOGE("%s: Invalid event group handle", __func__);
        return 0;
    }

    k_timeout_t timeout =
        (tickstowait == HAL_MAX_TIMEOUT) ? K_FOREVER : K_TICKS(tickstowait);
    uint32_t events = (bWaitForAllBits)
                          ? k_event_wait_all(kevent, bitmask, false, timeout)
                          : k_event_wait(kevent, bitmask, false, timeout);

    if (bClearOnExit) {
        k_event_clear(kevent, events);
    }

    return events;
}

int hal_get_max_syscall_prio()
{
    return 0; /* Zephyr doesn't have this concept */
}

int hal_get_os_max_prio()
{
    return CONFIG_NUM_PREEMPT_PRIORITIES - 1;
}

unsigned int hal_get_idle_percent()
{
    /* TODO: This would need to be implemented based on Zephyr's stats
     * For now, return 0 */
    return 0;
}

#endif /* MPP_OS_ZEPHYR */
