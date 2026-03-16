/*
 * Copyright 2026 NXP
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mpp_double_buffer.h"
#include "hal_os.h"
#include "mpp_heap.h"
#include <string.h>
#include <stdbool.h>
#include <assert.h>

/*
 * Internal structure (hidden from public API)
 */
struct mpp_dbuf_s {
    /* Synchronization */
    hal_mutex_t mutex;

    /* Buffer storage */
    void *buffers[2];
    uint32_t counts[2];

    /* Configuration */
    size_t elem_size;
    uint32_t max_count;

    /* Ownership indices */
    uint32_t consumer_idx;
    uint32_t producer_idx;

    /* State flags */
    bool consumer_active;
    bool producer_active;
    bool swap_pending;
    bool initialized;
};

/*
 *                         INTERNAL HELPERS
 */

/*
 * Attempt buffer swap if conditions are met
 * Must be called with mutex held
 */
static void try_swap_locked(mpp_dbuf_t *db)
{
    if (db->swap_pending && !db->consumer_active && !db->producer_active) {
        /* Swap indices */
        uint32_t tmp = db->consumer_idx;
        db->consumer_idx = db->producer_idx;
        db->producer_idx = tmp;

        db->swap_pending = false;
    }
}

/*
 *                         LIFECYCLE
 */

mpp_dbuf_status_t mpp_dbuf_create(const mpp_dbuf_config_t *config,
                                   mpp_dbuf_t **handle)
{
    mpp_dbuf_t *db = NULL;
    int ret;

    /* Validate parameters */
    if (config == NULL || handle == NULL) {
        return MPP_DBUF_ERR_INVALID_PARAM;
    }
    if (config->elem_size == 0 || config->max_count == 0) {
        return MPP_DBUF_ERR_INVALID_PARAM;
    }

    /* Allocate handle structure */
    db = (mpp_dbuf_t *)hal_malloc(sizeof(mpp_dbuf_t));
    if (db == NULL) {
        return MPP_DBUF_ERR_NO_MEMORY;
    }
    memset(db, 0, sizeof(mpp_dbuf_t));

    /* Store configuration */
    db->elem_size = config->elem_size;
    db->max_count = config->max_count;

    /* Create mutex */
    ret = hal_mutex_create(&db->mutex);
    if (ret != 0) {
        hal_free(db);
        return MPP_DBUF_ERR_NO_MEMORY;
    }

    /* Allocate both buffers */
    size_t buf_size = config->elem_size * config->max_count;

    db->buffers[0] = hal_malloc(buf_size);
    db->buffers[1] = hal_malloc(buf_size);

    if (db->buffers[0] == NULL || db->buffers[1] == NULL) {
        hal_free(db->buffers[0]);
        hal_free(db->buffers[1]);
        hal_mutex_remove(db->mutex);
        hal_free(db);
        return MPP_DBUF_ERR_NO_MEMORY;
    }

    /* Initialize buffers to zero */
    memset(db->buffers[0], 0, buf_size);
    memset(db->buffers[1], 0, buf_size);

    /* Set initial ownership */
    db->consumer_idx = 0;
    db->producer_idx = 1;

    /* Initialize state */
    db->consumer_active = false;
    db->producer_active = false;
    db->swap_pending = false;
    db->counts[0] = 0;
    db->counts[1] = 0;

    db->initialized = true;

    *handle = db;
    return MPP_DBUF_OK;
}

void mpp_dbuf_destroy(mpp_dbuf_t **handle)
{
    mpp_dbuf_t *db;

    if (handle == NULL || *handle == NULL) {
        return;
    }

    db = *handle;

    /* Wait for any active operations to complete */
    (void) hal_mutex_lock(db->mutex);

    /* Wait until all operations have completed */
    while (db->consumer_active || db->producer_active || db->swap_pending) {
        (void) hal_mutex_unlock(db->mutex);
        hal_task_delay(1); // yield
        (void) hal_mutex_lock(db->mutex);
    }

    db->initialized = false;

    hal_free(db->buffers[0]);
    hal_free(db->buffers[1]);
    db->buffers[0] = NULL;
    db->buffers[1] = NULL;

    (void) hal_mutex_unlock(db->mutex);
    hal_mutex_remove(db->mutex);

    hal_free(db);
    *handle = NULL;
}

/*
 *                         CONSUMER API
 */

mpp_dbuf_status_t mpp_dbuf_consumer_acquire(mpp_dbuf_t *handle,
                                             void **data,
                                             uint32_t *count)
{
    if (handle == NULL || data == NULL || count == NULL) {
        return MPP_DBUF_ERR_INVALID_PARAM;
    }

    if (!handle->initialized) {
        return MPP_DBUF_ERR_NOT_INITIALIZED;
    }

    (void) hal_mutex_lock(handle->mutex);

    assert(handle->initialized);
    assert(!handle->consumer_active);

    /* Try to get latest data before reading */
    try_swap_locked(handle);

    /* Mark as active */
    handle->consumer_active = true;

    /* Return consumer's buffer */
    *data = handle->buffers[handle->consumer_idx];
    *count = handle->counts[handle->consumer_idx];

    (void) hal_mutex_unlock(handle->mutex);

    return MPP_DBUF_OK;
}

mpp_dbuf_status_t mpp_dbuf_consumer_release(mpp_dbuf_t *handle)
{
    if (handle == NULL) {
        return MPP_DBUF_ERR_INVALID_PARAM;
    }

    if (!handle->initialized) {
        return MPP_DBUF_ERR_NOT_INITIALIZED;
    }

    (void) hal_mutex_lock(handle->mutex);

    assert(handle->consumer_active);

    handle->consumer_active = false;

    /* Try swap now that we're idle */
    try_swap_locked(handle);

    (void) hal_mutex_unlock(handle->mutex);

    return MPP_DBUF_OK;
}

/*
 *                         PRODUCER API
 */

mpp_dbuf_status_t mpp_dbuf_producer_acquire(mpp_dbuf_t *handle,
                                             void **data,
                                             uint32_t *max_count)
{
    if (handle == NULL || data == NULL || max_count == NULL) {
        return MPP_DBUF_ERR_INVALID_PARAM;
    }

    if (!handle->initialized) {
        return MPP_DBUF_ERR_NOT_INITIALIZED;
    }

    (void) hal_mutex_lock(handle->mutex);

    assert(handle->initialized);
    assert(!handle->producer_active);

    /* Try to get fresh buffer before writing */
    try_swap_locked(handle);

    /* Mark as active */
    handle->producer_active = true;

    /* Return producer's buffer */
    *data = handle->buffers[handle->producer_idx];
    *max_count = handle->max_count;

    (void) hal_mutex_unlock(handle->mutex);

    return MPP_DBUF_OK;
}

mpp_dbuf_status_t mpp_dbuf_producer_release(mpp_dbuf_t *handle,
                                             uint32_t count)
{
    if (handle == NULL) {
        return MPP_DBUF_ERR_INVALID_PARAM;
    }

    if (!handle->initialized) {
        return MPP_DBUF_ERR_NOT_INITIALIZED;
    }

    if (count > handle->max_count) {
        return MPP_DBUF_ERR_INVALID_PARAM;
    }

    (void) hal_mutex_lock(handle->mutex);

    assert(handle->producer_active);

    /* Store count for this buffer */
    handle->counts[handle->producer_idx] = count;

    /* Mark idle and request swap */
    handle->producer_active = false;
    handle->swap_pending = true;

    /* Try swap now that we're idle */
    try_swap_locked(handle);

    (void) hal_mutex_unlock(handle->mutex);

    return MPP_DBUF_OK;
}
