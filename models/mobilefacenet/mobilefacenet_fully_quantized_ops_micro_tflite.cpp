/*
 * Copyright 2024-2025 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Set the operations used in the fully quantized model Mobilefacenet. This allows reducing the code size.
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
	static tflite::MicroMutableOpResolver<2> s_microOpResolver;
    s_microOpResolver.AddMul();
	s_microOpResolver.AddCustom(tflite::GetString_NEUTRON_GRAPH(), tflite::Register_NEUTRON_GRAPH());
#else
    static tflite::MicroMutableOpResolver<9> s_microOpResolver;
    s_microOpResolver.AddSub();
    s_microOpResolver.AddMul();
    s_microOpResolver.AddPad();
    s_microOpResolver.AddMean();
    s_microOpResolver.AddConv2D();
    s_microOpResolver.AddDepthwiseConv2D();
    s_microOpResolver.AddPrelu();
    s_microOpResolver.AddAdd();
    s_microOpResolver.AddFullyConnected();

#endif
    return s_microOpResolver;
}
