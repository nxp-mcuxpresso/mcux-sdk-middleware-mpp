/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.
   Copyright 2021-2026 NXP

SPDX-License-Identifier: Apache-2.0

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

/* File modified by NXP. Changes are described in file
   /middleware/eiq/tensorflow-lite/readme.txt in section "Release notes" */

#include "mpp_config.h"

#if (HAL_ENABLE_INFERENCE_TFLITE == 1)

#include <stdio.h>
#include "model.h"

#include "hal_valgo_dev.h"
#include "tensorflow/lite/micro/kernels/micro_ops.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_op_resolver.h"
#include "tensorflow/lite/micro/recording_micro_allocator.h"
#include "tensorflow/lite/schema/schema_generated.h"

/* Lookup table implemetation flag */
/* Enables LUT-based implementation,
 mpp_config.h may set/unset the value */
#ifndef HAL_ENABLE_TENSOR_CONVERSION_LUT
#define HAL_ENABLE_TENSOR_CONVERSION_LUT 1
#endif

/* Maximum number of models that can be initialized simultaneously */
#ifndef MAX_MODEL_DATABASE_SIZE
#define MAX_MODEL_DATABASE_SIZE 8
#endif

/* trick to replace 'float division' with 'multiply by integer and bitshift'
   integer is the inverse multiplied by factor to keep precision */
#define FAST_DIV_BITS 16
#define FAST_DIV_FACTOR (1 << FAST_DIV_BITS)

extern tflite::MicroOpResolver &MODEL_GetOpsResolver();

uint8_t* MODEL_GetInputTensorData(tflite::MicroInterpreter* interpreter, mpp_tensor_dims_t* dims, mpp_tensor_type_t* type, float* scale, int32_t* zero_point);
uint8_t* MODEL_GetOutputTensorData(tflite::MicroInterpreter* interpreter, mpp_tensor_dims_t* dims, mpp_tensor_type_t* type, float* scale, int32_t* zero_point, int idx);

// An area of memory to use for input, output, and intermediate arrays.
// (Can be adjusted based on the model needs.)
constexpr int kTensorArenaSize = HAL_TFLM_TENSOR_ARENA_SIZE_KB * 1024;

// On some devices tensor arena should be non-cacheable
#if defined(HAL_TENSOR_ARENA_NCACHE) && (HAL_TENSOR_ARENA_NCACHE == 1)
static uint8_t s_tensorArena[kTensorArenaSize] __ALIGNED(HAL_TFLITE_BUFFER_ALIGN) __attribute__((section(".npu_ncache_data")));
#else
static uint8_t s_tensorArena[kTensorArenaSize] __ALIGNED(HAL_TFLITE_BUFFER_ALIGN);
#endif

/* Database entry structure to track each initialized model */
typedef struct {
    bool in_use;
    model_interpreter_data_t* interpreter_data;
    tflite::MicroInterpreter* interpreter;
    tflite::RecordingMicroAllocator* allocator;
    size_t allocator_arena_start;       // Start of allocator's arena (always 0 for shared non-persistent)
    size_t allocator_arena_size;        // Size given to allocator
    size_t persistent_buffer_start;     // Absolute start address of persistent buffer in s_tensorArena
    size_t persistent_buffer_used;      // Persistent buffers size
    size_t non_persistent_buffer_used;  // Non-persistent buffers (shared area)
} model_database_entry_t;

/* Global database to track all initialized models */
static model_database_entry_t s_modelDatabase[MAX_MODEL_DATABASE_SIZE] = {0};
static size_t s_maxNonPersistentUsed = 0;      // Maximum non-persistent buffer used by any model
static size_t s_lowestPersistentStart = kTensorArenaSize;     // Lowest address where persistent data starts (from end)

/* Helper function to find a free slot in the database */
static int FindFreeDatabaseSlot()
{
    for (int i = 0; i < MAX_MODEL_DATABASE_SIZE; i++)
    {
        if (!s_modelDatabase[i].in_use)
        {
            return i;
        }
    }
    return -1;
}

/* Helper function to find the database entry for a given interpreter_data */
static int FindDatabaseEntry(model_interpreter_data_t* interpreter_data)
{
    for (int i = 0; i < MAX_MODEL_DATABASE_SIZE; i++)
    {
        if (s_modelDatabase[i].in_use && s_modelDatabase[i].interpreter_data == interpreter_data)
        {
            return i;
        }
    }
    return -1;
}

/* Helper function to recalculate arena usage after deinitialization */
static void RecalculateArenaUsage()
{
    size_t max_non_persistent = 0;
    size_t lowest_persistent_start = kTensorArenaSize; // Start from the end
    bool has_active_models = false;

    /* Find the maximum non-persistent buffer usage and lowest persistent start */
    for (int i = 0; i < MAX_MODEL_DATABASE_SIZE; i++)
    {
        if (s_modelDatabase[i].in_use)
        {
            has_active_models = true;

            /* Track maximum non-persistent usage (all models share this space) */
            if (s_modelDatabase[i].non_persistent_buffer_used > max_non_persistent)
            {
                max_non_persistent = s_modelDatabase[i].non_persistent_buffer_used;
            }

            /* Track lowest persistent buffer start address */
            if (s_modelDatabase[i].persistent_buffer_start < lowest_persistent_start)
            {
                lowest_persistent_start = s_modelDatabase[i].persistent_buffer_start;
            }
        }
    }

    /* Update global tracking */
    s_maxNonPersistentUsed = max_non_persistent;

    if (has_active_models)
    {
        s_lowestPersistentStart = lowest_persistent_start;
    }
    else
    {
        /* No active models, reset everything */
        s_lowestPersistentStart = kTensorArenaSize;
    }

    HAL_LOGI("Arena recalculated: max_non_persistent=%d, lowest_persistent_start=%d, available=%d\r\n",
              s_maxNonPersistentUsed, s_lowestPersistentStart,
              s_lowestPersistentStart - s_maxNonPersistentUsed);
}

status_t MODEL_Init(const void *model_data,
        model_interpreter_data_t *interpreter_data,
        mpp_inference_tensor_params_t *inputTensor,
        mpp_inference_tensor_params_t *outputTensor[],
        int mean, int std,
        int nb_out_tensor)
{
    /* Find a free slot in the database */
    int slot = FindFreeDatabaseSlot();
    if (slot < 0)
    {
        HAL_LOGE("Model database is full. Maximum %d models can be initialized simultaneously\r\n", MAX_MODEL_DATABASE_SIZE);
        return kStatus_Fail;
    }

    /* Map the model into a usable data structure */
    interpreter_data->s_model = (void*)tflite::GetModel(model_data);
    const tflite::Model* model = static_cast<const tflite::Model*>(interpreter_data->s_model);

    if (model->version() != TFLITE_SCHEMA_VERSION)
    {
        HAL_LOGE("Model provided is schema version %d not equal to supported version %d\r\n",
               model->version(), TFLITE_SCHEMA_VERSION);
        return kStatus_Fail;
    }

    /* Pull in only the operation implementations we need */
    tflite::MicroOpResolver &micro_op_resolver = MODEL_GetOpsResolver();
    interpreter_data->s_micro_op_resolver = (void*)&micro_op_resolver;

    /* All models share the same non-persistent area starting from offset 0 */
    /* Give the allocator space from 0 up to the lowest persistent buffer start */
    size_t allocator_arena_start = 0;
    size_t allocator_arena_size = s_lowestPersistentStart;

    HAL_LOGI("Initializing model at slot %d:\r\n", slot);
    HAL_LOGI("  Allocator arena start: %d, size: %d\r\n", allocator_arena_start, allocator_arena_size);
    HAL_LOGI("  Current arena state: max_non_persistent=%d, lowest_persistent_start=%d\r\n",
             s_maxNonPersistentUsed, s_lowestPersistentStart);

    /* Create the recording micro allocator */
    tflite::RecordingMicroAllocator* allocator =
        tflite::RecordingMicroAllocator::Create(&s_tensorArena[allocator_arena_start],
                                                allocator_arena_size);

    if (allocator == nullptr)
    {
        HAL_LOGE("Failed to create RecordingMicroAllocator\r\n");
        return kStatus_Fail;
    }

    /* Build an interpreter on the heap */
    tflite::MicroInterpreter* interpreter = new tflite::MicroInterpreter(
            model,
            micro_op_resolver,
            allocator);

    if (interpreter == nullptr)
    {
        HAL_LOGE("Failed to create MicroInterpreter\r\n");
        return kStatus_Fail;
    }

    interpreter_data->s_interpreter = (void*)interpreter;

    /* Allocate memory from the tensor_arena for the model's tensors */
    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk)
    {
        HAL_LOGE("AllocateTensors() failed\r\n");
        /* Clean up on failure */
        delete interpreter;
        return kStatus_Fail;
    }

    /* Get memory usage statistics */
    /* Persistent buffers are allocated from the END of the allocator's arena */
    /* Non-persistent buffers are allocated from the START of the allocator's arena */
    size_t persistent_buffer_used = allocator->GetSimpleMemoryAllocator()->GetPersistentUsedBytes();
    size_t non_persistent_buffer_used = allocator->GetSimpleMemoryAllocator()->GetNonPersistentUsedBytes();

    /* Calculate absolute address of persistent buffer start in s_tensorArena */
    /* Persistent buffers start at: allocator_arena_start + allocator_arena_size - persistent_buffer_used */
    size_t persistent_buffer_start = allocator_arena_start + allocator_arena_size - persistent_buffer_used;

    HAL_LOGI("Model memory allocation:\r\n");
    HAL_LOGI("  Persistent buffer: start=%d, size=%d bytes\r\n", persistent_buffer_start, persistent_buffer_used);
    HAL_LOGI("  Non-persistent buffer: size=%d bytes\r\n", non_persistent_buffer_used);

    /* Check if buffers would overlap */
    if (non_persistent_buffer_used + persistent_buffer_used > allocator_arena_size)
    {
        HAL_LOGE("Buffer overlap detected!\r\nNon-persistent: %d,\r\nPersistent: %d,\r\nArena: %d\r\n",
                 non_persistent_buffer_used, persistent_buffer_used, allocator_arena_size);
        delete interpreter;
        return kStatus_Fail;
    }

    /* Check if this model's non-persistent usage exceeds current maximum */
    if (non_persistent_buffer_used > s_maxNonPersistentUsed)
    {
        /* Verify there's no overlap with existing persistent buffers */
        if (non_persistent_buffer_used > s_lowestPersistentStart)
        {
            HAL_LOGE("Non-persistent buffer would overlap with existing persistent buffers!\r\n");
            HAL_LOGE("  Required non-persistent: %d, Lowest persistent start: %d\r\n",
                     non_persistent_buffer_used, s_lowestPersistentStart);
            delete interpreter;
            return kStatus_Fail;
        }
        s_maxNonPersistentUsed = non_persistent_buffer_used;
    }
    else
    {
        /* Check if the previous models non-persistent mxa buffer would overlap with this model's persistent buffer */
        if (s_maxNonPersistentUsed > persistent_buffer_start)
        {
            HAL_LOGE("Previous model's non-persistent buffer would overlap with this model's persistent buffer!\r\n");
            HAL_LOGE("  Max non-persistent used: %d, This model's persistent start: %d\r\n",
                     s_maxNonPersistentUsed, persistent_buffer_start);
            delete interpreter;
            return kStatus_Fail;
        }
    }

    /* Update lowest persistent start if this model's persistent buffer is lower */
    if (persistent_buffer_start < s_lowestPersistentStart)
    {
        s_lowestPersistentStart = persistent_buffer_start;
    }

    /* Update database entry */
    s_modelDatabase[slot].in_use = true;
    s_modelDatabase[slot].interpreter_data = interpreter_data;
    s_modelDatabase[slot].interpreter = interpreter;
    s_modelDatabase[slot].allocator = allocator;
    s_modelDatabase[slot].allocator_arena_start = allocator_arena_start;
    s_modelDatabase[slot].allocator_arena_size = allocator_arena_size;
    s_modelDatabase[slot].persistent_buffer_start = persistent_buffer_start;
    s_modelDatabase[slot].persistent_buffer_used = persistent_buffer_used;
    s_modelDatabase[slot].non_persistent_buffer_used = non_persistent_buffer_used;

    HAL_LOGI("Model initialized successfully at slot %d\r\n", slot);
    HAL_LOGI("  Arena state: max_non_persistent=%d, lowest_persistent_start=%d\r\navailable=%d\r\n",
             s_maxNonPersistentUsed, s_lowestPersistentStart,
             s_lowestPersistentStart - s_maxNonPersistentUsed);

    /* Get input tensor data */
    inputTensor->data = MODEL_GetInputTensorData(interpreter, &inputTensor->dims, &inputTensor->type, &inputTensor->scale, &inputTensor->zero_point);

    /* LUT implementation for input tensor conversion optimization */
    #if (HAL_ENABLE_TENSOR_CONVERSION_LUT == 1)
        if (interpreter && interpreter->input(0))
        {
            float input_scale = interpreter->input(0)->params.scale;
            float input_zero_point = interpreter->input(0)->params.zero_point;
            int i_zero_point = input_zero_point;
            int inv_scale = 0;

            inv_scale = FAST_DIV_FACTOR / (input_scale * std);

            for (int i = 0; i < 256; i++)
            {
                interpreter_data->conversion_lut_int8[i] = (int8_t) (((i - mean) * inv_scale) >> FAST_DIV_BITS) + i_zero_point;
            }

            interpreter_data->lut_int8_initialized = true;
        }
    #endif

    /* Get output tensor data */
    for(int i = 0; i < nb_out_tensor; i++)
    {
        outputTensor[i]->data = MODEL_GetOutputTensorData(interpreter,
                                                          &outputTensor[i]->dims,
                                                          &outputTensor[i]->type,
                                                          &outputTensor[i]->scale,
                                                          &outputTensor[i]->zero_point,
                                                          i);
    }

    return kStatus_Success;
}

status_t MODEL_DeInit(model_interpreter_data_t *interpreter_data)
{
    /* Find the database entry for this interpreter */
    int slot = FindDatabaseEntry(interpreter_data);
    if (slot < 0)
    {
        HAL_LOGE("Interpreter not found in database\r\n");
        return kStatus_Fail;
    }

    tflite::MicroInterpreter* interpreter = s_modelDatabase[slot].interpreter;
    tflite::RecordingMicroAllocator* allocator = s_modelDatabase[slot].allocator;

    HAL_LOGI("Deinitializing model from slot %d\r\n", slot);
    HAL_LOGI("  Persistent buffer: start=%d, size=%d\r\n",
             s_modelDatabase[slot].persistent_buffer_start,
             s_modelDatabase[slot].persistent_buffer_used);
    HAL_LOGI("  Non-persistent buffer: size=%d\r\n",
             s_modelDatabase[slot].non_persistent_buffer_used);

    /* Check if this model has the lowest persistent buffer start address */
    bool is_lowest_persistent = (s_modelDatabase[slot].persistent_buffer_start == s_lowestPersistentStart);

    /* Reset and destroy the interpreter (allocated on heap) */
    interpreter->Reset();
    delete interpreter;

    /* Destroy the allocator (it was created with Create(), not new) */
    /* Note: RecordingMicroAllocator::Create() uses placement new internally,
     * so we need to call the destructor explicitly */
    if (allocator != nullptr)
    {
        allocator->~RecordingMicroAllocator();
    }

    /* Clear interpreter data */
    interpreter_data->s_interpreter = nullptr;
    interpreter_data->s_model = nullptr;
    interpreter_data->s_micro_op_resolver = nullptr;

    /* Mark database entry as free */
    s_modelDatabase[slot].in_use = false;
    s_modelDatabase[slot].interpreter_data = nullptr;
    s_modelDatabase[slot].interpreter = nullptr;
    s_modelDatabase[slot].allocator = nullptr;

    if (is_lowest_persistent)
    {
        HAL_LOGI("Freed model had the lowest persistent buffer, reclaiming space\r\n");
    }
    else
    {
        HAL_LOGI("Freed model's persistent buffer creates a gap (will be reclaimed when all models are freed)\r\n");
    }

    /* Recalculate arena usage based on remaining active models */
    RecalculateArenaUsage();

    return kStatus_Success;
}

status_t MODEL_RunInference(model_interpreter_data_t *interpreter_data)
{
    tflite::MicroInterpreter* interpreter = static_cast<tflite::MicroInterpreter*>(interpreter_data->s_interpreter);

    if (interpreter->Invoke() != kTfLiteOk)
    {
        HAL_LOGE("Invoke failed!\r\n");
        return kStatus_Fail;
    }

    return kStatus_Success;
}

uint8_t* GetTensorData(TfLiteTensor* tensor, mpp_tensor_dims_t* dims, mpp_tensor_type_t* type, float* scale, int32_t* zero_point)
{
    switch (tensor->type)
    {
        case kTfLiteFloat32:
            *type = MPP_TENSOR_TYPE_FLOAT32;
            break;
        case kTfLiteUInt8:
            *type = MPP_TENSOR_TYPE_UINT8;
            break;
        case kTfLiteInt8:
            *type = MPP_TENSOR_TYPE_INT8;
            break;
        default:
            assert("Unknown input tensor data type");
    };

    dims->size = tensor->dims->size;
    assert(dims->size <= MAX_TENSOR_DIMS);
    for (int i = 0; i < tensor->dims->size; i++)
    {
        dims->data[i] = tensor->dims->data[i];
    }
    TfLiteAffineQuantization *quant_params = static_cast<TfLiteAffineQuantization*>(tensor->quantization.params);
    if (quant_params != nullptr && quant_params->scale != nullptr && quant_params->zero_point != nullptr)
    {
        *scale = quant_params->scale->data[0];
        *zero_point = quant_params->zero_point->data[0];
    }
    else
    {
        *scale = 1.0f;
        *zero_point = 0;
    }

    return tensor->data.uint8;
}

uint8_t* MODEL_GetInputTensorData(tflite::MicroInterpreter* interpreter, mpp_tensor_dims_t* dims, mpp_tensor_type_t* type, float* scale, int32_t* zero_point)
{
    TfLiteTensor* inputTensor = interpreter->input(0);

    return GetTensorData(inputTensor, dims, type, scale, zero_point);
}

uint8_t* MODEL_GetOutputTensorData(tflite::MicroInterpreter* interpreter, mpp_tensor_dims_t* dims, mpp_tensor_type_t* type, float* scale, int32_t* zero_point, int idx)
{
    /* handles multiple outputs */
    TfLiteTensor* outputTensor = interpreter->output(idx);

    return GetTensorData(outputTensor, dims, type, scale, zero_point);
}

// Convert and normalize unsigned 8-bit image data to model input format in-place.
void MODEL_ConvertInput(model_interpreter_data_t *interpreter_data, uint8_t* data, mpp_tensor_dims_t* dims, mpp_tensor_type_t type, int mean, int std)
{
    tflite::MicroInterpreter* interpreter = static_cast<tflite::MicroInterpreter*>(interpreter_data->s_interpreter);

    int size = dims->data[2] * dims->data[1] * dims->data[3];
    /* Quantization parameters:
     * input_scale : model input scale.
     * input_zero_point: model input zero point.
     */
    float input_scale = interpreter->input(0)->params.scale;
    float input_zero_point = interpreter->input(0)->params.zero_point;
    int inv_scale = 0;
    int i_zero_point = input_zero_point;

    switch (type)
    {
        case MPP_TENSOR_TYPE_UINT8:
        case MPP_TENSOR_TYPE_INT8:
            if((mean != 0) || (std != 1))
            {
                if(std != 0)
                {
                    /* Optimized input tensor conversion
                    * by processing 4 elements at the
                    * time instead of one element per
                    * iteration.*/
                    if(interpreter_data->lut_int8_initialized)
                    {
                        /* Process 4 elements at a time
                        * and check if the size is a
                        * multiple of four. */
                        if (size % 4 == 0)
                        {
                            int i = 0;
                            for (; i <= size - 4; i += 4)
                            {
                                uint8_t val0 = data[i];
                                uint8_t val1 = data[i + 1];
                                uint8_t val2 = data[i + 2];
                                uint8_t val3 = data[i + 3];

                                data[i]     = interpreter_data->conversion_lut_int8[val0];
                                data[i + 1] = interpreter_data->conversion_lut_int8[val1];
                                data[i + 2] = interpreter_data->conversion_lut_int8[val2];
                                data[i + 3] = interpreter_data->conversion_lut_int8[val3];
                            }
                        }
                        /* If the size is not multiple of 4,
                        * switch to processing 1 elemnt per
                        * interation. */
                        else
                        {
                            int i = 0;
                            for (; i < size; i++)
                            {
                                data[i] = interpreter_data->conversion_lut_int8[data[i]];
                            }
                        }
                    }
                    else
                    /* Fallback to the original method */
                    /* to calculate quantized value:
                    * quantized_value = real_value / scale + zero_point
                    * to normalize the input data:
                    * normalized_value = (real_value - mean) / std
                    *
                    * these two formulas can be combined to perform both normalization and
                    * quantization at the same time:
                    * final_value = (real_value - mean) / (scale * std) + zero_point
                    */
                    {
                        inv_scale = FAST_DIV_FACTOR / (input_scale * std);
                        for (int i = size - 1; i >= 0; i--)
                        {
                            /* optimized form of: (data[i] / scale) + zero_point */
                            int32_t quantized_val = ((data[i] - mean) * inv_scale >> FAST_DIV_BITS) + i_zero_point;
                            data[i] = (type == MPP_TENSOR_TYPE_UINT8) ? (uint8_t)quantized_val : (int8_t)quantized_val;
                        }
                    }
                }
                else
                {
                    HAL_LOGE("Standard deviation should be different of 0\r\n");
                }
            }
            else  /* only quantization should be performed */
            {
                inv_scale = FAST_DIV_FACTOR / input_scale;
                for (int i = size - 1; i >= 0; i--)
                {
                    /* optimized form of: (data[i] / scale) + zero_point */
                    int32_t quantized_val = (data[i] * inv_scale >> FAST_DIV_BITS) + i_zero_point;
                    data[i] = (type == MPP_TENSOR_TYPE_UINT8) ? (uint8_t)quantized_val : (int8_t)quantized_val;
                }
            }
            break;

        case MPP_TENSOR_TYPE_FLOAT32:
            HAL_LOGI("No quantization needed for float32 input\r\n");
            break;

        default:
            assert("Unknown input tensor data type");
        }
    }
#endif /* (HAL_ENABLE_INFERENCE_TFLITE == 1) */
