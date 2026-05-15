/*
 * Copyright 2026 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief JPEG Image RTSP Streaming Test Application
 *
 * @section test_overview Overview:
 * This test application demonstrates JPEG image streaming via RTSP server:
 *
 *   Static JPEG Image → RTSP Sink
 *      (Compressed)     (Network)
 *
 * The pipeline streams a static JPEG image over the network using the RTSP
 * protocol with JPEG payload format (RFC 2435). The image is continuously
 * streamed to connected RTSP clients.
 * 
 * @section rtsp_client RTSP/RTP Client Connection:
 * After starting the test, the RTSP/RTP server runs on port 8554.
 * The IP address is configured in test_config.h.
 * Connect from a Linux PC using:
 * 
 * ffplay:
 *   ffplay -rtsp_transport udp -i rtsp://<IP_ADDRESS>:8554/
 * ffplay with no buffering and low latency:
 *   ffplay -rtsp_transport udp -fflags nobuffer -flags low_delay -framedrop -i rtsp://<IP_ADDRESS>:8554/
 *
 * GStreamer:
 *   gst-launch-1.0 rtspsrc location=rtsp://<IP_ADDRESS>:8554/ protocols=udp ! \
 *   rtpjpegdepay ! jpegparse ! jpegdec ! autovideosink
 *
 * Where <IP_ADDRESS> is configIP_ADDR0.configIP_ADDR1.configIP_ADDR2.configIP_ADDR3
 * as defined in test_config.h.
 *
 * Note: The board should be connected directly to the PC via ethernet cable.
 */

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "string.h"
#include "stdbool.h"

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

/* pick CPU for image decoding by default */
#ifndef IMG_DECODE_DEV_NAME
#define IMG_DECODE_DEV_NAME "jpeg_CPU"
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
#define TC_NAME "test_image_jpeg_rtsp_server_config" CONFIG2STR(APP_CONFIG)
#else
#define TC_NAME "test_image_jpeg_rtsp_server"
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

int mpp_event_listener(mpp_t mpp, mpp_evt_t evt, void *evt_data, void *user_data)
{
    checksum_data_t *chksm;
    static bool chksm_ok = false;
    static bool chksm_done = false;
    static int count = 0;   /* frame counter, to ignore first frames */
    static int chksm_time = 0;  /* time of checksum */

    switch(evt) 
    {
    case MPP_EVENT_INTERNAL_TEST_RESERVED:
        chksm = (checksum_data_t *)evt_data;
        if (chksm == NULL)
        {
            return 0;
        }
        if (chksm->type != CHECKSUM_TYPE_CRC_ELCDIF)
        {
            PRINTF("ERROR: checksum calculated should be using CRC LCDIF\n");
            return 0;
        }
        /* if check period elapsed, test again */
        int time = hal_tick_to_ms(hal_get_ostick());
        if (time > chksm_time + TEST_CHECK_PERIOD_MS)
        {
            chksm_done = false;
            chksm_time = time;
        }
        /* verify checksum if needed */
        if (!chksm_done && count > 1)
        {
            PRINTF("\r\nStart %s\r\n", TC_NAME);
            chksm_done = true;
            chksm_ok = ((chksm->value == EXPECTED_CHECKSUM) || (APP_STRIPE_MODE > 0));  /* ignore checksum for stripes */
            if (chksm_ok)
                PRINTF("%s - PASSED\r\n", TC_NAME);
            else
            {
                PRINTF("Bad checksum 0x%08x\r\n", chksm->value);
                PRINTF("%s - FAILED\r\n", TC_NAME);
            }
            PRINTF("%s finished\r\n", TC_NAME);
        }
        count++;
        break;

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

    PRINTF("****** %s TEST test_image_jpeg_rtsp_server ******\r\n", TEST_MODE);
    PRINTF("****** PARAMS: IMAGE_NAME = [%s] ******\r\n", IMAGE_NAME);
    PRINTF("\r\n");

    args_t *args = pvPortMalloc(sizeof(args_t));
    if (!args)
    {
        PRINTF("Allocation failed\n");
        goto err;
    }

    strcpy(args->display_name, APP_DISPLAY_NAME);
    args->display_format = APP_DISPLAY_FORMAT;
    args->source_format = SRC_IMAGE_FORMAT;

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
    int src_width = SRC_IMAGE_WIDTH;
    int src_height = SRC_IMAGE_HEIGHT;
    bool stripe_mode = (APP_STRIPE_MODE > 0)? true : false;
    int ret = 0;
    mpp_elem_handle_t convert_h = (mpp_elem_handle_t) NULL;

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

    /* add image static */
    /* Set static image element parameters */
    mpp_img_params_t img_params ;
    memset(&img_params, 0 , sizeof(img_params));
    img_params.height = src_height;
    img_params.width =  src_width;
    img_params.format = args->source_format;
    img_params.compressed_size = image_data_len;
    ret = mpp_static_img_add(mp, &img_params, (void *)image_data, NULL);
    if (ret)
    {
        PRINTF("Failed to add static image");
        goto err;
    }

    /* Set rtspsink element parameters */
    mpp_rtspsink_params_t rtspsink_params;
    memset(&rtspsink_params, 0, sizeof(mpp_rtspsink_params_t));
    /* Configure RTSP server parameters */
    rtspsink_params.port = RTSP_SERVER_PORT;
    /* Set IP address as octets from test_config.h */
    rtspsink_params.ip_addr[0] = configIP_ADDR0;
    rtspsink_params.ip_addr[1] = configIP_ADDR1;
    rtspsink_params.ip_addr[2] = configIP_ADDR2;
    rtspsink_params.ip_addr[3] = configIP_ADDR3;

    rtspsink_params.frame_width = src_width;
    rtspsink_params.frame_height = src_height;
    rtspsink_params.payload_type = RTP_PAYLOAD_TYPE_JPEG;
    rtspsink_params.frame_type = RTP_JPEG_FRAME_TYPE_YUV422;
    ret = mpp_rtspsink_add(mp, &rtspsink_params, NULL);
    if (ret) {
        PRINTF("Failed to add RTSP sink\n");
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
