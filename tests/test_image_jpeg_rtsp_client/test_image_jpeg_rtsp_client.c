/*
 * Copyright 2026 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief JPEG Image RTSP Client Test Application
 *
 * @section test_overview Overview:
 * This test application demonstrates receiving JPEG image streaming via RTSP client:
 *
 *   RTSP Server (PC) → Network → RTSP Source (Client) → Video Decoder → Format Converter → Display
 *
 * The pipeline receives a JPEG video stream from a remote RTSP server over the network
 * using the RTSP protocol with JPEG/RTP payload format (RFC 2435). The received frames
 * are decoded and displayed on the board's display.
 * 
 * @section rtsp_server RTSP/RTP Server Setup:
 * Before starting the test, set up an RTSP server and a publisher on the connected PC.
 * The board IP address, server IP address and port are configured in test_config.h.
 * The board should be connected directly to the PC via Ethernet cable.
 * 
 * Note: FFmpeg cannot act as an RTSP server on its own (it cannot respond to RTSP
 * commands such as DESCRIBE, SETUP, PLAY). It must publish to a dedicated RTSP server
 * such as MediaMTX, which handles the RTSP session and relays the RTP stream to clients.
 * 
 * @subsection step1 Step 1 - Start the RTSP server (MediaMTX):
 *   ./mediamtx
 *
 * @subsection step2 Step 2 - Publish a JPEG image as an MJPEG stream via FFmpeg:
 *   ffmpeg -loop 1 -re -i <IMAGE_FILE> -vcodec mjpeg -c:v copy -pkt_size 1400 \
 *          -f rtsp rtsp://<SERVER_IP>:8554/<STREAM_PATH>
 *
 * Where <SERVER_IP> is configGW_ADDR0.configGW_ADDR1.configGW_ADDR2.configGW_ADDR3
 * and <STREAM_PATH> matches RTSP_URL defined in test_config.h.
 *
 * Example (streaming tiger_rtp.jpg):
 *   ffmpeg -loop 1 -re -i ~/vtec_repos/mpp/images/tiger_rtp.jpg \
 *          -vcodec mjpeg -c:v copy -pkt_size 1400 \
 *          -f rtsp rtsp://192.168.0.100:8554/photo
 */

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "string.h"
#include "stdbool.h"
#include <stdio.h>

/* NXP includes. */
#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#include "app.h"

#include "hal_debug.h"
#include "hal_os.h"
#include "hal_utils.h"
#include "hal_os.h"

/* MPP includes */
#include "mpp_api.h"
#include "mpp_config.h"

#include "test_config.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

#define TEST_CHECK_PERIOD_MS 1000

#ifdef TEST_AUTO_DISABLE
#define TEST_MODE ""
#else
#define TEST_MODE "AUTO"
#endif

/* pick CPU for image conversion by default */
#ifndef IMG_CONVERT_DEV_NAME
#define IMG_CONVERT_DEV_NAME "gfx_CPU"
#endif

/* pick CPU for image decoding by default */
#ifndef IMG_DECODE_DEV_NAME
#define IMG_DECODE_DEV_NAME "jpeg_CPU"
#endif

/* set this flag to 1 to perform image scaling to display dimensions */
#ifndef IMG_FULL_SCREEN
#define IMG_FULL_SCREEN 1
#endif

#ifndef APP_STRIPE_MODE
#define APP_STRIPE_MODE 0
#endif

/* in case of Remote-FB display (and not full-screen mode): update only the image region on screen */
#if ( defined(APP_DISPLAY_REMOTE_FB) && (APP_DISPLAY_REMOTE_FB == 1) && (IMG_FULL_SCREEN != 1))
#if ( SRC_IMAGE_WIDTH > APP_DISPLAY_WIDTH)
    #define DISP_BUF_WIDTH APP_DISPLAY_WIDTH;
#else
    #define DISP_BUF_WIDTH SRC_IMAGE_WIDTH;
#endif
#if ( SRC_IMAGE_HEIGHT > APP_DISPLAY_HEIGHT)
    #define DISP_BUF_HEIGHT APP_DISPLAY_HEIGHT;
#else
    #define DISP_BUF_HEIGHT SRC_IMAGE_HEIGHT;
#endif
#else   /* (APP_DISPLAY_REMOTE_FB == 1) && (IMG_FULL_SCREEN != 1)) */
    #define DISP_BUF_WIDTH APP_DISPLAY_WIDTH;
    #define DISP_BUF_HEIGHT APP_DISPLAY_HEIGHT;
#endif  /* (APP_DISPLAY_REMOTE_FB == 1) && (IMG_FULL_SCREEN != 1)) */

#if APP_CONFIG
#define ARG2STR(x) #x
#define CONFIG2STR(x) ARG2STR(x)
#define TC_NAME "test_image_jpeg_rtsp_client_config" CONFIG2STR(APP_CONFIG)
#else
#define TC_NAME "test_image_jpeg_rtsp_client"
#endif

typedef struct _args_t
{
    char display_name[32];
    mpp_pixel_format_t display_format;
    mpp_pixel_format_t source_format;
} args_t;

/*******************************************************************************
 * Variables declaration
 ******************************************************************************/
/** Default priority for application tasks
   Tasks created by the application have a lower priority than pipeline tasks by default.
   Pipeline_task_max_prio in mpp_api_params_t structure should be adjusted with other application tasks.*/
#define APP_DEFAULT_PRIO        1

#if LWIP_IPV4 && LWIP_RAW && LWIP_SOCKET
static phy_handle_t phyHandle;
static struct netif netif;
#endif /* LWIP_IPV4 && LWIP_RAW && LWIP_SOCKET */

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static void app_task(void *params);

/*******************************************************************************
 * Code
 ******************************************************************************/

/* Set params for image convert element */
static void set_img_convert_params(mpp_element_params_t *elem_params)
{
    memset(elem_params, 0, sizeof(mpp_element_params_t));

    elem_params->convert.dev_name = IMG_CONVERT_DEV_NAME;
    elem_params->convert.out_buf.height = DISP_BUF_HEIGHT;
    elem_params->convert.out_buf.width  = DISP_BUF_WIDTH;
    /* pixel format */
    elem_params->convert.pixel_format = APP_DISPLAY_FORMAT;
    /* scaling parameters */
    elem_params->convert.scale.width  = DISP_BUF_WIDTH;
    elem_params->convert.scale.height = DISP_BUF_HEIGHT;
    /* rotate */
    elem_params->convert.angle = APP_DISPLAY_LANDSCAPE_ROTATE;
    elem_params->convert.flip = FLIP_NONE;

    elem_params->convert.ops = MPP_CONVERT_COLOR;
    elem_params->convert.ops |= MPP_CONVERT_ROTATE;
    elem_params->convert.ops |= MPP_CONVERT_SCALE;
}


int mpp_event_listener(mpp_t mpp, mpp_evt_t evt, void *evt_data, void *user_data)
{
    checksum_data_t *chksm;
    static bool chksm_ok = false;
    static bool chksm_done = false;
    static int count = 0;   /* frame counter, to ignore first frames */
    static int chksm_time = 0;  /* time of checksum */

    switch(evt) 
    {
    case MPP_EVENT_INVALID:
    default:
        /* nothing to do */
        break;
    }

    return 0;
}

/*!
 * @brief Application entry point.
 */
int main(int argc, char *argv[])
{
    BaseType_t ret = pdFAIL;
    TaskHandle_t handle = NULL;

    /* Init board hardware. */
    BOARD_Init();

    PRINTF("****** %s TEST test_image_jpeg_rtsp_client ******\r\n", TEST_MODE);
    PRINTF("\r\n");

    args_t *args = pvPortMalloc(sizeof(args_t));
    if (!args)
    {
        PRINTF("Allocation failed\n");
        goto err;
    }

    strcpy(args->display_name, APP_DISPLAY_NAME);
    args->display_format = APP_DISPLAY_FORMAT;

    ret = xTaskCreate(
          app_task,
          "app_task",
          configMINIMAL_STACK_SIZE + 1000,
          (void *) args,
          APP_DEFAULT_PRIO,
          &handle);

err:
    if (pdPASS != ret)
    {
        PRINTF("Failed to create app_task task");
        while (1);
    }

    vTaskStartScheduler();
    for (;;)
        vTaskSuspend(NULL);
    return 0;
}

static void app_task(void *params)
{
    int ret = 0;
    mpp_elem_handle_t convert_h = (mpp_elem_handle_t) NULL;

    int src_width = SRC_IMAGE_WIDTH;
    int src_height = SRC_IMAGE_HEIGHT;

    char rtsp_url[64];
    snprintf(rtsp_url, sizeof(rtsp_url), "rtsp://%d.%d.%d.%d:%d/" RTSP_STREAM_PATH,
             configGW_ADDR0, configGW_ADDR1, configGW_ADDR2, configGW_ADDR3,
             RTSP_SERVER_PORT);

    args_t *args = (args_t *) params;
    if (args == NULL)
    {
        PRINTF("ERROR: NULL parameters passed to app_task\n");
        goto err;
    }

    PRINTF("[%s]\r\n", mpp_get_version());

    /* add support for params */
    ret = mpp_api_init(NULL);
    if (ret)
        goto err;

    /* Initialize Ethernet network interface */
    uint8_t ip_addr[4] = {configIP_ADDR0, configIP_ADDR1, configIP_ADDR2, configIP_ADDR3};
    uint8_t netmask[4] = {configNET_MASK0, configNET_MASK1, configNET_MASK2, configNET_MASK3};
    uint8_t gateway[4] = {configGW_ADDR0, configGW_ADDR1, configGW_ADDR2, configGW_ADDR3};

    mpp_eth_netif_init(ip_addr, netmask, gateway);

    mpp_t mp;
    mpp_params_t mpp_params;
    memset(&mpp_params, 0, sizeof(mpp_params));
    mpp_params.exec_flag = MPP_EXEC_RC;
    mpp_params.evt_callback_f = &mpp_event_listener;
    mpp_params.mask = MPP_EVENT_ALL;
    mp = mpp_create(&mpp_params, &ret);
    if (mp == MPP_INVALID)
	goto err;

    /* Add RTSP source */
    mpp_rtspsrc_params_t rtspsrc_params;
    memset(&rtspsrc_params, 0 , sizeof(rtspsrc_params));
    rtspsrc_params.rtsp_buffer_size = RTSP_BUF_SIZE;
    rtspsrc_params.rtsp_url = rtsp_url;
    rtspsrc_params.width  = src_width;
    rtspsrc_params.height = src_height;
    rtspsrc_params.format = MPP_PIXEL_JPEG;
    ret = mpp_rtspsrc_add(mp, &rtspsrc_params, NULL);
    if (ret)
    {
        PRINTF("Failed to add RTSP source");
        goto err;
    }

    /* Add element jpeg decode */
    mpp_element_params_t elem_params;
    memset(&elem_params, 0, sizeof(mpp_element_params_t));
    elem_params.decode.dev_name = IMG_DECODE_DEV_NAME;
    elem_params.decode.width = src_width;
    elem_params.decode.height = src_height;

    if (strcmp(IMG_DECODE_DEV_NAME, "jpeg_CPU") == 0)
        elem_params.decode.out_format = MPP_PIXEL_BGR;
    else if (strcmp(IMG_DECODE_DEV_NAME, "jpeg_HW") == 0)
        elem_params.decode.out_format = MPP_PIXEL_YUYV; /* TODO auto detect */

    ret = mpp_element_add(mp, MPP_ELEMENT_IMG_DECODE, &elem_params, NULL);
    if (ret)
    {
        PRINTF("Failed to add element DECODE\n");
        goto err;
    }

    /* Add element convert */
    set_img_convert_params(&elem_params);
    ret = mpp_element_add(mp, MPP_ELEMENT_CONVERT, &elem_params, &convert_h);
    if (ret)
    {
        PRINTF("Failed to add element CONVERT\n");
        goto err;
    }

    /* Set display's parameters */
    mpp_display_params_t disp_params;
    memset(&disp_params, 0, sizeof(disp_params));
    disp_params.format = APP_DISPLAY_FORMAT;
    disp_params.width  = DISP_BUF_WIDTH;
    disp_params.height = DISP_BUF_HEIGHT;
    disp_params.stripe = (APP_STRIPE_MODE > 0) ? true : false;
    ret = mpp_display_add(mp, args->display_name, &disp_params);
    if (ret)
    {
        PRINTF("Failed to add display %s\r\n", args->display_name);
        goto err;
    }

    ret = mpp_start(mp, 1, false);
    if (ret)
    {
        PRINTF("Failed to start pipeline\r\n");
        goto err;
    }

    /* pause application task */
    vTaskSuspend(NULL);

err:
    for (;;)
    {
        PRINTF("Error building application pipeline : ret %d\r\n", ret);
        vTaskSuspend(NULL);
    }
}
