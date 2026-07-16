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

#ifndef _RTSP_CLIENT_H_
#define _RTSP_CLIENT_H_

#include <stdio.h>

#include "hal.h"
#include "hal_rtspsrc.h"

/* Task stack sizes */
#define RTSP_CLIENT_TASK_STACK_SIZE   4096
#define RTP_RECV_TASK_STACK_SIZE      4096
#define RTP_DEBUG_TASK_STACK_SIZE     2048

/* RTP recv task priority must be above lwip_rx to drain UDP packets promptly. */
#define RTSP_CLIENT_TASK_PRIORITY     (configMAX_PRIORITIES - 2)
#define RTP_RECV_TASK_PRIORITY        (configMAX_PRIORITIES - 1)

/* URL path buffer */
#define RTSP_URL_PATH_MAX_LEN         256

/* RTSP TCP receive buffer for responses */
#define RTSP_RECV_BUF_SIZE            1024

/* RTP UDP receive buffer (MTU-sized) */
#define RTP_RECV_BUF_SIZE             1500

/* UDP socket receive buffer - large enough to hold 2 complete frames. */
#define RTP_SOCKET_RCVBUF_SIZE        (512 * 1024)

/* Keepalive interval - must be shorter than the server session timeout. */
#define RTSP_KEEPALIVE_INTERVAL_MS    10000

/* Delay between reconnect attempts after server loss (ms). */
#define RTSP_RECONNECT_DELAY_MS       5000

/* Local UDP port to bind for RTP reception */
#define RTP_CLIENT_LOCAL_PORT         45680

/* Sizes of RTP and RFC 2435 JPEG-over-RTP subheaders */
#define RTP_HEADER_SIZE               12
#define JPEG_HEADER_SIZE              8

/* Size of JPEG header */
#define JPEG_HDR_MAX                  700u

/* RTP marker bit mask */
#define RTP_MARKER_BIT                0x80

/* RTSP request format strings */
#define STR_OPTIONS_REQ  "OPTIONS %s RTSP/1.0\r\nCSeq: %d\r\n\r\n"
#define STR_DESCRIBE_REQ "DESCRIBE %s RTSP/1.0\r\nCSeq: %d\r\nAccept: application/sdp\r\n\r\n"
#define STR_SETUP_REQ    "SETUP %s RTSP/1.0\r\nCSeq: %d\r\nTransport: RTP/AVP;unicast;client_port=%d-%d\r\n\r\n"
#define STR_PLAY_REQ     "PLAY %s RTSP/1.0\r\nCSeq: %d\r\nSession: %s\r\nRange: npt=0.000-\r\n\r\n"
#define STR_TEARDOWN_REQ "TEARDOWN %s RTSP/1.0\r\nCSeq: %d\r\nSession: %s\r\n\r\n"

/** @brief RTSP source internal error codes */
typedef enum _hal_rtspsrc_error_code
{
    RTSPSRC_SUCCESS = 0,                   /*!< Operation successful */
    RTSPSRC_ERR_INVALID_PARAMS = 1,        /*!< Invalid parameters */
    RTSPSRC_ERR_SOCKET_FAILED,             /*!< Socket create/bind/connect failed */
    RTSPSRC_ERR_URL_PARSE_FAILED,          /*!< Failed to parse RTSP URL */
    RTSPSRC_ERR_CONNECT_FAILED,            /*!< TCP connect to server failed */
    RTSPSRC_ERR_SEND_FAILED,               /*!< RTSP send failed */
    RTSPSRC_ERR_RECV_FAILED,               /*!< RTSP recv failed */
    RTSPSRC_ERR_BAD_RESPONSE,              /*!< Server returned non-200 response */
    RTSPSRC_ERR_TASK_CREATE_FAILED,        /*!< FreeRTOS task creation failed */
} hal_rtspsrc_error_code_t;

void rtsp_client_task(void *arg);
void rtp_recv_task(void *arg);
void rtp_debug_task(void *arg);

#ifdef HAL_ENABLE_RTSP_SRC
#include "lwip/sockets.h"
#include "lwip/netdb.h"

/* RTSP client state machine */
typedef enum
{
    RTSP_CLIENT_UNINIT = 0,
    RTSP_CLIENT_INIT,
    RTSP_CLIENT_READY,
    RTSP_CLIENT_PLAYING,
    RTSP_CLIENT_TEARDOWN
} rtsp_client_state_t;

/* RTP fixed header - 12 bytes, network byte order */
typedef struct {
    uint8_t  vpxcc;
    uint8_t  mpt;
    uint16_t seq;
    uint32_t timestamp;
    uint32_t ssrc;
} PACK_STRUCT_STRUCT rtp_recv_header_t;

/* RFC 2435 JPEG subheader - 8 bytes */
typedef struct {
    uint8_t  type_specific;
    uint8_t  fragment_offset1;
    uint8_t  fragment_offset2;
    uint8_t  fragment_offset3;
    uint8_t  type;
    uint8_t  q;
    uint8_t  width;
    uint8_t  height;
} PACK_STRUCT_STRUCT jpeg_recv_header_t;

typedef struct
{
    int                     sess_sock;          /*!< RTSP TCP socket */
    int                     rtp_sock;           /*!< RTP UDP receive socket */
    TaskHandle_t            handler;            /*!< RTSP client task handle */
    TaskHandle_t            rtp_task_handler;   /*!< RTP receive task handle */
    rtsp_client_state_t     state;
    rtspsrc_t              *dev;
    char                    server_ip[IPV4_STR_LEN];
    uint16_t                server_port;
    char                    url_path[RTSP_URL_PATH_MAX_LEN];
    char                    session_id[128];    /*!< RTSP Session header value */
    uint16_t                cseq;               /*!< Current CSeq counter */
    uint8_t                *rtp_buf;            /*!< Buffer the RTP task is currently filling (ping-pong write side) */
    uint32_t                rtp_buf_size;       /*!< Usable size of each ping-pong half */
    uint8_t                *dec_buf;            /*!< Buffer handed to the decoder (ping-pong read side) */
    volatile uint32_t       frame_size;         /*!< Byte count of last complete frame */
    volatile bool           frame_pending;      /*!< True when a complete frame is ready */
    SemaphoreHandle_t       frame_ready_sem;    /*!< Signalled when a complete frame is available */
    SemaphoreHandle_t       buf_mutex;          /*!< Mutex protecting rtp_buf/dec_buf */
    uint16_t                jpeg_width_pix;     /*!< Frame width in pixels */
    uint16_t                jpeg_height_pix;    /*!< Frame height in pixels */
    uint8_t                 jpeg_q;             /*!< RFC 2435 q field */
} rtsp_client_session_t;

/* Top-level RTSP client context */
typedef struct {
    rtsp_client_session_t   session;
    TaskHandle_t            client_task_handle;
    bool                    client_running;
} rtsp_client_context_t;

#endif /* HAL_ENABLE_RTSP_SRC */

#endif /* _RTSP_CLIENT_H_ */
