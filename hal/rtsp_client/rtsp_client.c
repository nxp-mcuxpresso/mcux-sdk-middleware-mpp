/*
 * Copyright 2026 NXP
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "rtsp_client.h"

#ifdef HAL_ENABLE_RTSP_SRC

#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/netifapi.h"

extern rtsp_client_context_t s_rtsp_client_ctx;

/* ---------------------------------------------------------------------------
 * RFC 2435 -> JPEG reconstruction
 * Rebuilds JPEG file headers stripped by the RTP sender (SOI, APP0, DQT,
 * SOF0, DHT, SOS) and appends EOI; operates in-place on buf[].
 * ---------------------------------------------------------------------------*/

/* Standard JPEG Annex K Huffman tables (RFC 2435 mandates these at the receiver) */
static const uint8_t s_dc_luma_bits[16]   = { 0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0 };
static const uint8_t s_dc_luma_val[12]    = { 0,1,2,3,4,5,6,7,8,9,10,11 };
static const uint8_t s_dc_chroma_bits[16] = { 0,3,1,1,1,1,1,1,1,1,1,0,0,0,0,0 };
static const uint8_t s_dc_chroma_val[12]  = { 0,1,2,3,4,5,6,7,8,9,10,11 };
static const uint8_t s_ac_luma_bits[16]   = { 0,2,1,3,3,2,4,3,5,5,4,4,0,0,1,125 };
static const uint8_t s_ac_luma_val[162]   = {
    0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,0x13,0x51,0x61,0x07,
    0x22,0x71,0x14,0x32,0x81,0x91,0xa1,0x08,0x23,0x42,0xb1,0xc1,0x15,0x52,0xd1,0xf0,
    0x24,0x33,0x62,0x72,0x82,0x09,0x0a,0x16,0x17,0x18,0x19,0x1a,0x25,0x26,0x27,0x28,
    0x29,0x2a,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x43,0x44,0x45,0x46,0x47,0x48,0x49,
    0x4a,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x63,0x64,0x65,0x66,0x67,0x68,0x69,
    0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x83,0x84,0x85,0x86,0x87,0x88,0x89,
    0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,
    0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xc2,0xc3,0xc4,0xc5,
    0xc6,0xc7,0xc8,0xc9,0xca,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xe1,0xe2,
    0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,
    0xf9,0xfa };
static const uint8_t s_ac_chroma_bits[16] = { 0,2,1,2,4,4,3,4,7,5,4,4,0,1,2,119 };
static const uint8_t s_ac_chroma_val[162] = {
    0x00,0x01,0x02,0x03,0x11,0x04,0x05,0x21,0x31,0x06,0x12,0x41,0x51,0x07,0x61,0x71,
    0x13,0x22,0x32,0x81,0x08,0x14,0x42,0x91,0xa1,0xb1,0xc1,0x09,0x23,0x33,0x52,0xf0,
    0x15,0x62,0x72,0xd1,0x0a,0x16,0x24,0x34,0xe1,0x25,0xf1,0x17,0x18,0x19,0x1a,0x26,
    0x27,0x28,0x29,0x2a,0x35,0x36,0x37,0x38,0x39,0x3a,0x43,0x44,0x45,0x46,0x47,0x48,
    0x49,0x4a,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x63,0x64,0x65,0x66,0x67,0x68,
    0x69,0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x82,0x83,0x84,0x85,0x86,0x87,
    0x88,0x89,0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5,
    0xa6,0xa7,0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xc2,0xc3,
    0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,
    0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,
    0xf9,0xfa };

static inline void hdr_u8(uint8_t *h, uint32_t *p, uint8_t v)
{
    h[(*p)++] = v;
}

static inline void hdr_u16be(uint8_t *h, uint32_t *p, uint16_t v)
{
    h[(*p)++] = v >> 8;
    h[(*p)++] = v & 0xff;
}

static void write_dht(uint8_t *h, uint32_t *p, uint8_t tc_th,
                      const uint8_t bits[16], const uint8_t *vals, uint16_t n)
{
    hdr_u8(h, p, 0xff); hdr_u8(h, p, 0xc4);
    hdr_u16be(h, p, (uint16_t)(19u + n));
    hdr_u8(h, p, tc_th);

    for (int i = 0; i < 16; i++)
        hdr_u8(h, p, bits[i]);
    for (int i = 0; i < n; i++)
        hdr_u8(h, p, vals[i]);
}

/* JPEG Annex K default Q-tables, used when RFC 2435 q < 128 (computed tables). */
static const uint8_t s_luma_base[64] = {
    16, 11, 10, 16, 24, 40, 51, 61,
    12, 12, 14, 19, 26, 58, 60, 55,
    14, 13, 16, 24, 40, 57, 69, 56,
    14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68,109,103, 77,
    24, 35, 55, 64, 81,104,113, 92,
    49, 64, 78, 87,103,121,120,101,
    72, 92, 95, 98,112,100,103, 99
};
static const uint8_t s_chroma_base[64] = {
    17, 18, 24, 47, 99, 99, 99, 99,
    18, 21, 26, 66, 99, 99, 99, 99,
    24, 26, 56, 99, 99, 99, 99, 99,
    47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99
};

/* Fills dst[64] with Q-table computed from RFC 2435 q factor and base table. */
static void compute_qtable(uint8_t *dst, const uint8_t *base, uint8_t q)
{
    uint32_t factor = (q < 50u) ? (5000u / q) : (200u - 2u * (uint32_t)q);
    for (int i = 0; i < 64; i++)
    {
        uint32_t v = ((uint32_t)base[i] * factor + 50u) / 100u;
        if (v < 1u)
            v = 1u;
        if (v > 255u)
            v = 255u;
        dst[i] = (uint8_t)v;
    }
}

/* Builds a complete JPEG header (SOI, APP0, DQT x2, SOF0, DHT x4, SOS) into h[].
 * Returns the number of bytes written. */
static uint32_t build_jpeg_header(uint8_t *h, uint16_t w, uint16_t h_px,
                                  const uint8_t *qtbl_luma, const uint8_t *qtbl_chroma)
{
    uint32_t p = 0u;

    hdr_u8(h, &p, 0xff); hdr_u8(h, &p, 0xd8);             /* SOI */

    hdr_u8(h, &p, 0xff); hdr_u8(h, &p, 0xe0);             /* APP0/JFIF */
    hdr_u16be(h, &p, 16u);
    h[p++] = 'J'; h[p++] = 'F'; h[p++] = 'I'; h[p++] = 'F'; h[p++] = 0;
    hdr_u8(h, &p, 1u); hdr_u8(h, &p, 1u);                 /* version 1.1 */
    hdr_u8(h, &p, 0u);                                    /* pixel aspect ratio units: none */
    hdr_u16be(h, &p, 1u); hdr_u16be(h, &p, 1u);           /* pixel aspect ratio 1:1 */
    hdr_u8(h, &p, 0u); hdr_u8(h, &p, 0u);                 /* no thumbnail */

    hdr_u8(h, &p, 0xff); hdr_u8(h, &p, 0xdb);             /* DQT luma */
    hdr_u16be(h, &p, 67u);
    hdr_u8(h, &p, 0u);                                    /* table ID 0 */
    for (int i = 0; i < 64; i++)
        hdr_u8(h, &p, qtbl_luma[i]);

    hdr_u8(h, &p, 0xff); hdr_u8(h, &p, 0xdb);             /* DQT chroma */
    hdr_u16be(h, &p, 67u);
    hdr_u8(h, &p, 1u);                                    /* table ID 1 */
    for (int i = 0; i < 64; i++)
        hdr_u8(h, &p, qtbl_chroma[i]);

    hdr_u8(h, &p, 0xff); hdr_u8(h, &p, 0xc0);             /* SOF0 baseline DCT */
    hdr_u16be(h, &p, 17u); hdr_u8(h, &p, 8u);             /* length, 8-bit precision */
    hdr_u16be(h, &p, h_px); hdr_u16be(h, &p, w);          /* height, width */
    hdr_u8(h, &p, 3u);                                    /* 3 components: Y Cb Cr */
    hdr_u8(h, &p, 1u); hdr_u8(h, &p, 0x22u); hdr_u8(h, &p, 0u);  /* Y:  4:2:0, QT 0 */
    hdr_u8(h, &p, 2u); hdr_u8(h, &p, 0x11u); hdr_u8(h, &p, 1u);  /* Cb: 1:1,   QT 1 */
    hdr_u8(h, &p, 3u); hdr_u8(h, &p, 0x11u); hdr_u8(h, &p, 1u);  /* Cr: 1:1,   QT 1 */

    write_dht(h, &p, 0x00u, s_dc_luma_bits,   s_dc_luma_val,   sizeof(s_dc_luma_val));
    write_dht(h, &p, 0x01u, s_dc_chroma_bits, s_dc_chroma_val, sizeof(s_dc_chroma_val));
    write_dht(h, &p, 0x10u, s_ac_luma_bits,   s_ac_luma_val,   sizeof(s_ac_luma_val));
    write_dht(h, &p, 0x11u, s_ac_chroma_bits, s_ac_chroma_val, sizeof(s_ac_chroma_val));

    hdr_u8(h, &p, 0xff); hdr_u8(h, &p, 0xda);             /* SOS */
    hdr_u16be(h, &p, 12u); hdr_u8(h, &p, 3u);             /* length, 3 components */
    hdr_u8(h, &p, 1u); hdr_u8(h, &p, 0x00u);              /* Y:  DC HT 0, AC HT 0 */
    hdr_u8(h, &p, 2u); hdr_u8(h, &p, 0x11u);              /* Cb: DC HT 1, AC HT 1 */
    hdr_u8(h, &p, 3u); hdr_u8(h, &p, 0x11u);              /* Cr: DC HT 1, AC HT 1 */
    hdr_u8(h, &p, 0u); hdr_u8(h, &p, 0x3fu); hdr_u8(h, &p, 0u); /* Ss=0, Se=63, Ah/Al=0 */

    return p;
}

/* Converts RFC 2435 payload in buf[] to a valid JPEG to be decoded.
 * Returns final JPEG size, or 0 on error. */
static uint32_t rfc2435_to_jpeg(uint8_t *buf, uint32_t rfc_size,
                                uint16_t w, uint16_t h, uint32_t capacity, uint8_t q)
{
    /* NOTE: s_luma_qtbl, s_chroma_qtbl and s_hdr are function-local statics
     * (non-reentrant scratch buffers). This function must only be called from
     * a single task context. */
    static uint8_t s_luma_qtbl[64];
    static uint8_t s_chroma_qtbl[64];
    const uint8_t *qtbl_luma;
    const uint8_t *qtbl_chroma;
    uint32_t scan_off; /* Offset of Q-tables data to be scanned */
    uint32_t scan_sz;
    uint32_t nqt; /* Number of Q-tables */

    static uint8_t s_hdr[JPEG_HDR_MAX]; /* Reconstructed JPEG header buffer */
    uint32_t hp = 0u; /* Header pointer/offset */

    if (q == 0u)
    {
        HAL_LOGE("rfc2435_to_jpeg: q=0 invalid\n");
        return 0u;
    }

    if (q < 128u)
    {
        /* Standard Q-tables computed from q factor */
        compute_qtable(s_luma_qtbl, s_luma_base, q);
        compute_qtable(s_chroma_qtbl, s_chroma_base, q);
        qtbl_luma = s_luma_qtbl;
        qtbl_chroma = s_chroma_qtbl;
        nqt = 2u;
        scan_off = 0u;
        scan_sz = rfc_size;
    }
    else
    {
        /* Custom Q-tables inline embedded in buf[] */
        if (rfc_size < 4u)
        {
            HAL_LOGE("rfc2435_to_jpeg: too small\n");
            return 0u;
        }

        /* Length of Q-tables */
        uint16_t qlen = (uint16_t)((buf[2] << 8) | buf[3]);

        scan_off = 4u + qlen;
        nqt = qlen / 64u;

        if (scan_off >= rfc_size || nqt == 0u || nqt > 2u)
        {
            HAL_LOGE("rfc2435_to_jpeg: bad qlen=%u\n", qlen);
            return 0u;
        }

        qtbl_luma = buf + 4u;
        qtbl_chroma = (nqt >= 2u) ? buf + 4u + 64u : buf + 4u;
        scan_sz = rfc_size - scan_off;
    }

    hp = build_jpeg_header(s_hdr, w, h, qtbl_luma, qtbl_chroma);

    (void)nqt;

    if (hp > JPEG_HDR_MAX)
    {
        HAL_LOGE("rfc2435_to_jpeg: hdr overflow\n");
        return 0u;
    }

    uint32_t total = hp + scan_sz + 2u;
    if (total > capacity)
    {
        HAL_LOGE("rfc2435_to_jpeg: %u > capacity\n", total);
        return 0u;
    }

    memmove(buf + hp, buf + scan_off, scan_sz);
    memcpy(buf, s_hdr, hp);
    buf[hp + scan_sz] = 0xff;
    buf[hp + scan_sz + 1] = 0xd9;

    return total;
}

static int rtsp_parse_url(const char *url, char *ip, uint16_t *port, char *path)
{
    if (strncmp(url, "rtsp://", 7) != 0)
    {
        HAL_LOGE("Invalid RTSP URL\n");
        return RTSPSRC_ERR_URL_PARSE_FAILED;
    }

    const char *start = url + 7;
    const char *colon = strchr(start, ':');
    const char *slash = strchr(start, '/');

    if (!slash)
    {
        slash = start + strlen(start);
        path[0] = '/';
        path[1] = '\0';
    }
    else
    {
        strncpy(path, slash, RTSP_URL_PATH_MAX_LEN - 1);
        path[RTSP_URL_PATH_MAX_LEN - 1] = '\0';
    }

    if (!colon || colon >= slash)
    {
        HAL_LOGE("Port not specified in RTSP URL\n");
        return RTSPSRC_ERR_URL_PARSE_FAILED;
    }

    int ip_len = (int)(colon - start);
    if (ip_len >= IPV4_STR_LEN)
    {
        HAL_LOGE("IP address too long\n");
        return RTSPSRC_ERR_URL_PARSE_FAILED;
    }

    strncpy(ip, start, ip_len);
    ip[ip_len] = '\0';
    *port = (uint16_t)atoi(colon + 1);
    HAL_LOGI("Parsed URL: IP=%s, Port=%d, Path=%s\n", ip, *port, path);
    return RTSPSRC_SUCCESS;
}

static void build_rtsp_url(rtsp_client_session_t *s, char *buf, size_t len)
{
    snprintf(buf, len, "rtsp://%s:%d%s", s->server_ip, s->server_port, s->url_path);
}

static int rtsp_send_request(rtsp_client_session_t *s, const char *req)
{
    HAL_LOGD("RTSP TX ->\n%s", req);
    int len = (int)strlen(req);
    if (send(s->sess_sock, req, len, 0) != len)
    {
        HAL_LOGE("RTSP send failed\n");
        return RTSPSRC_ERR_SEND_FAILED;
    }
    return RTSPSRC_SUCCESS;
}

static int rtsp_recv_response(rtsp_client_session_t *s, char *buf, int buf_size)
{
    int total = 0;

    memset(buf, 0, buf_size);

    while (total < buf_size - 1)
    {
        int n = recv(s->sess_sock, buf + total, buf_size - 1 - total, 0);
        if (n <= 0)
        {
            HAL_LOGI("RTSP recv failed (n=%d)\n", n);
            return RTSPSRC_ERR_RECV_FAILED;
        }
        total += n;
        buf[total] = '\0';
        if (strstr(buf, "\r\n\r\n"))
            break;
    }

    HAL_LOGD("RTSP RX <-\n%s", buf);
    return RTSPSRC_SUCCESS;
}

static bool rtsp_is_200_ok(const char *resp)
{
    return strstr(resp, "RTSP/1.0 200") != NULL;
}

static void rtsp_extract_session_id(const char *resp, char *dst, size_t dst_size)
{
    const char *p = strstr(resp, "Session:");

    if (!p)
    {
        dst[0] = '\0';
        return;
    }

    p += strlen("Session:");
    
    while (*p == ' ')
        p++;

    size_t i = 0;
    while (*p && *p != '\r' && *p != '\n' && *p != ';' && i < dst_size - 1)
        dst[i++] = *p++;
    dst[i] = '\0';
}

/* RTSP handshake */
static int rtsp_do_options(rtsp_client_session_t *s)
{
    char req[RTSP_RECV_BUF_SIZE], resp[RTSP_RECV_BUF_SIZE];
    char url[RTSP_URL_PATH_MAX_LEN + IPV4_STR_LEN + 16];

    build_rtsp_url(s, url, sizeof(url));
    snprintf(req, sizeof(req), STR_OPTIONS_REQ, url, ++s->cseq);

    if (rtsp_send_request(s, req) != RTSPSRC_SUCCESS)
        return RTSPSRC_ERR_SEND_FAILED;
    if (rtsp_recv_response(s, resp, sizeof(resp)) != RTSPSRC_SUCCESS)
        return RTSPSRC_ERR_RECV_FAILED;
    if (!rtsp_is_200_ok(resp))
    {
        HAL_LOGI("OPTIONS failed\n");
        return RTSPSRC_ERR_BAD_RESPONSE;
    }
    HAL_LOGI("OPTIONS OK\n");
    return RTSPSRC_SUCCESS;
}

static int rtsp_do_describe(rtsp_client_session_t *s)
{
    char req[RTSP_RECV_BUF_SIZE], resp[RTSP_RECV_BUF_SIZE * 2];
    char url[RTSP_URL_PATH_MAX_LEN + IPV4_STR_LEN + 16];

    build_rtsp_url(s, url, sizeof(url));
    snprintf(req, sizeof(req), STR_DESCRIBE_REQ, url, ++s->cseq);
    if (rtsp_send_request(s, req) != RTSPSRC_SUCCESS)
        return RTSPSRC_ERR_SEND_FAILED;

    memset(resp, 0, sizeof(resp));
    int total = 0;
    while (total < (int)sizeof(resp) - 1)
    {
        int n = recv(s->sess_sock, resp + total, sizeof(resp) - 1 - total, 0);
        if (n <= 0)
            return RTSPSRC_ERR_RECV_FAILED;
        total += n;
        resp[total] = '\0';
        if (strstr(resp, "\r\n\r\n"))
        {
            const char *cl = strstr(resp, "Content-Length:");
            if (!cl)
                break;
            int clen = atoi(cl + strlen("Content-Length:"));
            const char *body = strstr(resp, "\r\n\r\n");
            if (body && (total - (int)(body + 4 - resp)) >= clen)
                break;
        }
    }

    HAL_LOGD("DESCRIBE RX <-\n%s", resp);

    if (!rtsp_is_200_ok(resp))
    {
        HAL_LOGI("DESCRIBE failed\n");
        return RTSPSRC_ERR_BAD_RESPONSE;
    }
    HAL_LOGI("DESCRIBE OK\n");
    return RTSPSRC_SUCCESS;
}

static int rtsp_do_setup(rtsp_client_session_t *s)
{
    char req[RTSP_RECV_BUF_SIZE], resp[RTSP_RECV_BUF_SIZE];
    char url[RTSP_URL_PATH_MAX_LEN + IPV4_STR_LEN + 16];

    build_rtsp_url(s, url, sizeof(url));

    if (!strstr(url, "trackID"))
    {
        size_t ul = strlen(url);
        if (ul > 0 && url[ul - 1] != '/')
            strncat(url, "/", sizeof(url) - strlen(url) - 1);
        strncat(url, "trackID=0", sizeof(url) - strlen(url) - 1);
    }

    snprintf(req, sizeof(req), STR_SETUP_REQ, url, ++s->cseq,
             RTP_CLIENT_LOCAL_PORT, RTP_CLIENT_LOCAL_PORT + 1);
    if (rtsp_send_request(s, req) != RTSPSRC_SUCCESS)
        return RTSPSRC_ERR_SEND_FAILED;
    if (rtsp_recv_response(s, resp, sizeof(resp)) != RTSPSRC_SUCCESS)
        return RTSPSRC_ERR_RECV_FAILED;
    if (!rtsp_is_200_ok(resp))
    {
        HAL_LOGI("SETUP failed\n");
        return RTSPSRC_ERR_BAD_RESPONSE;
    }

    rtsp_extract_session_id(resp, s->session_id, sizeof(s->session_id));
    HAL_LOGI("SETUP OK - session ID: %s\n", s->session_id);

    return RTSPSRC_SUCCESS;
}

static int rtsp_do_play(rtsp_client_session_t *s)
{
    char req[RTSP_RECV_BUF_SIZE], resp[RTSP_RECV_BUF_SIZE];
    char url[RTSP_URL_PATH_MAX_LEN + IPV4_STR_LEN + 16];

    build_rtsp_url(s, url, sizeof(url));
    snprintf(req, sizeof(req), STR_PLAY_REQ, url, ++s->cseq, s->session_id);
    if (rtsp_send_request(s, req) != RTSPSRC_SUCCESS)
        return RTSPSRC_ERR_SEND_FAILED;
    if (rtsp_recv_response(s, resp, sizeof(resp)) != RTSPSRC_SUCCESS)
        return RTSPSRC_ERR_RECV_FAILED;
    if (!rtsp_is_200_ok(resp))
    {
        HAL_LOGI("PLAY failed\n");
        return RTSPSRC_ERR_BAD_RESPONSE;
    }

    HAL_LOGI("PLAY OK\n");

    return RTSPSRC_SUCCESS;
}

/* RTP receive task - reassembles RFC 2435 JPEG fragments */
void rtp_recv_task(void *arg)
{
    rtsp_client_session_t *session = (rtsp_client_session_t *)arg;
    uint8_t *pkt_buf = NULL;

    if (!session || !session->rtp_buf)
    {
        HAL_LOGE("rtp_recv_task: invalid args\n");
        goto cleanup;
    }

    pkt_buf = pvPortMalloc(RTP_RECV_BUF_SIZE);
    if (!pkt_buf)
    {
        HAL_LOGE("rtp_recv_task: alloc failed\n");
        goto cleanup;
    }

    HAL_LOGI("RTP receive task started, port %d\n", RTP_CLIENT_LOCAL_PORT);

    uint32_t recv_count = 0, timeout_count = 0, frag_count = 0;
    uint16_t prev_seq = 0;
    bool seq_init = false, frame_overflow = false, frame_incomplete = false, frame_started = false;
    uint32_t qtable_hdr_size = 0u;

    while (session->state == RTSP_CLIENT_PLAYING && s_rtsp_client_ctx.client_running)
    {
        struct sockaddr_in src_addr;
        socklen_t src_len = sizeof(src_addr);
        int pkt_len = recvfrom(session->rtp_sock, pkt_buf, RTP_RECV_BUF_SIZE, 0,
                               (struct sockaddr *)&src_addr, &src_len);
        if (pkt_len < 0)
        {
            if ((++timeout_count % 5) == 1)
                HAL_LOGD("rtp_recv_task: recvfrom %d errno=%d (cnt=%u)\n",
                         pkt_len, errno, timeout_count);
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        recv_count++;

        if (pkt_len < RTP_HEADER_SIZE + JPEG_HEADER_SIZE)
        {
            HAL_LOGD("rtp_recv_task: short pkt #%u len=%d\n", recv_count, pkt_len);
            continue;
        }

        rtp_recv_header_t  *rtp_hdr  = (rtp_recv_header_t *)pkt_buf;
        jpeg_recv_header_t *jpeg_hdr = (jpeg_recv_header_t *)(pkt_buf + RTP_HEADER_SIZE);
        bool marker = (rtp_hdr->mpt & RTP_MARKER_BIT) != 0;
        uint16_t cur_seq = ntohs(rtp_hdr->seq);
        uint32_t frag_offset = ((uint32_t)jpeg_hdr->fragment_offset1 << 16) |
                               ((uint32_t)jpeg_hdr->fragment_offset2 <<  8) |
                               ((uint32_t)jpeg_hdr->fragment_offset3);
        const uint8_t *payload = pkt_buf + RTP_HEADER_SIZE + JPEG_HEADER_SIZE;
        int payload_len = pkt_len  - RTP_HEADER_SIZE - JPEG_HEADER_SIZE;

        /* Lost RTP packet(s) detection */
        if (seq_init && (uint16_t)(cur_seq - (uint16_t)(prev_seq + 1u)) != 0)
        {
            /* Lost RTP packet(s) detected, skip current frame at marker */
            frame_incomplete = true;
        }
        prev_seq = cur_seq;
        seq_init = true;

        if (payload_len <= 0)
            continue;

        if (frag_offset == 0)
        {
            /* Start of new frame */
            frame_overflow = frame_incomplete = false;
            frame_started  = true;
            frag_count     = 0;
            qtable_hdr_size = 0u;
            session->jpeg_width_pix  = (uint16_t)jpeg_hdr->width  * 8u;
            session->jpeg_height_pix = (uint16_t)jpeg_hdr->height * 8u;
            session->jpeg_q          = jpeg_hdr->q;
            HAL_LOGI("rtp_recv_task: new frame seq=%u q=%u %ux%u type=%u plen=%d\n",
                     cur_seq, jpeg_hdr->q,
                     session->jpeg_width_pix, session->jpeg_height_pix,
                     jpeg_hdr->type, payload_len);

            /* RFC 2435 type >= 64 signals a restart interval (DRI marker required).
             * This implementation does not support DRI). */
            if (jpeg_hdr->type >= 64u)
            {
                HAL_LOGE("rtp_recv_task: unsupported JPEG type=%u (DRI not implemented) - discarding frame\n",
                         jpeg_hdr->type);
                frame_incomplete = true;
            }

            if (jpeg_hdr->q >= 128u && payload_len >= 4)
            {
                /* Special case: custom inline quantization table */
                uint16_t ql = (uint16_t)((payload[2] << 8) | payload[3]);
                qtable_hdr_size = 4u + ql;

                HAL_LOGI("rtp_recv_task: inline qtable qlen=%u qtable_hdr_size=%u\n",
                         ql, qtable_hdr_size);

                if (qtable_hdr_size >= (uint32_t)payload_len)
                {
                    HAL_LOGE("rtp_recv_task: bad qtable_hdr_size=%u plen=%d\n",
                             qtable_hdr_size, payload_len);
                    frame_incomplete = true;
                    qtable_hdr_size = 0u;
                }
            }
        }
        frag_count++;

        /* For first fragment, copy whole payload (including qtable header if this is the case)
           at the beginning of buffer. For subsequent fragments, copy payload at the offset shifted by previous
           qtable header size (as fragment offset is only the offset in raw JPEG payload data). */
        uint32_t buf_off = (frag_offset == 0u) ? 0u : frag_offset + qtable_hdr_size;

        if (buf_off + (uint32_t)payload_len > session->rtp_buf_size)
        {
            if (!frame_overflow)
                HAL_LOGD("rtp_recv_task: overflow off=%u plen=%d - discard\n",
                         buf_off, payload_len);
            frame_overflow = true;
            if (marker)
                goto next_frame;
            continue;
        }

        if (session->frame_pending)
            frame_incomplete = true;
        else if (!frame_incomplete)
            memcpy(session->rtp_buf + buf_off, payload, payload_len);

        if (marker)
        {
            /* Check if frame is complete and valid */
            if (frame_overflow || frame_incomplete || !frame_started)
                goto next_frame;

            uint32_t raw_size = buf_off + (uint32_t)payload_len;
            uint32_t jpeg_sz = rfc2435_to_jpeg(session->rtp_buf, raw_size,
                                               session->jpeg_width_pix, session->jpeg_height_pix,
                                               session->rtp_buf_size, session->jpeg_q);
            if (!jpeg_sz)
            {
                HAL_LOGE("rfc2435_to_jpeg failed\n");
                goto next_frame;
            }

            /* Lock the buf_mutex before touching frame_pending/rtp_buf/dec_buf. */
            if (session->buf_mutex &&
                xSemaphoreTake(session->buf_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
            {
                if (!session->frame_pending)
                {
                    session->frame_size = jpeg_sz;
                    session->frame_pending = true;
                    xSemaphoreGive(session->buf_mutex);
                    if (session->frame_ready_sem)
                        xSemaphoreGive(session->frame_ready_sem);
                }
                else
                {
                    xSemaphoreGive(session->buf_mutex);
                    /* Decoder has not consumed the previous frame yet; skip. */
                }
            }
            HAL_LOGD("rtp_recv_task: frame frags=%u bytes=%u\n",
                     frag_count, session->frame_size);

next_frame:
            frame_overflow = frame_incomplete = frame_started = false;
        }
    }

    HAL_LOGI("RTP receive task exiting\n");
cleanup:
    if (pkt_buf)
        vPortFree(pkt_buf);
    session->rtp_task_handler = NULL;
    vTaskDelete(NULL);
}

/* RTSP client main task */
void rtsp_client_task(void *arg)
{
    rtsp_client_session_t *session = (rtsp_client_session_t *)arg;
    if (!session)
    {
        HAL_LOGE("Invalid session\n");
        goto end;
    }
    rtspsrc_t *dev = session->dev;
    if (!dev)
    {
        HAL_LOGE("Invalid dev\n");
        goto end;
    }

    /* Parse the URL once - a bad URL is a permanent failure. */
    if (rtsp_parse_url(dev->config.rtsp_url, session->server_ip,
                       &session->server_port, session->url_path) != RTSPSRC_SUCCESS)
    {
        HAL_LOGE("Bad URL: %s\n", dev->config.rtsp_url);
        goto end;
    }

    /* Outer reconnect loop - retries after any connection or session failure. */
    while (s_rtsp_client_ctx.client_running)
    {
        /* Reset per-session state for a clean reconnect. */
        session->cseq          = 0;
        session->session_id[0] = '\0';
        session->state         = RTSP_CLIENT_UNINIT;
        session->frame_pending = false;

        /* Open RTSP TCP socket. */
        session->sess_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (session->sess_sock < 0)
        {
            HAL_LOGE("socket() failed errno=%d\n", errno);
            goto retry;
        }

        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port   = htons(session->server_port);
        if (inet_aton(session->server_ip, &server_addr.sin_addr) == 0)
        {
            HAL_LOGE("inet_aton: invalid IP address '%s'\n", session->server_ip);
            goto cleanup_sock;
        }

        if (connect(session->sess_sock, (struct sockaddr *)&server_addr,
                    sizeof(server_addr)) < 0)
        {
            HAL_LOGI("connect() failed errno=%d - server not ready yet\n", errno);
            goto cleanup_sock;
        }

        /* Receive timeout on the RTSP TCP socket - keepalive recv() never blocks forever. */
        struct timeval sess_tv = {5, 0};
        setsockopt(session->sess_sock, SOL_SOCKET, SO_RCVTIMEO,
                    &sess_tv, sizeof(sess_tv));

        session->state = RTSP_CLIENT_INIT;

        session->frame_ready_sem = xSemaphoreCreateBinary();
        if (!session->frame_ready_sem)
        {
            HAL_LOGE("frame_ready_sem create failed\n");
            goto cleanup_sock;
        }

        session->buf_mutex = xSemaphoreCreateMutex();
        if (!session->buf_mutex)
        {
            HAL_LOGE("buf_mutex create failed\n");
            goto cleanup_sock;
        }

        /* Open RTP UDP socket. */
        session->rtp_sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (session->rtp_sock < 0)
        {
            HAL_LOGE("RTP socket() failed errno=%d\n", errno);
            goto cleanup_sem;
        }

        struct timeval tv = {1, 0};
        setsockopt(session->rtp_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        int rcvbuf = RTP_SOCKET_RCVBUF_SIZE;
        setsockopt(session->rtp_sock, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

        struct sockaddr_in rtp_addr;
        memset(&rtp_addr, 0, sizeof(rtp_addr));
        rtp_addr.sin_family      = AF_INET;
        rtp_addr.sin_port        = htons(RTP_CLIENT_LOCAL_PORT);
        rtp_addr.sin_addr.s_addr = INADDR_ANY;
        if (bind(session->rtp_sock, (struct sockaddr *)&rtp_addr,
                    sizeof(rtp_addr)) < 0)
        {
            HAL_LOGE("RTP bind() failed errno=%d\n", errno);
            goto cleanup_rtp;
        }

        /* RTSP handshake. */
        if (rtsp_do_options(session) != RTSPSRC_SUCCESS)
            goto cleanup_rtp;

        if (rtsp_do_describe(session) != RTSPSRC_SUCCESS)
            goto cleanup_rtp;

        if (rtsp_do_setup(session) != RTSPSRC_SUCCESS)
            goto cleanup_rtp;
        session->state = RTSP_CLIENT_READY;

        if (rtsp_do_play(session) != RTSPSRC_SUCCESS)
            goto cleanup_rtp;
        session->state = RTSP_CLIENT_PLAYING;

        HAL_LOGI("RTSP stream playing\n");
        PRINTF("Connected to RTSP server %s:%d\r\n",
               session->server_ip, session->server_port);

        BaseType_t xret = xTaskCreate(rtp_recv_task, "rtp_recv",
                                        RTP_RECV_TASK_STACK_SIZE, session,
                                        RTP_RECV_TASK_PRIORITY,
                                        &session->rtp_task_handler);
        if (xret != pdPASS)
        {
            HAL_LOGE("Failed to create RTP task\n");
            goto cleanup_rtp;
        }

        /* Keepalive loop. */
        while (session->state == RTSP_CLIENT_PLAYING &&
               s_rtsp_client_ctx.client_running)
        {
            vTaskDelay(pdMS_TO_TICKS(RTSP_KEEPALIVE_INTERVAL_MS));
            if (session->state != RTSP_CLIENT_PLAYING)
                break;
            char req[RTSP_RECV_BUF_SIZE], resp[RTSP_RECV_BUF_SIZE];
            char url[RTSP_URL_PATH_MAX_LEN + IPV4_STR_LEN + 16];
            build_rtsp_url(session, url, sizeof(url));
            snprintf(req, sizeof(req),
                     "OPTIONS %s RTSP/1.0\r\nCSeq: %d\r\nSession: %s\r\n\r\n",
                     url, ++session->cseq, session->session_id);
            if (rtsp_send_request(session, req) != RTSPSRC_SUCCESS)
            {
                HAL_LOGI("Keepalive send failed\n");
                session->state = RTSP_CLIENT_TEARDOWN;
                break;
            }
            if (rtsp_recv_response(session, resp, sizeof(resp)) != RTSPSRC_SUCCESS)
            {
                HAL_LOGI("Keepalive recv failed - server closed\n");
                session->state = RTSP_CLIENT_TEARDOWN;
                break;
            }
            if (!rtsp_is_200_ok(resp))
            {
                HAL_LOGI("Keepalive response not 200 OK - triggering reconnect\n");
                session->state = RTSP_CLIENT_TEARDOWN;
                break;
            }
            HAL_LOGD("Keepalive sent (cseq=%u)\n", session->cseq);
        }

        /* Best-effort TEARDOWN. */
        if (session->state != RTSP_CLIENT_TEARDOWN)
        {
            char req[RTSP_RECV_BUF_SIZE], resp[RTSP_RECV_BUF_SIZE];
            char url[RTSP_URL_PATH_MAX_LEN + IPV4_STR_LEN + 16];
            build_rtsp_url(session, url, sizeof(url));
            snprintf(req, sizeof(req), STR_TEARDOWN_REQ, url,
                     ++session->cseq, session->session_id);
            rtsp_send_request(session, req);
            rtsp_recv_response(session, resp, sizeof(resp));
        }
        session->state = RTSP_CLIENT_TEARDOWN;

        /* Wait for the RTP receive task to exit before closing sockets. */
        while (session->rtp_task_handler != NULL)
            vTaskDelay(pdMS_TO_TICKS(50));

cleanup_rtp:
        if (session->rtp_sock >= 0)
        {
            close(session->rtp_sock);
            session->rtp_sock = -1;
        }
cleanup_sem:
        if (session->frame_ready_sem)
        {
            vSemaphoreDelete(session->frame_ready_sem);
            session->frame_ready_sem = NULL;
        }
        if (session->buf_mutex)
        {
            vSemaphoreDelete(session->buf_mutex);
            session->buf_mutex = NULL;
        }
cleanup_sock:
        if (session->sess_sock >= 0)
        {
            close(session->sess_sock);
            session->sess_sock = -1;
        }
retry:
        if (s_rtsp_client_ctx.client_running)
        {
            HAL_LOGI("RTSP session ended, reconnecting in %u ms\n",
                     RTSP_RECONNECT_DELAY_MS);
            vTaskDelay(pdMS_TO_TICKS(RTSP_RECONNECT_DELAY_MS));
        }
    }

end:
    session->state = RTSP_CLIENT_UNINIT;
    session->handler = NULL;
    s_rtsp_client_ctx.client_running = false;
    vTaskDelete(NULL);
}

#endif /* HAL_ENABLE_RTSP_SRC */
