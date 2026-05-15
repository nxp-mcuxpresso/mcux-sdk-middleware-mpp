/*
 * Copyright 2026 NXP
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

#ifndef _RTSP_SERVER_H_
#define _RTSP_SERVER_H_

#include <stdio.h>

#include "hal.h"
#include "hal_rtspsink.h"

/* RTSP Server Configuration */
#define RTSP_MAX_SESSION_NUM         1  /*!< Number of supported clients */
#define RTSP_SESSION_RECV_BUF_SIZE   512
#define RTSP_TEMP_BUFFER_SIZE        256
#define RTSP_RESPONSE_BUFFER_SIZE    512
#define RTSP_TRANSPORT_BUFFER_SIZE   256

/* Task stack size */
#define RTSP_TASKS_STACK_SIZE        2056

/* RTSP server task priority
 * Set to be lower than typical MPP control tasks */
#define RTSP_TASK_PRIORITY           2


/* RTP Configuration */
#define RTP_HEADER_SIZE              12
#define RTP_FRAGMENT_SIZE            1400 /*!< MTU-friendly payload fragment size */

/* JPEG specific config */
#define JPEG_HEADER_SIZE             8
#define JPEG_QTABLE                  80
#define RTP_JPEG_FRAGMENT_SIZE       RTP_FRAGMENT_SIZE
#define RTP_JPEG_VIDEO_SEND_BUF_SIZE (RTP_HEADER_SIZE + JPEG_HEADER_SIZE + RTP_JPEG_FRAGMENT_SIZE)

/* H.264 specific config */
#define H264_NAL_HEADER_SIZE          1   /*!< Size of standard NAL unit header */
#define H264_FU_A_HEADER_SIZE         2   /*!< Size of FU-A indicator + FU header */
#define RTP_H264_FRAGMENT_SIZE        RTP_FRAGMENT_SIZE
#define RTP_H264_VIDEO_SEND_BUF_SIZE  (RTP_HEADER_SIZE + H264_FU_A_HEADER_SIZE + RTP_H264_FRAGMENT_SIZE)

/* H.264 NAL unit types and masks */
#define H264_NAL_TYPE_MASK           0x1F
#define H264_NAL_TYPE_SPS            7
#define H264_NAL_TYPE_PPS            8
#define H264_NAL_TYPE_IDR            5
#define H264_NAL_TYPE_SLICE          1

/* H.264 FU-A indicator and header bits */
#define H264_FU_A_TYPE               28
#define H264_FU_START_BIT            0x80
#define H264_FU_END_BIT              0x40

/* RTSP Protocol Strings */
#define STR_RTSP_SERVER_PORT_VIDEO ";server_port=45678-45679\r\n"

#define STR_OK_RESPONSE "RTSP/1.0 200 OK\r\nCSeq: %d\r\n"
#define STR_BAD_REQUEST "RTSP/1.0 400 Bad Request\r\nCSeq: %d\r\n\r\n"
#define STR_INVALID_STATE "RTSP/1.0 455 Method Not Valid in This State\r\nCSeq: %d\r\n\r\n"
#define STR_UNSUPPORTED_RESPONSE "RTSP/1.0 551 Option not supported\r\nCSeq: %d\r\nUnsupported: %s\r\n"
#define STR_INTERNAL_ERROR_RESPONSE "RTSP/1.0 500 Internal Server Error\r\nCSeq: %d\r\n\r\n"
#define STR_OPTIONS_RESPONSE "RTSP/1.0 200 OK\r\nCSeq: %d\r\nPublic: OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN\r\n\r\n"
#define STR_DESCRIBE_RESPONSE "RTSP/1.0 200 OK\r\nCSeq: %d\r\nContent-Base: rtsp://%s:%d/\r\nContent-Type: application/sdp\r\nContent-Length: %d\r\n\r\n%s"
#define STR_SDP "v=0\r\no=- 0 0 IN IP4 %s\r\ns=RTSP Server\r\nt=0 0\r\nm=video 0 RTP/AVP %d\r\na=rtpmap:%d %s\r\na=control:trackID=0\r\n\r\n"

typedef enum
{
	RTSP_UNINIT = 0,
    RTSP_INIT,
    RTSP_READY,
    RTSP_PLAYING
} rtsp_session_state_t;

typedef enum
{
	MEDIA_TYPE_VIDEO = 0
} rtsp_media_type_t;

/** @brief RTSP sink detailed error codes for internal use and debugging */
typedef enum _hal_rtspsink_error_code
{
    RTSPSINK_SUCCESS = 0,                  /*!< Operation successful */

    /* Initialization errors (1-10) */
    RTSPSINK_ERR_INVALID_PARAMS = 1,       /*!< Invalid parameters provided */
    RTSPSINK_ERR_SOCKET_FAILED,            /*!< Socket create/bind/listen failed */
    RTSPSINK_ERR_PORT_IN_USE,              /*!< Port already in use */

    /* Runtime errors (11-20) */
    RTSPSINK_ERR_ACCEPT_FAILED = 11,       /*!< accept() failed */
    RTSPSINK_ERR_TASK_CREATE_FAILED,       /*!< Failed to create RTOS task */
    RTSPSINK_ERR_SOCKET_OPT_FAILED,        /*!< setsockopt/getpeername failed */

    /* Session/Protocol errors (21-30) */
    RTSPSINK_ERR_SESSION_TIMEOUT = 21,     /*!< Session timeout or recv error */
    RTSPSINK_ERR_INVALID_STATE,            /*!< Invalid RTSP state for operation */
    RTSPSINK_ERR_BAD_REQUEST,              /*!< Bad/unsupported RTSP request */
    RTSPSINK_ERR_MISSING_HEADER,           /*!< Missing required header (client_port, session ID, etc) */
    RTSPSINK_ERR_STREAM_ACTIVE,            /*!< Stream already active */

    /* Memory/Resource errors (31-40) */
    RTSPSINK_ERR_ALLOC_FAILED = 31,        /*!< Memory allocation failed (SDP/RTP/PDU buffer) */
    RTSPSINK_ERR_SEMAPHORE_FAILED,         /*!< Failed to create semaphore */

    /* RTP errors (41-50) */
    RTSPSINK_ERR_UNSUPPORTED_PAYLOAD = 41, /*!< Unsupported payload type */
    RTSPSINK_ERR_RTP_SOCKET_FAILED,        /*!< RTP socket create/bind failed */

    /* Feature not enabled (51-60) */
    RTSPSINK_ERR_NOT_ENABLED = 51          /*!< RTSP Sink not enabled in build */
} hal_rtspsink_error_code_t;

void rtsp_server_task(void *arg);
void rtsp_session_task(void *arg);
void rtp_task(void *arg);
void rtp_task_pdu(const void* buf, uint32_t len, void* params);
void safe_close_socket(int *sock_fd);

#ifdef HAL_ENABLE_RTSP_SINK
#include "lwip/sockets.h"
#include "lwip/netdb.h"

typedef struct
{
	uint32_t len;
	const uint8_t *data;
} stream_pdu_t;

typedef struct
{
    struct sockaddr_in client_addr;
    int rtp_sock;
    SemaphoreHandle_t sem;
    stream_pdu_t *rx_pdu;
    uint8_t *rtp_buffer;
    uint32_t timestamp;
    uint32_t ts_unit;
    uint16_t seq;
    volatile uint8_t send_pending;
    bool is_active;
    rtsp_media_type_t media_type;
    bool sps_sent;
    bool pps_sent;
} media_stream_t;

typedef struct
{
    int sess_sock;
    media_stream_t video;
    TaskHandle_t handler;
    uint16_t lastSeq;
    rtsp_session_state_t state;
    rtspsink_t *dev;
    char session_id[32];
} rtsp_session_t;

/** @brief RTSP context structure */
typedef struct {
    rtsp_session_t sessions[RTSP_MAX_SESSION_NUM];
    int listen_fd;
    TaskHandle_t server_task_handle;
    bool server_running;
} rtsp_context_t;

#endif /* HAL_ENABLE_RTSP_SINK */

#endif /* _RTSP_SERVER_H_ */
