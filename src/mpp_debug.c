/*
 * Copyright 2020-2025 NXP.
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

#include "hal_os.h"
#include <sys/time.h>
#include "mpp_api_types.h"
#include "mpp_api_types_internal.h"

int tick_check_rate(uint32_t *last, int *curr, int max)
{

    /*
     * Reset the last time and counter if this is the first call
     * or more than a second has passed since the last update of
     * lasttime.
     */

    uint32_t now = hal_get_ostick();
    if (!*last || (unsigned int)(now - *last) >= hal_get_tick_rate_hz()) {
        *last= now;
        *curr = 1;
        return (max != 0);
    } else {
        (*curr)++;            /* NB: ignore overflow */
        return (max < 0 || *curr < max);
    }
}

/* get string from element id */
char * elem_name(_elem_t *elem)
{
    char *str = NULL;
    switch(elem->type)
    {
    case MPP_TYPE_SINK:
        switch(elem->sink_typ)
        {
        case MPP_SINK_DISPLAY:
            str = "DISPLAY";
            break;
        case MPP_SINK_NULL:
            str = "NULL SINK";
            break;
        default:
            str = "INVALID SINK";
            break;
        }
        break;
    case MPP_TYPE_SOURCE:
        switch(elem->src_typ)
        {
        case MPP_SRC_CAMERA:
            str = "CAMERA";
            break;
        case MPP_SRC_STATIC_IMAGE:
            str = "STATIC_IMAGE";
            break;
        case MPP_SRC_FILE:
            str = "SRC_FILE";
            break;
        default:
            str = "INVALID SOURCE";
            break;
        }
        break;
    case MPP_TYPE_PROC:
        switch(elem->proc_typ)
        {
        case MPP_ELEMENT_IMG_COMPOSE:
            str = "COMPOSE";
            break;
        case MPP_ELEMENT_CONVERT:
            str = "CONVERT";
            break;
        case MPP_ELEMENT_INFERENCE:
            str = "INFERENCE";
            break;
        case MPP_ELEMENT_INVALID:
            str = "INVALID";
            break;
        case MPP_ELEMENT_LABELED_RECTANGLE:
            str = "LABELED_RECTANGLE";
            break;
        case MPP_ELEMENT_TEST:
            str = "TEST";
            break;
        case MPP_ELEMENT_IMG_DECODE:
            str = "IMG_DECODE";
            break;
        case MPP_ELEMENT_NUM:
        default:
            str = "***BAD ELEMENT!***";
            break;
        }
        break;
    default:
        str = "***BAD ELEMENT TYPE!***";
        break;
    }
    return str;
}

