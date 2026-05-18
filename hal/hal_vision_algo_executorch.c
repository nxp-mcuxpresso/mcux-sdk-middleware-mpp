  /* 
 * Copyright 2026 NXP
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

/*
 * @brief Vision algorithm ExecuTorch HAL driver implementation for the MCU Media Processing Pipeline.
 *
 * ExecuTorch models use NCHW tensor order:
 *   dims[0]=N (batch), dims[1]=C (channels), dims[2]=H (height), dims[3]=W (width)
 */

#include "mpp_config.h"
#include "hal_valgo_dev.h"
#include "hal_debug.h"
#include "hal.h"
#include "hal_os.h"

#if (HAL_ENABLE_INFERENCE_EXECUTORCH == 1)

#include <stdlib.h>
#include <stdio.h>
#include "executorch/model.h"
#include "mpp_api_types.h"

typedef struct _executorch_model_param
{
    model_param_t user_params;
    mpp_inference_tensor_params_t input_tensor;
    mpp_inference_cb_param_t out_param;
} executorch_model_param_t;

/*
 * Helper functions to extract model input dimensions.
 * ExecuTorch tensor order is NCHW:
 *   dims[0]=N, dims[1]=C, dims[2]=H, dims[3]=W
 */

static bool check_model_input_dims(executorch_model_param_t *param)
{
    if (param == NULL)
    {
        HAL_LOGE("Model parameters is NULL pointer\n");
        return false;
    }
    if (param->input_tensor.dims.size != 4)
    {
        HAL_LOGE("Input Tensor not supported, expected 4 dimensions, got %d\n",
                 param->input_tensor.dims.size);
        return false;
    }
    return true;
}

/* NCHW: width = dims[3] */
static int get_model_input_width(executorch_model_param_t *param)
{
    if (!check_model_input_dims(param)) return 0;
    return param->input_tensor.dims.data[3];
}

/* NCHW: height = dims[2] */
static int get_model_input_height(executorch_model_param_t *param)
{
    if (!check_model_input_dims(param)) return 0;
    return param->input_tensor.dims.data[2];
}

/* NCHW: channels = dims[1] */
static int get_model_input_channels(executorch_model_param_t *param)
{
    if (!check_model_input_dims(param)) return 0;
    return param->input_tensor.dims.data[1];
}


static hal_valgo_status_t HAL_VisionAlgoDev_ExecuTorch_Init(
    vision_algo_dev_t *dev,
    model_param_t *param)
{
    hal_valgo_status_t ret = kStatus_HAL_ValgoSuccess;
    executorch_model_param_t *exec_param;

    HAL_LOGD("++HAL_VisionAlgoDev_ExecuTorch_Init\n");

    if (dev == NULL || param == NULL){
        HAL_LOGE("HAL_VisionAlgoDev_ExecuTorch_Init: dev or param is NULL\n");
        return kStatus_HAL_ValgoError;
    }

    /* Initialize device capability structure */
    memset(&dev->cap, 0, sizeof(dev->cap));

    dev->priv_data = hal_malloc(sizeof(executorch_model_param_t));
    exec_param = (executorch_model_param_t *)dev->priv_data;

    if (dev->priv_data == NULL) {
        HAL_LOGE("Failed to allocate memory for ExecuTorch model parameters\n");
        return kStatus_HAL_ValgoMallocError;
    }

    memset(dev->priv_data, 0, sizeof(executorch_model_param_t));

    memcpy(&exec_param->user_params, param, sizeof(model_param_t));

    /* Allocate output tensor descriptors */
    int i;
    for (i = 0; i < MPP_INFERENCE_MAX_OUTPUTS; i++)
    {
        if (i >= param->inference_params.num_outputs)
        {
            exec_param->out_param.out_tensors[i] = NULL;
            continue;
        }

        exec_param->out_param.out_tensors[i] = hal_malloc(sizeof(mpp_inference_tensor_params_t));
        if (exec_param->out_param.out_tensors[i] == NULL) {
            HAL_LOGE("Failed to allocate memory for output tensor %d\n", i);

            /* Cleanup previously allocated tensors */
            for (int j = 0; j < i; j++) {
                if (exec_param->out_param.out_tensors[j] != NULL) {
                    hal_free(exec_param->out_param.out_tensors[j]);
                }
            }
            hal_free(dev->priv_data);
            dev->priv_data = NULL;

            return kStatus_HAL_ValgoMallocError;
        }
        memset(exec_param->out_param.out_tensors[i], 0, sizeof(mpp_inference_tensor_params_t));
    }

    /* Initialize ExecuTorch model and get tensor metadata */
    if (kStatus_Success != MODEL_EXECUTORCH_Init(
            param->model_data,
            param->model_size,
            &exec_param->input_tensor,
            exec_param->out_param.out_tensors,
            param->model_input_mean,
            param->model_input_std,
            param->inference_params.num_outputs))
    {
        HAL_LOGE("ERROR: MODEL_EXECUTORCH_Init() failed\n");

        /* Cleanup allocated memory */
        for (i = 0; i < param->inference_params.num_outputs; i++) {
            if (exec_param->out_param.out_tensors[i] != NULL) {
                hal_free(exec_param->out_param.out_tensors[i]);
            }
        }
        hal_free(dev->priv_data);
        dev->priv_data = NULL;

        return kStatus_HAL_ValgoInitError;
    }

    /* Display model input format information */
    HAL_LOGI("ExecuTorch Model Input Configuration (NCHW):\n");
    HAL_LOGI("  Expected width    = %d\n", get_model_input_width(exec_param));
    HAL_LOGI("  Expected height   = %d\n", get_model_input_height(exec_param));
    HAL_LOGI("  Expected channels = %d\n", get_model_input_channels(exec_param));

    /* Validate input tensor format */
    switch (exec_param->input_tensor.type) {
        case MPP_TENSOR_TYPE_UINT8:
        case MPP_TENSOR_TYPE_INT8:
            if (get_model_input_channels(exec_param) == 3) {
                HAL_LOGI("Expected format = MPP_PIXEL_RGB\n");
            } else {
                HAL_LOGE("Invalid number of channels: %d\n",
                         get_model_input_channels(exec_param));
                ret = kStatus_HAL_ValgoError;
            }
            break;
        case MPP_TENSOR_TYPE_FLOAT32:
        default:
            HAL_LOGE("--HAL_VisionAlgoDev_TFLite_getInput: input tensor format not supported\n");
            ret = kStatus_HAL_ValgoError;
            break;   
    }

    HAL_LOGD("--HAL_VisionAlgoDev_ExecuTorch_Init\n");
    return ret;
}

static hal_valgo_status_t HAL_VisionAlgoDev_ExecuTorch_Deinit(
    vision_algo_dev_t *dev)
{
    hal_valgo_status_t ret = kStatus_HAL_ValgoSuccess;
    HAL_LOGD("++HAL_VisionAlgoDev_ExecuTorch_Deinit\n");

    if (dev == NULL) {
        HAL_LOGE("HAL_VisionAlgoDev_ExecuTorch_Deinit: dev is NULL\n");
        return kStatus_HAL_ValgoError;
    }

    executorch_model_param_t *exec_param = (executorch_model_param_t *)dev->priv_data;

    if (exec_param == NULL) {
        HAL_LOGE("dev->priv_data is NULL\n");
        return kStatus_HAL_ValgoStop;
    }

    /* Deinitialize ExecuTorch model */
    MODEL_EXECUTORCH_DeInit();

    int i;
    for (i = 0; i < exec_param->user_params.inference_params.num_outputs; i++)
    {
        if (exec_param->out_param.out_tensors[i] != NULL)
        {
            hal_free(exec_param->out_param.out_tensors[i]);
            exec_param->out_param.out_tensors[i] = NULL;
        }
    }

    /* Free private data */
    if (dev->priv_data != NULL) {
        hal_free(dev->priv_data);
        dev->priv_data = NULL;
    }

    HAL_LOGD("--HAL_VisionAlgoDev_ExecuTorch_Deinit\n");
    return ret;
}

static hal_valgo_status_t HAL_VisionAlgoDev_ExecuTorch_Run(
    const vision_algo_dev_t *dev,
    void *data)
{
    hal_valgo_status_t ret = kStatus_HAL_ValgoSuccess;
    executorch_model_param_t *exec_param;

    HAL_LOGD("++HAL_VisionAlgoDev_ExecuTorch_Run\n");

    /* check only dev, data is not used in this implementation */
    if (dev == NULL) 
    {
        HAL_LOGE("HAL_VisionAlgoDev_ExecuTorch_Run: dev is NULL\n");
        return kStatus_HAL_ValgoError;
    }

    exec_param = (executorch_model_param_t *)dev->priv_data;

    if (exec_param == NULL) {
        HAL_LOGE("ExecuTorch model parameters is NULL\n");
        return kStatus_HAL_ValgoError;
    }

    exec_param->user_params.evt_callback_f(
        exec_param->user_params.mpp,
        MPP_EVENT_INFERENCE_INPUT_READY,
        (void *)exec_param->input_tensor.data,
        exec_param->user_params.cb_userdata);

    MODEL_EXECUTORCH_ConvertInput(
        (uint8_t *)exec_param->input_tensor.data,
        &(exec_param->input_tensor.dims),
        exec_param->input_tensor.type,
        exec_param->user_params.model_input_mean,
        exec_param->user_params.model_input_std);

    int startTime = hal_get_exec_time();
    if (kStatus_Success != MODEL_EXECUTORCH_RunInference()) {
        HAL_LOGE("ERROR: MODEL_EXECUTORCH_RunInference() failed\n");
        return kStatus_HAL_ValgoError;
    }
    exec_param->out_param.inference_time_ms = hal_get_exec_time() - startTime;
    exec_param->out_param.inference_type = MPP_INFERENCE_TYPE_EXECUTORCH;

    HAL_LOGD("Inference completed in %d ms\n", exec_param->out_param.inference_time_ms);

    exec_param->user_params.evt_callback_f(
        exec_param->user_params.mpp,
        MPP_EVENT_INFERENCE_OUTPUT_READY,
        (void *)&exec_param->out_param,
        exec_param->user_params.cb_userdata);

    HAL_LOGD("--HAL_VisionAlgoDev_ExecuTorch_Run\n");
    return ret;
}

static hal_valgo_status_t HAL_VisionAlgoDev_ExecuTorch_getBufDesc(
    const vision_algo_dev_t *dev,
    hw_buf_desc_t *in_buf,
    mpp_memory_policy_t *policy)
{
    hal_valgo_status_t ret = kStatus_HAL_ValgoSuccess;
    executorch_model_param_t *exec_param;

    HAL_LOGD("++HAL_VisionAlgoDev_ExecuTorch_getBufDesc\n");

    if ((in_buf == NULL) || (policy == NULL) || (dev == NULL))
    {
        HAL_LOGE("HAL_VisionAlgoDev_ExecuTorch_getBufDesc: in_buf, policy or dev is NULL\n");
        return kStatus_HAL_ValgoError;
    }

    /* ExecuTorch allocates method and temp arenas internally */
    *policy = HAL_MEM_ALLOC_BOTH;

    exec_param = (executorch_model_param_t *)dev->priv_data;

    if (exec_param == NULL) {
        HAL_LOGE("ExecuTorch model parameters is NULL\n");
        return kStatus_HAL_ValgoError;
    }

    /*
     * ExecuTorch models use NCHW order:
     *   dims[0] = batch size (N)
     *   dims[1] = channels (C)
     *   dims[2] = height (H)
     *   dims[3] = width (W)
     *
     * The image buffer is laid out as H lines of W*C bytes each.
     */

    in_buf->alignment = HAL_EXECUTORCH_BUFFER_ALIGN;

    /* Number of lines = height of input tensor */
    in_buf->nb_lines = get_model_input_height(exec_param);

    /* Stride = width * channels (bytes per line) */
    in_buf->stride = get_model_input_width(exec_param) *
                     get_model_input_channels(exec_param);

    /* Buffer address (allocated by ExecuTorch runtime) */
    in_buf->addr = (unsigned char *)exec_param->input_tensor.data;

    /* Total buffer size */
    in_buf->max_image_size = in_buf->nb_lines * in_buf->stride;

    /* Cacheability depends on whether NPU is used */
#if (HAL_EXECUTORCH_ARENA_NCACHE == 1)
    in_buf->cacheable = false;
#else
    in_buf->cacheable = true;
#endif

    HAL_LOGD("Buffer descriptor: alignment=%d, nb_lines=%d, stride=%d, size=%d, cacheable=%d\n",
             in_buf->alignment, in_buf->nb_lines, in_buf->stride,
             in_buf->max_image_size, in_buf->cacheable);

    HAL_LOGD("--HAL_VisionAlgoDev_ExecuTorch_getBufDesc\n");
    return ret;
}

/* ExecuTorch HAL Operations Table */
const static vision_algo_dev_operator_t s_VisionAlgoDev_ExecuTorchOps = {
    .init        = HAL_VisionAlgoDev_ExecuTorch_Init,
    .deinit      = HAL_VisionAlgoDev_ExecuTorch_Deinit,
    .run         = HAL_VisionAlgoDev_ExecuTorch_Run,
    .get_buf_desc = HAL_VisionAlgoDev_ExecuTorch_getBufDesc,
};

/* Public Setup Function */
int hal_inference_executorch_setup(vision_algo_dev_t *dev)
{
    if (dev == NULL) {
        HAL_LOGE("Device pointer is NULL\n");
        return -1;
    }

    dev->id = 0;
    dev->ops = &s_VisionAlgoDev_ExecuTorchOps;

    HAL_LOGI("ExecuTorch HAL driver initialized\n");

    return 0;
}

#else  /* (HAL_ENABLE_INFERENCE_EXECUTORCH != 1) */

int hal_inference_executorch_setup(vision_algo_dev_t *dev)
{
    HAL_LOGE("Inference ExecuTorch not enabled\n");
    return -1;
}

#endif /* (HAL_ENABLE_INFERENCE_EXECUTORCH == 1) */
