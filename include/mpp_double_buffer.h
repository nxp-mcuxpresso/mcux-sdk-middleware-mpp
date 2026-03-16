/*
 * Copyright 2026 NXP
 * SPDX-License-Identifier: Apache-2.0
 *
 * Generic Double-Buffer for Producer-Consumer Synchronization
 *
 * Features:
 *   - Thread safe producer-consumer pattern
 *   - Non-blocking for both producer and consumer
 *   - Type-agnostic (works with any data type)
 *   - Safe swap only when both parties are idle
 *   - No mem-copy during swap (index-based)
 *   - Minimal lock contention
 */

#ifndef MPP_DOUBLE_BUFFER_H
#define MPP_DOUBLE_BUFFER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Opaque handle - internals hidden from users
 */
typedef struct mpp_dbuf_s mpp_dbuf_t;

/*
 * Configuration for double buffer initialization
 * Each buffer can hold up to max_count elements of elem_size bytes
 */
typedef struct {
    size_t elem_size;       /* Size of each element in bytes */
    uint32_t max_count;     /* Maximum number of elements per buffer */
} mpp_dbuf_config_t;

/*
 * Status codes
 */
typedef enum {
    MPP_DBUF_OK = 0,
    MPP_DBUF_ERR_INVALID_PARAM,
    MPP_DBUF_ERR_NO_MEMORY,
    MPP_DBUF_ERR_NOT_INITIALIZED,
} mpp_dbuf_status_t;

/*                         LIFECYCLE API
 */

/*
 * Create a new double buffer instance
 *
 * @param config    Configuration (element size, max count)
 * @param handle    Output: opaque handle to created instance
 * @return          Status code
 */
mpp_dbuf_status_t mpp_dbuf_create(const mpp_dbuf_config_t *config,
                                   mpp_dbuf_t **handle);

/*
 * Destroy double buffer instance and free resources
 *
 * @param handle    Handle to destroy (set to NULL after)
 */
void mpp_dbuf_destroy(mpp_dbuf_t **handle);

/*                         CONSUMER API
 */

/*
 * Acquire buffer for reading (consumer side)
 * Never blocks - returns immediately with current data
 *
 * @param handle    Double buffer handle
 * @param data      Output: pointer to data array
 * @param count     Output: number of valid elements
 * @return          Status code
 */
mpp_dbuf_status_t mpp_dbuf_consumer_acquire(mpp_dbuf_t *handle,
                                             void **data,
                                             uint32_t *count);

/*
 * Release buffer after reading (consumer side)
 *
 * @param handle    Double buffer handle
 * @return          Status code
 */
mpp_dbuf_status_t mpp_dbuf_consumer_release(mpp_dbuf_t *handle);

/*                         PRODUCER API
 */

/*
 * Acquire buffer for writing (producer side)
 * Never blocks - returns immediately with writable buffer
 *
 * @param handle    Double buffer handle
 * @param data      Output: pointer to writable buffer
 * @param max_count Output: maximum elements that can be written
 * @return          Status code
 */
mpp_dbuf_status_t mpp_dbuf_producer_acquire(mpp_dbuf_t *handle,
                                             void **data,
                                             uint32_t *max_count);

/*
 * Release buffer after writing (producer side)
 *
 * @param handle    Double buffer handle
 * @param count     Number of valid elements written
 * @return          Status code
 */
mpp_dbuf_status_t mpp_dbuf_producer_release(mpp_dbuf_t *handle,
                                             uint32_t count);

/*                      TYPE-SAFE MACRO WRAPPERS
 */

/*
 * Declare type-safe wrapper functions for a specific type
 *
 * Usage:
 *   MPP_DBUF_DECLARE_TYPED(rect, mpp_labeled_rect_t)
 *
 * Generates:
 *   mpp_dbuf_rect_create(...)
 *   mpp_dbuf_rect_destroy(...)
 *   mpp_dbuf_rect_consumer_acquire(handle, &typed_ptr, &count)
 *   mpp_dbuf_rect_consumer_release(handle)
 *   mpp_dbuf_rect_producer_acquire(handle, &typed_ptr, &max)
 *   mpp_dbuf_rect_producer_release(handle, count)
 */

#define MPP_DBUF_DECLARE_TYPED(name, type)                                    \
                                                                              \
static inline mpp_dbuf_status_t                                               \
mpp_dbuf_##name##_create(uint32_t max_count, mpp_dbuf_t **handle)             \
{                                                                             \
    mpp_dbuf_config_t config = {                                              \
        .elem_size = sizeof(type),                                            \
        .max_count = max_count                                                \
    };                                                                        \
    return mpp_dbuf_create(&config, handle);                                  \
}                                                                             \
                                                                              \
static inline void                                                            \
mpp_dbuf_##name##_destroy(mpp_dbuf_t **handle)                                \
{                                                                             \
    mpp_dbuf_destroy(handle);                                                 \
}                                                                             \
                                                                              \
static inline mpp_dbuf_status_t                                               \
mpp_dbuf_##name##_consumer_acquire(mpp_dbuf_t *handle,                        \
                                   type **data,                               \
                                   uint32_t *count)                           \
{                                                                             \
    return mpp_dbuf_consumer_acquire(handle, (void **)data, count);           \
}                                                                             \
                                                                              \
static inline mpp_dbuf_status_t                                               \
mpp_dbuf_##name##_consumer_release(mpp_dbuf_t *handle)                        \
{                                                                             \
    return mpp_dbuf_consumer_release(handle);                                 \
}                                                                             \
                                                                              \
static inline mpp_dbuf_status_t                                               \
mpp_dbuf_##name##_producer_acquire(mpp_dbuf_t *handle,                        \
                                   type **data,                               \
                                   uint32_t *max_count)                       \
{                                                                             \
    return mpp_dbuf_producer_acquire(handle, (void **)data, max_count);       \
}                                                                             \
                                                                              \
static inline mpp_dbuf_status_t                                               \
mpp_dbuf_##name##_producer_release(mpp_dbuf_t *handle, uint32_t count)        \
{                                                                             \
    return mpp_dbuf_producer_release(handle, count);                          \
}

#ifdef __cplusplus
}
#endif

#endif /* MPP_DOUBLE_BUFFER_H */