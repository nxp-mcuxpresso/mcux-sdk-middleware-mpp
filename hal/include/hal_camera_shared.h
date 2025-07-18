/*
 * Copyright 2025 NXP.
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

 /**
 * @defgroup HAL_VIRTUAL_CAMERA_TYPES HAL Types
 *
 * This section provides the detailed documentation for the MPP HAL VIRTUAL CAMERA types
 *
 * @{
 */

#ifndef _HAL_CAMERA_SHARED_H_
#define _HAL_CAMERA_SHARED_H_

#include <stdint.h>


/** @brief Buffer alignment requirement for camera device buffers in bytes. */
#define CAMERA_DEV_BUFFER_ALIGN        64

/** @brief Endpoint address for Core 1 inter-core communication channel. */
#define CORE1_EPT_ADDRESS              (30U)
/** @brief Endpoint address for MPP (Media Processing Pipeline) inter-core communication channel.
 * MPP might use a range of endpoints starting with 40 and up to 49 included
 */
#define MPP_EPT_ADDRESSS               (40U)
/** @brief Endpoint address for RTSP (Real Time Streaming Protocol) inter-core communication channel. */
#define RTSP_EPT_ADDRESS               (50U)

/** @brief Event data value indicating that the application endpoint is ready for communication. */
#define APP_EP_READY_EVENT_DATA        (2U)

/** @brief Structure that characterizes the exchanged message types between core 0 and core 1. */
typedef enum virtual_usb_cam_msg_type {
    VIRT_USB_CAM_NOMSG,
    VIRT_USB_CAM_CONFIG,
    VIRT_USB_CAM_CONFIG_ACK,
    VIRT_USB_CAM_CONFIG_ERR,
    VIRT_USB_CAM_REQRGB,
    VIRT_USB_CAM_REQIR,
    VIRT_USB_CAM_REQRGBIR,
    VIRT_USB_CAM_RSPRGB,
    VIRT_USB_CAM_RSPIR,
    VIRT_USB_CAM_RSPRGBIR,
    VIRT_USB_CAM_ERROR
} virtual_usb_cam_msg_type_e;

typedef enum virtual_usb_cam_user_id {
    RTSP_USER_ID = 0,
    MPP_USER_ID
} virtual_usb_cam_user_id_e;

/** @brief Structure that characterizes the color format for the camera output. */
typedef enum virtual_usb_cam_col_format {
    VIRT_USB_CAM_JPEG
} virtual_usb_cam_col_format_e;

/** @brief Structure that characterizes the payload of the camera config message sent from core 0 to core 1. */
typedef struct {
    uint32_t camera_width;                   /*!< Width of the camera output in pixels */
    uint32_t camera_height;                  /*!< Height of the camera output in pixels */
    virtual_usb_cam_col_format_e color_format; /*!< Color format for the camera output (e.g., JPEG) */
    uint32_t fps;                           /*!< Frames per second for camera capture rate */
} virtual_usb_cam_config_msg_t;

/** @brief Structure that characterizes the payload of the camera request message containing frame buffer addresses for RGB and IR data. */
typedef struct {
    uint32_t rgb_frame_addr;    /*!< Physical address of the RGB frame buffer */
    uint32_t rgb_max_frame_size;  /*!< Maximum size allocated for RGB frame buffer */
    uint32_t ir_frame_addr;     /*!< Physical address of the IR frame buffer */
    uint32_t ir_max_frame_size; /*!< Maximum size allocated for IR frame buffer */
} virtual_usb_cam_req_msg_t;

/** @brief Structure that characterizes the payload of the camera request message containing frame addresses and sizes for RGB and IR data. */
typedef struct {
    uint32_t rgb_frame_addr;    /*!< Physical address of the RGB frame buffer (looped back by the core 1 camera app) */
    uint32_t rgb_frame_size;    /*!< Size in bytes of the RGB frame data */
    uint32_t ir_frame_addr;     /*!< Physical address of the IR frame buffer (looped back by the core 1 camera app) */
    uint32_t ir_frame_size;     /*!< Size in bytes of the IR frame data */
} virtual_usb_cam_rsp_msg_t;

/** @brief Structure that characterizes the messages sent between cores. */
typedef struct {
    virtual_usb_cam_msg_type_e msg_type;    /*!< Type of message being sent (config, request, response, etc.) */
    virtual_usb_cam_user_id_e  user_id;     /*!< Identifier for the user/component sending the message (RTSP or MPP) */
    union msg_payload_u
    {
        virtual_usb_cam_config_msg_t config; /*!< Configuration message payload for camera setup */
        virtual_usb_cam_req_msg_t    req;    /*!< Request message payload containing frame buffer addresses */
        virtual_usb_cam_rsp_msg_t    rsp;    /*!< Response message payload containing frame data and sizes */
    } msg_payload;                          /*!< Union containing the actual message data based on msg_type */
} virtual_usb_cam_msg_t;

/** @} */

#endif /* _HAL_CAMERA_SHARED_H_ */
