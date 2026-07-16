/*
 * Copyright 2020-2026 NXP.
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

#define MPP_LOG_MODULE_REGISTER

#include <assert.h>

#include "mpp_api.h"
#include "mpp_api_types_internal.h"
#include "mpp_heap.h"
#include "mpp_version.h"
#include "mpp_debug.h"

#include "hal_os.h"
#include "string.h"

#if defined(MCMGR_USED)
#include "mpp_multicore.h"
#endif

/* run to completion heap */
extern _mpp_t *rc_prio_lst[];
/* preemptable heap */
extern _mpp_t *preempt_prio_lst[];

/* Minimum control task priority(the highest among the pipeline tasks).
 * Pipeline MAX priority should at least be 4.*/
#define MIN_CTL_TASK_PRIO 4

/* global pipeline controller task */
#if MPP_OS_ZEPHYR
#define PIPELINE_CTL_STACK_SZ	2048 /* Zephyr has more nested function calls */
#endif
#if MPP_OS_FREERTOS
#define PIPELINE_CTL_STACK_SZ	1000
#endif

/* run-to-completion task runs the rc heap */
#ifdef CONFIG_MPP_RC_TASK_STACK_SIZE
#define RC_TASK_STACK_SZ CONFIG_MPP_RC_TASK_STACK_SIZE
#else
#define RC_TASK_STACK_SZ        1200
#endif
static hal_task_t hrcHeapTask = NULL;

/* preemptable task runs the preempt heap */
#ifdef CONFIG_MPP_PR_TASK_STACK_SIZE
#define PR_TASK_STACK_SZ CONFIG_MPP_PR_TASK_STACK_SIZE
#else
#define PR_TASK_STACK_SZ        1200
#endif
static hal_task_t hprHeapTask = NULL;

/* rc heap execution time in ticks */
static uint32_t rc_exec_ticks;
/* max source frame rate (miliseconds)
   lower the value higher the rate */
#define MAX_SRC_FRAME_MS     33
/* starting value of RC cycle maximum duration */
#define MAX_RC_CYCLE_FRAMES  3

/* synchronization between rc task and controller task */
static hal_event_group_t  xEventGroup1, xEventGroup2;
static hal_eventbits_t xEventGroupValue;
#define RC_HEAP_TASK_START_BIT  (1UL << 0UL)
#define RC_HEAP_TASK_DONE_BIT   (1UL << 1UL)

/* synchronization between pr task and controller task */
static hal_event_group_t xEventGroup3;
#define PR_HEAP_TASK_DONE_BIT   (1UL << 2UL)

static hal_task_t hPipelineCtl = NULL;
static hal_sema_t xCtlStartSem;
static int is_last = 0; /* flag: last mpp_start has been called */

const static char mpp_version[] = "MPP_VERSION_"STRING(MPP_VERSION_MAJOR)"."STRING(MPP_VERSION_MINOR)"."STRING(MPP_VERSION_COMMIT);

void mpp_execute_heap(_mpp_t *rc_prio_lst[]);

static mpp_stats_t *api_stats;
hal_mutex_t stats_lock[MPP_STATS_GRP_NUM];

int mpp_memory_alloc(_mpp_t *mpp);
int mpp_memory_check(_mpp_t *mpp);
void mpp_memory_free(_mpp_t *mpp);
mpp_memory_policy_t get_prev_policy(_elem_t *elem);

static void rcHeapTaskFunc(void *arg) {
    uint32_t start_ticks;
    uint32_t end_ticks;
    MPP_LOGI("rcHeapTask starting\r\n");
    for(;;) {
        /* wait for start bit set by controlling task*/
        xEventGroupValue  =  hal_eventgrp_wait_bits(xEventGroup1,
                                            RC_HEAP_TASK_START_BIT,
                                            1,
                                            1,
                                            HAL_MAX_TIMEOUT
                                            );
        if (!(xEventGroupValue & RC_HEAP_TASK_START_BIT)) {
            MPP_LOGE("RC_HEAP_TASK_START_BIT not set\r\n");
            break;
        }
        start_ticks = hal_get_ostick();
        /* go and execute RC heap */
        mpp_execute_heap(rc_prio_lst);
        end_ticks = hal_get_ostick();
        rc_exec_ticks = end_ticks - start_ticks;
        /* set complete bit to resume controlling task*/
        hal_eventgrp_set_bits(xEventGroup2, RC_HEAP_TASK_DONE_BIT);
    }
    /* should never get here */
    MPP_LOGE("rcHeapTask will suspend\r\n");
    hal_task_suspend(NULL);
}

static void prHeapTaskFunc(void *arg) {
    MPP_LOGI("prHeapTask starting\n");
    for(;;) {
        mpp_execute_heap(preempt_prio_lst);
        /* flag round finished */
        hal_eventgrp_set_bits(xEventGroup3, PR_HEAP_TASK_DONE_BIT);
        /* suspend and wait for resume */
        hal_task_suspend(NULL);
    }
}

static void vPipelineCtlTask( void *params )
{
    int ret = MPP_ERROR;
    mpp_api_params_t *mpp_params = params;
    uint32_t tick_period_ms = hal_get_tick_period_ms();
    /* RC task is the second highest priority task in the pipeline. */
    static int rc_task_prio = LOWER_PRIO(MIN_CTL_TASK_PRIO);
    /* PR task is the lowest-priority task in the pipeline. */
    static int pr_task_prio = LOWER_PRIO(LOWER_PRIO(MIN_CTL_TASK_PRIO));

// TODO: fix for Zephyr
#if MPP_OS_FREERTOS
    if (mpp_params)
    {
        if (mpp_params->pipeline_task_max_prio != 0)
        {
            rc_task_prio = mpp_params->pipeline_task_max_prio - 1;
            pr_task_prio = mpp_params->pipeline_task_max_prio - 2;
        }

        if (mpp_params->pipeline_rc_task_prio != 0)
        {
            rc_task_prio = mpp_params->pipeline_rc_task_prio;
            pr_task_prio = mpp_params->pipeline_rc_task_prio - 1;
        }

        if (mpp_params->pipeline_pr_task_prio != 0)
        {
            pr_task_prio = mpp_params->pipeline_pr_task_prio;
        }
    }
#endif

    /* wait until application finished pipeline construction for the first start */
    ret = hal_sema_take(xCtlStartSem, HAL_MAX_TIMEOUT);
    if (true != ret)
        return;
    mpp_dump_heap(rc_prio_lst);
    MPP_LOGI("Pipeline control running\n");
    /* create run to completion heap task */
    ret = hal_task_create(  rcHeapTaskFunc,
                            "rcHeapTask",
                            RC_TASK_STACK_SZ,
                            NULL,
                            rc_task_prio,
                            &hrcHeapTask);
    if (MPP_SUCCESS != ret) {
        MPP_LOGE("Failed to create rcHeapTask\n");
        return;
    }
    /* create preemptable heap task */
    ret = hal_task_create(  prHeapTaskFunc,
                            "prHeapTask",
                            PR_TASK_STACK_SZ,
                            NULL,
                            pr_task_prio,
                            &hprHeapTask);
    if (MPP_SUCCESS != ret) {
        MPP_LOGE("Failed to create prHeapTask\n");
        return;
    }

#if defined(MCMGR_USED)
    /* Initialize multicore support */
    if (mpp_params && mpp_params->multicore.n_remote_cores != 0)
    {
        if (mpp_params->multicore.n_remote_cores > MPP_MAX_CORES - 1)
        {
            MPP_LOGE("Invalid number of remote cores, expected less than %d, got %d\r\n", MPP_MAX_CORES, mpp_params->multicore.n_remote_cores);
            return;
        }
        ret = mpp_mc_multicore_init(mpp_params, pr_task_prio);
        if (ret != MPP_SUCCESS)
        {
            MPP_LOGE("Failed to initialize multicore support\r\n");
            return;
        }
    }
#endif

    /* suspend task to prevent running when RC sleeps */
    hal_task_suspend(hprHeapTask);

    unsigned int pr_rounds = 0;
    unsigned min_rc_cycle_ticks;
    unsigned rc_cycle_inc;
    if ((mpp_params != NULL) && (mpp_params->rc_cycle_min != 0))
        min_rc_cycle_ticks = mpp_params->rc_cycle_min / tick_period_ms;
    else
        min_rc_cycle_ticks = MAX_RC_CYCLE_FRAMES * MAX_SRC_FRAME_MS / tick_period_ms;

    unsigned max_rc_cycle_ticks = min_rc_cycle_ticks;

    if ((mpp_params != NULL) && (mpp_params->rc_cycle_inc != 0))
        rc_cycle_inc = mpp_params->rc_cycle_inc / tick_period_ms;
    else
        rc_cycle_inc = MAX_SRC_FRAME_MS / tick_period_ms;

    do {
        /* set bits to signal run-to-completion task to run */
        hal_eventgrp_set_bits(xEventGroup1, RC_HEAP_TASK_START_BIT);
        /* block and wait for completion */
        xEventGroupValue  = hal_eventgrp_wait_bits(xEventGroup2,
                                            RC_HEAP_TASK_DONE_BIT,
                                            1,
                                            1,
                                            HAL_MAX_TIMEOUT
                                            );
        if (!(xEventGroupValue & RC_HEAP_TASK_DONE_BIT)) {
            MPP_LOGE("RC_HEAP_TASK_DONE_BIT not set\r\n");
            break;
        }

        uint32_t start_ticks, end_ticks;
        start_ticks = hal_get_ostick();
        unsigned int delay = (rc_exec_ticks >= max_rc_cycle_ticks)?1:(max_rc_cycle_ticks - rc_exec_ticks);
        if (__builtin_expect((delay == 1), 0))
            max_rc_cycle_ticks += rc_cycle_inc;
        if ((delay > rc_cycle_inc) && (max_rc_cycle_ticks > min_rc_cycle_ticks))
            max_rc_cycle_ticks -= rc_cycle_inc;

        if (!pr_rounds)
            /* resume PR task only if PR task has been suspended */
            hal_task_resume(hprHeapTask);

        /* allow rc task to return from hal_eventgrp_set_bits and block on waiting for start bit */
        /* wait for delay to expire or PR task to finish one round */
        xEventGroupValue =  hal_eventgrp_wait_bits(xEventGroup3,
                                            PR_HEAP_TASK_DONE_BIT,
                                            1,
                                            1,
                                            delay
                                            );
       unsigned int pr_slot = delay;
       static int app_slot;
       static unsigned int pr_rounds_cnt = 0;

        /* get the actual sleep time */
        end_ticks = hal_get_ostick();
        if(xEventGroupValue & PR_HEAP_TASK_DONE_BIT) {
            if (!pr_rounds)
                MPP_LOGD("PR task completed before delay expiration, allowed %d used  %u\r\n",
                        delay , (end_ticks - start_ticks));
            /* available for app tasks : delay - (end_ticks - start_ticks) */
            app_slot = delay - (end_ticks - start_ticks);
            if (app_slot > 0)
                hal_task_delay(app_slot);
            else
                MPP_LOGI("Invalid value for app_slot (%d)\r\n", app_slot);
            pr_rounds_cnt = pr_rounds;
            pr_rounds = 0;
        } else {
            pr_rounds++;
        }

        if (api_stats && hal_mutex_lock_no_wait(stats_lock[MPP_STATS_GRP_API]) == MPP_SUCCESS) {
                api_stats->api.rc_cycle = hal_tick_to_ms(rc_exec_ticks);
                api_stats->api.rc_cycle_max = hal_tick_to_ms(max_rc_cycle_ticks);
                api_stats->api.pr_slot = hal_tick_to_ms(pr_slot);
                assert(pr_rounds_cnt < 0xFFFFFFFFU);
                api_stats->api.pr_rounds = pr_rounds_cnt + 1U;
                assert(app_slot >= 0);
                api_stats->api.app_slot = hal_tick_to_ms(app_slot);
                api_stats->api.cpu_load = 100U - hal_get_idle_percent();
                (void) hal_mutex_unlock(stats_lock[MPP_STATS_GRP_API]);
        }

    } while(1);
}

int mpp_api_init(mpp_api_params_t *params)
{
    int ret;
    /* controller task has max priority */
    /* default value is aligned with MIN_CTL_TASK_PRIO */
    int pipeline_ctl_task_prio = MIN_CTL_TASK_PRIO;
    int rc_task_prio;

    /* mpp heap initialization */
    mpp_heap_init(rc_prio_lst);

    mpp_heap_init(preempt_prio_lst);

    xCtlStartSem = hal_sema_create_binary();
    if (!xCtlStartSem)
        return MPP_ERR_ALLOC_MUTEX;

    hal_sema_give(xCtlStartSem);
    ret = hal_sema_take(xCtlStartSem, 0);
    if (true != ret)
        return MPP_MUTEX_ERROR;

    xEventGroup1 = hal_eventgrp_create();

    xEventGroup2 = hal_eventgrp_create();

    xEventGroup3 = hal_eventgrp_create();

    if (xEventGroup1 == NULL || xEventGroup2 == NULL || xEventGroup3 == NULL) {
        MPP_LOGE("Failed to create event groups\r\n");
        return MPP_ERROR;
    }

    /* check pipeline max priority parameter. */
    if (params)
    {
        if (params->pipeline_task_max_prio != 0)
        {
            // TODO, fix for Zephyr
            if (params->pipeline_task_max_prio > hal_get_os_max_prio())
            {
                MPP_LOGE("pipeline task maximum priority is higher than OS max priority: %d\r\n", hal_get_os_max_prio());
                return MPP_INVALID_PARAM;
            }

            if (params->pipeline_task_max_prio < MIN_CTL_TASK_PRIO)
            {
                MPP_LOGE("pipeline task maximum priority should be at least %d\r\n", MIN_CTL_TASK_PRIO);
                return MPP_INVALID_PARAM;
            }

            pipeline_ctl_task_prio = params->pipeline_task_max_prio;
        }

        if (params->pipeline_rc_task_prio != 0)
        {
            if (params->pipeline_rc_task_prio >= pipeline_ctl_task_prio)
            {
                MPP_LOGE("pipeline rc task priority should not be greater than or equal to pipeline_ctl_task_prio: %d\r\n", pipeline_ctl_task_prio);
                return MPP_INVALID_PARAM;
            }

            rc_task_prio = params->pipeline_rc_task_prio;
        }
        else
        {
            rc_task_prio = pipeline_ctl_task_prio - 1;
        }

        if (params->pipeline_pr_task_prio != 0)
        {
            if (params->pipeline_pr_task_prio >= rc_task_prio)
            {
                MPP_LOGE("pipeline preemptable task priority should not be greater than or equal to rc_task_prio: %d\r\n", rc_task_prio);
                return MPP_INVALID_PARAM;
            }
        }
    }

    /* create pipeline control task */
    /* task will not run until the xCtlStartSem is released
       by a last call to mpp_start*/
    ret = hal_task_create(
            vPipelineCtlTask,
            "PipelineCtl",
            PIPELINE_CTL_STACK_SZ,
            params,
			pipeline_ctl_task_prio,
            &hPipelineCtl);
    if (MPP_SUCCESS != ret) {
        MPP_LOGE("Failed to create vPipelineCtlTask\n");
        return MPP_ERROR;
     }

    if (params)
        api_stats = params->stats;

    for (int grp = 0; grp < MPP_STATS_GRP_NUM; grp++) {
        ret = hal_mutex_create(&stats_lock[grp]);
        if (MPP_SUCCESS != ret) {
             MPP_LOGE("Failed to create stats_lock[%d]\n", grp);
            return MPP_ERROR;
        }

        // Initialize to "locked" state (stats disabled by default)
        ret = hal_mutex_lock(stats_lock[grp]);
        if (MPP_SUCCESS != ret) {
            MPP_LOGE("Failed to take stats_lock[%d]\n", grp);
            return MPP_MUTEX_ERROR;
        }
    }

    return MPP_SUCCESS;
}

void mpp_stats_enable(mpp_stats_grp_t grp)
{
    (void) hal_mutex_unlock(stats_lock[grp]);
}

void mpp_stats_disable(mpp_stats_grp_t grp)
{
    (void) hal_mutex_lock_no_wait(stats_lock[grp]);
}

mpp_t mpp_create(mpp_params_t *params, int *ret)
{
    _mpp_t *m = NULL;
    /*check params*/
    if (!params) {
        *ret = MPP_INVALID_PARAM;
        return m;
    }
    /* no parent to inherit from */
    if (params->exec_flag == MPP_EXEC_INHERIT) {
        *ret = MPP_ERROR;
        return m;
    }

    /*allocate memory*/
    m = hal_malloc(sizeof(*m));
    if (m) {
        memset(m, 0, sizeof(*m));
        *ret = MPP_SUCCESS;
    }
    else
    {
        *ret = MPP_MALLOC_ERROR;
        return NULL;
    }
    /*copy params */
    m->params = *params;

#if defined(MCMGR_USED)
    /* Assign current core ID to pipeline */
    m->params.core_id = MCMGR_GetCurrentCore();
#else
    m->params.core_id = 0;
#endif

    /* initialize fields */
    m->status = MPP_CREATED;
    m->oper_status = MPP_NOT_STARTED;
    m->status_sema = hal_sema_create_binary();

    /* select the heap */
    if (params->exec_flag == MPP_EXEC_RC)
    {
        mpp_heap_insert(m, rc_prio_lst);
        m->exec_heap = rc_prio_lst;
    } else
    {
        mpp_heap_insert(m, preempt_prio_lst);
        m->exec_heap = preempt_prio_lst;
    }
    MPP_LOGI("%s - mpp@%p\r\n", __func__, m);

    return m;
}

/* link element to previous in the tree */
int mpp_link_elems(_mpp_t *mpp, _elem_t *cur)
{
    _elem_t *prev;
    if ((mpp == NULL) || (cur == NULL))
        return MPP_INVALID_PARAM;

    if (mpp->last_elem)
    {
        /* previous elem is in same mpp */
        prev = mpp->last_elem;
        /* index 0 reserved for elem of same mpp */
        prev->next[0] = cur;
    }
    else if (mpp->hook)
    {
        /* get hook elem from parent mpp */
        prev = mpp->hook;
        /* look for 'next' free slot */
        /* index 0 reserved for elem of same mpp */
        int i;
        for(i = 1; i < MPP_MAX_BRANCH_NUM; i++)
        {
            if(prev->next[i] == NULL)
            {
                prev->next[i] = cur;
                break;
            }
        }
        if (i >= MPP_MAX_BRANCH_NUM)
        {
            MPP_LOGE("\n\r Cannot link element %p, too many links!", cur);
            return MPP_ERROR;
        }
    }
    else
    {   /* no previous element */
        prev = NULL;
    }
    cur->prev = prev;
    return MPP_SUCCESS;
}

/* get the last element in mpp tree */
_elem_t *mpp_get_last_elem(_mpp_t *mpp)
{
    /* get last elem from current mpp */
    if (mpp->last_elem)
        return mpp->last_elem;
    /* search in parent */
    if (mpp->hook)
        return mpp->hook;
    /* no parent: no last elem */
    return NULL;
}

/* Get previous element output buffer */
buf_desc_t *get_in_buff_from_prev_elem(_elem_t *elem)
{
    _mpp_t *mpp = elem->mpp;
    buf_desc_t *ret = NULL;
    int out_buf_idx = 0;

    /* Search in parent pipeline */
    if (mpp->hook )
    {
        if (mpp->first_elem == elem)
        {
            /* If previous element has just one buffer, return that buffer */
            if (elem->prev->io.nb_out_buf > 1)
            {
                /* This is the first element in this branch
                 * Search the index of the element in the prev's element next list
                 * We don't need to search in index 0 because that output is reserved for elements in the same branch
                 * This case refers to secondary branches of a pipeline (see mpp->hook check above) */
                for (int i = 1; i < MPP_MAX_BRANCH_NUM; i++)
                {
                    if (elem->prev->next[i] == elem)
                    {
                        /* If there are more branches of a pipeline than output buffers, just use last output buffer */
                        if ((i < MAX_OUTPUT_PORTS) && (i < elem->prev->io.nb_out_buf))
                        {
                            out_buf_idx = i;
                            ret = elem->prev->io.out_buf[i];
                        }
                        else
                        {
                            out_buf_idx = MAX_OUTPUT_PORTS - 1;
                            ret = elem->prev->io.out_buf[MAX_OUTPUT_PORTS - 1];
                        }
                    }
                }
            }
            else
            {
                out_buf_idx = 0;
                ret = elem->prev->io.out_buf[0];
            }
        }
        else
        {
            /* This is not the first element in the current branch
             * Take the first output buffer in this case */
            out_buf_idx = 0;
            ret = elem->prev->io.out_buf[0];
        }
    }
    else
    {
        /* This is the main pipeline
         * Check if this is the first element */
        if (mpp->first_elem != elem)
        {
            /* This is not the first element in the current branch
             * Take the first output buffer in this case */
            out_buf_idx = 0;
            ret = elem->prev->io.out_buf[0];
        }
    }

    if (ret == NULL)
    {
        /* This is an error case - it means we didn't find the input buffer for current element
         * Either the element is the first element in the main pipeline (source type, camera or static image),
         * Or the current element was not found in the previous element's next list */
        MPP_LOGE("Current element: %s @ 0x%x\n\r", elem_name(elem), (uint32_t) elem);
        MPP_LOGE("\tprevious element 0x%x\r\n", (uint32_t) elem->prev);
        MPP_LOGE("\tinput buffer: NULL\r\n");
    }
    else
    {
        MPP_LOGD("Current element: %s @ 0x%x\r\n", elem_name(elem), (uint32_t) elem);
        MPP_LOGD("\tprevious element %s @ 0x%x\r\n", elem_name(elem->prev), (uint32_t) elem->prev);
        MPP_LOGD("\tinput buffer (prev elem out buf %d): 0x%x\r\n", out_buf_idx, (uint32_t) ret);
    }

    return ret;
}

/* Set crt element input buffers using previous element output buffers */
void set_in_buff_from_prev_elem(_elem_t *elem)
{
    _mpp_t *mpp = elem->mpp;
    elem->io.nb_in_buf = 0;

    /* Search in parent pipeline */
    if (mpp->hook )
    {
        if (mpp->first_elem == elem)
        {
            /* If previous element has just one buffer, return that buffer */
            if (elem->prev->io.nb_out_buf > 1)
            {
                /* This is the first element in this branch
                 * Search the index of the element in the prev's element next list
                 * We don't need to search in index 0 because that output is reserved for elements in the same branch 
                 * This case refers to secondary branches of a pipeline (see mpp->hook check above) */
                for (int i = 1; i < MPP_MAX_BRANCH_NUM; i++)
                {
                    if (elem->prev->next[i] == elem)
                    {
                        /* If there are more branches of a pipeline than output buffers, just use last output buffer */
                        if ((i < MAX_OUTPUT_PORTS) && (i < elem->prev->io.nb_out_buf))
                            elem->io.in_buf[elem->io.nb_in_buf++] = elem->prev->io.out_buf[i];
                        else
                            elem->io.in_buf[elem->io.nb_in_buf++] = elem->prev->io.out_buf[MAX_OUTPUT_PORTS - 1];
                    }
                }
            }
            else
            {
                elem->io.in_buf[elem->io.nb_in_buf++] = elem->prev->io.out_buf[0];
            }
        }
        else
        {
            /* This is not the first element in the current branch
             * Check if there is a split after previous element */
            int n_buffs = elem->prev->io.nb_out_buf;
            if (elem->prev->next[1] != NULL)
                n_buffs = 1;
            for (int i = 0; i < n_buffs; i++)
                elem->io.in_buf[elem->io.nb_in_buf++] = elem->prev->io.out_buf[i];
        }
    }
    else
    {
        /* This is the main pipeline
         * Check if this is the first element */
        if (mpp->first_elem != elem)
        {
            /* This is not the first element in the current branch
             * Check if there is a split after previous element */
            int n_buffs = elem->prev->io.nb_out_buf;
            if (elem->prev->next[1] != NULL)
                n_buffs = 1;
            for (int i = 0; i < n_buffs; i++)
                elem->io.in_buf[elem->io.nb_in_buf++] = elem->prev->io.out_buf[i];
        }
    }

    if (elem->io.nb_in_buf == 0)
    {
        /* This is an error case - it means we didn't find the input buffer for current element 
         * Either the element is the first element in the main pipeline (source type, camera or static image),
         * Or the current element was not found in the previous element's next list */
        MPP_LOGE("Current element: %s @ 0x%x\n\r", elem_name(elem), (uint32_t) elem);
        MPP_LOGE("\tprevious element 0x%x\r\n", (uint32_t) elem->prev);
        MPP_LOGE("\tinput buffer: NULL\r\n");
    }
    else if (elem->io.nb_in_buf > MAX_INPUT_PORTS)
    {
        /* This is a warning case - more input buffers than supported */
        MPP_LOGE("Current element: %s @ 0x%x\r\n", elem_name(elem), (uint32_t) elem);
        MPP_LOGE("\tprevious element %s @ 0x%x\r\n", elem_name(elem->prev), (uint32_t) elem->prev);
        MPP_LOGE("\tinput buffers: %d (capped at %d)\r\n", elem->io.nb_in_buf, MAX_INPUT_PORTS);
    }
    else
    {
        MPP_LOGI("Current element: %s @ 0x%x\r\n", elem_name(elem), (uint32_t) elem);
        MPP_LOGI("\tprevious element %s @ 0x%x\r\n", elem_name(elem->prev), (uint32_t) elem->prev);
        for (int i = 0; i < elem->io.nb_in_buf; i++)
            MPP_LOGI("\tinput buffer %d: 0x%x\r\n", i, (uint32_t) elem->io.in_buf[i]);
    }
}

uint32_t get_out_buff_index(_elem_t *elem, buf_desc_t *buf)
{
    for (int i = 0; i < elem->io.nb_out_buf; i++)
    {
        if (elem->io.out_buf[i] == buf)
            return i;
    }

    return MAX_OUTPUT_PORTS;
}

uint32_t get_in_buff_index(_elem_t *elem, buf_desc_t *buf)
{
    for (int i = 0; i < elem->io.nb_in_buf; i++)
    {
        if (elem->io.in_buf[i] == buf)
            return i;
    }

    return MAX_INPUT_PORTS;
}

void *mpp_get_output_buff_address(mpp_elem_handle_t elem, uint32_t index)
{
    _elem_t *_elem = (_elem_t *)elem;

    if (!_elem || index >= MAX_OUTPUT_PORTS || index >= _elem->io.nb_out_buf)
        return NULL;

    return (void *)_elem->io.out_buf[index]->hw->addr;
}

void *mpp_get_input_buff_address(mpp_elem_handle_t elem, uint32_t index)
{
    _elem_t *_elem = (_elem_t *)elem;

    if (!_elem || index >= MAX_INPUT_PORTS || index >= _elem->io.nb_in_buf)
        return NULL;

    return (void *)_elem->io.in_buf[index]->hw->addr;
}

/* allocate the element and link it to the mpp */
int mpp_create_elem(_mpp_t *mpp, _elem_t **p_elem)
{
    _elem_t *elem;
    int ret;

    if (!p_elem)
        return MPP_INVALID_PARAM;

    /* allocate memory for element */
    elem = hal_malloc(sizeof(*elem));
    if (!elem)
        return MPP_MALLOC_ERROR;
    memset(elem, 0, sizeof(_elem_t));

    /* link to previous element */
    ret = mpp_link_elems(mpp, elem);
    if (ret != MPP_SUCCESS)
        return ret;

    /* backpointer to owning mpp */
    elem->mpp = mpp;

    /* last added element */
    mpp->last_elem = elem;
    /* first added element ? */
    if (mpp->first_elem == NULL)
        mpp->first_elem = elem;

    /* clear the io descriptors */
    memset((void *)&elem->io, 0, sizeof(io_desc_t));

    /* return created elem */
    *p_elem = elem;
    return MPP_SUCCESS;
}

int mpp_element_add(mpp_t mpp, mpp_element_id_t id, mpp_element_params_t *params, mpp_elem_handle_t *elem_h)
{
    int ret = MPP_SUCCESS;
    if (!mpp)
        return MPP_INVALID_PARAM;

    /* check type of element added */
    if (!can_add(id))
        return MPP_ERROR;

    /* check pipeline status */
    _mpp_t *_mpp = (_mpp_t *)mpp;
    if (_mpp->status != MPP_OPENED)
        return MPP_ERROR;
    if (id >= MPP_ELEMENT_NUM)
        return MPP_INVALID_ELEM;

    _elem_t *elem;
    ret = mpp_create_elem(_mpp, &elem);
    if (ret != MPP_SUCCESS)
        return ret;

    /* copy params */
    memcpy(&elem->params, params, sizeof(*params));

    /* set element id */
    elem->type = MPP_TYPE_PROC;
    elem->proc_typ = id;

    /* element specific instantiation function */
    elem_setup_func_t setup_func = get_setup_function(id);
    if (!setup_func)
        return MPP_ERROR;
    ret = setup_func(elem);
    if (ret != MPP_SUCCESS)
        return ret;

    if (elem_h != NULL) {
        *elem_h = (mpp_elem_handle_t) elem;
    }

    return ret;
}

int mpp_multipass_element_add(mpp_t mpp, mpp_element_id_t id,
                          mpp_element_params_t *params_array,
                          uint32_t n_params,
                          mpp_elem_handle_t *elem_h)
{
    int ret = MPP_SUCCESS;
    if (!mpp || !params_array || n_params == 0)
        return MPP_INVALID_PARAM;

    if (!can_add(id))
        return MPP_ERROR;

    _mpp_t *_mpp = (_mpp_t *)mpp;
    if (_mpp->status != MPP_OPENED)
        return MPP_ERROR;
    if (id >= MPP_ELEMENT_NUM)
        return MPP_INVALID_ELEM;

    _elem_t *elem;
    ret = mpp_create_elem(_mpp, &elem);
    if (ret != MPP_SUCCESS)
        return ret;

    /* copy first param set as the default active params */
    memcpy(&elem->params, &params_array[0], sizeof(*params_array));

    /* allocate and copy the full pass_params array */
    elem->pass_params = hal_malloc(n_params * sizeof(mpp_element_params_t));
    if (!elem->pass_params)
        return MPP_MALLOC_ERROR;
    memcpy(elem->pass_params, params_array, n_params * sizeof(mpp_element_params_t));
    elem->n_pass_params = n_params;

    elem->type = MPP_TYPE_PROC;
    elem->proc_typ = id;

    /* use pass-specific setup; error if element does not support multipass operations */
    multipass_elem_setup_func_t pass_setup_func = get_multipass_elem_setup_function(id);
    if (!pass_setup_func) {
        hal_free(elem->pass_params);
        elem->pass_params = NULL;
        elem->n_pass_params = 0;
        return MPP_UNSUPPORTED;
    }
    ret = pass_setup_func(elem);
    if (ret != MPP_SUCCESS) {
        hal_free(elem->pass_params);
        elem->pass_params = NULL;
        elem->n_pass_params = 0;
        return ret;
    }

    if (elem_h != NULL)
        *elem_h = (mpp_elem_handle_t) elem;

    return ret;
}

int mpp_set_n_passes(mpp_t mpp, uint32_t n_passes)
{
    int ret = MPP_SUCCESS;

    if (!mpp)
        return MPP_INVALID_PARAM;
    if (n_passes == 0) {
        MPP_LOGE("n_passes must be at least 1\n");
        return MPP_INVALID_PARAM;
    }

    _mpp_t *_mpp = (_mpp_t *)mpp;

#if defined(MCMGR_USED)
    /* Check if this is a remote call */
    mcmgr_core_t current_core = MCMGR_GetCurrentCore();
    if (_mpp->params.core_id != current_core)
    {
        /* Send remote API call using mpp_multicore.c */
        mpp_remote_api_msg_t msg, response;
        msg.api_id = MPP_API_SET_N_PASSES;
        msg.payload.set_n_passes_params.mpp = mpp;
        msg.payload.set_n_passes_params.n_passes = n_passes;

        ret = mpp_mc_remote_api_send(&msg, &response, _mpp->params.core_id);

        if (ret != MPP_SUCCESS)
        {
            MPP_LOGE("Failed to send remote mpp_set_n_passes call (error %d)\r\n", ret);
            return MPP_ERROR;
        }

        return ret;
    }
#endif

    _mpp->n_passes = n_passes;
    return ret;
}

static inline int split_dequeue(_mpp_t *mpp)
{
    return MPP_SUCCESS;
}

int mpp_split(mpp_t mpp, unsigned int num, mpp_params_t *params, mpp_t *out_list)
{
    int ret = MPP_SUCCESS;
    if (mpp == MPP_INVALID)
        return MPP_INVALID_PARAM;
    _mpp_t *_mpp = (_mpp_t *)mpp;
    if (_mpp->status == MPP_CLOSED)
        return MPP_ERROR;
    /* check split count */
    if ((_mpp->split_cnt + num) > MPP_MAX_SPLIT_NUM)
        return MPP_ERROR;

    /* allocate output mpps */
    int i;
    for (i = 0; i < num; i++) {
        /* if parent is preemptable all splits may be only preemptable */
        if (params[i].exec_flag == MPP_EXEC_RC && _mpp->exec_heap == preempt_prio_lst)
            return MPP_ERROR;

        _mpp_t *m = hal_malloc(sizeof(_mpp_t));
        if (!m)
            return MPP_MALLOC_ERROR;
        memset(m, 0, sizeof(*m));
        /* copy params*/
        m->params = params[i];

        /* link to parent mpp */
        m->hook = _mpp->last_elem;

        if (params[i].exec_flag == MPP_EXEC_INHERIT) {
            /* insert split on the same layer as the parent */
            mpp_heap_insert(m, _mpp->exec_heap);
            m->exec_heap = _mpp->exec_heap;
        } else if (params[i].exec_flag == MPP_EXEC_PREEMPT) {
            /* insert split on the preemptable heap */
            mpp_heap_insert(m, preempt_prio_lst);
            m->exec_heap = preempt_prio_lst;
        } else { /* split is RC, parent is RC*/
            mpp_heap_insert(m, rc_prio_lst);
            m->exec_heap = rc_prio_lst;
        }
        m->status = MPP_OPENED;
        m->oper_status = MPP_NOT_STARTED;
        m->status_sema = hal_sema_create_binary();

        /* return the handle to user */
        out_list[i] = m;
    }
    _mpp->split_cnt++;
    return ret;
}

int mpp_background(mpp_t mpp, mpp_params_t *params, mpp_t *out_mpp)
{
    int ret = MPP_SUCCESS;
    if (mpp == MPP_INVALID)
        return MPP_INVALID_PARAM;
    _mpp_t *_mpp = (_mpp_t *)mpp;
    if (_mpp->status == MPP_CLOSED)
        return MPP_ERROR;

    /* allocate output mpps */
    _mpp_t *m = hal_malloc(sizeof(_mpp_t));
    if (!m)
        return MPP_MALLOC_ERROR;
    memset(m, 0, sizeof(*m));
    /* copy params*/
    m->params = *params;

    /* link to parent mpp */
    m->hook = _mpp->last_elem;

    if (params->exec_flag != MPP_EXEC_PREEMPT)
    {
        MPP_LOGE("\n\rMPP in background must have exec_flag = MPP_EXEC_PREEMPT");
        return MPP_ERROR;
    }
    /* insert split on the preemptable heap */
    mpp_heap_insert(m, preempt_prio_lst);
    m->exec_heap = preempt_prio_lst;

    /* open new pipeline */
    /* close the old one */
    m->status = MPP_OPENED;
    m->oper_status = MPP_NOT_STARTED;
    _mpp->status = MPP_CLOSED;
    m->status_sema = hal_sema_create_binary();

    /* return the handle to user */
    *out_mpp = (mpp_t) m;

    return ret;
}

/*
 * Link a newly built (closed, not-yet-started) pipeline as a live branch
 * off an existing element in a running pipeline.
 *
 * The new pipeline must have been fully constructed (mpp_create +
 * mpp_element_add + ... + mpp_close equivalent) before this call.
 * Its exec_flag (set at mpp_create time) determines which heap it joins.
 *
 * Requirements:
 *  - hook_elem_h must be a valid element in a running pipeline
 *  - new_mpp must be closed (MPP_CLOSED) and not running
 *  - If new_mpp->first_elem does not own its input buffer (ALLOC_NONE /
 *    ALLOC_OUTPUT policy), the hook element's output buffer must already
 *    be allocated (heap_p != NULL) because live re-allocation is not
 *    possible.
 *  - If new_mpp->first_elem owns its input (ALLOC_INPUT / ALLOC_BOTH)
 *    and the hook element also allocates its output (ALLOC_BOTH / ALLOC_OUTPUT),
 *    an unresolvable live buffer conflict exists; the function returns an error.
 */
int mpp_link_branch(mpp_elem_handle_t hook_elem_h, mpp_t new_mpp)
{
    int ret = MPP_SUCCESS;

    if (hook_elem_h == 0 || new_mpp == MPP_INVALID)
        return MPP_INVALID_PARAM;

    _elem_t *hook_elem = (_elem_t *)hook_elem_h;
    _mpp_t  *_new      = (_mpp_t *)new_mpp;

    /* new pipeline must be fully built and closed */
    if (_new->status != MPP_CLOSED)
    {
        MPP_LOGE("mpp_link_branch: new_mpp @%p is not closed\n", _new);
        return MPP_ERROR;
    }
    if (_new->oper_status == MPP_RUNNING)
    {
        MPP_LOGE("mpp_link_branch: new_mpp @%p is already running\n", _new);
        return MPP_ERROR;
    }
    if (_new->first_elem == NULL)
    {
        MPP_LOGE("mpp_link_branch: new_mpp @%p has no elements\n", _new);
        return MPP_ERROR;
    }
    if (hook_elem->mpp == NULL)
    {
        MPP_LOGE("mpp_link_branch: hook element @%p has no owning pipeline\n", hook_elem);
        return MPP_ERROR;
    }

    /* exec_flag constraint: RC branch not allowed on a preempt-only parent */
    if (_new->params.exec_flag == MPP_EXEC_RC &&
        hook_elem->mpp->exec_heap == preempt_prio_lst)
    {
        MPP_LOGE("mpp_link_branch: RC branch not allowed on preempt-only parent\n");
        return MPP_ERROR;
    }

    /* check buffer compatibility between hook element and first element of new branch */
    _elem_t *first = _new->first_elem;
    mpp_memory_policy_t first_policy = first->io.mem_policy;
    mpp_memory_policy_t hook_policy  = hook_elem->io.mem_policy;

    if (first_policy == HAL_MEM_ALLOC_NONE || first_policy == HAL_MEM_ALLOC_OUTPUT)
    {
        /* first element does not own its input: needs hook's output buffer to exist */
        bool buf_allocated = false;
        for (int i = 0; i < hook_elem->io.nb_out_buf; i++)
        {
            if (hook_elem->io.out_buf[i] != NULL &&
                hook_elem->io.out_buf[i]->hw != NULL &&
                hook_elem->io.out_buf[i]->hw->heap_p != NULL)
            {
                buf_allocated = true;
                break;
            }
        }
        if (!buf_allocated && hook_elem->mpp->oper_status == MPP_RUNNING)
        {
            MPP_LOGE("mpp_link_branch: hook elem @%p output buffer not yet allocated"
                     " and parent pipeline is running\n", hook_elem);
            return MPP_ERROR;
        }
    }
    else
    {
        /* first element owns its own input buffer (ALLOC_INPUT or ALLOC_BOTH).
         * If the hook also provides an output buffer (ALLOC_BOTH or ALLOC_OUTPUT),
         * the conflict cannot be resolved at runtime. */
        if (hook_policy == HAL_MEM_ALLOC_BOTH || hook_policy == HAL_MEM_ALLOC_OUTPUT)
        {
            MPP_LOGE("mpp_link_branch: unresolvable live buffer conflict between"
                     " hook elem @%p (policy %d) and first elem @%p (policy %d)\n",
                     hook_elem, hook_policy, first, first_policy);
            return MPP_ERROR;
        }
    }

    /* find a free next[] slot on hook element (slots 1..MPP_MAX_BRANCH_NUM-1) */
    int slot = -1;
    for (int i = 1; i < MPP_MAX_BRANCH_NUM; i++)
    {
        if (hook_elem->next[i] == NULL)
        {
            slot = i;
            break;
        }
    }
    if (slot < 0)
    {
        MPP_LOGE("mpp_link_branch: no free branch slot on hook elem @%p\n", hook_elem);
        return MPP_ERROR;
    }

    /* assign heap and hook relationship */
    _new->hook = hook_elem;
    switch (_new->params.exec_flag)
    {
    case MPP_EXEC_RC:
        mpp_heap_insert(_new, rc_prio_lst);
        _new->exec_heap = rc_prio_lst;
        break;
    case MPP_EXEC_PREEMPT:
        mpp_heap_insert(_new, preempt_prio_lst);
        _new->exec_heap = preempt_prio_lst;
        break;
    case MPP_EXEC_INHERIT:
    default:
        mpp_heap_insert(_new, hook_elem->mpp->exec_heap);
        _new->exec_heap = hook_elem->mpp->exec_heap;
        break;
    }

    /* wire first element's prev and in_buf[] from hook output.
     * set_in_buff_from_prev_elem uses hook_elem->next[] to find its slot index,
     * so we temporarily wire the slot before calling it, then use the result. */
    first->prev = hook_elem;
    hook_elem->next[slot] = first;  /* temporary for set_in_buff_from_prev_elem */

    set_in_buff_from_prev_elem(first);

    /* clear the slot again — we re-wire it atomically after allocation succeeds */
    hook_elem->next[slot] = NULL;

    /* allocate buffers for all elements in the new pipeline.
     * mpp_alloc_input_buf skips buffers with heap_p != NULL, so already-allocated
     * hook output buffers (shared as input to first elem) are never re-allocated. */
    ret = mpp_memory_alloc(_new);
    if (ret != MPP_SUCCESS)
    {
        MPP_LOGE("mpp_link_branch: buffer allocation failed for new_mpp @%p\n", _new);
        mpp_memory_free(_new);
        mpp_heap_remove(_new);
        first->prev = NULL;
        _new->hook = NULL;
        return ret;
    }

    /* initialise buffer status for newly allocated buffers */
    mpp_memory_check(_new);

    /* start sink HAL if present (mirrors mpp_start logic) */
    if (_new->last_elem != NULL && _new->last_elem->type == MPP_TYPE_SINK)
    {
        switch (_new->last_elem->sink_typ)
        {
        case MPP_SINK_DISPLAY:
        {
            _display_dev_t *disp = _new->last_elem->dev.disp;
            if (disp && disp->dev.ops && disp->dev.ops->start)
            {
                if (disp->dev.ops->start(&disp->dev) != 0)
                {
                    MPP_LOGE("mpp_link_branch: display sink fails to start\n");
                    ret = MPP_ERROR;
                }
            }
            break;
        }
        case MPP_SINK_FILE:
        {
            _filesink_t *filesink = _new->last_elem->dev.filesink;
            if (filesink && filesink->dev.ops && filesink->dev.ops->start)
            {
                if (filesink->dev.ops->start(&filesink->dev) != 0)
                {
                    MPP_LOGE("mpp_link_branch: file sink fails to start\n");
                    ret = MPP_ERROR;
                }
            }
            break;
        }
        case MPP_SINK_RTSP:
        {
            _rtspsink_t *rtspsink = _new->last_elem->dev.rtspsink;
            if (rtspsink && rtspsink->dev.ops && rtspsink->dev.ops->start)
            {
                if (rtspsink->dev.ops->start(&rtspsink->dev) != 0)
                {
                    MPP_LOGE("mpp_link_branch: RTSP sink fails to start\n");
                    ret = MPP_ERROR;
                }
            }
            break;
        }
        case MPP_SINK_MC:
        {
            _multicore_dev_t *mc = _new->last_elem->dev.mc;
            if (mc && mc->dev.ops && mc->dev.ops->start)
            {
                if (mc->dev.ops->start(&mc->dev) != 0)
                {
                    MPP_LOGE("mpp_link_branch: MC sink fails to start\n");
                    ret = MPP_ERROR;
                }
            }
            break;
        }
        default:
            break;
        }
    }

    if (ret != MPP_SUCCESS)
    {
        mpp_memory_free(_new);
        mpp_heap_remove(_new);
        first->prev = NULL;
        _new->hook = NULL;
        return ret;
    }

    /* set running: force-update so the first pass is not skipped */
    _new->force_update = true;
    _new->oper_status  = MPP_RUNNING;
    hal_sema_give(_new->status_sema);

    /* atomically wire the hook next[] slot — parent pipeline can now deliver frames */
    {
        hal_ctx_t ctx;
        hal_atomic_enter(&ctx);
        hook_elem->next[slot] = first;
        hal_atomic_exit(&ctx);
    }

    /* update split count on parent */
    hook_elem->mpp->split_cnt++;

    MPP_LOGI("mpp_link_branch: new_mpp @%p linked to hook elem @%p slot %d\n",
             _new, hook_elem, slot);
    return MPP_SUCCESS;
}

bool mpp_is_running(mpp_t mpp)
{
    if (mpp == MPP_INVALID)
        return false;

    _mpp_t *_mpp = (_mpp_t *)mpp;

    return (_mpp->oper_status == MPP_RUNNING);
}

int mpp_start(mpp_t mpp, int last, bool force_update)
{

    int ret = MPP_SUCCESS;

    if (mpp == MPP_INVALID)
        return MPP_INVALID_PARAM;
    _mpp_t *_mpp = (_mpp_t *)mpp;

#if defined(MCMGR_USED)
    /* Check if this is a remote call */
    mcmgr_core_t current_core = MCMGR_GetCurrentCore();
    if (_mpp->params.core_id != current_core)
    {
        /* Send remote API call using mpp_multicore.c */
        mpp_remote_api_msg_t msg, response;
        msg.api_id = MPP_API_START;
        msg.payload.start_params.mpp = mpp;
        msg.payload.start_params.last = last;
        msg.payload.start_params.force_update = force_update;

        ret = mpp_mc_remote_api_send(&msg, &response, _mpp->params.core_id);

        if (ret != MPP_SUCCESS)
        {
            MPP_LOGE("Failed to send remote mpp_start call (error %d)\r\n", ret);
            return MPP_ERROR;
        }

        return ret;
    }
#endif

    if (_mpp->status != MPP_CLOSED)
        return MPP_ERROR;
    /* no start allowed after the last one (unless mpp has been stopped before) */
    if (is_last && ((_mpp->oper_status != MPP_STOPPED) || last)) {
        MPP_LOGE("The pipeline execution has already started\n");
        return MPP_ERROR;
    }

    if (_mpp->oper_status == MPP_RUNNING)
        goto last;

    /* start source / sink */
    if ((_mpp->first_elem->type == MPP_TYPE_SOURCE) && (_mpp->first_elem->src_typ== MPP_SRC_CAMERA))
    {
        _elem_t *elem = _mpp->first_elem;
        _camera_dev_t *cam = elem->dev.cam;
        uint32_t min_req_cnt = 0;
        mpp_exec_flag_t req_cnt_type = MPP_EXEC_RC;

        /* Set the minimum number of stream enqueue calls until the enqueue to camera is completed */
        for (int i = 0; i < MPP_MAX_BRANCH_NUM; i++)
        {
            if ((elem->next[i]) && 
                ((elem->next[i]->mpp->oper_status == MPP_RUNNING) || (elem->next[i]->mpp == _mpp)) && 
                (elem->next[i]->mpp->params.exec_flag == MPP_EXEC_RC))
                min_req_cnt++;
        }

        /* Check if there is no RC stream running */
        if (min_req_cnt == 0)
        {
            /* In this case, set the min_req_cnt to the number of active PREEMPT streams */
            for (int i = 0; i < MPP_MAX_BRANCH_NUM; i++)
            {
                if ((elem->next[i]) && 
                    ((elem->next[i]->mpp->oper_status == MPP_RUNNING) || (elem->next[i]->mpp == _mpp)) &&
                    (elem->next[i]->mpp->params.exec_flag == MPP_EXEC_PREEMPT))
                    min_req_cnt++;
            }
            if (min_req_cnt == 0)
                MPP_LOGI("No active streams found for camera dequeue\r\n");
            else
                req_cnt_type = MPP_EXEC_PREEMPT;
        }

        cam->dev.config.min_stream_req_cnt = min_req_cnt;
        cam->dev.config.req_cnt_type = req_cnt_type;

        /* start HAL function */
        if (cam->dev.ops->start != NULL) {
            if (cam->dev.ops->lock != NULL) {
                ret = cam->dev.ops->lock(&cam->dev);
                if (ret != kStatus_HAL_CameraSuccess)
                {
                    MPP_LOGE("Failed to lock camera device\n");
                    return MPP_ERROR;
                }
            }
            for (int i = 0; i < cam->dev.config.n_streams; i++)
                cam->dev.config.stream_requested[i] = cam->dev.config.stream[i].active;
            if (cam->dev.ops->start(&cam->dev) != 0) {
                MPP_LOGE("camera fails to start\n");
                return MPP_ERROR;
            }
            if (cam->dev.ops->unlock != NULL) {
                ret = cam->dev.ops->unlock(&cam->dev);
                if (ret != kStatus_HAL_CameraSuccess)
                {
                    MPP_LOGE("Failed to unlock camera device\n");
                    return MPP_ERROR;
                }
            }
        }
    }
    else if ((_mpp->first_elem->type == MPP_TYPE_SOURCE) && (_mpp->first_elem->src_typ== MPP_SRC_MC))
    {
        _elem_t *elem = _mpp->first_elem;
        _multicore_dev_t *mc = elem->dev.mc;
        /* start HAL function */
        if (mc->dev.ops->start != NULL) {
            if (mc->dev.ops->start(&mc->dev) != 0) {
                MPP_LOGE("MC source fails to start\n");
                return MPP_ERROR;
            }
        }
    }
    else if ((_mpp->first_elem->type == MPP_TYPE_SOURCE) && (_mpp->first_elem->src_typ== MPP_SRC_RTSP))
    {
        _elem_t *elem = _mpp->first_elem;
        _rtspsrc_t *rtspsrc = elem->dev.rtspsrc;
        /* start HAL function */
        if (rtspsrc->dev.ops->start != NULL) {
            if (rtspsrc->dev.ops->start(&rtspsrc->dev) != 0) {
                MPP_LOGE("RTSP source fails to start\n");
                return MPP_ERROR;
            }
        }
    }

    if ((_mpp->last_elem->type == MPP_TYPE_SINK) && (_mpp->last_elem->sink_typ == MPP_SINK_DISPLAY))
    {
        _display_dev_t *disp = _mpp->last_elem->dev.disp;
        /* start HAL function */
        if (disp->dev.ops->start != NULL) {
            if (disp->dev.ops->start(&disp->dev) != 0) {
                MPP_LOGE("display fails to start\n");
                return MPP_ERROR;
            }
        }
    }
    else if ((_mpp->last_elem->type == MPP_TYPE_SINK) && (_mpp->last_elem->sink_typ == MPP_SINK_FILE))
    {
        _filesink_t *filesink = _mpp->last_elem->dev.filesink;
        /* start HAL function */
        if (filesink->dev.ops->start != NULL) {
            if (filesink->dev.ops->start(&filesink->dev) != 0) {
                MPP_LOGE("file sink fails to start\n");
                return MPP_ERROR;
            }
        }
    }
    else if ((_mpp->last_elem->type == MPP_TYPE_SINK) && (_mpp->last_elem->sink_typ == MPP_SINK_RTSP))
    {
        _rtspsink_t *rtspsink = _mpp->last_elem->dev.rtspsink;
        /* start HAL function */
        if (rtspsink->dev.ops->start != NULL) {
            if (rtspsink->dev.ops->start(&rtspsink->dev) != 0) {
                MPP_LOGE("RTSP sink fails to start\n");
                return MPP_ERROR;
            }
        }
    }
    else if ((_mpp->last_elem->type == MPP_TYPE_SINK) && (_mpp->last_elem->sink_typ == MPP_SINK_MC))
    {
        _multicore_dev_t *mc = _mpp->last_elem->dev.mc;
        /* start HAL function */
        if (mc->dev.ops->start != NULL) {
            if (mc->dev.ops->start(&mc->dev) != 0) {
                MPP_LOGE("multicore sink fails to start\n");
                return MPP_ERROR;
            }
        }
    }

    /* If force update was requested before and
     * the pipeline did not run at least once, do not overwrite it */
    if (!_mpp->force_update)
        _mpp->force_update = force_update;
    _mpp->oper_status = MPP_RUNNING;
    bool released = hal_sema_give(_mpp->status_sema);
    if (!released)
    {
        MPP_LOGD("Pipeline unlocking issue\r\n");
    }

last:
    if (last) {
        /* run memory manager */
        ret = mpp_memory_manage_heap(rc_prio_lst);
        if (ret == MPP_SUCCESS)
            ret = mpp_memory_manage_heap(preempt_prio_lst);
        mpp_memory_check_list(rc_prio_lst);
        mpp_memory_check_list(preempt_prio_lst);
        if (ret == MPP_SUCCESS)
        {
            /* start pipeline processing by releasing semaphore */
            hal_sema_give(xCtlStartSem);
            is_last = 1;
        }
    }

    return ret;
}

int mpp_stop(mpp_t mpp)
{
    int ret = MPP_SUCCESS;

    do {
        if (mpp == MPP_INVALID) {
            MPP_LOGE("failed to stop mpp: invalid mpp object\n");
            ret = MPP_INVALID_PARAM;
            break;
        }

        _mpp_t *_mpp = (_mpp_t *)mpp;

#if defined(MCMGR_USED)
        /* Check if this is a remote call */
        mcmgr_core_t current_core = MCMGR_GetCurrentCore();
        if (_mpp->params.core_id != current_core)
        {
            /* Send remote API call using mpp_multicore.c */
            mpp_remote_api_msg_t msg, response;
            msg.api_id = MPP_API_STOP;
            msg.payload.stop_params.mpp = mpp;

            ret = mpp_mc_remote_api_send(&msg, &response, _mpp->params.core_id);

            if (ret != MPP_SUCCESS)
            {
                MPP_LOGE("Failed to send remote mpp_stop call (error %d)\r\n", ret);
                return ret;
            }

            return ret;
        }
#endif

        if (_mpp->status != MPP_CLOSED) {
            MPP_LOGE("failed to stop mpp: pipeline is not closed\n");
            ret = MPP_INVALID_PARAM;
            break;
        }

        if (_mpp->oper_status == MPP_STOPPED) {
            MPP_LOGI("failed to stop mpp: mpp is already stopped\n");
            break;
        }

        /* wait for pipeline processing to be finished */
        bool released = hal_sema_take(_mpp->status_sema, HAL_MAX_TIMEOUT);
        if (!released)
        {
            MPP_LOGE("Stop timeout while waiting to finish processing\r\n");
            break;
        }

        /* execution of mpp should be stopped before peripherals */
        _mpp->oper_status = MPP_STOPPED;

        /* stop source */
        if (_mpp->first_elem->type == MPP_TYPE_SOURCE)
        {
            switch (_mpp->first_elem->src_typ)
            {
            case MPP_SRC_CAMERA:
                _camera_dev_t *cam = _mpp->first_elem->dev.cam;
                /* stop HAL function */
                if (cam->dev.ops->stop != NULL)
                {
                    if (cam->dev.ops->lock != NULL) {
                        ret = cam->dev.ops->lock(&cam->dev);
                        if (ret != kStatus_HAL_CameraSuccess)
                        {
                            MPP_LOGE("Failed to lock camera device\n");
                            return MPP_ERROR;
                        }
                    }
                    if (cam->dev.ops->stop(&cam->dev) != 0)
                    {
                        MPP_LOGE("camera fails to stop\n");
                        ret = MPP_ERROR;
                    }
                    /* Reset camera stream configuration:
                     * - Clear current stream request count
                     * - Mark all streams as not requested
                     * - Clear all stream requesters so the next pipeline cycle
                     *   starts with a clean requester table */
                    cam->dev.config.crt_stream_req_cnt = 0;
                    for (int i = 0; i < NUM_STREAMS; i++)
                    {
                        cam->dev.config.stream_requested[i] = false;
                        for (int j = 0; j < MAX_STREAM_REQUESTERS; j++)
                            cam->dev.config.stream_requester[i][j] = NULL;
                    }
                    if (cam->dev.ops->unlock != NULL) {
                        ret = cam->dev.ops->unlock(&cam->dev);
                        if (ret != kStatus_HAL_CameraSuccess)
                        {
                            MPP_LOGE("Failed to unlock camera device\n");
                            return MPP_ERROR;
                        }
                    }
                }
                break;
            case MPP_SRC_MC:
                _multicore_dev_t *mc = _mpp->first_elem->dev.mc;
                if (mc->dev.ops->stop != NULL) {
                   if (mc->dev.ops->stop(&mc->dev) != 0)
                    {
                        MPP_LOGE("MC source fails to stop\n");
                        ret = MPP_ERROR;
                    }
                }
                break;
            case MPP_SRC_RTSP:
                _rtspsrc_t *rtspsrc = _mpp->first_elem->dev.rtspsrc;
                if (rtspsrc->dev.ops->stop != NULL) {
                   if (rtspsrc->dev.ops->stop(&rtspsrc->dev) != 0)
                    {
                        MPP_LOGE("RTSP source fails to stop\n");
                        ret = MPP_ERROR;
                    }
                }
                break;
            case MPP_SRC_STATIC_IMAGE:
            case MPP_SRC_FILE:
                MPP_LOGI("No stop implementation for source type [%d]\n",
                        _mpp->first_elem->type);
                break;
            default:
                MPP_LOGE("source type [%d] is invalid\n", _mpp->first_elem->type);
                break;
            }
        }

        if (ret != MPP_SUCCESS)
            break;

        /* stop sink */
        if (_mpp->last_elem->type == MPP_TYPE_SINK)
        {

            switch (_mpp->last_elem->sink_typ) {
            case MPP_SINK_DISPLAY:
                _display_dev_t *disp = _mpp->last_elem->dev.disp;
                /* stop HAL function */
                if (disp->dev.ops->stop != NULL)
                {
                    if (disp->dev.ops->stop(&disp->dev) != 0)
                    {
                        MPP_LOGE("display fails to stop\n");
                        ret = MPP_ERROR;
                    }
                }
                break;
            case MPP_SINK_FILE:
                _filesink_t *filesink = _mpp->last_elem->dev.filesink;
                /* stop HAL function */
                if (filesink->dev.ops->stop != NULL)
                {
                    if (filesink->dev.ops->stop(&filesink->dev) != 0)
                    {
                        MPP_LOGE("file sink fails to stop\n");
                        ret = MPP_ERROR;
                    }
                }
                break;
            case MPP_SINK_RTSP:
                _rtspsink_t *rtspsink = _mpp->last_elem->dev.rtspsink;
                /* stop HAL function */
                if (rtspsink->dev.ops->stop != NULL)
                {
                    if (rtspsink->dev.ops->stop(&rtspsink->dev) != 0)
                    {
                        MPP_LOGE("RTSP sink fails to stop\n");
                        ret = MPP_ERROR;
                    }
                }
                break;
            case MPP_SINK_MC:
                _multicore_dev_t *mc = _mpp->last_elem->dev.mc;
                /* stop HAL function */
                if (mc->dev.ops->stop != NULL)
                {
                    if (mc->dev.ops->stop(&mc->dev) != 0)
                    {
                        MPP_LOGE("multicore sink fails to stop\n");
                        ret = MPP_ERROR;
                    }
                }
                break;
            case MPP_SINK_NULL:
                MPP_LOGI("No stop implementation for sink type [%d]\n",
                        _mpp->last_elem->type);
                break;
            default:
                MPP_LOGE("sink type [%d] is invalid\n", _mpp->last_elem->type);
                break;
            }
        }
    } while (false);
    return ret;
}

int mpp_force_update(mpp_t mpp)
{
    int ret = MPP_SUCCESS;

    if (mpp == MPP_INVALID)
    {
        MPP_LOGE("failed to force update mpp: invalid mpp object\r\n");
        return MPP_INVALID_PARAM;
    }

    _mpp_t *_mpp = (_mpp_t *)mpp;

#if defined(MCMGR_USED)
    /* Check if this is a remote call */
    mcmgr_core_t current_core = MCMGR_GetCurrentCore();
    if (_mpp->params.core_id != current_core)
    {
        /* Send remote API call using mpp_multicore.c */
        mpp_remote_api_msg_t msg, response;
        msg.api_id = MPP_API_FORCE_UPDATE;
        msg.payload.force_update_params.mpp = mpp;

        ret = mpp_mc_remote_api_send(&msg, &response, _mpp->params.core_id);

        if (ret != MPP_SUCCESS)
        {
            MPP_LOGE("Failed to send remote mpp_force_update call (error %d)\r\n", ret);
            return MPP_ERROR;
        }

        return ret;
    }
#endif

    /* If force update was requested before and
     * the pipeline did not run at least once, do not overwrite it */
    _mpp->force_update = true;

    return ret;
}

int mpp_element_update(mpp_t mpp, mpp_elem_handle_t elem_h, mpp_element_params_t *params, bool force_update)
{
    int ret = MPP_SUCCESS;
    _elem_t *elem = (_elem_t *) elem_h;

    do {
        if (!mpp) {
            MPP_LOGE("invalid mpp pointer @%p\n", mpp);
            ret = MPP_INVALID_PARAM;
            break;
        }

        /* check if element exist or not */
        if (elem == MPP_INVALID) {
            MPP_LOGE("invalid element handle @%p\n", elem);
            ret = MPP_INVALID_PARAM;
            break;
        }

        if (!elem->mpp) {
            MPP_LOGE("element has NULL mpp pointer @%p\n", elem);
            ret = MPP_INVALID_PARAM;
            break;
        }

        /* check if element belongs to the same mpp */
        if (elem->mpp != mpp) {
            MPP_LOGE("element seems invalid wrt pipeline @%p\n", elem);
            ret = MPP_INVALID_PARAM;
            break;
        }

#if defined(MCMGR_USED)
        /* Check if this is a remote call */
        _mpp_t *_mpp = (_mpp_t *)mpp;
        mcmgr_core_t current_core = MCMGR_GetCurrentCore();
        if (_mpp->params.core_id != current_core)
        {
            /* Send remote API call using mpp_multicore.c */
            mpp_remote_api_msg_t msg, response;
            msg.api_id = MPP_API_ELEMENT_UPDATE;
            msg.payload.element_update_params.mpp = mpp;
            msg.payload.element_update_params.elem_h = elem_h;
            msg.payload.element_update_params.params = *params;
            msg.payload.element_update_params.force_update = force_update;

            ret = mpp_mc_remote_api_send(&msg, &response, _mpp->params.core_id);

            if (ret != MPP_SUCCESS)
            {
                MPP_LOGE("Failed to send remote mpp_element_update call (error %d)\r\n", ret);
                return MPP_ERROR;
            }

            return ret;
        }
#endif

        /* force an update: ensures processing steps will not be skipped in this branch */
        /* If force update was requested before and
        * the pipeline did not run at least once, do not overwrite it */
        if (!elem->mpp->force_update)
            elem->mpp->force_update = force_update;

        /* Check element type */
        switch (elem->type) {
        case MPP_TYPE_SOURCE:
            /* Check source type */
            switch (elem->src_typ) {
            case MPP_SRC_CAMERA:
                ret = mpp_camera_update(elem, params);
                break;
            case MPP_SRC_STATIC_IMAGE:
                ret = mpp_static_image_update(elem, params);
                break;
            default:
                MPP_LOGI("Nothing to update for source type %d\n", elem->src_typ);
                break;
            }
            break;
        case MPP_TYPE_PROC:
            /* Copy the pointer to stats to the new params structure */
            if (params->stats == NULL) params->stats = elem->params.stats;

            /* check processing element id */
            switch (elem->proc_typ) {
            case MPP_ELEMENT_LABELED_RECTANGLE:
                ret = mpp_lbl_rectangle_update(elem, params);
                break;
            case MPP_ELEMENT_CONVERT:
                ret = mpp_convert_update(elem, params);
                break;
            case MPP_ELEMENT_IMG_COMPOSE:
                ret = mpp_compose_update(elem, params);
                break;
            case MPP_ELEMENT_INFERENCE:
                ret = mpp_inference_update(elem, params);
                break;
            case MPP_ELEMENT_IMG_QUALITY_CHECK:
                ret = mpp_img_quality_check_update(elem, params);
                break;
            default:
                MPP_LOGI("Nothing to update for element %s\n", elem_name(elem));
                break;
            }
            break;
        default:
            MPP_LOGI("Nothing to update for element type %d\n", elem->type);
            break;
        }
    } while (false);

    return ret;
}

int mpp_multipass_element_update(mpp_t mpp, mpp_elem_handle_t elem_h,
                             mpp_element_params_t *params_array, uint32_t n_params,
                             bool force_update)
{
    int ret = MPP_SUCCESS;
    _elem_t *elem = (_elem_t *) elem_h;

    if (!mpp || !params_array || n_params == 0)
        return MPP_INVALID_PARAM;

    if (elem == MPP_INVALID || !elem->mpp || elem->mpp != mpp)
        return MPP_INVALID_PARAM;

#if defined(MCMGR_USED) && defined(CONFIG_MPP_MC_API_WAIT_FOR_RESPONSE) && CONFIG_MPP_MC_API_WAIT_FOR_RESPONSE
    /* Remote dispatch for mpp_multipass_element_update.
     * Only supported in blocking mode (CONFIG_MPP_MC_API_WAIT_FOR_RESPONSE=1):
     * the message carries a pointer to params_array; we block here until the
     * remote core finishes, so the caller's buffer remains valid throughout.
     * In fire-and-forget mode the pointer would be dangerously stale by the
     * time the remote core dereferences it, so the feature is disabled. */
    {
        _mpp_t *_mpp = (_mpp_t *)mpp;
        mcmgr_core_t current_core = MCMGR_GetCurrentCore();
        if (_mpp->params.core_id != current_core)
        {
            mpp_remote_api_msg_t msg, response;
            msg.api_id = MPP_API_MULTIPASS_ELEMENT_UPDATE;
            msg.payload.multipass_element_update_params.mpp = mpp;
            msg.payload.multipass_element_update_params.elem_h = elem_h;
            msg.payload.multipass_element_update_params.n_params = n_params;
            msg.payload.multipass_element_update_params.force_update = force_update;
            msg.payload.multipass_element_update_params.params_array = params_array;

            ret = mpp_mc_remote_api_send(&msg, &response, _mpp->params.core_id);

            if (ret != MPP_SUCCESS)
            {
                MPP_LOGE("Failed to send remote mpp_multipass_element_update call (error %d)\r\n", ret);
                return MPP_ERROR;
            }

            return ret;
        }
    }
#endif

    /* allocate working copy; element-specific update will complete all entries in-place */
    mpp_element_params_t *new_arr = hal_malloc(n_params * sizeof(mpp_element_params_t));
    if (!new_arr)
        return MPP_MALLOC_ERROR;
    memcpy(new_arr, params_array, n_params * sizeof(mpp_element_params_t));

    /* apply force_update flag */
    if (!elem->mpp->force_update)
        elem->mpp->force_update = force_update;

    /* element-type-specific update: validates, completes all pass entries in new_arr,
     * and updates elem->params + gfx structure for entry 0. */
    if (elem->type == MPP_TYPE_PROC) {
        /* preserve stats pointer if caller did not supply one */
        for (uint32_t i = 0; i < n_params; i++) {
            if (new_arr[i].stats == NULL)
                new_arr[i].stats = elem->params.stats;
        }
        switch (elem->proc_typ) {
        case MPP_ELEMENT_CONVERT:
            ret = mpp_convert_multipass_update(elem, new_arr, n_params);
            break;
        default:
            MPP_LOGE("element %s does not support multipass update\n", elem_name(elem));
            ret = MPP_UNSUPPORTED;
            break;
        }
    }
    else
    {
        MPP_LOGE("element type %d does not support multipass update\r\n", elem->type);
        ret = MPP_UNSUPPORTED;
    }

    if (ret != MPP_SUCCESS) {
        hal_free(new_arr);
        return ret;
    }

    /* swap pass_params array atomically after successful update */
    hal_ctx_t ctx;
    hal_atomic_enter(&ctx);
    mpp_element_params_t *old_arr = elem->pass_params;
    elem->pass_params = new_arr;
    elem->n_pass_params = n_params;
    hal_atomic_exit(&ctx);
    if (old_arr != NULL)
        hal_free(old_arr);

    return ret;
}

/*
 * Teardown a single element: call HAL deinit, free device structs and out_buf descriptors.
 * Input buffer descriptors are owned by the previous element and must not be freed here.
 */
static void mpp_elem_teardown(_elem_t *elem)
{
    int i;

    if (elem == NULL)
        return;

    MPP_LOGI("mpp_elem_teardown: elem@%p type=%d\n", elem, elem->type);

    switch (elem->type)
    {
    case MPP_TYPE_SOURCE:
        switch (elem->src_typ)
        {
        case MPP_SRC_CAMERA:
        {
            _camera_dev_t *cam = elem->dev.cam;
            if (cam)
            {
                MPP_LOGI("mpp_elem_teardown: camera src '%s' @%p\n", cam->name, cam);
                if (cam->dev.ops && cam->dev.ops->deinit)
                    cam->dev.ops->deinit(&cam->dev);
                for (i = 0; i < cam->params.n_streams && i < MAX_OUTPUT_PORTS; i++)
                {
                    hal_free(elem->io.out_buf[i]);
                    elem->io.out_buf[i] = NULL;
                }
                hal_free(cam);
                elem->dev.cam = NULL;
            }
            break;
        }
        case MPP_SRC_STATIC_IMAGE:
        {
            _static_image_t *img = elem->dev.img;
            if (img)
            {
                MPP_LOGI("mpp_elem_teardown: static image src @%p\n", img);
                // if (img->elt.ops && img->elt.ops->deinit)
                //     img->elt.ops->deinit(&img->elt);
                for (i = 0; i < MAX_OUTPUT_PORTS; i++)
                    if (elem->io.out_buf[i]) { hal_free(elem->io.out_buf[i]); elem->io.out_buf[i] = NULL; }
                hal_free(img);
                elem->dev.img = NULL;
            }
            break;
        }
        case MPP_SRC_FILE:
        {
            _filesrc_t *filesrc = elem->dev.filesrc;
            if (filesrc)
            {
                MPP_LOGI("mpp_elem_teardown: file src @%p\n", filesrc);
                if (filesrc->dev.ops && filesrc->dev.ops->deinit)
                    filesrc->dev.ops->deinit(&filesrc->dev);
                for (i = 0; i < MAX_OUTPUT_PORTS; i++)
                    if (elem->io.out_buf[i]) { hal_free(elem->io.out_buf[i]); elem->io.out_buf[i] = NULL; }
                hal_free(filesrc);
                elem->dev.filesrc = NULL;
            }
            break;
        }
        case MPP_SRC_MC:
        {
            _multicore_dev_t *mc = elem->dev.mc;
            if (mc)
            {
                MPP_LOGI("mpp_elem_teardown: MC src '%s' @%p\n", mc->name, mc);
                if (mc->dev.ops && mc->dev.ops->deinit)
                    mc->dev.ops->deinit(&mc->dev);
                for (i = 0; i < elem->io.nb_out_buf; i++)
                {
                    hal_free(elem->io.out_buf[i]);
                    elem->io.out_buf[i] = NULL;
                }
                hal_free(mc);
                elem->dev.mc = NULL;
            }
            break;
        }
        default:
            MPP_LOGE("mpp_elem_teardown: unknown source type %d\n", elem->src_typ);
            break;
        }
        break;

    case MPP_TYPE_SINK:
        switch (elem->sink_typ)
        {
        case MPP_SINK_DISPLAY:
        {
            _display_dev_t *disp = elem->dev.disp;
            if (disp)
            {
                MPP_LOGI("mpp_elem_teardown: display sink '%s' @%p\n", disp->name, disp);
                if (disp->dev.ops && disp->dev.ops->deinit)
                    disp->dev.ops->deinit(&disp->dev);
                hal_free(disp);
                elem->dev.disp = NULL;
            }
            break;
        }
        case MPP_SINK_FILE:
        {
            _filesink_t *filesink = elem->dev.filesink;
            if (filesink)
            {
                MPP_LOGI("mpp_elem_teardown: file sink @%p\n", filesink);
                if (filesink->dev.ops && filesink->dev.ops->deinit)
                    filesink->dev.ops->deinit(&filesink->dev);
                hal_free(filesink);
                elem->dev.filesink = NULL;
            }
            break;
        }
        case MPP_SINK_RTSP:
        {
            _rtspsink_t *rtspsink = elem->dev.rtspsink;
            if (rtspsink)
            {
                MPP_LOGI("mpp_elem_teardown: RTSP sink @%p\n", rtspsink);
                if (rtspsink->dev.ops && rtspsink->dev.ops->deinit)
                    rtspsink->dev.ops->deinit(&rtspsink->dev);
                hal_free(rtspsink);
                elem->dev.rtspsink = NULL;
            }
            break;
        }
        case MPP_SINK_MC:
        {
            _multicore_dev_t *mc = elem->dev.mc;
            if (mc)
            {
                MPP_LOGI("mpp_elem_teardown: MC sink '%s' @%p\n", mc->name, mc);
                if (mc->dev.ops && mc->dev.ops->deinit)
                    mc->dev.ops->deinit(&mc->dev);
                hal_free(mc);
                elem->dev.mc = NULL;
            }
            break;
        }
        case MPP_SINK_NULL:
            /* null sink requires no cleanup */
            break;
        default:
            MPP_LOGE("mpp_elem_teardown: unknown sink type %d\n", elem->sink_typ);
            break;
        }
        break;

    case MPP_TYPE_PROC:
        switch (elem->proc_typ)
        {
        case MPP_ELEMENT_CONVERT:
        {
            gfx_dev_t *gfx = elem->dev.gfx;
            if (gfx)
            {
                MPP_LOGI("mpp_elem_teardown: convert (gfx) @%p\n", gfx);
                if (gfx->ops && gfx->ops->deinit)
                    gfx->ops->deinit(gfx);
                for (i = 0; i < MAX_OUTPUT_PORTS; i++)
                    if (elem->io.out_buf[i]) { hal_free(elem->io.out_buf[i]); elem->io.out_buf[i] = NULL; }
                hal_free(gfx);
                elem->dev.gfx = NULL;
            }
            break;
        }
        case MPP_ELEMENT_IMG_COMPOSE:
        {
            gfx_dev_t *gfx = elem->dev.gfx;
            if (gfx)
            {
                MPP_LOGI("mpp_elem_teardown: compose (gfx) @%p\n", gfx);
                if (gfx->ops && gfx->ops->deinit)
                    gfx->ops->deinit(gfx);
                for (i = 0; i < MAX_OUTPUT_PORTS; i++)
                    if (elem->io.out_buf[i]) { hal_free(elem->io.out_buf[i]); elem->io.out_buf[i] = NULL; }
                hal_free(gfx);
                elem->dev.gfx = NULL;
            }
            break;
        }
        case MPP_ELEMENT_INFERENCE:
        {
            vision_algo_dev_t *valgo = elem->dev.valgo;
            if (valgo)
            {
                MPP_LOGI("mpp_elem_teardown: inference (valgo) @%p\n", valgo);
                if (valgo->ops && valgo->ops->deinit)
                    valgo->ops->deinit(valgo);
                for (i = 0; i < MAX_OUTPUT_PORTS; i++)
                    if (elem->io.out_buf[i]) { hal_free(elem->io.out_buf[i]); elem->io.out_buf[i] = NULL; }
                hal_free(valgo);
                elem->dev.valgo = NULL;
            }
            break;
        }
        case MPP_ELEMENT_LABELED_RECTANGLE:
        {
            MPP_LOGI("mpp_elem_teardown: labeled rectangle @%p priv=%p\n", elem, elem->priv);
            /* reuse the existing close logic via the priv context */
            void elem_lbl_rct_close(_elem_t *);
            elem_lbl_rct_close(elem);
            break;
        }
        case MPP_ELEMENT_VIDEO_DECODE:
        {
            vdec_h264_dev_t *vdec = elem->dev.vdec_h264;
            if (vdec)
            {
                MPP_LOGI("mpp_elem_teardown: video decode @%p\n", vdec);
                if (vdec->ops && vdec->ops->deinit)
                    vdec->ops->deinit(vdec);
                for (i = 0; i < MAX_OUTPUT_PORTS; i++)
                    if (elem->io.out_buf[i]) { hal_free(elem->io.out_buf[i]); elem->io.out_buf[i] = NULL; }
                hal_free(vdec);
                elem->dev.vdec_h264 = NULL;
            }
            break;
        }
        case MPP_ELEMENT_VIDEO_ENCODE:
        {
            venc_h264_dev_t *venc = elem->dev.venc_h264;
            if (venc)
            {
                MPP_LOGI("mpp_elem_teardown: video encode @%p\n", venc);
                if (venc->ops && venc->ops->deinit)
                    venc->ops->deinit(venc);
                for (i = 0; i < MAX_OUTPUT_PORTS; i++)
                    if (elem->io.out_buf[i]) { hal_free(elem->io.out_buf[i]); elem->io.out_buf[i] = NULL; }
                hal_free(venc);
                elem->dev.venc_h264 = NULL;
            }
            break;
        }
        case MPP_ELEMENT_IMG_DECODE:
        {
            vdec_dev_t *vdec = elem->dev.vdec;
            if (vdec)
            {
                MPP_LOGI("mpp_elem_teardown: img decode (vdec) @%p\n", vdec);
                if (vdec->ops && vdec->ops->deinit)
                    vdec->ops->deinit(vdec);
                for (i = 0; i < MAX_OUTPUT_PORTS; i++)
                    if (elem->io.out_buf[i]) { hal_free(elem->io.out_buf[i]); elem->io.out_buf[i] = NULL; }
                hal_free(vdec);
                elem->dev.vdec = NULL;
            }
            break;
        }
        case MPP_ELEMENT_IMG_QUALITY_CHECK:
            /* inplace element: out_buf[0] aliases in_buf[0] and is not owned here;
             * no device struct is allocated - nothing to free */
            break;
        case MPP_ELEMENT_TEST:
            /* test element: no device struct allocated */
            break;
        default:
            MPP_LOGE("mpp_elem_teardown: proc element type %d not handled\n", elem->proc_typ);
            break;
        }
        break;

    default:
        MPP_LOGE("mpp_elem_teardown: unknown element type %d\n", elem->type);
        break;
    }
}

/*
 * Destroy a single pipeline struct.
 * Caller must ensure all splits are already destroyed first.
 */
static int mpp_destroy_single(_mpp_t *_mpp)
{
    MPP_LOGI("mpp_destroy_single: destroying pipeline @%p\n", _mpp);

    /* ensure the pipeline is stopped */
    if (_mpp->oper_status == MPP_RUNNING)
    {
        MPP_LOGE("mpp_destroy_single: pipeline @%p is still running, stop it first\n", _mpp);
        return MPP_ERROR;
    }

    /* If this pipeline is a branch (has a hook), remove its first element from
     * the hook element's next[] list first. This stops the still-running parent
     * pipeline from delivering new frames into this branch before we remove it
     * from the heap and free its elements. */
    if (_mpp->hook != NULL && _mpp->first_elem != NULL)
    {
        _elem_t *hook_elem = _mpp->hook;
        for (int i = 1; i < MPP_MAX_BRANCH_NUM; i++)
        {
            if (hook_elem->next[i] == _mpp->first_elem)
            {
                MPP_LOGI("mpp_destroy_single: unlinking first elem @%p from hook next[%d]\n",
                         _mpp->first_elem, i);
                hook_elem->next[i] = NULL;
                /* keep split count consistent */
                if (hook_elem->mpp != NULL)
                    hook_elem->mpp->split_cnt--;
                break;
            }
        }
    }

    /* Remove from the exec heap prio list so that the higher-priority heap task
     * (rcHeapTask/prHeapTask) cannot find this pipeline and try to execute it
     * while its elements and buffers are being freed below. */
    mpp_heap_remove(_mpp);

    /* Pre-teardown pass: call mpp_elem_teardown on any MC source/sink elements
     * before freeing buffers. This ensures that the DEINIT_NOTIFY handshake
     * with the remote peer is completed while the shared buffers are still
     * valid. Non-MC elements do not interact with the buffers during teardown
     * and are handled in the second pass below. */
    {
        _elem_t *e = _mpp->first_elem;
        while (e != NULL && e->mpp == _mpp)
        {
            bool is_mc_elem =
                (e->type == MPP_TYPE_SOURCE && e->src_typ == MPP_SRC_MC) ||
                (e->type == MPP_TYPE_SINK   && e->sink_typ == MPP_SINK_MC);
            if (is_mc_elem)
            {
                MPP_LOGI("mpp_destroy_single: pre-teardown MC elem @%p\n", e);
                mpp_elem_teardown(e);
            }
            e = e->next[0];
        }
    }

    /* free input-side buffers (accesses elem pointers before they are freed) */
    MPP_LOGI("mpp_destroy_single: freeing input buffers for pipeline @%p\n", _mpp);
    mpp_memory_free(_mpp);

    /* tear down all non-MC elements and free all elements */
    _elem_t *elem = _mpp->first_elem;
    while (elem != NULL && elem->mpp == _mpp)
    {
        _elem_t *next = elem->next[0];
        bool is_mc_elem =
            (elem->type == MPP_TYPE_SOURCE && elem->src_typ == MPP_SRC_MC) ||
            (elem->type == MPP_TYPE_SINK   && elem->sink_typ == MPP_SINK_MC);
        if (!is_mc_elem)
            mpp_elem_teardown(elem);
        hal_free(elem);
        elem = next;
    }

    /* release the status semaphore */
    hal_sema_delete(_mpp->status_sema);

    MPP_LOGI("mpp_destroy_single: pipeline @%p destroyed\n", _mpp);
    hal_free(_mpp);

    return MPP_SUCCESS;
}

/*
 * Walk the hook chain of candidate upstream toward the root.
 * Returns true when _mpp is found upstream of candidate, meaning
 * candidate is a split that belongs to the _mpp tree.
 */
static bool mpp_is_in_tree(_mpp_t *candidate, _mpp_t *root)
{
    _mpp_t *cur = candidate;
    while (cur != NULL)
    {
        if (cur == root)
            return true;
        /* go one level up: the hook element's owning mpp is the parent */
        if (cur->hook == NULL)
            break;
        cur = cur->hook->mpp;
    }
    return false;
}

int mpp_destroy(mpp_t mpp)
{
    int i, ret;
    bool found;

    if (mpp == MPP_INVALID)
        return MPP_INVALID_PARAM;

    _mpp_t *_mpp = (_mpp_t *) mpp;

    MPP_LOGI("mpp_destroy: starting destroy for root pipeline @%p\n", _mpp);

    /* Safety check: if the pipeline on which mpp_destroy was called is itself a
     * branch (has hook != NULL), verify that the upstream is not still running
     * and owns the first element's input buffer.
     * This check is performed once here for the root of the destroy call only;
     * it is not repeated inside mpp_destroy_single for every split. */
    if (_mpp->hook != NULL && _mpp->first_elem != NULL)
    {
        _elem_t *first = _mpp->first_elem;
        mpp_memory_policy_t first_policy = first->io.mem_policy;
        mpp_memory_policy_t prev_policy  = get_prev_policy(first);
        bool will_free_upstream_buf =
            ((first_policy == HAL_MEM_ALLOC_NONE) || (first_policy == HAL_MEM_ALLOC_OUTPUT))
            && ((prev_policy == HAL_MEM_ALLOC_NONE) || (prev_policy == HAL_MEM_ALLOC_INPUT));
        if (will_free_upstream_buf && (_mpp->hook->mpp->oper_status == MPP_RUNNING))
        {
            MPP_LOGE("mpp_destroy: upstream pipeline @%p is still running"
                     " and owns the input buffer of pipeline @%p; stop it first\n",
                     _mpp->hook->mpp, _mpp);
            return MPP_ERROR;
        }
    }

    /* pass 1: verify every pipeline in the tree (root + all splits) is stopped */
    for (i = 0; i < MAX_MPP_HEAP_PRIO; i++)
    {
        _mpp_t *c = rc_prio_lst[i];
        if (c && mpp_is_in_tree(c, _mpp) && c->oper_status == MPP_RUNNING)
        {
            MPP_LOGE("mpp_destroy: pipeline @%p is still running\n", c);
            return MPP_ERROR;
        }
        c = preempt_prio_lst[i];
        if (c && mpp_is_in_tree(c, _mpp) && c->oper_status == MPP_RUNNING)
        {
            MPP_LOGE("mpp_destroy: pipeline @%p is still running\n", c);
            return MPP_ERROR;
        }
    }

    /*
     * pass 2: destroy iteratively.
     * Keep scanning both prio lists; destroy any leaf pipeline that belongs
     * to the tree and has no remaining splits of its own in the lists.
     * A leaf in this context is a pipeline P in the tree where no other
     * pipeline in the lists has P somewhere in its upstream hook chain
     * (other than P itself).
     * Repeat until only the root remains, then destroy the root.
     */
    do {
        found = false;
        for (i = 0; i < MAX_MPP_HEAP_PRIO; i++)
        {
            _mpp_t *c = rc_prio_lst[i];
            if (c == NULL)
                continue;
            /* must belong to our tree but must not be the root */
            if (!mpp_is_in_tree(c, _mpp) || c == _mpp)
                continue;
            /* check it is a leaf: no other tree member has c in its hook chain */
            bool is_leaf = true;
            int j;
            for (j = 0; j < MAX_MPP_HEAP_PRIO && is_leaf; j++)
            {
                _mpp_t *other = rc_prio_lst[j];
                if (other && other != c && mpp_is_in_tree(other, _mpp))
                {
                    /* walk other's hook chain looking for c */
                    _mpp_t *up = other;
                    while (up != NULL && up != _mpp)
                    {
                        if (up == c) { is_leaf = false; break; }
                        if (up->hook == NULL) break;
                        up = up->hook->mpp;
                    }
                }
            }
            for (j = 0; j < MAX_MPP_HEAP_PRIO && is_leaf; j++)
            {
                _mpp_t *other = preempt_prio_lst[j];
                if (other && other != c && mpp_is_in_tree(other, _mpp))
                {
                    _mpp_t *up = other;
                    while (up != NULL && up != _mpp)
                    {
                        if (up == c) { is_leaf = false; break; }
                        if (up->hook == NULL) break;
                        up = up->hook->mpp;
                    }
                }
            }
            if (is_leaf)
            {
                MPP_LOGI("mpp_destroy: destroying leaf split @%p\n", c);
                ret = mpp_destroy_single(c);
                if (ret != MPP_SUCCESS)
                    return ret;
                found = true;
                break; /* restart scan after list changed */
            }
        }
        if (!found)
        {
            /* check preempt list as well */
            for (i = 0; i < MAX_MPP_HEAP_PRIO; i++)
            {
                _mpp_t *c = preempt_prio_lst[i];
                if (c == NULL)
                    continue;
                if (!mpp_is_in_tree(c, _mpp) || c == _mpp)
                    continue;
                bool is_leaf = true;
                int j;
                for (j = 0; j < MAX_MPP_HEAP_PRIO && is_leaf; j++)
                {
                    _mpp_t *other = rc_prio_lst[j];
                    if (other && other != c && mpp_is_in_tree(other, _mpp))
                    {
                        _mpp_t *up = other;
                        while (up != NULL && up != _mpp)
                        {
                            if (up == c) { is_leaf = false; break; }
                            if (up->hook == NULL) break;
                            up = up->hook->mpp;
                        }
                    }
                }
                for (j = 0; j < MAX_MPP_HEAP_PRIO && is_leaf; j++)
                {
                    _mpp_t *other = preempt_prio_lst[j];
                    if (other && other != c && mpp_is_in_tree(other, _mpp))
                    {
                        _mpp_t *up = other;
                        while (up != NULL && up != _mpp)
                        {
                            if (up == c) { is_leaf = false; break; }
                            if (up->hook == NULL) break;
                            up = up->hook->mpp;
                        }
                    }
                }
                if (is_leaf)
                {
                    MPP_LOGI("mpp_destroy: destroying leaf split @%p\n", c);
                    ret = mpp_destroy_single(c);
                    if (ret != MPP_SUCCESS)
                        return ret;
                    found = true;
                    break;
                }
            }
        }
    } while (found);

    /* destroy root pipeline */
    MPP_LOGI("mpp_destroy: destroying root pipeline @%p\n", _mpp);
    return mpp_destroy_single(_mpp);
}

int mpp_api_ctl_tasks_destroy(void)
{
    int i;

    /* refuse to destroy if any pipeline is still queued in either heap */
    for (i = 0; i < MAX_MPP_HEAP_PRIO; i++)
    {
        if (rc_prio_lst[i] != NULL || preempt_prio_lst[i] != NULL)
        {
            MPP_LOGE("mpp_api_ctl_tasks_destroy: prio lists are not empty\n");
            return MPP_ERROR;
        }
    }

    /* delete pipeline control task and heap tasks */
    MPP_LOGI("mpp_api_ctl_tasks_destroy: deleting pipeline control tasks\n");
    if (hPipelineCtl != NULL)
    {
        hal_task_delete(hPipelineCtl);
        hPipelineCtl = NULL;
    }
    if (hrcHeapTask != NULL)
    {
        hal_task_delete(hrcHeapTask);
        hrcHeapTask = NULL;
    }
    if (hprHeapTask != NULL)
    {
        hal_task_delete(hprHeapTask);
        hprHeapTask = NULL;
    }

    /* delete event groups */
    MPP_LOGI("mpp_api_ctl_tasks_destroy: deleting event groups and semaphore\n");
    hal_eventgrp_delete(xEventGroup1);
    xEventGroup1 = NULL;
    hal_eventgrp_delete(xEventGroup2);
    xEventGroup2 = NULL;
    hal_eventgrp_delete(xEventGroup3);
    xEventGroup3 = NULL;

    /* delete control semaphore */
    hal_sema_delete(xCtlStartSem);
    xCtlStartSem = NULL;

    /* delete stats mutexes */
    MPP_LOGI("mpp_api_ctl_tasks_destroy: deleting stats mutexes\n");
    for (i = 0; i < MPP_STATS_GRP_NUM; i++)
    {
        hal_mutex_remove(stats_lock[i]);
        stats_lock[i] = NULL;
    }

#if defined(MCMGR_USED)
    /* tear down multicore remote API resources */
    mpp_mc_multicore_deinit();
#endif /* MCMGR_USED */

    is_last = 0;
    MPP_LOGI("mpp_api_ctl_tasks_destroy: all control tasks and resources deleted\n");
    return MPP_SUCCESS;
}

char* mpp_get_version(void)
{
    return (char*)mpp_version;
}
