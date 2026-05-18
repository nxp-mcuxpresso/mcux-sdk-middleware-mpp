  /* 
 * Copyright 2026 NXP
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "mpp_config.h"

#if (HAL_ENABLE_INFERENCE_EXECUTORCH == 1)

#include <stdio.h>
#include <memory>
#include <vector>
#include "model.h"
#include "hal_valgo_dev.h"

/* ExecuTorch core includes */
#include <executorch/extension/data_loader/buffer_data_loader.h>
#include <executorch/runtime/executor/method.h>
#include <executorch/runtime/executor/program.h>
#include <executorch/runtime/platform/runtime.h>
#include <executorch/runtime/core/exec_aten/exec_aten.h>
#include <executorch/runtime/core/memory_allocator.h>

using executorch::extension::BufferDataLoader;
using executorch::runtime::Error;
using executorch::runtime::EValue;
using executorch::runtime::HierarchicalAllocator;
using executorch::runtime::Method;
using executorch::runtime::MethodMeta;
using executorch::runtime::MemoryAllocator;
using executorch::runtime::MemoryManager;
using executorch::runtime::Program;
using executorch::runtime::Result;
using executorch::runtime::Span;
using executorch::aten::ScalarType;
using executorch::aten::Tensor;

/* Memory arenas */
constexpr size_t kMethodArenaSize = HAL_EXECUTORCH_METHOD_ARENA_SIZE_KB * 1024;
constexpr size_t kTempArenaSize = HAL_EXECUTORCH_TEMP_ARENA_SIZE_KB * 1024;

#if defined(HAL_EXECUTORCH_ARENA_NCACHE) && (HAL_EXECUTORCH_ARENA_NCACHE == 1)
static uint8_t s_methodArena[kMethodArenaSize]
    __ALIGNED(HAL_EXECUTORCH_BUFFER_ALIGN)
    __attribute__((section(".npu_ncache_data")));
static uint8_t s_tempArena[kTempArenaSize]
    __ALIGNED(HAL_EXECUTORCH_BUFFER_ALIGN)
    __attribute__((section(".npu_ncache_data")));
#else
static uint8_t s_methodArena[kMethodArenaSize]
    __ALIGNED(HAL_EXECUTORCH_BUFFER_ALIGN);
static uint8_t s_tempArena[kTempArenaSize]
    __ALIGNED(HAL_EXECUTORCH_BUFFER_ALIGN);
#endif

/* Runtime objects */
static std::unique_ptr<BufferDataLoader> s_loader;
static std::unique_ptr<Program> s_program;
static std::unique_ptr<MemoryAllocator> s_method_allocator;
static std::unique_ptr<MemoryAllocator> s_temp_allocator;
static Method *s_method = nullptr;

/* Planned memory buffers */
static std::vector<uint8_t *> s_planned_buffers;
static std::vector<Span<uint8_t>> s_planned_spans;

/*
 * Map ExecuTorch ScalarType to MPP tensor type
 */
static mpp_tensor_type_t map_scalar_type(ScalarType type)
{
    switch (type) {
        case ScalarType::Byte:  return MPP_TENSOR_TYPE_UINT8;
        case ScalarType::Char:  return MPP_TENSOR_TYPE_INT8;
        case ScalarType::Float: return MPP_TENSOR_TYPE_FLOAT32;
        default:
            HAL_LOGE("Unknown ScalarType: %d", static_cast<int>(type));
            return MPP_TENSOR_TYPE_UINT8;
    }
}

/*
 * Extract tensor dimensions from ExecuTorch Tensor into MPP dims
 */
static void extract_dims(const Tensor &tensor, mpp_tensor_dims_t *dims)
{
    dims->size = tensor.dim();
    if (dims->size > MAX_TENSOR_DIMS) {
        HAL_LOGE("Tensor has too many dimensions: %d (max %d)",
                 dims->size, MAX_TENSOR_DIMS);
        dims->size = MAX_TENSOR_DIMS;
    }

    for (int i = 0; i < dims->size; i++) {
        dims->data[i] = tensor.size(i);
    }
}

/*
 * Called on error paths and from MODEL_EXECUTORCH_DeInit.
 */
static void cleanup_runtime_objects(void)
{
    if (s_method) {
        delete s_method;
        s_method = nullptr;
    }
    s_planned_buffers.clear();
    s_planned_spans.clear();
    s_program.reset();
    s_loader.reset();
    s_temp_allocator.reset();
    s_method_allocator.reset();
}

status_t MODEL_EXECUTORCH_Init(
    const void *pte_data,
    size_t pte_size,
    mpp_inference_tensor_params_t *inputTensor,
    mpp_inference_tensor_params_t *outputTensor[],
    int mean,
    int std,
    int nb_out_tensor)
{
    HAL_LOGD("++MODEL_EXECUTORCH_Init");

    /* Initialize ExecuTorch runtime */
    executorch::runtime::runtime_init();

    /* Create memory allocators */
    s_method_allocator = std::make_unique<MemoryAllocator>(
        kMethodArenaSize, s_methodArena);
    s_temp_allocator = std::make_unique<MemoryAllocator>(
        kTempArenaSize, s_tempArena);

    if (!s_method_allocator || !s_temp_allocator) {
        HAL_LOGE("Failed to create allocators");
        return kStatus_Fail;
    }

    HAL_LOGD("Memory allocators created:");
    HAL_LOGD("  Method arena: %zu KB at 0x%p", kMethodArenaSize / 1024, s_methodArena);
    HAL_LOGD("  Temp arena: %zu KB at 0x%p", kTempArenaSize / 1024, s_tempArena);

    /* Create data loader from PTE buffer */
    s_loader = std::make_unique<BufferDataLoader>(pte_data, pte_size);
    if (!s_loader) {
        HAL_LOGE("Failed to create BufferDataLoader");
        return kStatus_Fail;
    }

    /* Load program */
    Result<Program> program_res = Program::load(s_loader.get());
    if (!program_res.ok()) {
        HAL_LOGE("Program load failed: %d",
                 static_cast<int>(program_res.error()));
        return kStatus_Fail;
    }
    s_program = std::make_unique<Program>(std::move(program_res.get()));

    HAL_LOGI("Model buffer loaded, has %d methods", s_program->num_methods());

    /* Get method name and metadata for memory planning */
    const char *method_name = nullptr;
    {
        const auto method_name_result = s_program->get_method_name(0);
        if (!method_name_result.ok()) {
            HAL_LOGE("Program has no methods");
            cleanup_runtime_objects();
            return kStatus_Fail;
        }
        method_name = *method_name_result;
    }
    HAL_LOGI("Using method: %s", method_name);

    Result<MethodMeta> method_meta = s_program->method_meta(method_name);
    if (!method_meta.ok()) {
        HAL_LOGE("Failed to get method_meta for %s: 0x%x",
                 method_name, static_cast<unsigned int>(method_meta.error()));
        cleanup_runtime_objects();
        return kStatus_Fail;
    }

    /* Setup planned memory buffers */
    size_t num_memory_planned_buffers = method_meta->num_memory_planned_buffers();
    HAL_LOGI("Method requires %zu planned memory buffers", num_memory_planned_buffers);

    s_planned_buffers.clear();
    s_planned_spans.clear();

    for (size_t id = 0; id < num_memory_planned_buffers; ++id) {
        size_t buffer_size = static_cast<size_t>(
            method_meta->memory_planned_buffer_size(id).get());
        HAL_LOGI("Setting up planned buffer %zu, size %zu", id, buffer_size);

        uint8_t *buffer = reinterpret_cast<uint8_t *>(
            s_method_allocator->allocate(buffer_size, HAL_EXECUTORCH_BUFFER_ALIGN));

        if (buffer == nullptr) {
            HAL_LOGE("Failed to allocate planned buffer %zu", id);
            cleanup_runtime_objects();
            return kStatus_Fail;
        }

        s_planned_buffers.push_back(buffer);
        s_planned_spans.push_back({s_planned_buffers.back(), buffer_size});
    }

    /* Create HierarchicalAllocator and MemoryManager */
    HierarchicalAllocator planned_memory(
        {s_planned_spans.data(), s_planned_spans.size()});
    MemoryManager memory_manager(
        s_method_allocator.get(), &planned_memory, s_temp_allocator.get());

    /* 8. Load method */
    Result<Method> method_res =
        s_program->load_method(method_name, &memory_manager);
    if (!method_res.ok()) {
        HAL_LOGE("Loading of method %s failed with status 0x%x",
                 method_name, static_cast<unsigned int>(method_res.error()));
        cleanup_runtime_objects();
        return kStatus_Fail;
    }
    s_method = new Method(std::move(method_res.get()));

    HAL_LOGI("Method loaded successfully");

    /*
     * ExecuTorch marks mutable_input/mutable_output as deprecated but they are
     * needed to obtain writable data pointers for in-place input conversion and
     * to expose output buffers to post-processing.
     */
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

    /* Get input tensor metadata */
    size_t num_inputs = s_method->inputs_size();
    if (num_inputs == 0) {
        HAL_LOGE("No input tensors found");
        cleanup_runtime_objects();
        return kStatus_Fail;
    }
    HAL_LOGI("%zu input tensors found", num_inputs);

    EValue &input_evalue = s_method->mutable_input(0);
    if (!input_evalue.isTensor()) {
        HAL_LOGE("Input 0 is not a tensor");
        cleanup_runtime_objects();
        return kStatus_Fail;
    }
    Tensor input_tensor = input_evalue.toTensor();

    inputTensor->data = static_cast<uint8_t *>(input_tensor.mutable_data_ptr());
    inputTensor->type = map_scalar_type(input_tensor.scalar_type());
    extract_dims(input_tensor, &inputTensor->dims);

    HAL_LOGI("Input tensor: type=%d, dims=[%d,%d,%d,%d] (NCHW)",
             inputTensor->type,
             inputTensor->dims.data[0],
             inputTensor->dims.data[1],
             inputTensor->dims.data[2],
             inputTensor->dims.data[3]);

    /* Get output tensor metadata */
    size_t num_outputs = s_method->outputs_size();
    HAL_LOGI("%zu output tensors found", num_outputs);

    if ((int)num_outputs < nb_out_tensor) {
        HAL_LOGE("Model has %zu outputs, but %d requested",
                 num_outputs, nb_out_tensor);
        cleanup_runtime_objects();
        return kStatus_Fail;
    }

    for (int i = 0; i < nb_out_tensor; i++) {
        EValue &output_evalue = s_method->mutable_output(i);
        if (!output_evalue.isTensor()) {
            HAL_LOGE("Output %d is not a tensor", i);
            cleanup_runtime_objects();
            return kStatus_Fail;
        }
        Tensor output_tensor = output_evalue.toTensor();

        outputTensor[i]->data = static_cast<uint8_t *>(output_tensor.mutable_data_ptr());
        outputTensor[i]->type = map_scalar_type(output_tensor.scalar_type());
        extract_dims(output_tensor, &outputTensor[i]->dims);

        HAL_LOGI("Output[%d] tensor: type=%d, dims=[%d,%d,%d,%d]",
                 i,
                 outputTensor[i]->type,
                 outputTensor[i]->dims.data[0],
                 outputTensor[i]->dims.data[1],
                 outputTensor[i]->dims.data[2],
                 outputTensor[i]->dims.data[3]);
    }

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

    HAL_LOGD("--MODEL_EXECUTORCH_Init");
    return kStatus_Success;
}

status_t MODEL_EXECUTORCH_DeInit(void)
{
    HAL_LOGD("++MODEL_EXECUTORCH_DeInit");

    cleanup_runtime_objects();

    HAL_LOGD("--MODEL_EXECUTORCH_DeInit");
    return kStatus_Success;
}

status_t MODEL_EXECUTORCH_RunInference(void)
{
    HAL_LOGD("++MODEL_EXECUTORCH_RunInference");

    if (s_method == nullptr) {
        HAL_LOGE("Method not initialized");
        return kStatus_Fail;
    }

    Error err = s_method->execute();
    if (err != Error::Ok) {
        HAL_LOGE("Execution failed: %d", static_cast<int>(err));
        return kStatus_Fail;
    }

    HAL_LOGD("--MODEL_EXECUTORCH_RunInference");
    return kStatus_Success;
}

/*
 * Unlike TFLite, ExecuTorch models handle quantization/dequantization
 * internally through quantized_decomposed operators in the PTE file.
 *
 * Tensor dimensions are expected in NCHW order:
 *   dims[0]=N, dims[1]=C, dims[2]=H, dims[3]=W
 */
void MODEL_EXECUTORCH_ConvertInput(
    uint8_t *data,
    mpp_tensor_dims_t *dims,
    mpp_tensor_type_t type,
    int mean,
    int std)
{
    /* Total number of elements excluding batch dimension */
    int size = dims->data[1] * dims->data[2] * dims->data[3];

    switch (type)
    {
        case MPP_TENSOR_TYPE_INT8:
            if ((mean != 0) || (std != 1))
            {
                if (std != 0)
                {
                    /* Normalize uint8 pixels to int8 range:
                     * result = (pixel - mean) clamped to [-128, 127]
                     * For CIFAR-10 with mean=128: maps [0,255] -> [-128,127] */
                    for (int i = size - 1; i >= 0; i--)
                    {
                        int32_t val = (int32_t)data[i] - (int32_t)mean;
                        if (val < -128) val = -128;
                        if (val > 127) val = 127;
                        ((int8_t *)data)[i] = (int8_t)val;
                    }
                }
                else
                {
                    HAL_LOGE("Standard deviation should be different of 0.");
                }
            }
            break;

        case MPP_TENSOR_TYPE_UINT8:
            if ((mean != 0) || (std != 1))
            {
                if (std != 0)
                {
                    for (int i = size - 1; i >= 0; i--)
                    {
                        int32_t val = (int32_t)data[i] - (int32_t)mean;
                        if (val < 0) val = 0;
                        if (val > 255) val = 255;
                        data[i] = (uint8_t)val;
                    }
                }
                else
                {
                    HAL_LOGE("Standard deviation should be different of 0.");
                }
            }
            break;

        case MPP_TENSOR_TYPE_FLOAT32:
        {
            for (int i = size - 1; i >= 0; i--)
            {
                reinterpret_cast<float *>(data)[i] =
                    (static_cast<float>(data[i]) - mean) / std;
            }
        }
            break;
        default:
            assert("Unknown input tensor data type");
    }
}

#endif /* HAL_ENABLE_INFERENCE_EXECUTORCH */