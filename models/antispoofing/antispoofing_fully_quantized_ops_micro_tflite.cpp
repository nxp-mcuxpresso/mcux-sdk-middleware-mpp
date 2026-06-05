/*
 * Copyright 2025-2026 NXP
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * Set the operations used in the fully quantized antispoofing model. This allows reducing the code size.
 * Important Notice: User may find the list of operations needed by the model using tool https://netron.app
 */

#include "tensorflow/lite/micro/kernels/micro_ops.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "mpp_config.h"

#if defined(APP_USE_NEUTRON64_MODEL)
#include "tensorflow/lite/micro/kernels/neutron/neutron.h"
#endif

tflite::MicroOpResolver &MODEL_GetOpsResolver()
{
#if defined(APP_USE_NEUTRON64_MODEL)
	static tflite::MicroMutableOpResolver<6> s_microOpResolver;
    s_microOpResolver.AddMean();
    s_microOpResolver.AddQuantize();
    s_microOpResolver.AddReshape();
    s_microOpResolver.AddSoftmax();
    s_microOpResolver.AddSlice();
	s_microOpResolver.AddCustom(tflite::GetString_NEUTRON_GRAPH(), tflite::Register_NEUTRON_GRAPH());

#else
    static tflite::MicroMutableOpResolver<9> s_microOpResolver;
    s_microOpResolver.AddMean();
    s_microOpResolver.AddConv2D();
    s_microOpResolver.AddDepthwiseConv2D();
    s_microOpResolver.AddRelu();
    s_microOpResolver.AddAdd();
    s_microOpResolver.AddMaxPool2D();
    s_microOpResolver.AddSoftmax();
    s_microOpResolver.AddQuantize();
    s_microOpResolver.AddTranspose();

#endif
    return s_microOpResolver;
}
