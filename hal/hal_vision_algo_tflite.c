/*
 * Copyright 2022-2026 NXP.
 * All rights reserved.
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

/*
 * @brief vision algorithm TFLite HAL driver implementation for the MCU Media Processing Pipeline.
 */

#include "mpp_config.h"
#include "hal_valgo_dev.h"
#include "hal_debug.h"
#include "hal.h"
#include "hal_os.h"

#if (HAL_ENABLE_INFERENCE_TFLITE == 1)

#include <stdlib.h>
#include <stdio.h>
#include "tflite/model.h"
#include "mpp_api_types.h"

typedef struct _tflite_model_param
{
    model_param_t user_params;
    model_interpreter_data_t model_interpreter_data;
    mpp_inference_tensor_params_t input_tensor;
    mpp_inference_cb_param_t out_param;
} tflite_model_param_t;

/* returns true if ok, false in case of issue */
static bool check_model_input_dims(tflite_model_param_t *param)
{
    if (param == NULL)
    {
        HAL_LOGE("Model parameters is NULL pointer\n");
        return false;
    }
    if (param->input_tensor.dims.size < 2)
    {
        HAL_LOGE("Input Tensor not supported, expected at least 2 dimensions\n");
        return false;
    }
    return true;
}

static int get_model_input_width(tflite_model_param_t *param)
{
    if (!check_model_input_dims(param)) return 0;

    if (param->user_params.tensor_order == MPP_TENSOR_ORDER_NCHW)
    {
        if (param->input_tensor.dims.size > 3)
            return param->input_tensor.dims.data[3];
        else
            return 1;
    }
    else if (param->user_params.tensor_order == MPP_TENSOR_ORDER_NHWC)
    {
        if (param->input_tensor.dims.size > 2)
            return param->input_tensor.dims.data[2];
        else
            return 1;
    }
    else
    {
        HAL_LOGE("Bad Input Tensor order\n");
        return 0;
    }
}

static int get_model_input_height(tflite_model_param_t *param)
{
    if (!check_model_input_dims(param)) return 0;

    if (param->user_params.tensor_order == MPP_TENSOR_ORDER_NCHW)
    {
        if (param->input_tensor.dims.size > 2)
            return param->input_tensor.dims.data[2];
        else
            return 1;
    }
    else if (param->user_params.tensor_order == MPP_TENSOR_ORDER_NHWC)
    {
        if (param->input_tensor.dims.size > 1)
            return param->input_tensor.dims.data[1];
        else
            return 1;
    }
    else
    {
        HAL_LOGE("Bad Input Tensor order\n");
        return 0;
    }
}

static int get_model_input_channels(tflite_model_param_t *param)
{
    if (!check_model_input_dims(param)) return 0;

    if (param->user_params.tensor_order == MPP_TENSOR_ORDER_NCHW)
    {
        if (param->input_tensor.dims.size > 1)
            return param->input_tensor.dims.data[1];
        else
            return 1;
    }
    else if (param->user_params.tensor_order == MPP_TENSOR_ORDER_NHWC)
    {
        if (param->input_tensor.dims.size > 3)
            return param->input_tensor.dims.data[3];
        else
            return 1;
    }
    else
    {
        HAL_LOGE("Bad Input Tensor order\n");
        return 0;
    }
}

static hal_valgo_status_t HAL_VisionAlgoDev_TFLite_Init(vision_algo_dev_t *dev, model_param_t *param)
{
    hal_valgo_status_t ret = kStatus_HAL_ValgoSuccess;
    tflite_model_param_t *tflite_model_param;

    HAL_LOGD("++HAL_VisionAlgoDev_TFLite_Init\n");

    // init the device
    memset(&dev->cap, 0, sizeof(dev->cap));
    dev->priv_data = hal_malloc(sizeof(tflite_model_param_t));
    if(dev->priv_data == NULL){
        HAL_LOGE("NULL pointer\n");
        return kStatus_HAL_ValgoMallocError ;
    }
    memset(dev->priv_data, 0, sizeof(tflite_model_param_t));
    tflite_model_param = (tflite_model_param_t *)dev->priv_data;
    // get parameters from user passed to HAL
    memcpy(&tflite_model_param->user_params, param, sizeof(model_param_t));

    // allocate output tensors description
    int i;
    for(i = 0; i < MPP_INFERENCE_MAX_OUTPUTS; i++)
    {
        if (i >= param->inference_params.num_outputs)
        {   /* set to null unused output tensors descriptions */
            tflite_model_param->out_param.out_tensors[i] = NULL;
            continue;
        }
        tflite_model_param->out_param.out_tensors[i] = hal_malloc(sizeof(mpp_inference_tensor_params_t));
        if(tflite_model_param->out_param.out_tensors[i] == NULL){
            HAL_LOGE("NULL pointer\n");
            return kStatus_HAL_ValgoMallocError ;
        }
        memset(tflite_model_param->out_param.out_tensors[i], 0, sizeof(mpp_inference_tensor_params_t));
    }

    // initialize TFLite with model and get missing in/out tensor info
    if (kStatus_Success != MODEL_Init(param->model_data,
            &tflite_model_param->model_interpreter_data,
            &tflite_model_param->input_tensor,
            tflite_model_param->out_param.out_tensors,
            tflite_model_param->user_params.model_input_mean,
            tflite_model_param->user_params.model_input_std,
            param->inference_params.num_outputs))
    {
        HAL_LOGE("ERROR: MODEL_Init() failed\n");
        for (i = 0; i < MPP_INFERENCE_MAX_OUTPUTS; i++) {
            if (tflite_model_param->out_param.out_tensors[i] != NULL) {
                hal_free(tflite_model_param->out_param.out_tensors[i]);
            }
        }
        hal_free(dev->priv_data);
        dev->priv_data = NULL;
        return kStatus_HAL_ValgoInitError;
    }

    /* display model input format info */
    HAL_LOGI("Model expects width = %d\r\n", get_model_input_width(tflite_model_param));
    HAL_LOGI("Model expects height = %d\r\n", get_model_input_height(tflite_model_param));

    switch(tflite_model_param->input_tensor.type) {
    case MPP_TENSOR_TYPE_UINT8:
    case MPP_TENSOR_TYPE_INT8:
        if (get_model_input_channels(tflite_model_param) == 3) {
            HAL_LOGI("Model expects format = MPP_PIXEL_RGB\r\n");
        } else if (get_model_input_channels(tflite_model_param) == 1) {
            HAL_LOGI("Model expects format = MPP_PIXEL_GRAY\r\n");
        } else {
            HAL_LOGE("--HAL_VisionAlgoDev_TFLite_getInput: invalid number of channels\n");
            ret = kStatus_HAL_ValgoError;
        }
        break;
    case MPP_TENSOR_TYPE_FLOAT32:
        HAL_LOGI("Model expects float input format, number of channels is %d\r\n", get_model_input_channels(tflite_model_param));
        break;
    default:
        HAL_LOGE("--HAL_VisionAlgoDev_TFLite_getInput: input tensor format not supported\n");
        ret = kStatus_HAL_ValgoError;
        break;
    }

    HAL_LOGD("--HAL_VisionAlgoDev_TFLite_Init\n");
    return ret;
}

// deinitialize the dev
static hal_valgo_status_t HAL_VisionAlgoDev_TFLite_Deinit(vision_algo_dev_t *dev)
{
    hal_valgo_status_t ret = kStatus_HAL_ValgoSuccess;
    HAL_LOGD("++HAL_VisionAlgoDev_TFLite_Deinit\n");

    if (dev == NULL) {
        HAL_LOGE("--HAL_VisionAlgoDev_TFLite_Deinit: dev is NULL\n");
        return kStatus_HAL_ValgoError;
    }

    tflite_model_param_t *tflite_model_param = (tflite_model_param_t *)dev->priv_data;

    if (tflite_model_param == NULL) {
        HAL_LOGE("--HAL_VisionAlgoDev_TFLite_Deinit: dev->priv_data is NULL\n");
        return kStatus_HAL_ValgoError;
    }

    MODEL_DeInit(&tflite_model_param->model_interpreter_data);

    int i;
    for(i = 0; i < tflite_model_param->user_params.inference_params.num_outputs; i++)
    {
        if(tflite_model_param->out_param.out_tensors[i] != NULL)
        {
            hal_free(tflite_model_param->out_param.out_tensors[i]);
            tflite_model_param->out_param.out_tensors[i] = NULL;
        }
    }

    if (dev->priv_data != NULL) {
        hal_free(dev->priv_data);
        dev->priv_data = NULL;
    }

    HAL_LOGD("--HAL_VisionAlgoDev_TFLite_Deinit\n");
    return ret;
}

// start the dev
static hal_valgo_status_t HAL_VisionAlgoDev_TFLite_Run(const vision_algo_dev_t *dev, void *data)
{
    hal_valgo_status_t ret = kStatus_HAL_ValgoSuccess;
    tflite_model_param_t *tflite_model_param;
    mpp_inference_inp_cb_params inf_inp_cb_param;
    HAL_LOGD("++HAL_VisionAlgoDev_TFLite_Run\n");

    if (dev == NULL || dev->priv_data == NULL) {
        HAL_LOGE("Invalid device or private data\n");
        return kStatus_HAL_ValgoError;
    }

    tflite_model_param = (tflite_model_param_t *)dev->priv_data;

    if (tflite_model_param->user_params.evt_callback_f == NULL) {
        HAL_LOGE("Event callback not set\n");
        return kStatus_HAL_ValgoError;
    }

    inf_inp_cb_param.in_tensors[0] = (void *) &tflite_model_param->input_tensor;
    inf_inp_cb_param.model_id = tflite_model_param->user_params.model_id;
    tflite_model_param->user_params.evt_callback_f(
            tflite_model_param->user_params.mpp,
            MPP_EVENT_INFERENCE_INPUT_READY,
            (void *) &inf_inp_cb_param,
            tflite_model_param->user_params.cb_userdata);

    // TODO replace by a generic model->ConvertInput() call
    MODEL_ConvertInput(&tflite_model_param->model_interpreter_data,
            (uint8_t *) tflite_model_param->input_tensor.data,
            &(tflite_model_param->input_tensor.dims),
            tflite_model_param->input_tensor.type,  /* use type returned by model interpreter */
            tflite_model_param->user_params.model_input_mean,
            tflite_model_param->user_params.model_input_std); /* use tensor normalization pamaeters for LUT implementation*/

    uint32_t startTime = hal_get_exec_time();
    if (kStatus_Success != MODEL_RunInference(&tflite_model_param->model_interpreter_data)) {
        HAL_LOGE("ERROR: MODEL_RunInference() failed\n");
        return kStatus_HAL_ValgoError;
    }
    tflite_model_param->out_param.inference_time_ms = hal_get_exec_time() - startTime;
    tflite_model_param->out_param.inference_type = MPP_INFERENCE_TYPE_TFLITE;
    tflite_model_param->out_param.model_id = tflite_model_param->user_params.model_id;

    tflite_model_param->user_params.evt_callback_f(
            tflite_model_param->user_params.mpp,
            MPP_EVENT_INFERENCE_OUTPUT_READY,
            (void *)&tflite_model_param->out_param,
            tflite_model_param->user_params.cb_userdata);

    HAL_LOGD("--HAL_VisionAlgoDev_TFLite_Run\n");
    return ret;
}

static hal_valgo_status_t HAL_VisionAlgoDev_TFLite_getBufDesc(const vision_algo_dev_t *dev, hw_buf_desc_t *in_buf, mpp_memory_policy_t *policy)
{
    hal_valgo_status_t ret = kStatus_HAL_ValgoSuccess;
    tflite_model_param_t *tflite_model_param;
    HAL_LOGD("++HAL_VisionAlgoDev_TFLite_getInput\n");

    if (dev == NULL || dev->priv_data == NULL) {
        HAL_LOGE("Invalid device or private data\n");
        return kStatus_HAL_ValgoError;
    }

    if ((in_buf == NULL) || (policy == NULL))
    {
        HAL_LOGE("\nNULL pointer\n");
        return kStatus_HAL_ValgoError;
    }
    /* TFlite allocates tensor arena */
    *policy = HAL_MEM_ALLOC_BOTH;

    tflite_model_param = (tflite_model_param_t *)dev->priv_data;
    /*
     * TODO: add support for other TFlite models with different component order.
     * For now, this is only assuming NHWC order.
     */
    in_buf->alignment = HAL_TFLITE_BUFFER_ALIGN;
    in_buf->nb_lines = get_model_input_height(tflite_model_param); /* number of lines required is the input height */
#if (HAL_TENSOR_ARENA_NCACHE == 1)
    in_buf->cacheable = false;
#else
    in_buf->cacheable = true;
#endif
    /* width * channels */
    in_buf->stride = get_model_input_width(tflite_model_param) * get_model_input_channels(tflite_model_param);
    in_buf->addr = (unsigned char *)tflite_model_param->input_tensor.data;
    in_buf->max_image_size = in_buf->nb_lines * in_buf->stride;

    HAL_LOGD("--HAL_VisionAlgoDev_TFLite_getInput\n");
    return ret;
}

const static vision_algo_dev_operator_t s_VisionAlgoDev_TFLiteOps = {
    .init        = HAL_VisionAlgoDev_TFLite_Init,
    .deinit      = HAL_VisionAlgoDev_TFLite_Deinit,
    .run         = HAL_VisionAlgoDev_TFLite_Run,
    .get_buf_desc   = HAL_VisionAlgoDev_TFLite_getBufDesc,
};

int hal_inference_tflite_setup(vision_algo_dev_t *dev)
{
    dev->id = 0;    /* TODO set unique id */
    dev->ops = &s_VisionAlgoDev_TFLiteOps;

    return 0;
}
#else  /* (HAL_ENABLE_INFERENCE_TFLITE != 1) */
int hal_inference_tflite_setup(vision_algo_dev_t *dev)
{
    HAL_LOGE("Inference TF-lite not enabled\n");
    return -1;
}
#endif /* (HAL_ENABLE_INFERENCE_TFLITE == 1) */
