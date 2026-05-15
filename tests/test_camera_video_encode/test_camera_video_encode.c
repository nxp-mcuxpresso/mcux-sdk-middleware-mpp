/*
 * Copyright 2026 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief H.264 Video Encode and RTSP Streaming Test Application
 *
 * @section test_overview Overview:
 * This test application demonstrates the H.264 video encode and streaming pipeline:
 *
 *   Camera/Static Image → Format Converter → Video Encoder → RTSP Sink
 *        (RGB/YUV)           (YUV420P)          (H.264)      (Network)
 *
 * The pipeline captures frames from a camera (or uses a static image), converts
 * the format to YUV420P, encodes it to H.264, and streams the compressed H.264
 * video over the network via RTSP protocol.
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
 *   rtph264depay ! h264parse ! avdec_h264 ! autovideosink
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

#ifndef EMULATOR
/* Freescale includes. */
#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "app.h"
#include "board.h"
#else
#include <stdio.h>
#define PRINTF printf
#define main app_main
#endif

/* MPP includes */
#include "mpp_api.h"
#include "mpp_config.h"

#include "test_config.h"
#include "hal_os.h"
#include "hal_utils.h"

/* test image include file */
#ifndef EMULATOR   /* i.MX RT1170 EVK */
#define IMAGE_WIDTH  DECODED_VIDEO_WIDTH
#define IMAGE_HEIGHT DECODED_VIDEO_HEIGHT
#define IMAGE_FORMAT DECODED_VIDEO_FORMAT
#endif /* EMULATOR */

/* set IMG_CONVERT_CPU or IMG_CONVERT_GPU flag to 1 to perform image conversion using CPU or GPU
 * (otherwise first gfx device in the list is used by default) */
#if !defined(IMG_CONVERT_CPU) && !defined(IMG_CONVERT_GPU)
#define IMG_CONVERT_CPU 0
#define IMG_CONVERT_GPU 0
#elif defined(IMG_CONVERT_CPU)
#define IMG_CONVERT_GPU 0
#elif defined(IMG_CONVERT_GPU)
#define IMG_CONVERT_CPU 0
#endif

#if (IMG_CONVERT_CPU == 1)
#define IMG_CONVERT_DEV_NAME "gfx_CPU"
#elif (IMG_CONVERT_GPU == 1)
#define IMG_CONVERT_DEV_NAME "gfx_GPU"
#else
/* pick default device from the first listed and supported by Hw */
#define IMG_CONVERT_DEV_NAME NULL
#endif

#include "lwip/opt.h"
#include "lwip/sockets.h"

#if LWIP_IPV4 && LWIP_RAW && LWIP_SOCKET
#include "ping.h"
#include "lwip/netifapi.h"
#include "lwip/tcpip.h"
#include "netif/ethernet.h"
#include "ethernetif.h"

#if ETH_USE_GPIO_ADAPTER
#include "fsl_adapter_gpio.h"
#endif /* ETH_USE_GPIO_ADAPTER */
#endif /* LWIP_IPV4 && LWIP_RAW && LWIP_SOCKET */

/*******************************************************************************
 * Definitions
 ******************************************************************************/
typedef struct _args_t {
    char display_name[32];
    mpp_pixel_format_t display_format;
    mpp_pixel_format_t image_format;
} args_t;

#if LWIP_IPV4 && LWIP_RAW && LWIP_SOCKET
#ifndef EXAMPLE_NETIF_INIT_FN
/*! @brief Network interface initialization function. */
#define EXAMPLE_NETIF_INIT_FN ethernetif0_init
#endif /* EXAMPLE_NETIF_INIT_FN */

/*! @brief Selection of GPIO perihperal and its pin for the reception of PHY interrupts. */
#if ETH_LINK_POLLING_INTERVAL_MS == 0
#if ETH_USE_GPIO_ADAPTER == 0
#error "Interrupt-based link-state detection is enabled but GPIO adapter is not used."
#endif /* ETH_USE_GPIO_ADAPTER */
#ifndef EXAMPLE_PHY_INT_PORT
#if (!defined(BOARD_NETWORK_USE_100M_ENET_PORT) || !BOARD_NETWORK_USE_100M_ENET_PORT) && \
    defined(BOARD_INITENET1GPINS_PHY_INTR_PERIPHERAL)
#define EXAMPLE_PHY_INT_PORT BOARD_INITENET1GPINS_PHY_INTR_PERIPHERAL
#elif defined(BOARD_INITENETPINS_PHY_INTR_PERIPHERAL)
#define EXAMPLE_PHY_INT_PORT BOARD_INITENETPINS_PHY_INTR_PERIPHERAL
#elif defined(BOARD_INITPINS_PHY_INTR_PERIPHERAL)
#define EXAMPLE_PHY_INT_PORT BOARD_INITPINS_PHY_INTR_PERIPHERAL
#else
#error "Interrupt-based link-state detection was enabled on an unsupported board."
#endif
#endif // #ifndef EXAMPLE_PHY_INT_PORT

#ifndef EXAMPLE_PHY_INT_PIN
#if (!defined(BOARD_NETWORK_USE_100M_ENET_PORT) || !BOARD_NETWORK_USE_100M_ENET_PORT) && \
    defined(BOARD_INITENET1GPINS_PHY_INTR_CHANNEL)
#define EXAMPLE_PHY_INT_PIN BOARD_INITENET1GPINS_PHY_INTR_CHANNEL
#elif defined(BOARD_INITENETPINS_PHY_INTR_CHANNEL)
#define EXAMPLE_PHY_INT_PIN BOARD_INITENETPINS_PHY_INTR_CHANNEL
#elif defined(BOARD_INITPINS_PHY_INTR_CHANNEL)
#define EXAMPLE_PHY_INT_PIN BOARD_INITPINS_PHY_INTR_CHANNEL
#else
#error "Interrupt-based link-state detection was enabled on an unsupported board."
#endif
#endif // #ifndef EXAMPLE_PHY_INT_PIN
#endif // #if ETH_LINK_POLLING_INTERVAL_MS == 0
#endif /* LWIP_IPV4 && LWIP_RAW && LWIP_SOCKET */

/*******************************************************************************
 * Variables declaration
 ******************************************************************************/

/* set this flag to 1 in order to replace the camera source by static image */
#ifndef SOURCE_STATIC_IMAGE
#define SOURCE_STATIC_IMAGE 0
#endif

#ifndef APP_STRIPE_MODE
#define APP_STRIPE_MODE 0
#endif

#if (SOURCE_STATIC_IMAGE == 0)
static const char s_camera_name[] = APP_CAMERA_NAME;
#endif

/* Encoded H.264 video parameters */
#if (SOURCE_STATIC_IMAGE == 0)
#define ENCODED_VIDEO_FORMAT MPP_PIXEL_YUV420P
#define ENCODED_VIDEO_WIDTH  100
#define ENCODED_VIDEO_HEIGHT 100
#else
#define ENCODED_VIDEO_FORMAT MPP_PIXEL_YUV420P
#define ENCODED_VIDEO_WIDTH  SRC_IMAGE_WIDTH
#define ENCODED_VIDEO_HEIGHT SRC_IMAGE_HEIGHT
#endif

/** Default priority for application tasks
   Tasks created by the application have a lower priority than pipeline tasks by default.
   Pipeline_task_max_prio in mpp_api_params_t structure should be adjusted with other application tasks.*/
#define APP_DEFAULT_PRIO        1

#if LWIP_IPV4 && LWIP_RAW && LWIP_SOCKET
static phy_handle_t phyHandle;
static struct netif netif;
#endif /* LWIP_IPV4 && LWIP_RAW && LWIP_SOCKET */

#if APP_CONFIG
#define ARG2STR(x) #x
#define CONFIG2STR(x) ARG2STR(x)
#define TC_NAME "test_camera_video_encode_config" CONFIG2STR(APP_CONFIG)
#else
#define TC_NAME "test_camera_video_encode"
#endif

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static void main_task(void *params);

/*******************************************************************************
 * Code
 ******************************************************************************/

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize)
{
    /* If the buffers to be provided to the Idle task are declared inside this
    function then they must be declared static - otherwise they will be allocated on
    the stack and so not exists after this function exits. */
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle task's
    state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task's stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
/*-----------------------------------------------------------*/

/* configUSE_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
application must provide an implementation of vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize)
{
    /* If the buffers to be provided to the Timer task are declared inside this
    function then they must be declared static - otherwise they will be allocated on
    the stack and so not exists after this function exits. */
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];

    /* Pass out a pointer to the StaticTask_t structure in which the Timer
    task's state will be stored. */
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

    /* Pass out the array that will be used as the Timer task's stack. */
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;

    /* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configTIMER_TASK_STACK_DEPTH is specified in words, not bytes. */
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

/*
 * @brief   Application entry point.
 */
int main(int argc, char *argv[]) {
    BaseType_t ret = pdFAIL;
    TaskHandle_t handle = NULL;

	/* Init board hardware. */
#ifndef EMULATOR
    BOARD_Init();
#endif /* i.MX RT1170 EVK */

    PRINTF("****** TEST test_camera_video_encode ******\r\n");
    PRINTF("\n");

	args_t *args = pvPortMalloc(sizeof(args_t));
    if (!args) {
        PRINTF("Allocation failed\n");
        goto err;
    }

    /* Set input arguments*/
#ifdef EMULATOR
    strcpy(args->display_name, "opencv0");
    if (argc == 3) {
        if (strcmp(argv[1], "rgb_sim") &&
            strcmp(argv[1], "ir_sim") &&
            strcmp(argv[1], "yuv_sim")) {
                goto err;
        }

        if (!strncmp(argv[2], "rgb",3)) {
            args->image_format = MPP_PIXEL_RGB;
        } else if (!strncmp(argv[2], "gray",3)) {
            args->image_format = MPP_PIXEL_GRAY;
        } else if (!strncmp(argv[2], "yuv",3)) {
            args->image_format = MPP_PIXEL_YUYV;
        } else {
            PRINTF("Invalid pixel format %s\n", argv[2]);
            goto err;
        }
        args->display_format = args->image_format;
    } else {
        PRINTF("Invalid argument number %d\n", argc);
        goto err;
    }
#else /* i.MX RT1170 EVK */
    strcpy(args->display_name, APP_DISPLAY_NAME);
    args->image_format = APP_DISPLAY_FORMAT;
#endif /* EMULATOR */

	ret = xTaskCreate(
			main_task,
			"main_task",
			configMINIMAL_STACK_SIZE + 2000,
			(void *) args,
			APP_DEFAULT_PRIO,
			&handle);

err:
	if (pdPASS != ret)
	{
		PRINTF("Failed to create main_task task");
		while (1);
	}

	vTaskStartScheduler();
	for (;;)
		vTaskSuspend(NULL);

	return 0;
}

int mpp_event_listener(mpp_t mpp, mpp_evt_t evt, void *evt_data, void *user_data) {

    checksum_data_t *chksm = (checksum_data_t *)evt_data;
    static bool test_done = false;
    static int chksm_time = 0;  /* time of checksum */
    static int cnt = 0;
    int time;

    switch(evt) {
    case MPP_EVENT_INTERNAL_TEST_RESERVED:
    case MPP_EVENT_INVALID:
    default:
        /* nothing to do */
        break;
    }
    return 0;
}

/* Set params for H.264 video encoder element */
static void set_h264_venc_params(mpp_element_params_t *elem_params)
{
    memset(elem_params, 0, sizeof(mpp_element_params_t));

    elem_params->encode.width = ENCODED_VIDEO_WIDTH;
    elem_params->encode.height = ENCODED_VIDEO_HEIGHT;
    elem_params->encode.fps = H264_ENCODER_FPS;
    elem_params->encode.format = ENCODED_VIDEO_FORMAT;
#if (SOURCE_STATIC_IMAGE == 1)
    elem_params->encode.intraframe = true;
#else
    elem_params->encode.intraframe = false;
#endif /* SOURCE_STATIC_IMAGE */

    /* H.264 encoder configuration parameters */
    elem_params->encode.intra_period = H264_ENCODER_INTRA_PERIOD;
    elem_params->encode.num_ref_frame = H264_ENCODER_NUM_REF_FRAME;
    elem_params->encode.rc_mode = H264_ENCODER_RC_MODE;
    elem_params->encode.target_bitrate = H264_ENCODER_TARGET_BITRATE;
    elem_params->encode.max_bitrate = H264_ENCODER_MAX_BITRATE;
    elem_params->encode.enable_frame_skip = H264_ENCODER_ENABLE_FRAME_SKIP;
    elem_params->encode.temporal_layer_num = H264_ENCODER_TEMPORAL_LAYER_NUM;
    elem_params->encode.profile_idc = H264_ENCODER_PROFILE_IDC;
    elem_params->encode.level_idc = H264_ENCODER_LEVEL_IDC;
    elem_params->encode.spatial_bitrate = H264_ENCODER_SPATIAL_BITRATE;
    elem_params->encode.max_spatial_bitrate = H264_ENCODER_MAX_SPATIAL_BITRATE;
    elem_params->encode.entropy_coding_mode = H264_ENCODER_ENTROPY_CODING_MODE;
}

/* Set params for image convert element */
static void set_img_convert_params(mpp_element_params_t *elem_params)
{
    memset(elem_params, 0, sizeof(mpp_element_params_t));

    elem_params->convert.dev_name = "gfx_CPU";
    elem_params->convert.out_buf.height = ENCODED_VIDEO_HEIGHT;
    elem_params->convert.out_buf.width  = ENCODED_VIDEO_WIDTH;
    /* pixel format */
    elem_params->convert.pixel_format = ENCODED_VIDEO_FORMAT;

    elem_params->convert.ops = MPP_CONVERT_COLOR;
}

/* Set params for image convert element (scaling) */
static void set_img_convert_params_only_scale(mpp_element_params_t *elem_params)
{
    memset(elem_params, 0, sizeof(mpp_element_params_t));

    elem_params->convert.dev_name = IMG_CONVERT_DEV_NAME;
    elem_params->convert.out_buf.height = ENCODED_VIDEO_HEIGHT;
    elem_params->convert.out_buf.width  = ENCODED_VIDEO_WIDTH;
    /* scaling parameters */
    elem_params->convert.scale.width  = ENCODED_VIDEO_WIDTH;
    elem_params->convert.scale.height = ENCODED_VIDEO_HEIGHT;

    elem_params->convert.ops = MPP_CONVERT_SCALE;
}

static void main_task(void *params) {
	args_t *args = (args_t *) params;
	int ret;
	bool stripe_mode = (APP_STRIPE_MODE > 0)? true : false;
    mpp_elem_handle_t convert_h = (mpp_elem_handle_t) NULL;

    PRINTF("[%s]\r\n", mpp_get_version());

    mpp_api_params_t api_param = {0};
#if ((defined APP_RC_CYCLE_INC) && (defined APP_RC_CYCLE_MIN))
    api_param.rc_cycle_inc = APP_RC_CYCLE_INC;
    api_param.rc_cycle_min = APP_RC_CYCLE_MIN;
#endif
    ret = mpp_api_init(&api_param);
    if (ret)
        goto err;

    /* Initialize Ethernet network interface */
    uint8_t ip_addr[4] = {configIP_ADDR0, configIP_ADDR1, configIP_ADDR2, configIP_ADDR3};
    uint8_t netmask[4] = {configNET_MASK0, configNET_MASK1, configNET_MASK2, configNET_MASK3};
    uint8_t gateway[4] = {configGW_ADDR0, configGW_ADDR1, configGW_ADDR2, configGW_ADDR3};

    mpp_eth_netif_init(ip_addr, netmask, gateway);

    int src_width, src_height;
#if (SOURCE_STATIC_IMAGE == 1)
    src_width = SRC_IMAGE_WIDTH;
    src_height = SRC_IMAGE_HEIGHT;
#else
    src_width = APP_CAMERA_WIDTH;
    src_height = APP_CAMERA_HEIGHT;
#endif /* SOURCE_STATIC_IMAGE */

	mpp_t mp;
	mpp_params_t mpp_params;
	memset(&mpp_params, 0, sizeof(mpp_params));
    mpp_params.evt_callback_f = &mpp_event_listener;
    mpp_params.mask = MPP_EVENT_ALL;
    mpp_params.cb_userdata = NULL;
    mpp_params.exec_flag = MPP_EXEC_RC;
	mp = mpp_create(&mpp_params, &ret);
	if (mp == MPP_INVALID)
		goto err;

#if (SOURCE_STATIC_IMAGE == 1)
    /*Set static image element parameters*/
	mpp_img_params_t img_params ;
	memset(&img_params, 0 , sizeof(img_params));
	img_params.height = SRC_IMAGE_HEIGHT;
	img_params.width =  SRC_IMAGE_WIDTH;
	img_params.format = SRC_IMAGE_FORMAT;
	img_params.stripe = false;
	ret = mpp_static_img_add(mp, &img_params, (void *) image_data, NULL);
	if (ret) {
		PRINTF("Failed to add static image");
		goto err;
	}
#else
    mpp_camera_params_t cam_params;
    memset(&cam_params, 0 , sizeof(cam_params));
    cam_params.height = APP_CAMERA_HEIGHT;
    cam_params.width =  APP_CAMERA_WIDTH;
    cam_params.format = APP_CAMERA_FORMAT;
    cam_params.fps    = 30;
    ret = mpp_camera_add(mp, s_camera_name, &cam_params, NULL);
    if (ret) {
        PRINTF("Failed to add camera %s\r\n", s_camera_name);
        goto err;
    }

    /* Add element convert for image scaling */
    mpp_element_params_t elem_params_scaling;
    set_img_convert_params_only_scale(&elem_params_scaling);
    ret = mpp_element_add(mp, MPP_ELEMENT_CONVERT, &elem_params_scaling, &convert_h);
    if (ret)
    {
        PRINTF("Failed to add element CONVERT - scale\n");
        goto err;
    }
#endif /* SOURCE_STATIC_IMAGE */

    /* Add element convert for format conversion */
    mpp_element_params_t elem_params_conversion;
    set_img_convert_params(&elem_params_conversion);
    ret = mpp_element_add(mp, MPP_ELEMENT_CONVERT, &elem_params_conversion, &convert_h);
    if (ret)
    {
        PRINTF("Failed to add element CONVERT - format conversion\n");
        goto err;
    }

    mpp_elem_handle_t mpp_elem_handle = (mpp_elem_handle_t) NULL;
    mpp_element_params_t venc_elem_params;
    set_h264_venc_params(&venc_elem_params);
    ret = mpp_element_add(mp, MPP_ELEMENT_VIDEO_ENCODE, &venc_elem_params, &mpp_elem_handle);
    if (ret ) {
        PRINTF("Failed to add video encode element\n");
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
    rtspsink_params.payload_type = RTP_PAYLOAD_TYPE_H264;
    /* rtspsink_params.frame_type doesn't matter for H.264
       as format lies directly inside the compressed SPS NAL unit  */
    ret = mpp_rtspsink_add(mp, &rtspsink_params, NULL);
    if (ret) {
        PRINTF("Failed to add RTSP sink\n");
        goto err;
    }

	ret = mpp_start(mp, 1, false);
	if (ret) {
		PRINTF("Failed to start pipeline\n");
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

