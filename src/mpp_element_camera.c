/*
 * Copyright 2020-2022,2024-2026 NXP.
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

#include "mpp_api.h"
#include "mpp_api_types_internal.h"
#include "mpp_heap.h"
#include "hal_os.h"
#include "string.h"
#include "hal_utils.h"
#include "mpp_debug.h"

#include "hal.h"

#define CAMERA_UPDATE_TIMEOUT_MS       10 /* ms */

static inline _elem_t *get_camera_elem(_elem_t *elem)
{
    _elem_t *cam_elem_p = NULL;

    while (elem != NULL) 
    {
        if (elem->prev->type == MPP_TYPE_SOURCE && elem->prev->src_typ == MPP_SRC_CAMERA) 
        {
            cam_elem_p = elem->prev;
            break;
        }
        elem = elem->prev;
    }

    return cam_elem_p;
}

static inline int camera_dequeue(_mpp_t *mpp)
{
    int ret = MPP_SUCCESS;
    _elem_t *elem = mpp->first_elem;
    _camera_dev_t *cam = elem->dev.cam;

    if (cam->dev.ops->lock != NULL)
    {
        ret = cam->dev.ops->lock(&cam->dev);
        if (ret != kStatus_HAL_CameraSuccess)
        {
            MPP_LOGE("Failed to lock camera device\n");
            return ret;
        }
    }

    if (cam->params.in_advance_enqueue == false && cam->update)
    {
        for (int i = 0; i < cam->params.n_streams; i++)
        {
            /* When a stream gets activated, set it as requested by default */
            if (cam->params.stream[i].active == false && cam->update_stream[i].active == true)
                cam->dev.config.stream_requested[i] = true;
        }
        memcpy((void *) &cam->params.stream[0], (void *) &cam->update_stream[0], sizeof(cam->params.stream));
        memcpy((void *) &cam->dev.config.stream[0], (void *) &cam->update_stream[0], sizeof(cam->dev.config.stream));
        cam->update = false;
    }

    for (int i = 0; i < MAX_OUTPUT_PORTS; i++)
    {
        /* check stream status */
        if (i >= cam->params.n_streams)
            break;
        /* We only need to check here if the stream is active 
         * The active flag can change only inside camera_enequeue function 
         * Because of that, we cannot have a stream enqueued if active is not set */
        if ((elem->io.out_buf[i]->callback != NULL) && (cam->params.in_advance_enqueue == false) &&
            ((cam->params.stream[i].active != true) || (cam->dev.config.stream_requested[i] != true)))
        {
            MPP_LOGD("Buffer %d is inactive or not requested. Skipping dequeue\n", i);
            continue;
        }

        /* check buffer status */
        if (elem->io.out_buf[i]->status == MPP_BUFFER_READING)
        {
            MPP_LOGI("Warning: camera may overwrite buffer in use.\n");
        }
        /* Set buffer status to writting only when in advance enqueue is disabled 
         * When it is enabled, the status is already set during enqueue */
        if (cam->params.in_advance_enqueue == false)
        {
            elem->io.out_buf[i]->status = MPP_BUFFER_WRITTING;
        }

        MPP_LOGD("Dequeue camera buffer %d, frame_id %d\r\n", i, elem->io.out_buf[i]->frame_id + 1);

        ret = cam->dev.ops->dequeue(&cam->dev, 
                                    (void **)(&elem->io.out_buf[i]->hw->addr),
                                    &elem->io.out_buf[i]->stripe_num,
                                    &elem->io.out_buf[i]->compressed_size);

        if (ret == kStatus_HAL_CameraNoData)
        {
            MPP_LOGD("No data available for dequeue\n");
            elem->io.out_buf[i]->status = MPP_BUFFER_EMPTY;
            /* This should not be treated as an error by the caller of the dequeue function */
            ret = kStatus_HAL_CameraSuccess;
            continue;
        }

        /* update buffer status */
        elem->io.out_buf[i]->status = MPP_BUFFER_READY;
        elem->io.out_buf[i]->frame_id++;
    }

    if (cam->params.in_advance_enqueue == false)
    {
        for (int i = 0; i < MAX_OUTPUT_PORTS; i++)
            cam->dev.config.stream_requested[i] = false;

        cam->dev.config.crt_stream_req_cnt = 0;
    }

    if (cam->dev.ops->unlock != NULL)
    {
        ret = cam->dev.ops->unlock(&cam->dev);
        if (ret != kStatus_HAL_CameraSuccess)
        {
            MPP_LOGE("Failed to unlock camera device\n");
            return ret;
        }
    }

    return ret;
}

static inline int camera_enqueue(_elem_t *elem, void *buf)
{
    int ret = MPP_SUCCESS;
    _mpp_t *mpp = elem->mpp;
    buf_desc_t *buf_desc = (buf_desc_t *)buf;
    uint32_t buf_index;

    /* Find camera element. Ussualy it is elem->prev
     * But there are situations when in_place processing is active for an element and
     * elem->prev is not actually camera element */
    _elem_t *cam_elem =  get_camera_elem(elem);

    if (cam_elem == NULL)
    {
        MPP_LOGE("Could not find camera element\r\n");
        return MPP_INVALID_ELEM;
    }

    _camera_dev_t *cam = cam_elem->dev.cam;

    if (cam->dev.ops->lock != NULL)
    {
        ret = cam->dev.ops->lock(&cam->dev);
        if (ret != kStatus_HAL_CameraSuccess)
        {
            MPP_LOGE("Failed to lock camera device\n");
            return ret;
        }
    }

    /* Check if camera element update was requested */
    if (cam->params.in_advance_enqueue == true && cam->update)
    {
        for (int i = 0; i < cam->dev.config.n_streams; i++)
        {
            /* If a requested stream is deactivated, set it to not requested */
            if(cam->dev.config.stream_requested[i] == true && cam->update_stream[i].active == false)
                cam->dev.config.stream_requested[i] = false;
            /* If a stream is activated, set it as requested by default */
            if(cam->dev.config.stream[i].active == false && cam->update_stream[i].active == true)
                cam->dev.config.stream_requested[i] = true;
        }
        memcpy((void *) &cam->params.stream[0], (void *) &cam->update_stream[0], sizeof(cam->params.stream));
        memcpy((void *) &cam->dev.config.stream[0], (void *) &cam->update_stream[0], sizeof(cam->dev.config.stream));
        cam->update = false;
    }

    /* Get the buffer index in the list to associate it with the stream */
    buf_index = get_out_buff_index(cam_elem, buf_desc);

    if (buf_index >= MAX_OUTPUT_PORTS)
    {
        MPP_LOGE("Invalid output buffer index (%d)\r\n", buf_index);
        MPP_LOGE("Could not find buf desc %p in camera element %p\r\n", buf_desc, cam_elem);
        if (cam->dev.ops->unlock != NULL)
        {
            ret = cam->dev.ops->unlock(&cam->dev);
            if (ret != kStatus_HAL_CameraSuccess)
            {
                MPP_LOGE("Failed to unlock camera device\n");
                return ret;
            }
        }
        return MPP_INVALID_ELEM;
    }

    /* Update request count only for RC mpp */
    if (mpp->params.exec_flag == cam->dev.config.req_cnt_type)
    {
        cam->dev.config.crt_stream_req_cnt++;
        cam->dev.config.stream_requested[buf_index] = cam->params.stream[buf_index].active & true;
    }
    else
    {
        /* Do not make the acutal enqueue call to the device 
         * Wait for the enqueue coming from the pipelines with same exec_flag as req_cnt_type */
        cam->dev.config.stream_requested[buf_index] = cam->params.stream[buf_index].active & true;
        MPP_LOGD("camera buffer %d requested\n", buf_index);
        if (cam->dev.ops->unlock != NULL)
        {
            ret = cam->dev.ops->unlock(&cam->dev);
            if (ret != kStatus_HAL_CameraSuccess)
            {
                MPP_LOGE("Failed to unlock camera device\n");
                return ret;
            }
        }
        return MPP_SUCCESS;
    }

    /* Check if we received the expected number of enqueue calls */
    if ((cam->dev.config.in_advance_enqueue == true) && (cam->dev.config.crt_stream_req_cnt >= cam->dev.config.min_stream_req_cnt))
    {
        /* Check stream config status */
        uint32_t req_cnt = 0;
        for (int i = 0; i < MAX_OUTPUT_PORTS; i++)
        {
            /* check stream status */
            if (i >= cam->params.n_streams)
                break;
            if (cam->dev.config.stream_requested[i] == true)
            {
                req_cnt++;
                cam_elem->io.out_buf[i]->status = MPP_BUFFER_WRITTING;
            }
        }

        if (req_cnt > 0)
        {
            /* Call camera enqueue function */
            ret = cam->dev.ops->enqueue(&cam->dev, NULL);
        }
        else
        {
            MPP_LOGI("No streams requested, skipping camera enqueue\r\n");
            ret = kStatus_HAL_CameraNoData;
        }

        /* Clear requested flag */
        for (int i = 0; i < MAX_OUTPUT_PORTS; i++)
            cam->dev.config.stream_requested[i] = false;

        cam->dev.config.crt_stream_req_cnt = 0;
    }

    if (cam->dev.ops->unlock != NULL)
    {
        ret = cam->dev.ops->unlock(&cam->dev);
        if (ret != kStatus_HAL_CameraSuccess)
        {
            MPP_LOGE("Failed to unlock camera device\n");
            return ret;
        }
    }

    return ret;
}

int mpp_camera_add(mpp_t mpp, const char* name, mpp_camera_params_t *params, mpp_elem_handle_t *elem_h)
{
    int ret = MPP_SUCCESS;
    if (!mpp)
        return MPP_INVALID_PARAM;
    _mpp_t *_mpp = (_mpp_t *)mpp;
    if (_mpp->status != MPP_CREATED)
        return MPP_ERROR;

    _elem_t *elem;
    ret  = mpp_create_elem(_mpp, &elem);
    if (ret != MPP_SUCCESS)
        return ret;

    elem->type = MPP_TYPE_SOURCE;
    elem->sink_typ = MPP_SRC_CAMERA;

    _camera_dev_t *cam = hal_malloc(sizeof(*cam) + CAMERA_MAX_PRIV_SIZE);
    if (!cam)
        return MPP_MALLOC_ERROR;
    memset(cam, 0, sizeof(_camera_dev_t));
    elem->dev.cam = cam;

    if (params->n_streams)
    {
        if (params->n_streams > MAX_OUTPUT_PORTS)
        {
            MPP_LOGE("\nInvalid number of output streams for camera: %d. Max allowed is %d\n", params->n_streams, MAX_OUTPUT_PORTS);
            return MPP_INVALID_PARAM;
        }
    }
    else
    {
        /* n_streams cannot be 0. It defaults to 1 */
        MPP_LOGI("Setting n_streams by default to 1. Input value was 0 which is not allowed\r\n");
        params->n_streams = 1;
    }

    uint32_t active_streams = 0;
    for (int i = 0; i < params->n_streams; i++)
    {
        if (params->stream[i].active == true)
            active_streams++;
    }

    if (active_streams > params->n_streams)
    {
        MPP_LOGE("Number of configured active streams (%d) is higher than the n_streams param (%d)\n", active_streams, params->n_streams);
        return MPP_INVALID_PARAM;
    }

    /* active streams cannot be 0. Activate stream 0 by default */
    if (active_streams == 0)
    {
        MPP_LOGI("Setting stream[0].active by default to true. Input value was false which is not allowed\r\n");
        params->stream[0].active = true;
    }

    /* copy params */
    memcpy(&cam->params, params, sizeof(*params));

    /* setup HAL camera structure */
    ret = hal_camera_setup(name, &cam->dev);
    if (ret != MPP_SUCCESS)
        return ret;
    if (cam->dev.ops == NULL) {
        ret = MPP_ERROR;
        return ret;
    }

    /* init HAL function */
    ret = cam->dev.ops->init(&cam->dev, &cam->params, NULL, NULL);
    if (ret != MPP_SUCCESS)
        return ret;

    memset(cam->name, 0, sizeof(cam->name));
    strncpy(cam->name, name, MAX_DEV_NAME);
    
    buf_processed_func_t  camera_enqueue_callback;
    if (strcmp(name, "Virtual_USB_cam") == 0)
        camera_enqueue_callback = camera_enqueue;
    else
        camera_enqueue_callback = NULL;

    MPP_LOGD("Adding camera %s\n", cam->name);

    /* set dequeue function */
    elem->src_dequeue = camera_dequeue;

    /* create buffer parameters to be passed to next element */
    elem->io.inplace = false;
    elem->io.nb_in_buf = 0;

    elem->io.nb_out_buf = params->n_streams;
    for (int i = 0; i < params->n_streams; i++)
    {
        elem->io.out_buf[i] = hal_malloc(sizeof(buf_desc_t));
        if (elem->io.out_buf[i] == NULL)
        {
            MPP_LOGE("\nAllocation failed\n");
            return MPP_MALLOC_ERROR;
        }
        elem->io.out_buf[i]->format = cam->params.format;
        /* If per stream height and width are provided, use those, else use camera's default */
        if (cam->params.stream[i].height != 0 && cam->params.stream[i].width != 0)
        {
            elem->io.out_buf[i]->width = cam->params.stream[i].width;
            elem->io.out_buf[i]->height = cam->params.stream[i].height;
        }
        else
        {
            elem->io.out_buf[i]->width = cam->params.width;
            elem->io.out_buf[i]->height = cam->params.height;
        }
        /* init stripes */
        if (cam->params.stripe)
            elem->io.out_buf[i]->stripe_num = 1;
        else
            elem->io.out_buf[i]->stripe_num = 0;

        /* This function will be called when an output buffer is processed by the next element */
        elem->io.out_buf[i]->callback = camera_enqueue_callback;

        cam->dev.ops->get_buf_desc(&cam->dev, &elem->io.out_buf[i]->hw_req_prod, &elem->io.mem_policy);
    }

    /* pipeline has been opened */
    _mpp->status = MPP_OPENED;

    if (elem_h != NULL) {
        *elem_h = (mpp_elem_handle_t) elem;
    }

    return ret;
}

uint32_t mpp_camera_update(_elem_t *elem, mpp_element_params_t *params)
{
    volatile uint32_t ret = MPP_SUCCESS;
    mpp_camera_params_t *cam_params = &params->camera;
    uint32_t active_streams = 0;

    if (!cam_params->n_streams)
    {
        /* If not set, use initially configured value */
        cam_params->n_streams = elem->dev.cam->params.n_streams;
    }
    else
    {
        /* n_streams cannot be updated - only update of stream configuration is allowed */
        if (cam_params->n_streams != elem->dev.cam->params.n_streams)
        {
            MPP_LOGE("n_streams camera param cannot be updated. Please update only stream configuration\n");
            return MPP_INVALID_PARAM;
        }
    }

    if (cam_params->in_advance_enqueue != elem->dev.cam->params.in_advance_enqueue)
    {
        MPP_LOGE("in_advance_enqueue camera param update not implemented yet. Please update only stream configuration\n");
        return MPP_INVALID_PARAM;
    }

    for (int i = 0; i < cam_params->n_streams; i++)
    {
        if (cam_params->stream[i].active == true)
            active_streams++;
    }

    if (active_streams > cam_params->n_streams)
    {
        MPP_LOGE("Number of configured active streams (%d) is higher than the n_streams param (%d)\n", active_streams, cam_params->n_streams);
        return MPP_INVALID_PARAM;
    }

    bool set = (
        cam_params->height != 0 ||
        cam_params->width != 0 ||
        cam_params->format != 0 ||
        cam_params->fps != 0 ||
        cam_params->stripe != 0 ||
        cam_params->rpmsg_inst != NULL
    );

    if (set)
    {
        MPP_LOGE("Only stream configuration params update is supported\r\n");
        return MPP_INVALID_PARAM;
    }

    /* Store stream configs to be updated later by the camera element itself */
    memcpy((void *) &elem->dev.cam->update_stream[0], (void *) &cam_params->stream[0], sizeof(elem->dev.cam->update_stream));
    elem->dev.cam->update = true;

    return ret;
}
