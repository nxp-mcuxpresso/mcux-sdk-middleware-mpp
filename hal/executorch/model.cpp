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
#if (HAL_EXECUTORCH_BACKEND_NEUTRON == 1)
#include <executorch/backends/nxp/runtime/NeutronDriver.h>
#endif

#include <executorch/extension/data_loader/buffer_data_loader.h>
#include <executorch/extension/evalue_util/print_evalue.h>
#include <executorch/extension/runner_util/inputs.h>
#include <executorch/runtime/executor/method.h>
#include <executorch/runtime/executor/program.h>
#include <executorch/runtime/platform/platform.h>
#include <executorch/runtime/platform/runtime.h>

using executorch::aten::ScalarType;
using executorch::aten::Tensor;
using executorch::aten::TensorImpl;
using executorch::extension::BufferCleanup;
using executorch::extension::BufferDataLoader;
using executorch::runtime::Error;
using executorch::runtime::EValue;
using executorch::runtime::HierarchicalAllocator;
using executorch::runtime::MemoryAllocator;
using executorch::runtime::MemoryManager;
using executorch::runtime::Method;
using executorch::runtime::MethodMeta;
using executorch::runtime::Program;
using executorch::runtime::Result;
using executorch::runtime::Span;
using executorch::runtime::Tag;
using executorch::runtime::TensorInfo;

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

static int s_neutronRefCount = 0;

// ============================================================
// ✅ Memory Allocator with size tracking
// ============================================================
class CustomMemoryAllocator : public executorch::runtime::MemoryAllocator {
    public:
        CustomMemoryAllocator(uint32_t size, uint8_t* base_address)
            : MemoryAllocator(size, base_address), used_(0) {}

        void* allocate(size_t size, size_t alignment = kDefaultAlignment) override {
            void* ret = executorch::runtime::MemoryAllocator::allocate(size, alignment);
            if (ret != nullptr) {
                size_t allocator_size = executorch::runtime::MemoryAllocator::size();
                if ((size & (alignment - 1)) == 0) {
                    if (used_ > allocator_size - size) {
                        HAL_LOGE("Executorh MemoryAllocator failed(size %d, align %d, used %d)\r\n", size, alignment, used_);
                        return nullptr;
                    }
                    used_ += size;
                } else {
                    size_t aligned = (used_ | (alignment - 1)) + 1;
                    if (aligned > allocator_size - size) {
                        HAL_LOGE("Executorh MemoryAllocator failed(size %d, align %d, used %d)\r\n", size, alignment, used_);
                        return nullptr;
                    }
                    used_ = aligned + size;
                }
            }
            else {
                HAL_LOGE("Executorh MemoryAllocator failed(size %d, align %d, used %d)\r\n", size, alignment, used_);
            }
            return ret;
        }

        void reset () override {
            executorch::runtime::MemoryAllocator::reset();
            used_ = 0;
        }

        // Returns the used size of the allocator's memory buffer.
        size_t used_size() const {
            return used_;
        }

        // Returns the free size of the allocator's memory buffer.
        size_t free_size() const {
            size_t allocator_size = executorch::runtime::MemoryAllocator::size();

            if (used_ > allocator_size) {
                return 0;
            }
            return allocator_size - used_;
        }

    private:
        size_t used_;
};


/* Maximum number of models that can be initialized simultaneously */
#ifndef MAX_EXECUTORCH_MODEL_DATABASE_SIZE
#define MAX_EXECUTORCH_MODEL_DATABASE_SIZE 8
#endif

/* Database entry structure to track each initialized model */
typedef struct {
    bool in_use;
    std::unique_ptr<BufferDataLoader> loader;
    std::unique_ptr<Program> program;
    std::unique_ptr<CustomMemoryAllocator> method_allocator;
    std::unique_ptr<CustomMemoryAllocator> temp_allocator;
    Method *method;
    std::vector<uint8_t *> planned_buffers;
    std::vector<Span<uint8_t>> planned_spans;
    size_t method_arena_start;          // Start offset in s_methodArena for this model
    size_t method_arena_size;           // Size used by this model in s_methodArena
} executorch_model_database_entry_t;

/* Global database to track all initialized models */
static executorch_model_database_entry_t s_modelDatabase[MAX_EXECUTORCH_MODEL_DATABASE_SIZE] = {0};
static size_t s_nextMethodArenaOffset = 0;      // Next available offset in s_methodArena
static size_t s_highestMethodArenaEnd = 0;      // Highest end address used in s_methodArena

/* Helper function to find a free slot in the database */
static int ExecutorchFindFreeDatabaseSlot()
{
    for (int i = 0; i < MAX_EXECUTORCH_MODEL_DATABASE_SIZE; i++)
    {
        if (!s_modelDatabase[i].in_use)
        {
            return i;
        }
    }
    return -1;
}

/* Helper function to find the database entry for a given method pointer */
static int ExecutorchFindDatabaseEntry(Method* method)
{
    for (int i = 0; i < MAX_EXECUTORCH_MODEL_DATABASE_SIZE; i++)
    {
        if (s_modelDatabase[i].in_use && s_modelDatabase[i].method == method)
        {
            return i;
        }
    }
    return -1;
}

/* Helper function to recalculate method arena usage after deinitialization */
static void RecalculateMethodArenaUsage()
{
    size_t highest_end = 0;
    bool has_active_models = false;

    /* Find the highest end address of all active models */
    for (int i = 0; i < MAX_EXECUTORCH_MODEL_DATABASE_SIZE; i++)
    {
        if (s_modelDatabase[i].in_use)
        {
            has_active_models = true;
            size_t model_end = s_modelDatabase[i].method_arena_start + s_modelDatabase[i].method_arena_size;
            
            if (model_end > highest_end)
            {
                highest_end = model_end;
            }
        }
    }

    /* Update global tracking */
    if (has_active_models)
    {
        s_highestMethodArenaEnd = highest_end;
        s_nextMethodArenaOffset = highest_end;
    }
    else
    {
        /* No active models, reset everything */
        s_nextMethodArenaOffset = 0;
        s_highestMethodArenaEnd = 0;
    }

    HAL_LOGI("Method arena recalculated: next_offset=%d, highest_end=%d, available=%d\r\n",
              s_nextMethodArenaOffset, s_highestMethodArenaEnd,
              kMethodArenaSize - s_nextMethodArenaOffset);
}

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
static void cleanup_runtime_objects(int slot)
{
    if (slot < 0 || slot >= MAX_EXECUTORCH_MODEL_DATABASE_SIZE)
    {
        return;
    }

    if (s_modelDatabase[slot].method)
    {
        delete s_modelDatabase[slot].method;
        s_modelDatabase[slot].method = nullptr;
    }
    s_modelDatabase[slot].planned_buffers.clear();
    s_modelDatabase[slot].planned_spans.clear();
    s_modelDatabase[slot].program.reset();
    s_modelDatabase[slot].loader.reset();
    s_modelDatabase[slot].temp_allocator.reset();
    s_modelDatabase[slot].method_allocator.reset();
    s_modelDatabase[slot].in_use = false;
}

status_t MODEL_EXECUTORCH_Init(
    const void *pte_data,
    size_t pte_size,
    model_executorch_interpreter_data_t *interpreter_data,
    mpp_inference_tensor_params_t *inputTensor,
    mpp_inference_tensor_params_t *outputTensor[],
    int mean,
    int std,
    int nb_out_tensor)
{
    /* Find a free slot in the database */
    int slot = ExecutorchFindFreeDatabaseSlot();
    if (slot < 0)
    {
        HAL_LOGE("Model database is full. Maximum %d models can be initialized simultaneously\r\n", MAX_EXECUTORCH_MODEL_DATABASE_SIZE);
        return kStatus_Fail;
    }

    #if (HAL_EXECUTORCH_BACKEND_NEUTRON == 1)
    if (s_neutronRefCount == 0) {
        NeutronError error = ENONE;
        error = neutronInit();
        if (error != ENONE) {
            HAL_LOGE("Internal Neutron NPU driver error %x in init!\n", error);
            return kStatus_Fail;
        }
    }
    s_neutronRefCount++;
    #endif

    HAL_LOGD("++MODEL_EXECUTORCH_Init (slot %d)\r\n", slot);

    /* Initialize ExecuTorch runtime */
    executorch::runtime::runtime_init();

    /* Allocate method arena slice for this model */
    size_t method_arena_start = s_nextMethodArenaOffset;
    size_t method_arena_size = kMethodArenaSize - s_nextMethodArenaOffset;

    /* Create memory allocators */
    s_modelDatabase[slot].method_allocator = std::make_unique<CustomMemoryAllocator>(
        method_arena_size, &s_methodArena[method_arena_start]);
    s_modelDatabase[slot].temp_allocator = std::make_unique<CustomMemoryAllocator>(
        kTempArenaSize, s_tempArena);

    if (!s_modelDatabase[slot].method_allocator || !s_modelDatabase[slot].temp_allocator) {
        HAL_LOGE("Failed to create allocators");
        cleanup_runtime_objects(slot);
        return kStatus_Fail;
    }

    HAL_LOGD("Memory allocators created:\r\n");
    HAL_LOGD("  Method arena: %u bytes at offset %d ( start 0x%x)\r\n", 
             method_arena_size, method_arena_start, &s_methodArena[method_arena_start]);
    HAL_LOGD("  Temp arena: %u bytes at 0x%x (shared)\r\n", kTempArenaSize, s_tempArena);

    /* Create data loader from PTE buffer */
    s_modelDatabase[slot].loader = std::make_unique<BufferDataLoader>(pte_data, pte_size);
    if (!s_modelDatabase[slot].loader) {
        HAL_LOGE("Failed to create BufferDataLoader\r\n");
        return kStatus_Fail;
    }

    /* Load program */
    Result<Program> program_res = Program::load(s_modelDatabase[slot].loader.get());
    if (!program_res.ok()) {
        HAL_LOGE("Program load failed: %d\r\n",
                 static_cast<int>(program_res.error()));
        return kStatus_Fail;
    }
    s_modelDatabase[slot].program = std::make_unique<Program>(std::move(program_res.get()));

    HAL_LOGI("Model buffer loaded, has %d methods\r\n", s_modelDatabase[slot].program->num_methods());

    /* Get method name and metadata for memory planning */
    const char *method_name = nullptr;
    {
        const auto method_name_result = s_modelDatabase[slot].program->get_method_name(0);
        if (!method_name_result.ok()) {
            HAL_LOGE("Program has no methods\r\n");
            cleanup_runtime_objects(slot);
            return kStatus_Fail;
        }
        method_name = *method_name_result;
    }
    HAL_LOGI("Using method: %s\r\n", method_name);

    Result<MethodMeta> method_meta = s_modelDatabase[slot].program->method_meta(method_name);
    if (!method_meta.ok()) {
        HAL_LOGE("Failed to get method_meta for %s: 0x%x\r\n",
                 method_name, static_cast<unsigned int>(method_meta.error()));
        cleanup_runtime_objects(slot);
        return kStatus_Fail;
    }

    /* Setup planned memory buffers */
    size_t num_planned_buffers = method_meta->num_memory_planned_buffers();
    HAL_LOGI("Method requires %u planned memory buffers\r\n", num_planned_buffers);

    // Reserve capacity to prevent vector reallocation
    s_modelDatabase[slot].planned_buffers.reserve(num_planned_buffers);
    s_modelDatabase[slot].planned_spans.reserve(num_planned_buffers);

    for (size_t id = 0; id < num_planned_buffers; ++id) {
        size_t buffer_size = static_cast<size_t>(
            method_meta->memory_planned_buffer_size(id).get());
        HAL_LOGI("Setting up planned buffer %u, size %u\r\n", id, buffer_size);

        // Allocate with proper alignment
        uint8_t *buffer = reinterpret_cast<uint8_t *>(
            s_modelDatabase[slot].method_allocator->allocate(buffer_size, HAL_EXECUTORCH_BUFFER_ALIGN));

        if (buffer == nullptr) {
            HAL_LOGE("Failed to allocate planned buffer %u\r\n", id);
            cleanup_runtime_objects(slot);
            return kStatus_Fail;
        }

        // Verify alignment
        if (((uintptr_t)buffer & (HAL_EXECUTORCH_BUFFER_ALIGN - 1)) != 0) {
            HAL_LOGE("Planned buffer %u not properly aligned (addr=0x%x)\r\n",
                     id, (unsigned int)buffer);
            cleanup_runtime_objects(slot);
            return kStatus_Fail;
        }

        s_modelDatabase[slot].planned_buffers.push_back(buffer);
        s_modelDatabase[slot].planned_spans.push_back({s_modelDatabase[slot].planned_buffers.back(), buffer_size});
    }

    /* Create HierarchicalAllocator and MemoryManager */
    HierarchicalAllocator planned_memory(
        {s_modelDatabase[slot].planned_spans.data(), s_modelDatabase[slot].planned_spans.size()});
    MemoryManager memory_manager(
        s_modelDatabase[slot].method_allocator.get(), &planned_memory, s_modelDatabase[slot].temp_allocator.get());

    /* 8. Load method */
    Result<Method> method_res =
        s_modelDatabase[slot].program->load_method(method_name, &memory_manager);
    if (!method_res.ok()) {
        HAL_LOGE("Loading of method %s failed with status 0x%x\r\n",
                 method_name, static_cast<unsigned int>(method_res.error()));
        cleanup_runtime_objects(slot);
        return kStatus_Fail;
    }
    s_modelDatabase[slot].method = new Method(std::move(method_res.get()));

    HAL_LOGI("Method loaded successfully\r\n");

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
    size_t num_inputs = s_modelDatabase[slot].method->inputs_size();
    if (num_inputs == 0) {
        HAL_LOGE("No input tensors found\r\n");
        cleanup_runtime_objects(slot);
        return kStatus_Fail;
    }
    HAL_LOGI("%u input tensors found\r\n", num_inputs);

    EValue &input_evalue = s_modelDatabase[slot].method->mutable_input(0);
    if (!input_evalue.isTensor()) {
        HAL_LOGE("Input 0 is not a tensor\r\n");
        cleanup_runtime_objects(slot);
        return kStatus_Fail;
    }
    Tensor input_tensor = input_evalue.toTensor();

    inputTensor->data = static_cast<uint8_t *>(input_tensor.mutable_data_ptr());
    inputTensor->type = map_scalar_type(input_tensor.scalar_type());
    extract_dims(input_tensor, &inputTensor->dims);

    HAL_LOGI("Input tensor: type=%d, dims=[%d,%d,%d,%d] (NCHW)\r\n",
             inputTensor->type,
             inputTensor->dims.data[0],
             inputTensor->dims.data[1],
             inputTensor->dims.data[2],
             inputTensor->dims.data[3]);

    /* Get output tensor metadata */
    size_t num_outputs = s_modelDatabase[slot].method->outputs_size();
    HAL_LOGI("%u output tensors found\r\n", num_outputs);

    if ((int)num_outputs < nb_out_tensor) {
        HAL_LOGE("Model has %u outputs, but %d requested\r\n",
                 num_outputs, nb_out_tensor);
        cleanup_runtime_objects(slot);
        return kStatus_Fail;
    }

    for (int i = 0; i < nb_out_tensor; i++) {
        EValue &output_evalue = s_modelDatabase[slot].method->mutable_output(i);
        if (!output_evalue.isTensor()) {
            HAL_LOGE("Output %d is not a tensor\r\n", i);
            cleanup_runtime_objects(slot);
            return kStatus_Fail;
        }
        Tensor output_tensor = output_evalue.toTensor();

        outputTensor[i]->data = static_cast<uint8_t *>(output_tensor.mutable_data_ptr());
        outputTensor[i]->type = map_scalar_type(output_tensor.scalar_type());
        extract_dims(output_tensor, &outputTensor[i]->dims);

        HAL_LOGI("Output[%d] tensor: type=%d, dims=[%d,%d,%d,%d]\r\n",
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

    size_t total_method_arena_used = s_modelDatabase[slot].method_allocator->used_size();

    /* Align the used size to HAL_EXECUTORCH_BUFFER_ALIGN */
    total_method_arena_used = (total_method_arena_used + HAL_EXECUTORCH_BUFFER_ALIGN - 1) 
                                & ~(HAL_EXECUTORCH_BUFFER_ALIGN - 1);

    HAL_LOGI("Method arena used: %u bytes (aligned to %u)\r\n", total_method_arena_used, HAL_EXECUTORCH_BUFFER_ALIGN);

    /* Update database entry with memory tracking info */
    s_modelDatabase[slot].method_arena_start = method_arena_start;
    s_modelDatabase[slot].method_arena_size = total_method_arena_used;

    /* Update global method arena tracking */
    s_nextMethodArenaOffset = method_arena_start + total_method_arena_used;
    if (s_nextMethodArenaOffset > s_highestMethodArenaEnd) {
        s_highestMethodArenaEnd = s_nextMethodArenaOffset;
    }

    /* Mark database entry as in use and store interpreter data */
    s_modelDatabase[slot].in_use = true;
    interpreter_data->s_method = (void*)s_modelDatabase[slot].method;

    HAL_LOGI("Model initialized successfully at slot %d\r\n", slot);
    HAL_LOGI("  Method arena state:\r\n    next_offset=%d,\r\n    highest_end=%d,\r\n    available=%d\r\n",
             s_nextMethodArenaOffset, s_highestMethodArenaEnd,
             kMethodArenaSize - s_nextMethodArenaOffset);

    HAL_LOGD("--MODEL_EXECUTORCH_Init\r\n");
    return kStatus_Success;
}

status_t MODEL_EXECUTORCH_DeInit(model_executorch_interpreter_data_t *interpreter_data)
{
    HAL_LOGD("++MODEL_EXECUTORCH_DeInit");

    Method* method = static_cast<Method*>(interpreter_data->s_method);
    
    /* Find the database entry for this method */
    int slot = ExecutorchFindDatabaseEntry(method);
    if (slot < 0)
    {
        HAL_LOGE("Method not found in database\r\n");
        return kStatus_Fail;
    }

    HAL_LOGI("Deinitializing model from slot %d\r\n", slot);
    HAL_LOGI("  Method arena: start=%d, size=%d\r\n",
             s_modelDatabase[slot].method_arena_start,
             s_modelDatabase[slot].method_arena_size);

    /* Check if this model has the highest end address */
    size_t model_end = s_modelDatabase[slot].method_arena_start + s_modelDatabase[slot].method_arena_size;
    bool is_highest_end = (model_end == s_highestMethodArenaEnd);

    #if (HAL_EXECUTORCH_BACKEND_NEUTRON == 1)
        s_neutronRefCount--;
        if (s_neutronRefCount == 0) {
            neutronDeinit();
        }
    #endif

    cleanup_runtime_objects(slot);

    /* Clear interpreter data */
    interpreter_data->s_method = nullptr;

    if (is_highest_end)
    {
        HAL_LOGI("Freed model had the highest end address, reclaiming space\r\n");
    }
    else
    {
        HAL_LOGI("Freed model's method arena creates a gap (will be reclaimed when all models are freed)\r\n");
    }

    /* Recalculate method arena usage based on remaining active models */
    RecalculateMethodArenaUsage();

    HAL_LOGD("--MODEL_EXECUTORCH_DeInit");
    return kStatus_Success;
}

status_t MODEL_EXECUTORCH_RunInference(model_executorch_interpreter_data_t *interpreter_data)
{
    HAL_LOGD("++MODEL_EXECUTORCH_RunInference");

    Method* method = static_cast<Method*>(interpreter_data->s_method);
    if (method == nullptr) {
        HAL_LOGE("Method not initialized");
        return kStatus_Fail;
    }

    Error err = method->execute();
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
 * Tensor dimensions are expected in NHWC, input is NCHW order:
 *   dims[0]=N, dims[1]=C, dims[2]=H, dims[3]=W
 *      ------->
 *   dims[0]=N, dims[1]=H, dims[2]=W, dims[3]=C
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
                HAL_LOGD("FLOAT32 input already normalized.");
            }
            break;

        default:
            assert("Unknown input tensor data type");
    }
}

#endif /* HAL_ENABLE_INFERENCE_EXECUTORCH */