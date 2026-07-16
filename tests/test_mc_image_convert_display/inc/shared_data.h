/*
 * Copyright 2026 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _SHARED_DATA_H_
#define _SHARED_DATA_H_

#include "mpp_api.h"

typedef struct _user_data_t {
    mpp_t core0_mpp;
    mpp_t core1_mpp;
} user_data_t;

#endif /* _SHARED_DATA_H_ */