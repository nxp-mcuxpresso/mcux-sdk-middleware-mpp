/*
 * Copyright 2019, 2025-2026 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _HOST_VIDEO_H_
#define _HOST_VIDEO_H_

#ifdef USE_USB_CAMERA

#include "usb_host_config.h"
#include "usb_host.h"
#include "usb_host_video.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "app.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/* Number of USB video camera instances supported. Override in mpp_config.h if needed. */
#ifndef USB_HOST_VIDEO_INST_NUM
#define USB_HOST_VIDEO_INST_NUM 1
#endif
_Static_assert(USB_HOST_VIDEO_INST_NUM >= 1, "USB_HOST_VIDEO_INST_NUM must be at least 1");

/* Probe/commit control block sizes per UVC specification version.
 * UVC 1.0 defines a 22-byte short form (through dwMaxVideoFrameSize) and a
 * 26-byte full form (also including dwMaxPayloadTransferSize).
 * The host sends the short form on the initial SET_CUR and uses the full form
 * once the device has confirmed support. */
#define USB_HOST_VIDEO_PROBE_CTRL_SIZE_UVC10_SHORT  22U /* UVC 1.0: up to and including dwMaxVideoFrameSize */
#define USB_HOST_VIDEO_PROBE_CTRL_SIZE_UVC10        26U /* UVC 1.0: up to and including dwMaxPayloadTransferSize */

#define MATCH_FORMAT_ANY          0
#define MATCH_FORMAT_MJPEG        1
#define MATCH_FORMAT_UNCOMPRESSED 2

#define FULL_SPEED_ISO_MAX_PACKET_SIZE               (1023U)
#define HIGH_SPEED_ISO_MAX_PACKET_SIZE_ZERO_ADDITION (1024U)

#define USB_VIDEO_PICTURE_COUNT (500U)
/* The prime count - value should not exceed 6 due to USB dedicated RAM limitations */
#define USB_VIDEO_STREAM_BUFFER_COUNT (6U)
/* Approximate MJPEG compression ratio of device camera; 100 means 100% (no compression assumed for buffer sizing) */
#define USB_MJPEG_COMPRESSION_RATIO (100U)

#define CAMERA_USB_MAX_WIDTH  640
#define CAMERA_USB_MAX_HEIGHT 480
#define CAMERA_USB_MAX_BPP    2 /* YUYV */
#define CAMERA_DEV_BUFFER_ALIGN    64
#define CAMERA_USB_RAW_BUFF_SIZE   (CAMERA_USB_MAX_WIDTH * CAMERA_USB_MAX_HEIGHT * CAMERA_USB_MAX_BPP)
#define CAMERA_USB_JPEG_BUFF_SIZE  ((CAMERA_USB_RAW_BUFF_SIZE * USB_MJPEG_COMPRESSION_RATIO) / 100)

/* Add extra ISO packet size to avoid out-of-bounds on last packet */
#if (MATCH_FORMAT == MATCH_FORMAT_MJPEG)
#define CAMERA_USB_MAX_BUFF_SIZE (CAMERA_USB_JPEG_BUFF_SIZE + HIGH_SPEED_ISO_MAX_PACKET_SIZE_ZERO_ADDITION)
#else
#define CAMERA_USB_MAX_BUFF_SIZE (CAMERA_USB_RAW_BUFF_SIZE + HIGH_SPEED_ISO_MAX_PACKET_SIZE_ZERO_ADDITION)
#endif

/* Number of ping-pong picture buffers per stream instance */
#define USB_HOST_VIDEO_PIC_BUFFER_NUM 2

#define USB_HOST_VIDEO_INVALID_PIC_BUFFER_IDX 0xFFU

/*! @brief Host application run state */
typedef enum _usb_host_vidio_run_state
{
    kUSB_HostVideoRunIdle = 0,
    kUSB_HostVideoRunSetControlInterface,
    kUSB_HostVideoRunSetControlInterfaceDone,
    kUSB_HostVideoRunWaitSetControlInterface,
    kUSB_HostVideoRunWaitSetStreamInterface,
    kUSB_HostVideoRunSetInterfaceDone,
    kUSB_HostVideoRunFindOptimalSetting,
    kUSB_HostVideoRunGetSetProbeCommit,
    kUSB_HostVideoRunWaitGetSetProbeCommit,
    kUSB_HostVideoRunGetSetProbeCommitDone,
    kUSB_HostVideoStart,
    kUSB_HostVideoRunVideoDone,
} usb_host_video_camera_run_state_t;

/*! @brief Picture receive state machine */
typedef enum
{
    HOST_VIDEO_PIC_RX_STATE_NORMAL,
    HOST_VIDEO_PIC_RX_STATE_DIS_ONE_FRAME,
    HOST_VIDEO_PIC_RX_STATE_NO_BUF_AVAIL,
    HOST_VIDEO_PIC_RX_STATE_INVALID,
} usb_host_video_pic_rx_state_t;

/*! @brief A single picture frame buffer descriptor */
typedef struct _usb_host_video_camera_instance_buf
{
    uint8_t  *buffer;          /* pointer to frame data memory */
    uint32_t  size;            /* capacity of the buffer in bytes */
    volatile uint32_t dataLen; /* valid data length; written by ISR, cleared by HAL (volatile for strict C compliance) */
    uint32_t  ID;              /* frame ID from USB payload header */
    uint32_t  timeStamp;       /* presentation time stamp */
} usb_host_video_camera_instance_buf_t;

/*! @brief Ping-pong picture buffer set for one stream instance */
typedef struct _usb_host_video_camera_instance_pic_buf
{
    usb_host_video_camera_instance_buf_t bufs[USB_HOST_VIDEO_PIC_BUFFER_NUM];
    uint32_t curBufIdx; /* index of the buffer currently being filled */
} usb_host_video_camera_instance_pic_buf_t;

/*! @brief USB host video camera instance */
typedef struct _usb_host_video_camera_instance
{
    usb_device_handle deviceHandle;
    usb_host_class_handle classHandle;
    usb_host_interface_handle controlIntfHandle;
    usb_host_interface_handle streamIntfHandle;
    usb_host_video_stream_payload_format_common_desc_t      *videoStreamFormatDescriptor;
    usb_host_video_stream_payload_mjpeg_format_desc_t       *videoStreamMjpegFormatDescriptor;
    usb_host_video_stream_payload_uncompressed_format_desc_t *videoStreamUncompressedFormatDescriptor;
    usb_host_video_stream_payload_frame_common_desc_t       *videoStreamFrameDescriptor;
    usb_host_video_stream_payload_mjpeg_frame_desc_t        *videoStreamMjpegFrameDescriptor;
    usb_host_video_stream_payload_uncompressed_frame_desc_t *videoStreamUncompressedFrameDescriptor;
    uint32_t  videoCameraPictureBufferSize;
    volatile uint32_t discardPictureCount;
    uint16_t  unMultipleIsoMaxPacketSize;
    uint8_t   cameraDeviceFormatType;
    uint8_t   devState;
    uint8_t   prevState;
    uint8_t   runState;
    uint8_t   runWaitState;
    volatile uint8_t isControlTransferring;
    uint8_t   currentStreamInterfaceAlternate;
    volatile uint8_t streamBufferIndex;
    uint8_t   step;

    /* Target resolution used when negotiating with the camera */
    uint32_t  targetWidth;
    uint32_t  targetHeight;

    usb_host_video_pic_rx_state_t           picRXState;
    usb_host_video_camera_instance_pic_buf_t pictureBuffer;

    /* Queue to deliver completed frames to the MPP HAL layer */
    QueueHandle_t mppQueue;

    /* Pointer to the USB RX stream buffers for this instance */
    uint8_t  *streamBuffer;
} usb_host_video_camera_instance_t;

/*! @brief Message sent from the USB ISR callback to the MPP HAL via mppQueue */
typedef struct usb_frame_ready_msg
{
    uint8_t  *pictureBuffer;      /* pointer to the frame data */
    uint32_t  pictureBufferIndex; /* index into pictureBuffer.bufs[] - used for release on Enqueue */
    uint32_t  pictureLength;      /* valid frame data length in bytes */
} usb_frame_ready_msg_t;

/* Global instance array - indexed by camera instance number */
extern usb_host_video_camera_instance_t g_Videos[USB_HOST_VIDEO_INST_NUM];

/*******************************************************************************
 * API
 ******************************************************************************/

/*!
 * @brief USB host video task - runs the USB state machine for one instance.
 *
 * @param param  pointer to usb_host_video_camera_instance_t
 */
extern void USB_HostVideoTask(void *param);

/*!
 * @brief USB host event callback - called by the USB host middleware.
 */
extern usb_status_t USB_HostVideoEvent(usb_device_handle deviceHandle,
                                       usb_host_configuration_handle configurationHandle,
                                       uint32_t eventCode);

/*!
 * @brief SD card initialization helper (no-op when USE_SD is not defined).
 */
extern usb_status_t USB_HostVideoAppSDcardInit(void);

/*!
 * @brief Stop the ISO stream for a single instance before tearing down the host.
 *
 * Sends SET_INTERFACE(alternateSetting=0) to close the ISO pipe for the given
 * instance, which allows USB_HostDeinit to complete without blocking on
 * pending transfers. The caller must set inst->devState to kStatus_DEV_Idle
 * BEFORE calling this function so that ITD cancellation callbacks do not
 * re-queue new transfers onto the closing pipe.
 * Safe to call even when no device is attached.
 *
 * @param inst  pointer to the video camera instance to stop
 */
extern void USB_HostVideoAppStopStream(usb_host_video_camera_instance_t *inst);

/*!
 * @brief Release a picture buffer back to the USB video instance.
 *
 * Called by the HAL Enqueue path to signal that MPP has finished using the frame.
 *
 * @param inst        the video camera instance that owns the buffer
 * @param bufferIndex index into inst->pictureBuffer.bufs[]
 */
extern void USB_HostVideoReleaseBuffer(usb_host_video_camera_instance_t *inst, uint32_t bufferIndex);

#endif /* USE_USB_CAMERA */

#endif /* _HOST_VIDEO_H_ */
