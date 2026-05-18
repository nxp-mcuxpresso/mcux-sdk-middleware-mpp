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

#include "mpp_api.h"
#include "mpp_api_types_internal.h"
#include "mpp_heap.h"
#include "mpp_version.h"
#include "mpp_debug.h"

#include "hal_os.h"
#include "string.h"

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

const static char mpp_version[] = "MPP_VERSION_"STRING(MPP_VERSION_MAJOR)"."STRING(MPP_VERSION_MINOR)"."STRING(MPP_VERSION_COMMIT);

void mpp_execute_heap(_mpp_t *rc_prio_lst[]);

static mpp_stats_t *api_stats;
hal_mutex_t stats_lock[MPP_STATS_GRP_NUM];

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
                MPP_LOGI("PR task completed before delay expiration, allowed %d used  %u\r\n",
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
                api_stats->api.pr_rounds = pr_rounds_cnt + 1;
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
            uint32_t n_buffs = elem->prev->io.nb_out_buf;
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
            uint32_t n_buffs = elem->prev->io.nb_out_buf;
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

bool mpp_is_running(mpp_t mpp)
{
    if (mpp == MPP_INVALID)
        return false;

    _mpp_t *_mpp = (_mpp_t *)mpp;
    return (_mpp->oper_status == MPP_RUNNING);
}

int mpp_start(mpp_t mpp, int last, bool force_update)
{
    static int is_last = 0;
    int ret = MPP_SUCCESS;

    if (mpp == MPP_INVALID)
        return MPP_INVALID_PARAM;
    _mpp_t *_mpp = (_mpp_t *)mpp;
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
                     * - Mark all streams as not requested */
                    cam->dev.config.crt_stream_req_cnt = 0;
                    for (int i = 0; i < NUM_STREAMS; i++)
                        cam->dev.config.stream_requested[i] = false;
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

char* mpp_get_version(void)
{
    return (char*)mpp_version;
}
