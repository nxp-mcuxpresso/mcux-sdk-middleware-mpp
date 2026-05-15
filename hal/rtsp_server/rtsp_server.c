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

#include "rtsp_server.h"

#ifdef HAL_ENABLE_RTSP_SINK

#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/netifapi.h"

extern rtsp_context_t s_rtsp_ctx;

/* Standard RTP header for a RTP packet. */
typedef struct {
   uint8_t  vpxcc;
   uint8_t  mpt;
   uint16_t seq;
   uint32_t timestamp;
   uint32_t ssrc;
} PACK_STRUCT_STRUCT rtp_header_t;

/* JPEG frame subheader for a RTP packet. */
typedef struct {
   uint8_t  type_specific;
   uint8_t  fragment_offset1;
   uint8_t  fragment_offset2;
   uint8_t  fragment_offset3;
   uint8_t  type;
   uint8_t  q;
   uint8_t  width;
   uint8_t  height;
} PACK_STRUCT_STRUCT jpeg_header_t ;

void safe_close_socket(int *sock_fd)
{
    if (*sock_fd >= 0)
    {
        close(*sock_fd);
        *sock_fd = -1;
    }
}

static void rtsp_send(int s, const void *data, size_t size, int flags)
{
    HAL_LOGD("%s", (const char *)data);
    send(s, data, size, flags);
}

static const char* get_codec_string(rtp_payload_type_t payload_type)
{
    switch (payload_type)
    {
        case RTP_PAYLOAD_TYPE_JPEG:
            return "JPEG/90000";
        case RTP_PAYLOAD_TYPE_H264:
            return "H264/90000";
        default:
            return "JPEG/90000";
    }
}

static int generate_sdp(char *buffer, size_t max_len, const char *ip_addr,
                        int video_pt, const char *video_codec)
{
    return snprintf(buffer, max_len, STR_SDP, ip_addr, video_pt, video_pt, video_codec);
}

static int parse_client_port_from_transport(const char *header)
{
    const char *p = strstr(header, "client_port=");
    if (!p) return -1;
    return atoi(p + strlen("client_port="));
}

static int handle_options(rtsp_session_t *session, char *recv_buf, int size)
{
    uint16_t seq = session->lastSeq;
    memset(recv_buf, 0, size);
    snprintf(recv_buf, size, STR_OPTIONS_RESPONSE, seq);
    rtsp_send(session->sess_sock, recv_buf, strlen(recv_buf), 0);
    return RTSPSINK_SUCCESS;
}

static int handle_describe(rtsp_session_t *session, char *recv_buf, int size, rtspsink_t *dev)
{
    uint16_t seq = session->lastSeq;

    char *required = NULL;
    if ((required = strstr(recv_buf, "Require:")) != NULL)
    {
        required = required + strlen("Require:");
        snprintf(recv_buf, size, STR_UNSUPPORTED_RESPONSE, seq, required);
        rtsp_send(session->sess_sock, recv_buf, strlen(recv_buf), 0);
        return RTSPSINK_ERR_BAD_REQUEST;
    }

    memset(recv_buf, 0, size);

    uint16_t sdp_size = 512;
    char *sdp_buf = pvPortMalloc(sdp_size);
    if (sdp_buf == NULL)
    {
        HAL_LOGE("Failed to allocate SDP buffer\n");
        snprintf(recv_buf, size, STR_INTERNAL_ERROR_RESPONSE, seq);
        rtsp_send(session->sess_sock, recv_buf, strlen(recv_buf), 0);
        return RTSPSINK_ERR_ALLOC_FAILED;
    }
    memset(sdp_buf, 0, sdp_size);

    generate_sdp(sdp_buf, sdp_size, dev->config.ip_addr, dev->config.payload_type, get_codec_string(dev->config.payload_type));

    snprintf(recv_buf, size, STR_DESCRIBE_RESPONSE, seq, dev->config.ip_addr, dev->config.port, strlen(sdp_buf), sdp_buf);

    vPortFree(sdp_buf);
    rtsp_send(session->sess_sock, recv_buf, strlen(recv_buf), 0);
    return RTSPSINK_SUCCESS;
}

static int handle_setup(rtsp_session_t *session, char *recv_buf, int size)
{
    uint16_t seq = session->lastSeq;
    media_stream_t *stream = NULL;
    char temp_buf[RTSP_TEMP_BUFFER_SIZE];
    char response[RTSP_RESPONSE_BUFFER_SIZE];
    char transport_buf[RTSP_TRANSPORT_BUFFER_SIZE];

    if (session->state == RTSP_PLAYING)
    {
        snprintf(temp_buf, sizeof(temp_buf), STR_INVALID_STATE, seq);
        rtsp_send(session->sess_sock, temp_buf, strlen(temp_buf), 0);
        return RTSPSINK_ERR_INVALID_STATE;
    }

    if (strstr(recv_buf, "trackID=0"))
    {
        stream = &session->video;
        stream->media_type = MEDIA_TYPE_VIDEO;
    }
    else if (strstr(recv_buf, "SETUP rtsp://"))
    {
        /* Aggregate SETUP - no specific track ID, setup the default (video) stream */
        stream = &session->video;
        stream->media_type = MEDIA_TYPE_VIDEO;
    }
    else
    {
        snprintf(temp_buf, sizeof(temp_buf), STR_BAD_REQUEST, seq);
        rtsp_send(session->sess_sock, temp_buf, strlen(temp_buf), 0);
        return RTSPSINK_ERR_BAD_REQUEST;
    }

    if (stream->is_active)
    {
        snprintf(temp_buf, sizeof(temp_buf), STR_INVALID_STATE, seq);
        rtsp_send(session->sess_sock, temp_buf, strlen(temp_buf), 0);
        return RTSPSINK_ERR_STREAM_ACTIVE;
    }

    int port = parse_client_port_from_transport(recv_buf);
    if (port <= 0)
    {
        snprintf(temp_buf, sizeof(temp_buf), STR_BAD_REQUEST, seq);
        rtsp_send(session->sess_sock, temp_buf, strlen(temp_buf), 0);
        return RTSPSINK_ERR_MISSING_HEADER;
    }

    struct sockaddr_in addr;
    socklen_t peerlen = sizeof(addr);
    if (getpeername(session->sess_sock, (struct sockaddr *)&addr, &peerlen) < 0)
    {
        return RTSPSINK_ERR_SOCKET_OPT_FAILED;
    }
    
    addr.sin_port = htons(port);
    stream->client_addr = addr;

    stream->is_active = true;
    stream->rtp_sock = -1;

    if (session->state == RTSP_INIT)
    {
        session->state = RTSP_READY;
    }

    /* Build response using a separate buffer to avoid corrupting recv_buf */
    char *transport = strstr(recv_buf, "Transport:");
    if (transport)
    {
        char *transport_end = strstr(transport, "\r\n");
        if (transport_end)
        {
            int transport_len = transport_end - transport;
            if (transport_len < sizeof(transport_buf))
            {
                memcpy(transport_buf, transport, transport_len);
                transport_buf[transport_len] = '\0';
                snprintf(transport_buf + transport_len, sizeof(transport_buf) - transport_len, 
                          STR_RTSP_SERVER_PORT_VIDEO);
            }
        }
    }

    snprintf(response, sizeof(response), STR_OK_RESPONSE, seq);
    snprintf(response + strlen(response), sizeof(response) - strlen(response), "%s", transport_buf);
    snprintf(response + strlen(response), sizeof(response) - strlen(response), "%s", session->session_id);
    snprintf(response + strlen(response), sizeof(response) - strlen(response), "\r\n");

    rtsp_send(session->sess_sock, response, strlen(response), 0);

    return RTSPSINK_SUCCESS;
}

static int handle_play(rtsp_session_t *session, char *recv_buf, int size)
{
    uint16_t seq = session->lastSeq;
    BaseType_t ret;

    if (session->state == RTSP_INIT)
    {
        memset(recv_buf, 0, size);
        snprintf(recv_buf, size, STR_INVALID_STATE, seq);
        rtsp_send(session->sess_sock, recv_buf, strlen(recv_buf), 0);
        return RTSPSINK_ERR_INVALID_STATE;
    }

    if (session->state == RTSP_PLAYING)
    {
        goto ok;
    }

    if (NULL == strstr(recv_buf, session->session_id))
    {
        memset(recv_buf, 0, size);
        snprintf(recv_buf, size, STR_BAD_REQUEST, seq);
        rtsp_send(session->sess_sock, recv_buf, strlen(recv_buf), 0);
        return RTSPSINK_ERR_MISSING_HEADER;
    }

    session->state = RTSP_PLAYING;

    if (session->video.is_active)
    {
        ret = xTaskCreate(rtp_task, "rtp_task", RTSP_TASKS_STACK_SIZE, session, RTSP_TASK_PRIORITY, NULL);
        if (pdPASS != ret)
        {
            HAL_LOGE("Failed to create rtp_task task");
            return RTSPSINK_ERR_TASK_CREATE_FAILED;
        }
    }

ok:
    memset(recv_buf, 0, size);
    snprintf(recv_buf, size, STR_OK_RESPONSE, seq);
    snprintf(recv_buf + strlen(recv_buf), size - strlen(recv_buf), session->session_id);
    snprintf(recv_buf + strlen(recv_buf), size - strlen(recv_buf), "\r\n");
    rtsp_send(session->sess_sock, recv_buf, strlen(recv_buf), 0);
    return RTSPSINK_SUCCESS;
}

static int handle_teardown(rtsp_session_t *session, char *recv_buf, int size)
{
    uint16_t seq = session->lastSeq;

    session->video.is_active = false;
    session->state = RTSP_UNINIT;
    session->video.sps_sent = false;
    session->video.pps_sent = false;

    memset(recv_buf, 0, size);
    snprintf(recv_buf, size, STR_OK_RESPONSE, seq);
    snprintf(recv_buf + strlen(recv_buf), size - strlen(recv_buf), session->session_id);
    snprintf(recv_buf + strlen(recv_buf), size - strlen(recv_buf), "\r\n");
    rtsp_send(session->sess_sock, recv_buf, strlen(recv_buf), 0);
    return RTSPSINK_SUCCESS;
}

void rtp_task_pdu(const void* buf, uint32_t len, void* params)
{
    if (buf == NULL || len == 0 || params == NULL)
    {
        HAL_LOGE("Invalid parameters\r\n");
        return;
    }

    media_stream_t *stream = (media_stream_t*) params;
    if (stream->rx_pdu == NULL || stream->sem == NULL)
    {
        /* RTP task did not create them yet, skip the PDU. */
        return;
    }

    stream_pdu_t* pdu = stream->rx_pdu;

    /* if current frame has not been sent out completely, drop this one.*/
    if (stream->send_pending == 1)
    {
        HAL_LOGD("DROP FRAME (send_pending=%d, len=%u)\r\n", stream->send_pending, len);
        return;
    }

    if (stream->media_type == MEDIA_TYPE_VIDEO)
    {
        /* Use tick count for timestamps. */
        stream->timestamp = xTaskGetTickCount() * stream->ts_unit;
        pdu->data = (const uint8_t *)buf;
    }
    else
    {
        HAL_LOGE("Unknown media type\r\n");
        return;
    }

    pdu->len = len;

    /* Notify RTP task, pdu is ready. */
    stream->send_pending = 1;
    xSemaphoreGive(stream->sem);
}

// Length of H.264 NALU start code (3 or 4), or 0 if not found
static int find_nalu_start_code(const uint8_t *data, uint32_t len, uint32_t *offset)
{
    if (len < 3)
        return 0;

    /* Check for 4-byte start code: 0x00000001 */
    if (len >= 4 && data[0] == 0x00 && data[1] == 0x00 && 
        data[2] == 0x00 && data[3] == 0x01)
    {
        *offset = 4;
        return 4;
    }

    /* Check for 3-byte start code: 0x000001 */
    if (data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01)
    {
        *offset = 3;
        return 3;
    }

    return 0;
}

// Send H.264 NALU unit as RTP packet(s) according to RFC 6184
static void send_rtp_h264_nalu(media_stream_t *stream, rtp_header_t *rtp_hdr, 
                              const uint8_t *nal_data, uint32_t nal_len, bool is_last_nal)
{
    const uint32_t max_payload = RTP_H264_FRAGMENT_SIZE;
    
    if (nal_len == 0 || stream == NULL || rtp_hdr == NULL)
    {
        HAL_LOGE("Invalid parameters\r\n");
        return;
    }

    uint8_t nal_header = nal_data[0];
    uint8_t nal_type = nal_header & H264_NAL_TYPE_MASK;

    /* Drop VCL NALU if SPS/PPS not yet sent */
    bool is_parameter_set_nalu = (nal_type == H264_NAL_TYPE_SPS || nal_type == H264_NAL_TYPE_PPS);
    bool stream_initialized = (stream->sps_sent && stream->pps_sent);

    if (!is_parameter_set_nalu && !stream_initialized)
    {
        HAL_LOGD("Waiting for SPS/PPS before sending other NALUs\r\n");
        return;
    }

    /* Cache payload type and basic configuration to avoid mutating original rtp_hdr directly */
    uint8_t base_mpt = rtp_hdr->mpt & 0x7F; /* Strip marker bit if already present */
    uint32_t current_timestamp = htonl(stream->timestamp);

    /* Single NAL unit packet - fits in one RTP packet */
    if (nal_len <= max_payload)
    {
        /* Reconstruct local temporary fields to ensure safe writing */
        rtp_hdr->timestamp = current_timestamp;
        rtp_hdr->seq = htons(stream->seq);
        
        if (is_last_nal)
            rtp_hdr->mpt = base_mpt | 0x80;  /* Set marker bit */
        else
            rtp_hdr->mpt = base_mpt & ~0x80; /* Clear marker bit */

        memcpy(stream->rtp_buffer, rtp_hdr, RTP_HEADER_SIZE);
        memcpy(stream->rtp_buffer + RTP_HEADER_SIZE, nal_data, nal_len);

        sendto(stream->rtp_sock, stream->rtp_buffer, RTP_HEADER_SIZE + nal_len,
               0, (struct sockaddr *)&stream->client_addr, sizeof(stream->client_addr));

        stream->seq++;
    }
    /* Fragmentation Unit A (FU-A) - NAL too large, needs fragmentation */
    else
    {
        uint8_t fu_indicator = (nal_header & 0xE0) | H264_FU_A_TYPE;
        uint32_t offset = H264_NAL_HEADER_SIZE; /* Skip original 1-byte NAL header */
        bool is_first_fragment = true;
        
        /* Calculate actual space available for data inside an FU-A packet */
        const uint32_t max_fu_payload_space = max_payload - H264_FU_A_HEADER_SIZE;

        while (offset < nal_len)
        {
            uint32_t fragment_size = nal_len - offset;
            bool is_last_fragment = (fragment_size <= max_fu_payload_space);
            
            if (!is_last_fragment)
                fragment_size = max_fu_payload_space;

            /* Explicitly calculate FU-A header flags on every loop */
            uint8_t fu_header = nal_type;
            if (is_first_fragment)
                fu_header |= H264_FU_START_BIT;
            else if (is_last_fragment)
                fu_header |= H264_FU_END_BIT;

            /* Apply sequential updates locally safely */
            rtp_hdr->timestamp = current_timestamp;
            rtp_hdr->seq = htons(stream->seq);
            
            if (is_last_fragment && is_last_nal)
                rtp_hdr->mpt = base_mpt | 0x80;  /* Set marker bit */
            else
                rtp_hdr->mpt = base_mpt & ~0x80; /* Clear marker bit */

            /* Build RTP packet safely */
            memcpy(stream->rtp_buffer, rtp_hdr, RTP_HEADER_SIZE);
            stream->rtp_buffer[RTP_HEADER_SIZE] = fu_indicator;
            stream->rtp_buffer[RTP_HEADER_SIZE + 1] = fu_header;
            
            memcpy(stream->rtp_buffer + RTP_HEADER_SIZE + H264_FU_A_HEADER_SIZE,
                   nal_data + offset, fragment_size);

            sendto(stream->rtp_sock, stream->rtp_buffer,
                   RTP_HEADER_SIZE + H264_FU_A_HEADER_SIZE + fragment_size,
                   0, (struct sockaddr *)&stream->client_addr, sizeof(stream->client_addr));

            stream->seq++;
            offset += fragment_size;
            is_first_fragment = false;
        }
    }

    if (nal_type == H264_NAL_TYPE_SPS)
    {
        stream->sps_sent = true;
    }
    else if (nal_type == H264_NAL_TYPE_PPS)
    {
        stream->pps_sent = true;
    }
}

// Send H.264 NALU as multiple RTP packets with fragmentation and H.264 header
static void send_rtp_h264_packet(media_stream_t *stream, rtp_header_t *rtp_hdr)
{
    uint32_t pdu_len = stream->rx_pdu->len;
    const uint8_t *pdu_data = stream->rx_pdu->data;
    uint32_t pos = 0;

    /* Parse and send each NALU */
    while (pos < pdu_len)
    {
        uint32_t start_code_offset = 0;
        int start_code_len = find_nalu_start_code(pdu_data + pos, pdu_len - pos, &start_code_offset);

        if (start_code_len == 0)
        {
            /* No start code found, might be raw NAL data */
            if (pos == 0)
            {
                /* Treat entire buffer as single NAL unit */
                send_rtp_h264_nalu(stream, rtp_hdr, pdu_data, pdu_len, true);
            }
            break;
        }

        pos += start_code_offset;

        /* Find next start code to determine NALU unit length */
        uint32_t nalu_start = pos;
        uint32_t search_pos = pos + 1;
        uint32_t nalu_end = pdu_len;

        while (search_pos < pdu_len)
        {
            uint32_t next_offset = 0;
            if (find_nalu_start_code(pdu_data + search_pos, pdu_len - search_pos, &next_offset))
            {
                nalu_end = search_pos;
                break;
            }
            search_pos++;
        }

        uint32_t nalu_len = nalu_end - nalu_start;
        bool is_last_nalu = (nalu_end >= pdu_len);

        if (nalu_len > 0)
        {
            send_rtp_h264_nalu(stream, rtp_hdr, pdu_data + nalu_start, nalu_len, is_last_nalu);
        }

        pos = nalu_end;
    }
}

// Send PDU frame as multiple RTP packets with fragmentation and JPEG header (RFC 2435)
static void send_rtp_jpeg_packet(media_stream_t *stream, rtp_header_t *rtp_hdr, jpeg_header_t *jpeg_hdr)
{
    uint32_t pdu_len = stream->rx_pdu->len;
    const uint8_t *pdu_data = stream->rx_pdu->data;
    uint32_t offset = 0;

    rtp_hdr->timestamp = htonl(stream->timestamp);

    while (offset < pdu_len)
    {
        uint32_t payload_size = pdu_len - offset;
        
        if (payload_size <= RTP_JPEG_FRAGMENT_SIZE)
        {
            rtp_hdr->mpt |= 0x80;  /* Set marker bit for last fragment */
        }
        else
        {
            rtp_hdr->mpt &= ~0x80;
            payload_size = RTP_JPEG_FRAGMENT_SIZE;
        }

        rtp_hdr->seq = htons(stream->seq);
        memcpy(stream->rtp_buffer, rtp_hdr, sizeof(rtp_header_t));

        jpeg_hdr->fragment_offset1 = (offset >> 16) & 0xFF;
        jpeg_hdr->fragment_offset2 = (offset >> 8) & 0xFF;
        jpeg_hdr->fragment_offset3 = offset & 0xFF;
        memcpy(stream->rtp_buffer + sizeof(rtp_header_t), jpeg_hdr, sizeof(jpeg_header_t));

        memcpy(stream->rtp_buffer + sizeof(rtp_header_t) + sizeof(jpeg_header_t),
               pdu_data + offset, payload_size);

        sendto(stream->rtp_sock, stream->rtp_buffer,
               sizeof(rtp_header_t) + sizeof(jpeg_header_t) + payload_size,
               0, (struct sockaddr *)&stream->client_addr, sizeof(stream->client_addr));

        stream->seq++;
        offset += payload_size;
    }
}

void rtp_task(void *arg)
{
    rtsp_session_t *session = (rtsp_session_t *)arg;
    media_stream_t *stream = &session->video;
    rtp_payload_type_t payload_type = session->dev->config.payload_type;

    /* Calculate timestamp unit based on codec clock rate */
    stream->ts_unit = atoi(strstr(get_codec_string(payload_type), "/") + 1) / configTICK_RATE_HZ;

    /* Allocate RTP buffer based on payload type */
    size_t buffer_size;
    if (payload_type == RTP_PAYLOAD_TYPE_JPEG)
    {
        buffer_size = RTP_JPEG_VIDEO_SEND_BUF_SIZE;
    }
    else if (payload_type == RTP_PAYLOAD_TYPE_H264)
    {
        buffer_size = RTP_H264_VIDEO_SEND_BUF_SIZE;
    }
    else
    {
        HAL_LOGE("Unsupported payload type: %d\n", payload_type);
        goto cleanup;
    }

    stream->rtp_buffer = pvPortMalloc(buffer_size);
    if (!stream->rtp_buffer)
    {
        HAL_LOGE("Failed to allocate RTP buffer (size=%u)\n", buffer_size);
        goto cleanup;
    }

    stream->sem = xSemaphoreCreateBinary();
    if (!stream->sem)
    {
        HAL_LOGE("Failed to create semaphore\n");
        goto cleanup;
    }

    stream->rx_pdu = pvPortMalloc(sizeof(stream_pdu_t));
    if (!stream->rx_pdu)
    {
        HAL_LOGE("Failed to allocate PDU\n");
        goto cleanup;
    }

    stream->rtp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (stream->rtp_sock < 0)
    {
        HAL_LOGE("Failed to create RTP socket\n");
        goto cleanup;
    }

    uint16_t port = atoi(strstr(STR_RTSP_SERVER_PORT_VIDEO, "=") + 1);
    struct sockaddr_in addr =
    {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(stream->rtp_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        HAL_LOGE("Socket bind failed\n");
        goto cleanup;
    }

    stream->send_pending = 0;

    stream->seq = rand() & 0xFFFF; // Random initial sequence number incremented for each packet
    stream->timestamp = 0;         // Will be set for each frame

    uint32_t ssrc = rand();        // Random SSRC kept for all packets in session

    /* Initialize RTP header */
    rtp_header_t rtp_hdr =
    {
        .vpxcc = 0x80,
        .mpt = payload_type,
        .seq = 0,            // Will be set per-packet
        .timestamp = 0,      // Will be set per-packet
        .ssrc = htonl(ssrc),
    };

    HAL_LOGI("RTP task started for %s stream\n", 
             payload_type == RTP_PAYLOAD_TYPE_JPEG ? "JPEG" : "H264");

    /* Main RTP streaming loop */
    while (stream->is_active)
    {
        xSemaphoreTake(stream->sem, portMAX_DELAY);

        /* Check if stream is still active */
        if (!stream->is_active)
        {
            break;
        }
        /* Send packet based on payload type */
        if (payload_type == RTP_PAYLOAD_TYPE_JPEG)
        {
            jpeg_header_t jpeg_hdr = {0};
            jpeg_hdr.type_specific = 0;
            jpeg_hdr.fragment_offset1 = 0;
            jpeg_hdr.fragment_offset2 = 0;
            jpeg_hdr.fragment_offset3 = 0;
            jpeg_hdr.type = session->dev->config.frame_type;
            jpeg_hdr.q = JPEG_QTABLE;
            jpeg_hdr.width = session->dev->config.frame_width >> 3;
            jpeg_hdr.height = session->dev->config.frame_height >> 3;

            send_rtp_jpeg_packet(stream, &rtp_hdr, &jpeg_hdr);
        }
        else if (payload_type == RTP_PAYLOAD_TYPE_H264)
        {
            send_rtp_h264_packet(stream, &rtp_hdr);
        }
        else
        {
            HAL_LOGE("Unsupported payload type: %d\n", payload_type);
        }

        stream->send_pending = 0;
    }

cleanup:
    HAL_LOGI("RTP task cleanup\n");

    if (stream->sem)
    {
        vSemaphoreDelete(stream->sem);
        stream->sem = NULL;
    }
    if (stream->rx_pdu)
    {
        vPortFree(stream->rx_pdu);
        stream->rx_pdu = NULL;
    }
    if (stream->rtp_buffer)
    {
        vPortFree(stream->rtp_buffer);
        stream->rtp_buffer = NULL;
    }
    safe_close_socket(&stream->rtp_sock);
    vTaskDelete(NULL);
}

void rtsp_session_task(void *arg)
{
    int rx_len;
    rtsp_session_t *session = (rtsp_session_t *)arg;
    char rx_buf[RTSP_SESSION_RECV_BUF_SIZE];
    struct timeval timeout;

    memset(rx_buf, 0, sizeof(rx_buf));

    while ((rx_len = recv(session->sess_sock, rx_buf, RTSP_SESSION_RECV_BUF_SIZE - 1, 0)) > 0)
    {
        rx_buf[rx_len] = 0;
        session->lastSeq = atoi(strstr(rx_buf, "CSeq:") + strlen("CSeq:"));

        if (strstr(rx_buf, "OPTIONS"))
        {
            handle_options(session, rx_buf, RTSP_SESSION_RECV_BUF_SIZE);
        }
        else if (strstr(rx_buf, "DESCRIBE"))
        {
            rtspsink_t *dev = (rtspsink_t *)pvTaskGetThreadLocalStoragePointer(NULL, 0);
            handle_describe(session, rx_buf, RTSP_SESSION_RECV_BUF_SIZE, dev);
        }
        else if (strstr(rx_buf, "SETUP"))
        {
            handle_setup(session, rx_buf, RTSP_SESSION_RECV_BUF_SIZE);
        }
        else if (strstr(rx_buf, "PLAY"))
        {
            handle_play(session, rx_buf, RTSP_SESSION_RECV_BUF_SIZE);
        }
        else if (strstr(rx_buf, "TEARDOWN"))
        {
            handle_teardown(session, rx_buf, RTSP_SESSION_RECV_BUF_SIZE);
            break;
        }
        else
        {
            memset(rx_buf, 0, RTSP_SESSION_RECV_BUF_SIZE);
            snprintf(rx_buf, RTSP_SESSION_RECV_BUF_SIZE, STR_BAD_REQUEST, session->lastSeq);
            rtsp_send(session->sess_sock, rx_buf, strlen(rx_buf), 0);
        }

        memset(rx_buf, 0, sizeof(rx_buf));
    }

    if (rx_len == 0)
    {
        HAL_LOGI("Client closed connection\n");
    }

    /* Clean up session resources */
    session->video.is_active = false;
    session->video.sps_sent = false;
    session->video.pps_sent = false;

    if (session->video.sem != NULL)
    {
        xSemaphoreGive(session->video.sem);
    }

    /* Give RTP task time to clean up */
    vTaskDelay(pdMS_TO_TICKS(100));

    session->state = RTSP_UNINIT;

    /* Close and reset the session socket */
    safe_close_socket(&session->sess_sock);

    vTaskDelete(NULL);
}

void rtsp_server_task(void *arg)
{
    rtspsink_t *dev = (rtspsink_t *)arg;
    BaseType_t ret;

    HAL_LOGI("RTSP Server listening on port %d\r\n", dev->config.port);

    if (listen(s_rtsp_ctx.listen_fd, RTSP_MAX_SESSION_NUM) < 0)
    {
        HAL_LOGE("ERROR: listen() failed: errno=%d\r\n", errno);
        goto end;
    }

    while (s_rtsp_ctx.server_running)
    {
        struct sockaddr_in client;
        socklen_t len = sizeof(client);

        HAL_LOGD("Waiting for client connection...\r\n");

        int client_fd = accept(s_rtsp_ctx.listen_fd, (struct sockaddr *)&client, &len);

        if (client_fd < 0)
        {
            HAL_LOGE("ERROR: accept() failed: errno=%d\r\n", errno);
            continue;
        }

        HAL_LOGI("Client connected from %s:%d (fd=%d)\r\n",
                 inet_ntoa(client.sin_addr),
                 ntohs(client.sin_port),
                 client_fd);

        /* Single client */
        if (s_rtsp_ctx.sessions[0].sess_sock == -1 && s_rtsp_ctx.sessions[0].state == RTSP_UNINIT)
        {
            s_rtsp_ctx.sessions[0].sess_sock = client_fd;
            s_rtsp_ctx.sessions[0].state = RTSP_INIT;
            s_rtsp_ctx.sessions[0].dev = dev;
            
            // Generate unique session ID
            uint32_t session_rand = rand();
            snprintf(s_rtsp_ctx.sessions[0].session_id, 
                      sizeof(s_rtsp_ctx.sessions[0].session_id),
                      "Session: %08X\r\n", (unsigned int)session_rand);

            ret = xTaskCreate(rtsp_session_task, "rtsp_session_task",
                        RTSP_TASKS_STACK_SIZE,
                        (void *)&s_rtsp_ctx.sessions[0],
                        RTSP_TASK_PRIORITY,
                        &s_rtsp_ctx.sessions[0].handler);
            if (pdPASS != ret)
            {
                HAL_LOGE("Failed to create rtsp_session_task task\n");
                close(client_fd);
                s_rtsp_ctx.sessions[0].sess_sock = -1;
                s_rtsp_ctx.sessions[0].state = RTSP_UNINIT;
            }
            else
            {
                /* Store dev pointer in task local storage */
                vTaskSetThreadLocalStoragePointer(s_rtsp_ctx.sessions[0].handler, 0, dev);
                HAL_LOGI("RTSP session task created for session 0 (ID: %08X)\r\n", session_rand);
            }
        }
        else
        {
            HAL_LOGI("Session already active (sess_sock=%d, state=%d). Rejecting new connection.\r\n",
                     s_rtsp_ctx.sessions[0].sess_sock, s_rtsp_ctx.sessions[0].state);
            close(client_fd);
        }
    }

end:
    close(s_rtsp_ctx.listen_fd);
    s_rtsp_ctx.listen_fd = -1;
    s_rtsp_ctx.server_task_handle = NULL;
    vTaskDelete(NULL);
    return;
}

#endif /* HAL_ENABLE_RTSP_SINK */
