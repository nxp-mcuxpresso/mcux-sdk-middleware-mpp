/*
 * Copyright 2019, 2025-2026 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * USB host video driver implementation.
 *
 * This file is shared across all boards that use the USB camera HAL.
 * It implements the multi-stream USB video state machine and delivers
 * completed frames to the MPP pipeline via each instance's mppQueue.
 *
 * Design notes:
 *  - g_Videos[] holds up to USB_HOST_VIDEO_INST_NUM stream instances.
 *    USB_HOST_VIDEO_INST_NUM defaults to 1 and can be overridden in mpp_config.h.
 *  - Static frame buffers are allocated for all USB_HOST_VIDEO_INST_NUM instances.
 *    Memory placement is selected by USE_UNCACHED_JPG_BUFFERS, USE_PSRAM_JPG_BUFFERS,
 *    or USB_IMG_BUFFER_DYNALLOC (see host_video.c buffer section for details).
 *  - On frame complete the ISR callback sends a usb_frame_ready_msg_t to
 *    inst->mppQueue which is consumed by HAL_CameraDev_USB_Dequeue().
 *  - The frame buffer is released back (dataLen set to 0) by the HAL on
 *    Enqueue(), which maps to USB_HostVideoReleaseBuffer().
 */

#include "usb_host_config.h"
#include "usb_host.h"
#include "usb_host_video.h"

#if USE_SD
#include "fsl_sd.h"
#include "ff.h"
#include "diskio.h"
#include "fsl_sd_disk.h"
#endif

#include "hal_debug.h"

#ifdef USE_USB_CAMERA

#include "mpp_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include <string.h>
#include <stdlib.h>

#include "host_video.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

#ifndef ENABLE_FPS_LOG
#define ENABLE_FPS_LOG 0
#endif

#define HOST_VIDEO_USB_RX_BUF_SIZE (1024U)

/* Align picture frame buffer length to 1 kB boundaries as required by JPEG decoder */
#define PING_PONG_BUF_LENGTH_ALIGN(len, align) (((len) + (align) - 1U) / (align) * (align))

#ifndef APP_CAMERA_FPS
#define APP_CAMERA_FPS 30
#endif

/*******************************************************************************
 * Variables
 ******************************************************************************/

/* Global video instance array */
usb_host_video_camera_instance_t g_Videos[USB_HOST_VIDEO_INST_NUM];

/* USB RX stream buffers - one row per instance */
USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
static uint8_t s_streamBuffers[USB_HOST_VIDEO_INST_NUM]
                               [USB_VIDEO_STREAM_BUFFER_COUNT * HOST_VIDEO_USB_RX_BUF_SIZE];

/*
 * Static picture frame buffers.
 * Each instance has USB_HOST_VIDEO_PIC_BUFFER_NUM ping-pong slots.
 * Buffer length is aligned to 1 kB as required by the JPEG decoder.
 *
 * Memory placement is controlled by the following macros (set in mpp_config.h):
 *   USB_IMG_BUFFER_DYNALLOC  - skip static allocation entirely (caller provides buffers)
 *   USE_UNCACHED_JPG_BUFFERS - place buffers in non-cacheable SRAM (DMA safe without flush)
 *   USE_PSRAM_JPG_BUFFERS    - use PSRAM at fixed addresses (single-instance only)
 *   (default)                - cacheable SRAM with __ALIGNED placement
 */
#ifndef USB_IMG_BUFFER_DYNALLOC
#ifdef USE_UNCACHED_JPG_BUFFERS
AT_NONCACHEABLE_SECTION_ALIGN(
    static uint8_t s_picFrameBuffers[USB_HOST_VIDEO_INST_NUM][USB_HOST_VIDEO_PIC_BUFFER_NUM]
        [PING_PONG_BUF_LENGTH_ALIGN(CAMERA_USB_MAX_BUFF_SIZE, 1024U)],
    CAMERA_DEV_BUFFER_ALIGN);
#elif defined(USE_PSRAM_JPG_BUFFERS)
/*
 * PSRAM-backed ping-pong buffers.
 * Two PSRAM regions are defined by JPEG_BUFF0_ADDR and JPEG_BUFF1_ADDR.
 * Instance 0 uses JPEG_BUFF0_ADDR as base; USB_HOST_VIDEO_PIC_BUFFER_NUM
 * buffers of CAMERA_USB_MAX_BUFF_SIZE bytes are placed contiguously starting
 * there and must all fit before JPEG_BUFF1_ADDR.
 * Instance 1 uses JPEG_BUFF1_ADDR as base in the same manner.
 * A maximum of 2 instances is supported with PSRAM addressing.
 */
static const uintptr_t s_psramBase[2] = { JPEG_BUFF0_ADDR, JPEG_BUFF1_ADDR };
/* Compile-time guard: each instance must fit within its PSRAM region without
 * overlapping the next. JPEG_BUFF1_ADDR must be at least
 * USB_HOST_VIDEO_PIC_BUFFER_NUM * CAMERA_USB_MAX_BUFF_SIZE above JPEG_BUFF0_ADDR. */
_Static_assert(
    (JPEG_BUFF1_ADDR - JPEG_BUFF0_ADDR) >=
        ((uintptr_t)USB_HOST_VIDEO_PIC_BUFFER_NUM * (uintptr_t)CAMERA_USB_MAX_BUFF_SIZE),
    "PSRAM region for instance 0 overlaps instance 1: increase JPEG_BUFF1_ADDR or reduce CAMERA_USB_MAX_BUFF_SIZE");
#else
__ALIGNED(CAMERA_DEV_BUFFER_ALIGN)
static uint8_t s_picFrameBuffers[USB_HOST_VIDEO_INST_NUM][USB_HOST_VIDEO_PIC_BUFFER_NUM]
    [PING_PONG_BUF_LENGTH_ALIGN(CAMERA_USB_MAX_BUFF_SIZE, 1024U)];
#endif
#endif /* USB_IMG_BUFFER_DYNALLOC */

/* USB probe/commit negotiation working buffers */
USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
static usb_host_video_probe_commit_controls_t s_VideoProbe;
USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
static usb_host_video_probe_commit_controls_t s_Probe[3];

/* GUID constants for uncompressed format identification */
static const uint8_t k_YUY2FormatGUID[16] = {0x59, 0x55, 0x59, 0x32, 0x00, 0x00, 0x10, 0x00,
                                              0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71};
static const uint8_t k_NV12FormatGUID[16] = {0x4E, 0x56, 0x31, 0x32, 0x00, 0x00, 0x10, 0x00,
                                              0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71};

#if ENABLE_FPS_LOG
static uint32_t          s_curTime[USB_HOST_VIDEO_INST_NUM];
static uint32_t          s_totalTime4Frames[USB_HOST_VIDEO_INST_NUM];
static uint32_t          s_totalLen4Frames[USB_HOST_VIDEO_INST_NUM];
static uint32_t          s_frameNumbers[USB_HOST_VIDEO_INST_NUM];
static uint32_t          s_maxFrameLen[USB_HOST_VIDEO_INST_NUM];
/* ISR frame counters and length accumulators - updated in
 * USB_HostVideoStreamDataInCallback on each successfully queued frame.
 * Read from the task side to measure FPS and frame sizes without
 * touching mppQueue. */
static volatile uint32_t s_isrFrameCount[USB_HOST_VIDEO_INST_NUM];
static volatile uint32_t s_isrTotalLen[USB_HOST_VIDEO_INST_NUM];
static volatile uint32_t s_isrMaxLen[USB_HOST_VIDEO_INST_NUM];
static uint32_t          s_lastIsrFrameCount[USB_HOST_VIDEO_INST_NUM];

/* Provide a millisecond tick if not supplied by the board */
#ifndef getTime
static inline uint32_t getTime(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}
#endif
#endif /* ENABLE_FPS_LOG */

/*******************************************************************************
 * Forward declarations
 ******************************************************************************/

static void USB_HostVideoCommandCallback(void *param, uint8_t *data, uint32_t dataLen, usb_status_t status);
static void USB_HostVideoControlCallback(void *param, uint8_t *data, uint32_t dataLen, usb_status_t status);
static void USB_HostVideoStreamDataInCallback(void *param, uint8_t *data, uint32_t dataLen, usb_status_t status);

/*******************************************************************************
 * Helper utilities
 ******************************************************************************/

/* Return the index of inst inside g_Videos[], or USB_HOST_VIDEO_INST_NUM if not found */
static uint32_t USB_HostVideoInstance2Index(const usb_host_video_camera_instance_t *inst)
{
    for (uint32_t i = 0U; i < (uint32_t)USB_HOST_VIDEO_INST_NUM; i++)
    {
        if (&g_Videos[i] == inst)
        {
            return i;
        }
    }
    return (uint32_t)USB_HOST_VIDEO_INST_NUM;
}

/* Find a free (idle) instance slot, or NULL if all are occupied */
static usb_host_video_camera_instance_t *USB_HostVideoGetFreeVideoInstance(void)
{
    for (int i = 0; i < USB_HOST_VIDEO_INST_NUM; i++)
    {
        if (g_Videos[i].devState == kStatus_DEV_Idle)
        {
            return &g_Videos[i];
        }
    }
    return NULL;
}

/* Find the instance that owns the given device handle, or NULL */
static usb_host_video_camera_instance_t *USB_HostVideoDeviceHandle2VideoInstance(usb_device_handle dev_handle)
{
    for (int i = 0; i < USB_HOST_VIDEO_INST_NUM; i++)
    {
        if (g_Videos[i].deviceHandle == dev_handle)
        {
            return &g_Videos[i];
        }
    }
    return NULL;
}

/* Reset all picture buffer descriptors for an instance */
static void USB_HostVideoInstanceDeinitBuffer(usb_host_video_camera_instance_t *inst)
{
    for (int i = 0; i < USB_HOST_VIDEO_PIC_BUFFER_NUM; i++)
    {
        inst->pictureBuffer.bufs[i].dataLen   = 0U;
        inst->pictureBuffer.bufs[i].ID        = 0U;
        inst->pictureBuffer.bufs[i].size      = 0U;
        inst->pictureBuffer.bufs[i].timeStamp = 0U;
    }
    inst->pictureBuffer.curBufIdx = USB_HOST_VIDEO_INVALID_PIC_BUFFER_IDX;
}

/* Initialise picture buffer descriptors pointing at the static storage */
static int USB_HostVideoInstanceInitBuffer(usb_host_video_camera_instance_t *inst)
{
    uint32_t idx = USB_HostVideoInstance2Index(inst);

    if (idx >= (uint32_t)USB_HOST_VIDEO_INST_NUM)
    {
        HAL_LOGE("No static picture buffer for instance %d\r\n", (int)idx);
        return -1;
    }

#ifdef USB_IMG_BUFFER_DYNALLOC
    /* Dynamic allocation: caller is responsible for assigning inst->pictureBuffer.bufs[].buffer */
    if (inst->pictureBuffer.bufs[0].buffer == NULL)
    {
        HAL_LOGE("Dynamic buffer not assigned for instance %d\r\n", (int)idx);
        return -1;
    }
#elif defined(USE_PSRAM_JPG_BUFFERS)
    if (idx >= 2U)
    {
        HAL_LOGE("PSRAM addressing supports only 2 instances (got %d)\r\n", (int)idx);
        return -1;
    }
    for (int i = 0; i < USB_HOST_VIDEO_PIC_BUFFER_NUM; i++)
    {
        inst->pictureBuffer.bufs[i].buffer  = (uint8_t *)s_psramBase[idx] +
                                              (uint32_t)i * CAMERA_USB_MAX_BUFF_SIZE;
        inst->pictureBuffer.bufs[i].size    = PING_PONG_BUF_LENGTH_ALIGN(CAMERA_USB_MAX_BUFF_SIZE, 1024U);
        inst->pictureBuffer.bufs[i].dataLen = 0U;
        HAL_LOGD("picture buf[%d] size:%u addr:%p\r\n",
                 i,
                 (unsigned)inst->pictureBuffer.bufs[i].size,
                 inst->pictureBuffer.bufs[i].buffer);
    }
#else
    for (int i = 0; i < USB_HOST_VIDEO_PIC_BUFFER_NUM; i++)
    {
        inst->pictureBuffer.bufs[i].buffer  = s_picFrameBuffers[idx][i];
        inst->pictureBuffer.bufs[i].size    = sizeof(s_picFrameBuffers[idx][i]);
        inst->pictureBuffer.bufs[i].dataLen = 0U;
        HAL_LOGD("picture buf[%d] size:%u addr:%p\r\n",
                 i,
                 (unsigned)inst->pictureBuffer.bufs[i].size,
                 inst->pictureBuffer.bufs[i].buffer);
    }
#endif /* USB_IMG_BUFFER_DYNALLOC */

    inst->pictureBuffer.curBufIdx = 0U;
    return 0;
}

/* Clear the data in a frame buffer slot so it can be reused */
static void USB_HostVideoInstanceCleanDataInBuffer(usb_host_video_camera_instance_buf_t *buf)
{
    if (buf != NULL)
    {
        buf->ID      = 0U;
        buf->dataLen = 0U;
        buf->timeStamp = 0U;
    }
}

/*
 * Accumulate an incoming USB packet into the current picture buffer.
 *
 * Returns a pointer to the completed buffer when a full frame has been received,
 * or NULL if the frame is still being assembled (or was discarded).
 */
static usb_host_video_camera_instance_buf_t *USB_HostVideoInstancePushDataToBuffer(
    usb_host_video_camera_instance_t *inst,
    uint8_t *data, uint32_t len,
    uint32_t frameID, uint32_t timeStamp,
    uint8_t endOfFrame, uint8_t error)
{
    uint32_t curIdx  = inst->pictureBuffer.curBufIdx;
    uint32_t nextIdx = (curIdx + 1U) % USB_HOST_VIDEO_PIC_BUFFER_NUM;
    usb_host_video_camera_instance_buf_t *curBuf  = &inst->pictureBuffer.bufs[curIdx];
    usb_host_video_camera_instance_buf_t *nextBuf = &inst->pictureBuffer.bufs[nextIdx];

    switch (inst->picRXState)
    {
        case HOST_VIDEO_PIC_RX_STATE_NORMAL:
            if ((curBuf->dataLen > 0U) && (curBuf->ID == frameID))
            {
                /* Validate before appending */
                if ((curBuf->dataLen + len > curBuf->size) ||
                    ((curBuf->timeStamp != 0xFFFFFFFFUL) && (curBuf->timeStamp != timeStamp)) ||
                    (error != 0U))
                {
                    curBuf->dataLen   = 0U;
                    curBuf->ID        = 0U;
                    curBuf->timeStamp = 0U;
                    if (endOfFrame)
                    {
                        inst->discardPictureCount++;
                    }
                    else
                    {
                        inst->picRXState = HOST_VIDEO_PIC_RX_STATE_DIS_ONE_FRAME;
                    }
                }
                else
                {
                    /* Good packet - append */
                    goto append_data;
                }
            }
            else
            {
                /* First packet of a new frame */
                curBuf->dataLen = 0U;
                if (len > 0U)
                {
                    curBuf->ID        = frameID;
                    curBuf->timeStamp = timeStamp;
                    goto append_data;
                }
                /* Zero-length first packet - nothing to do */
            }
            break;

append_data:
            memcpy(&curBuf->buffer[curBuf->dataLen], data, len);
            curBuf->dataLen += len;

            if (endOfFrame)
            {
                /* Switch to next buffer and return the completed frame */
                inst->pictureBuffer.curBufIdx = nextIdx;
                if (nextBuf->dataLen > 0U)
                {
                    inst->picRXState = HOST_VIDEO_PIC_RX_STATE_NO_BUF_AVAIL;
                }
                return curBuf;
            }
            break;

        case HOST_VIDEO_PIC_RX_STATE_DIS_ONE_FRAME:
            if (endOfFrame)
            {
                curBuf->dataLen   = 0U;
                curBuf->ID        = 0U;
                curBuf->timeStamp = 0U;
                inst->picRXState  = HOST_VIDEO_PIC_RX_STATE_NORMAL;
                inst->discardPictureCount++;
            }
            break;

        case HOST_VIDEO_PIC_RX_STATE_NO_BUF_AVAIL:
            if (endOfFrame)
            {
                inst->discardPictureCount++;
                if (curBuf->dataLen == 0U)
                {
                    inst->picRXState = HOST_VIDEO_PIC_RX_STATE_NORMAL;
                }
            }
            break;

        default:
            /* Should never happen */
            break;
    }
    return NULL;
}

/*
 * Select the best ISO alternate setting for the requested max packet size.
 */
static usb_status_t USB_HostGetProperEndpointInterfaceInfo(usb_host_class_handle classHandle,
                                                           uint16_t expectMaxPacketSize,
                                                           uint8_t *interfaceAlternate,
                                                           uint16_t *videoIsoPacketSize)
{
    usb_host_video_instance_struct_t *videoInstance = (usb_host_video_instance_struct_t *)classHandle;
    usb_host_interface_t             *streamInterface;
    usb_host_video_descriptor_union_t descriptor;
    uint16_t epTransactionsPerMicroFrame = 0U;
    uint16_t epMaxPacketSize             = 0U;
    uint16_t length                      = 0U;
    uint8_t  epCount                     = 0U;
    uint8_t  alternate                   = 0U;

    streamInterface = (usb_host_interface_t *)videoInstance->streamIntfHandle;
    if (NULL == streamInterface)
    {
        return kStatus_USB_InvalidHandle;
    }
    if (NULL == streamInterface->interfaceDesc)
    {
        return kStatus_USB_InvalidParameter;
    }

    descriptor.bufr = streamInterface->interfaceExtension;
    length          = 0U;
    while (length < streamInterface->interfaceExtensionLength)
    {
        if (descriptor.common->bDescriptorType == USB_DESCRIPTOR_TYPE_INTERFACE)
        {
            alternate = descriptor.interface->bAlternateSetting;
            epCount   = descriptor.interface->bNumEndpoints;
            while (epCount)
            {
                if (descriptor.endpoint->bDescriptorType == USB_DESCRIPTOR_TYPE_ENDPOINT)
                {
                    epTransactionsPerMicroFrame =
                        (((USB_SHORT_FROM_LITTLE_ENDIAN_DATA(descriptor.endpoint->wMaxPacketSize)) &
                          USB_DESCRIPTOR_ENDPOINT_MAXPACKETSIZE_MULT_TRANSACTIONS_MASK) >>
                         USB_DESCRIPTOR_ENDPOINT_MAXPACKETSIZE_MULT_TRANSACTIONS_SHFIT) +
                        1U;
                    epMaxPacketSize =
                        (USB_SHORT_FROM_LITTLE_ENDIAN_DATA(descriptor.endpoint->wMaxPacketSize)) &
                        USB_DESCRIPTOR_ENDPOINT_MAXPACKETSIZE_SIZE_MASK;
                    if (((epMaxPacketSize * epTransactionsPerMicroFrame) <= expectMaxPacketSize) &&
                        (*videoIsoPacketSize < (epMaxPacketSize * epTransactionsPerMicroFrame)))
                    {
                        *videoIsoPacketSize = epMaxPacketSize * epTransactionsPerMicroFrame;
                        *interfaceAlternate = alternate;
                    }
                    epCount--;
                }
                length += descriptor.common->bLength;
                descriptor.bufr += descriptor.common->bLength;
            }
        }
        else
        {
            length += descriptor.common->bLength;
            descriptor.bufr += descriptor.common->bLength;
        }
    }

    if ((0U == *videoIsoPacketSize) || (0U == *interfaceAlternate))
    {
        return kStatus_USB_Error;
    }
    return kStatus_USB_Success;
}

/*******************************************************************************
 * USB callbacks
 ******************************************************************************/

static void USB_HostVideoCommandCallback(void *param, uint8_t *data, uint32_t dataLen, usb_status_t status)
{
    usb_host_video_camera_instance_t *videoInstance = (usb_host_video_camera_instance_t *)param;
    videoInstance->isControlTransferring = 0U;
}

static void USB_HostVideoControlCallback(void *param, uint8_t *data, uint32_t dataLen, usb_status_t status)
{
    usb_host_video_camera_instance_t *videoInstance = (usb_host_video_camera_instance_t *)param;
    if (status != kStatus_USB_Success)
    {
        HAL_LOGE("data transfer error, status = %d\r\n", status);
        return;
    }

    if (videoInstance->runState == kUSB_HostVideoRunIdle)
    {
        if (videoInstance->runWaitState == kUSB_HostVideoRunSetControlInterface)
        {
            videoInstance->runState = kUSB_HostVideoRunSetControlInterfaceDone;
        }
        else if (videoInstance->runWaitState == kUSB_HostVideoRunWaitSetStreamInterface)
        {
            videoInstance->runState = kUSB_HostVideoRunSetInterfaceDone;
        }
        else if (videoInstance->runWaitState == kUSB_HostVideoRunWaitGetSetProbeCommit)
        {
            videoInstance->runState = kUSB_HostVideoRunGetSetProbeCommitDone;
        }
    }
}

/*
 * ISO IN callback - called from USB IRQ context for every received USB packet.
 *
 * Parses the UVC payload header, accumulates the frame data and, when a
 * complete frame is available, sends a usb_frame_ready_msg_t to inst->mppQueue
 * for consumption by HAL_CameraDev_USB_Dequeue().
 */
static void USB_HostVideoStreamDataInCallback(void *param, uint8_t *data, uint32_t dataLen, usb_status_t status)
{
    usb_host_video_camera_instance_t *videoAppInstance = (usb_host_video_camera_instance_t *)param;
    uint32_t headLength;
    int      dataSize;
    uint8_t  endOfFrame;
    uint8_t  error;
    uint8_t  ptsExist;
    uint8_t  frame_id;
    uint32_t presentationTime;
    usb_host_video_camera_instance_buf_t *completedBuf;

    uint8_t *streamBuffer =
        &videoAppInstance->streamBuffer[videoAppInstance->streamBufferIndex * HOST_VIDEO_USB_RX_BUF_SIZE];

    if (videoAppInstance->devState != kStatus_DEV_Attached)
    {
        goto submit_buffer;
    }

    if (dataLen < sizeof(usb_host_video_payload_header_t))
    {
        goto submit_buffer;
    }

    endOfFrame = ((usb_host_video_payload_header_t *)streamBuffer)->HeaderInfo.bitMap.end_of_frame;
    frame_id   = ((usb_host_video_payload_header_t *)streamBuffer)->HeaderInfo.bitMap.frame_id;
    error      = ((usb_host_video_payload_header_t *)streamBuffer)->HeaderInfo.bitMap.error;
    ptsExist   = ((usb_host_video_payload_header_t *)streamBuffer)->HeaderInfo.bitMap.presentation_time;

    if (ptsExist)
    {
        presentationTime = USB_LONG_FROM_LITTLE_ENDIAN_ADDRESS(
            ((usb_host_video_payload_header_t *)streamBuffer)->dwPresentationTime);
    }
    else
    {
        presentationTime = 0xFFFFFFFFUL;
    }

    headLength = ((usb_host_video_payload_header_t *)streamBuffer)->bHeaderLength;
    dataSize   = (int)dataLen - (int)headLength;
    if (dataSize < 0)
    {
        goto submit_buffer;
    }

    completedBuf = USB_HostVideoInstancePushDataToBuffer(
        videoAppInstance,
        &streamBuffer[headLength],
        (uint32_t)dataSize,
        frame_id,
        presentationTime,
        endOfFrame,
        error);

    if (completedBuf != NULL)
    {
        /* Determine which slot index this buffer occupies */
        uint32_t bufIdx = USB_HOST_VIDEO_INVALID_PIC_BUFFER_IDX;
        for (uint32_t i = 0U; i < USB_HOST_VIDEO_PIC_BUFFER_NUM; i++)
        {
            if (&videoAppInstance->pictureBuffer.bufs[i] == completedBuf)
            {
                bufIdx = i;
                break;
            }
        }

        /* Send frame-ready notification to the MPP HAL dequeue path */
        usb_frame_ready_msg_t frameReadyMsg;
        frameReadyMsg.pictureBuffer      = completedBuf->buffer;
        frameReadyMsg.pictureBufferIndex = bufIdx;
        frameReadyMsg.pictureLength      = completedBuf->dataLen;

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        if ((videoAppInstance->mppQueue == NULL) || 
            (xQueueSendFromISR(videoAppInstance->mppQueue, 
                               &frameReadyMsg, 
                               &xHigherPriorityTaskWoken) == pdFALSE))
        {
            /* MPP queue is full - drop the frame and release the buffer */
            USB_HostVideoInstanceCleanDataInBuffer(completedBuf);
            videoAppInstance->discardPictureCount++;
        }
#if ENABLE_FPS_LOG
        else
        {
            /* Count successfully queued frames and accumulate length stats
             * for task-side FPS and frame-size computation. */
            uint32_t instIdx = USB_HostVideoInstance2Index(videoAppInstance);
            if (instIdx < (uint32_t)USB_HOST_VIDEO_INST_NUM)
            {
                uint32_t flen = completedBuf->dataLen;
                s_isrFrameCount[instIdx]++;
                s_isrTotalLen[instIdx] += flen;
                if (flen > s_isrMaxLen[instIdx])
                {
                    s_isrMaxLen[instIdx] = flen;
                }
            }
        }
#endif

        if (xHigherPriorityTaskWoken)
        {
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }

submit_buffer:
    /* Re-arm the USB receive for this stream slot */
    USB_HosVideoStreamRecv(
        videoAppInstance->classHandle,
        streamBuffer,
        (HOST_VIDEO_USB_RX_BUF_SIZE / videoAppInstance->unMultipleIsoMaxPacketSize) *
            videoAppInstance->unMultipleIsoMaxPacketSize,
        USB_HostVideoStreamDataInCallback,
        videoAppInstance);

    videoAppInstance->streamBufferIndex =
        (videoAppInstance->streamBufferIndex + 1U) % USB_VIDEO_STREAM_BUFFER_COUNT;
}

/*******************************************************************************
 * Public API
 ******************************************************************************/

void USB_HostVideoReleaseBuffer(usb_host_video_camera_instance_t *inst, uint32_t bufferIndex)
{
    if ((inst != NULL) && (bufferIndex < USB_HOST_VIDEO_PIC_BUFFER_NUM))
    {
        USB_HostVideoInstanceCleanDataInBuffer(&inst->pictureBuffer.bufs[bufferIndex]);
    }
}

/*
 * USB host video task.
 *
 * Implements the UVC negotiation state machine and, once streaming starts,
 * handles the FPS logging by processing FPS accounting on each completed frame.
 * The actual frame delivery to MPP happens in the ISR callback
 * (USB_HostVideoStreamDataInCallback).
 *
 * This function is intended to be called in a loop from a dedicated FreeRTOS task.
 */
void USB_HostVideoTask(void *param)
{
    static usb_status_t status = kStatus_USB_Success;
    usb_host_video_stream_payload_frame_common_desc_t *frameDesc = NULL;
    usb_host_video_camera_instance_t *videoAppInstance           = (usb_host_video_camera_instance_t *)param;
    uint32_t index, count;
    uint32_t minResolution        = 0xFFFFFFFFUL;
    uint8_t  minSolutionFrameIndex = 0xFFU;
    uint32_t frameInterval        = 0U;
    uint32_t resolution           = 0U;
    uint32_t speed                = 0U;
    uint8_t  i                    = 0U;
    uint32_t idx                  = USB_HostVideoInstance2Index(videoAppInstance);

    /* ------------------------------------------------------------------ */
    /* Device state transitions                                             */
    /* ------------------------------------------------------------------ */
    if (videoAppInstance->devState != videoAppInstance->prevState)
    {
        videoAppInstance->prevState = videoAppInstance->devState;

        switch (videoAppInstance->devState)
        {
            case kStatus_DEV_Idle:
                break;

            case kStatus_DEV_Attached:
                videoAppInstance->runState                        = kUSB_HostVideoRunSetControlInterface;
                videoAppInstance->isControlTransferring           = 0U;
                videoAppInstance->streamBufferIndex               = 0U;
                videoAppInstance->streamBuffer                    = s_streamBuffers[idx];
                videoAppInstance->discardPictureCount             = 0U;
                videoAppInstance->cameraDeviceFormatType          = 0U;
                videoAppInstance->unMultipleIsoMaxPacketSize      = 0U;
                videoAppInstance->currentStreamInterfaceAlternate = 0U;
                /* Discard the very first frame to avoid partial data */
                videoAppInstance->picRXState = HOST_VIDEO_PIC_RX_STATE_DIS_ONE_FRAME;
                videoAppInstance->step       = 0U;
                USB_HostVideoInit(videoAppInstance->deviceHandle, &videoAppInstance->classHandle);
                HAL_LOGD("USB video attached\r\n");
                break;

            case kStatus_DEV_Detached:
                videoAppInstance->devState     = kStatus_DEV_Idle;
                videoAppInstance->runState     = kUSB_HostVideoRunIdle;
                videoAppInstance->runWaitState = (uint8_t)kStatus_DEV_Idle;
                USB_HostVideoInstanceDeinitBuffer(videoAppInstance);
                USB_HostVideoDeinit(videoAppInstance->deviceHandle, videoAppInstance->classHandle);
                videoAppInstance->classHandle = NULL;
                HAL_LOGD("USB video detached\r\n");
                HAL_LOGD("total discarded frames: %u\r\n", (unsigned)videoAppInstance->discardPictureCount);
                break;

            default:
                break;
        }
    }

    /* ------------------------------------------------------------------ */
    /* Run-state machine                                                    */
    /* ------------------------------------------------------------------ */
    switch (videoAppInstance->runState)
    {
        case kUSB_HostVideoRunIdle:
            break;

        case kUSB_HostVideoRunSetControlInterface:
            videoAppInstance->runWaitState = kUSB_HostVideoRunSetControlInterface;
            videoAppInstance->runState     = kUSB_HostVideoRunIdle;
            if (USB_HostVideoControlSetInterface(videoAppInstance->classHandle,
                                                 videoAppInstance->controlIntfHandle, 0U,
                                                 USB_HostVideoControlCallback,
                                                 videoAppInstance) != kStatus_USB_Success)
            {
                HAL_LOGE("set control interface error\r\n");
            }
            break;

        case kUSB_HostVideoRunSetControlInterfaceDone:
            videoAppInstance->runWaitState                    = kUSB_HostVideoRunWaitSetStreamInterface;
            videoAppInstance->runState                        = kUSB_HostVideoRunIdle;
            videoAppInstance->currentStreamInterfaceAlternate = 0U;
            if (USB_HostVideoStreamSetInterface(videoAppInstance->classHandle,
                                                videoAppInstance->streamIntfHandle,
                                                videoAppInstance->currentStreamInterfaceAlternate,
                                                USB_HostVideoControlCallback,
                                                videoAppInstance) != kStatus_USB_Success)
            {
                HAL_LOGE("set stream interface error\r\n");
            }
            break;

        case kUSB_HostVideoRunSetInterfaceDone:
            videoAppInstance->runState = kUSB_HostVideoRunFindOptimalSetting;
            break;

        case kUSB_HostVideoRunFindOptimalSetting:
            /*
             * Query the camera for the format descriptor selected by MATCH_FORMAT.
             *   MATCH_FORMAT_MJPEG        -> MJPEG only (error if not available)
             *   MATCH_FORMAT_UNCOMPRESSED -> uncompressed only (error if not available)
             *   MATCH_FORMAT_ANY          -> try MJPEG first, fall back to uncompressed
             */
#if (MATCH_FORMAT == MATCH_FORMAT_UNCOMPRESSED)
            status = USB_HostVideoStreamGetFormatDescriptor(
                videoAppInstance->classHandle,
                USB_HOST_DESC_SUBTYPE_VS_FORMAT_UNCOMPRESSED,
                (void *)&videoAppInstance->videoStreamFormatDescriptor);
            if (status == kStatus_USB_Error)
            {
                HAL_LOGE("host can't support uncompressed camera format\r\n");
                videoAppInstance->runState = kUSB_HostVideoRunIdle;
            }
#elif (MATCH_FORMAT == MATCH_FORMAT_MJPEG)
            status = USB_HostVideoStreamGetFormatDescriptor(
                videoAppInstance->classHandle,
                USB_HOST_DESC_SUBTYPE_VS_FORMAT_MJPEG,
                (void *)&videoAppInstance->videoStreamFormatDescriptor);
            if (status == kStatus_USB_Error)
            {
                HAL_LOGE("host can't support MJPEG camera format\r\n");
                videoAppInstance->runState = kUSB_HostVideoRunIdle;
            }
#else /* MATCH_FORMAT_ANY: prefer MJPEG, fall back to uncompressed */
            status = USB_HostVideoStreamGetFormatDescriptor(videoAppInstance->classHandle,
                                                            USB_HOST_DESC_SUBTYPE_VS_FORMAT_MJPEG,
                                                            (void *)&videoAppInstance->videoStreamFormatDescriptor);
            if (status == kStatus_USB_Error)
            {
                status = USB_HostVideoStreamGetFormatDescriptor(
                    videoAppInstance->classHandle,
                    USB_HOST_DESC_SUBTYPE_VS_FORMAT_UNCOMPRESSED,
                    (void *)&videoAppInstance->videoStreamFormatDescriptor);
                if (status == kStatus_USB_Error)
                {
                    HAL_LOGE("host can't support this camera format\r\n");
                    videoAppInstance->runState = kUSB_HostVideoRunIdle;
                }
            }
#endif

            if (status == kStatus_USB_Success)
            {
                count = videoAppInstance->videoStreamFormatDescriptor->bNumFrameDescriptors;
                videoAppInstance->cameraDeviceFormatType =
                    videoAppInstance->videoStreamFormatDescriptor->bDescriptorSubtype;

                if (videoAppInstance->cameraDeviceFormatType == USB_HOST_DESC_SUBTYPE_VS_FORMAT_MJPEG)
                {
                    videoAppInstance->videoStreamMjpegFormatDescriptor =
                        (usb_host_video_stream_payload_mjpeg_format_desc_t *)
                            videoAppInstance->videoStreamFormatDescriptor;
                }
                else if (videoAppInstance->cameraDeviceFormatType == USB_HOST_DESC_SUBTYPE_VS_FORMAT_UNCOMPRESSED)
                {
                    videoAppInstance->videoStreamUncompressedFormatDescriptor =
                        (usb_host_video_stream_payload_uncompressed_format_desc_t *)
                            videoAppInstance->videoStreamFormatDescriptor;
                }

                /* List and select the frame resolution closest to the target */
                usb_echo("Available resolutions:\r\n");
                uint32_t targetResolution = videoAppInstance->targetWidth * videoAppInstance->targetHeight;
                for (index = 1U; index <= count; index++)
                {
                    if (videoAppInstance->cameraDeviceFormatType == USB_HOST_DESC_SUBTYPE_VS_FORMAT_MJPEG)
                    {
                        status = USB_HostVideoStreamGetFrameDescriptor(
                            videoAppInstance->classHandle,
                            videoAppInstance->videoStreamFormatDescriptor,
                            USB_HOST_DESC_SUBTYPE_VS_FRAME_MJPEG, index, (void *)&frameDesc);
                    }
                    else
                    {
                        status = USB_HostVideoStreamGetFrameDescriptor(
                            videoAppInstance->classHandle,
                            videoAppInstance->videoStreamFormatDescriptor,
                            USB_HOST_DESC_SUBTYPE_VS_FRAME_UNCOMPRESSED, index, (void *)&frameDesc);
                    }

                    if ((kStatus_USB_Success == status) && (NULL != frameDesc))
                    {
                        resolution =
                            ((uint32_t)USB_SHORT_FROM_LITTLE_ENDIAN_ADDRESS(frameDesc->wHeight)) *
                            ((uint32_t)USB_SHORT_FROM_LITTLE_ENDIAN_ADDRESS(frameDesc->wWitd));

                        /* Retrieve the default frame interval to compute fps for the log */
                        uint32_t defInterval = 0U;
                        if (videoAppInstance->cameraDeviceFormatType == USB_HOST_DESC_SUBTYPE_VS_FORMAT_MJPEG)
                        {
                            defInterval = USB_LONG_FROM_LITTLE_ENDIAN_ADDRESS(
                                ((usb_host_video_stream_payload_mjpeg_frame_desc_t *)frameDesc)->dwDefaultFrameInterval);
                        }
                        else
                        {
                            defInterval = USB_LONG_FROM_LITTLE_ENDIAN_ADDRESS(
                                ((usb_host_video_stream_payload_uncompressed_frame_desc_t *)frameDesc)->dwDefaultFrameInterval);
                        }
                        uint32_t fps = (defInterval > 0U) ? (10000000U / defInterval) : 0U;

                        usb_echo("  [%u] %ux%u @ %u fps\r\n",
                                 (unsigned)index,
                                 (unsigned)USB_SHORT_FROM_LITTLE_ENDIAN_ADDRESS(frameDesc->wWitd),
                                 (unsigned)USB_SHORT_FROM_LITTLE_ENDIAN_ADDRESS(frameDesc->wHeight),
                                 (unsigned)fps);

                        /* Skip this descriptor if either dimension exceeds the maximum
                         * supported size - selecting it would overflow the frame buffers. */
                        if ((USB_SHORT_FROM_LITTLE_ENDIAN_ADDRESS(frameDesc->wWitd)   > CAMERA_USB_MAX_WIDTH) ||
                            (USB_SHORT_FROM_LITTLE_ENDIAN_ADDRESS(frameDesc->wHeight) > CAMERA_USB_MAX_HEIGHT))
                        {
                            usb_echo("       -> skipped (exceeds max %ux%u)\r\n",
                                     (unsigned)CAMERA_USB_MAX_WIDTH, (unsigned)CAMERA_USB_MAX_HEIGHT);
                            continue;
                        }
                        uint32_t diff = (resolution > targetResolution) ?
                            (resolution - targetResolution) : (targetResolution - resolution);

                        if (diff < minResolution)
                        {
                            minResolution          = diff;
                            minSolutionFrameIndex  = (uint8_t)index;
                            if (videoAppInstance->cameraDeviceFormatType == USB_HOST_DESC_SUBTYPE_VS_FORMAT_MJPEG)
                            {
                                videoAppInstance->videoStreamMjpegFrameDescriptor =
                                    (usb_host_video_stream_payload_mjpeg_frame_desc_t *)frameDesc;
                                videoAppInstance->videoStreamUncompressedFrameDescriptor = NULL;
                            }
                            else
                            {
                                videoAppInstance->videoStreamUncompressedFrameDescriptor =
                                    (usb_host_video_stream_payload_uncompressed_frame_desc_t *)frameDesc;
                                videoAppInstance->videoStreamMjpegFrameDescriptor = NULL;
                            }
                        }
                    }
                }

                if (minSolutionFrameIndex != 0xFFU)
                {
                    videoAppInstance->runState = kUSB_HostVideoRunGetSetProbeCommit;
                    videoAppInstance->step     = 0U;
                }
                else
                {
                    videoAppInstance->runState = kUSB_HostVideoRunIdle;
                }
            }
            break;

        case kUSB_HostVideoRunGetSetProbeCommit:
            switch (videoAppInstance->step)
            {
                case 0U:
                    if (videoAppInstance->isControlTransferring == 0U)
                    {
                        videoAppInstance->isControlTransferring = 1U;
                        USB_HostVideoGetProbe(videoAppInstance->classHandle, USB_HOST_VIDEO_GET_CUR,
                                             (void *)&s_Probe[0], USB_HostVideoCommandCallback, videoAppInstance);
                        videoAppInstance->step++;
                    }
                    break;
                case 1U:
                    if (videoAppInstance->isControlTransferring == 0U)
                    {
                        videoAppInstance->isControlTransferring = 1U;
                        USB_HostVideoGetProbe(videoAppInstance->classHandle, USB_HOST_VIDEO_GET_MAX,
                                             (void *)&s_Probe[1], USB_HostVideoCommandCallback, videoAppInstance);
                        videoAppInstance->step++;
                    }
                    break;
                case 2U:
                    if (videoAppInstance->isControlTransferring == 0U)
                    {
                        videoAppInstance->isControlTransferring = 1U;
                        USB_HostVideoGetProbe(videoAppInstance->classHandle, USB_HOST_VIDEO_GET_MIN,
                                             (void *)&s_Probe[2], USB_HostVideoCommandCallback, videoAppInstance);
                        videoAppInstance->step++;
                    }
                    break;
                case 3U:
                    if (videoAppInstance->isControlTransferring == 0U)
                    {
                        /* Select the frame interval closest to APP_CAMERA_FPS */
                        uint32_t targetInterval  = 1000U * 1000U * 10U / APP_CAMERA_FPS;
                        uint32_t minDiff         = 0xFFFFFFFFUL;
                        uint32_t selectedInterval = 0U;

                        usb_echo("Available fps:");
                        if (videoAppInstance->cameraDeviceFormatType == USB_HOST_DESC_SUBTYPE_VS_FORMAT_MJPEG)
                        {
                            usb_host_video_stream_payload_mjpeg_frame_desc_t *fDesc =
                                videoAppInstance->videoStreamMjpegFrameDescriptor;
                            if (fDesc->bFrameIntervalType > 0U)
                            {
                                for (uint8_t fi = 0U; fi < fDesc->bFrameIntervalType; fi++)
                                {
                                    const uint8_t *fi_bytes = &fDesc->dwMinFrameInterval[fi * 4U];
                                    frameInterval = USB_LONG_FROM_LITTLE_ENDIAN_ADDRESS(fi_bytes);
                                    usb_echo(" %u", (unsigned)(10000000U / frameInterval));
                                    uint32_t d = (frameInterval > targetInterval) ?
                                        (frameInterval - targetInterval) :
                                        (targetInterval - frameInterval);
                                    if (d < minDiff)
                                    {
                                        minDiff          = d;
                                        selectedInterval = frameInterval;
                                    }
                                }
                            }
                            else
                            {
                                const uint8_t *min_fi_bytes = &fDesc->dwMinFrameInterval[0];
                                const uint8_t *max_fi_bytes = &fDesc->dwMinFrameInterval[0] + 4U;
                                uint32_t minFI = USB_LONG_FROM_LITTLE_ENDIAN_ADDRESS(min_fi_bytes);
                                uint32_t maxFI = USB_LONG_FROM_LITTLE_ENDIAN_ADDRESS(max_fi_bytes);
                                usb_echo(" %u-%u (continuous)",
                                         (unsigned)(10000000U / maxFI),
                                         (unsigned)(10000000U / minFI));
                                for (frameInterval = minFI; frameInterval <= maxFI; frameInterval += 100U)
                                {
                                    uint32_t d = (frameInterval > targetInterval) ?
                                        (frameInterval - targetInterval) :
                                        (targetInterval - frameInterval);
                                    if (d < minDiff)
                                    {
                                        minDiff          = d;
                                        selectedInterval = frameInterval;
                                    }
                                }
                            }
                            usb_echo("\r\n");
                            USB_LONG_TO_LITTLE_ENDIAN_DATA(selectedInterval, s_VideoProbe.dwFrameInterval);
                            s_VideoProbe.bFormatIndex = videoAppInstance->videoStreamMjpegFormatDescriptor->bFormatIndex;
                            s_VideoProbe.bFrameIndex  = videoAppInstance->videoStreamMjpegFrameDescriptor->bFrameIndex;
                        }
                        else
                        {
                            usb_host_video_stream_payload_uncompressed_frame_desc_t *fDesc =
                                videoAppInstance->videoStreamUncompressedFrameDescriptor;
                            if (fDesc->bFrameIntervalType > 0U)
                            {
                                /* Discrete intervals - print all and pick closest to target */
                                for (uint8_t fi = 0U; fi < fDesc->bFrameIntervalType; fi++)
                                {
                                    const uint8_t *fi_bytes = &fDesc->dwMinFrameInterval[fi * 4U];
                                    frameInterval = USB_LONG_FROM_LITTLE_ENDIAN_ADDRESS(fi_bytes);
                                    usb_echo(" %u", (unsigned)(10000000U / frameInterval));
                                    uint32_t d = (frameInterval > targetInterval) ?
                                        (frameInterval - targetInterval) :
                                        (targetInterval - frameInterval);
                                    if (d < minDiff)
                                    {
                                        minDiff          = d;
                                        selectedInterval = frameInterval;
                                    }
                                }
                                usb_echo("\r\n");
                                frameInterval = selectedInterval;
                            }
                            else
                            {
                                const uint8_t *uc_min_fi = &fDesc->dwMinFrameInterval[0];
                                const uint8_t *uc_max_fi = &fDesc->dwMinFrameInterval[0] + 4U;
                                uint32_t minFI2 = USB_LONG_FROM_LITTLE_ENDIAN_ADDRESS(uc_min_fi);
                                uint32_t maxFI2 = USB_LONG_FROM_LITTLE_ENDIAN_ADDRESS(uc_max_fi);
                                usb_echo(" %u-%u (continuous)\r\n",
                                         (unsigned)(10000000U / maxFI2),
                                         (unsigned)(10000000U / minFI2));
                                /* Continuous range - step through and pick closest to target */
                                for (uint32_t fi2 = minFI2; fi2 <= maxFI2; fi2 += 100U)
                                {
                                    uint32_t d = (fi2 > targetInterval) ?
                                        (fi2 - targetInterval) :
                                        (targetInterval - fi2);
                                    if (d < minDiff)
                                    {
                                        minDiff          = d;
                                        selectedInterval = fi2;
                                    }
                                }
                                frameInterval = selectedInterval;
                            }
                            USB_LONG_TO_LITTLE_ENDIAN_DATA(frameInterval, s_VideoProbe.dwFrameInterval);
                            s_VideoProbe.bFormatIndex =
                                videoAppInstance->videoStreamUncompressedFormatDescriptor->bFormatIndex;
                            s_VideoProbe.bFrameIndex =
                                videoAppInstance->videoStreamUncompressedFrameDescriptor->bFrameIndex;
                        }

                        videoAppInstance->isControlTransferring = 1U;
                        USB_HostVideoSetProbe(videoAppInstance->classHandle, USB_HOST_VIDEO_SET_CUR,
                                             (void *)&s_VideoProbe, USB_HostVideoCommandCallback, videoAppInstance);
                        videoAppInstance->step++;
                    }
                    break;
                case 4U:
                case 5U:
                case 6U:
                    if (videoAppInstance->isControlTransferring == 0U)
                    {
                        videoAppInstance->isControlTransferring = 1U;
                        if (videoAppInstance->step == 4U)
                        {
                            USB_HostVideoGetProbe(videoAppInstance->classHandle, USB_HOST_VIDEO_GET_CUR,
                                                 (void *)&s_Probe[0], USB_HostVideoCommandCallback, videoAppInstance);
                        }
                        else if (videoAppInstance->step == 5U)
                        {
                            USB_HostVideoGetProbe(videoAppInstance->classHandle, USB_HOST_VIDEO_GET_MAX,
                                                 (void *)&s_Probe[1], USB_HostVideoCommandCallback, videoAppInstance);
                        }
                        else
                        {
                            USB_HostVideoGetProbe(videoAppInstance->classHandle, USB_HOST_VIDEO_GET_MIN,
                                                 (void *)&s_Probe[2], USB_HostVideoCommandCallback, videoAppInstance);
                        }
                        videoAppInstance->step++;
                    }
                    break;
                case 7U:
                    if (videoAppInstance->isControlTransferring == 0U)
                    {
                        videoAppInstance->isControlTransferring = 1U;
                        memcpy((void *)&s_VideoProbe, (void *)&s_Probe[0], USB_HOST_VIDEO_PROBE_CTRL_SIZE_UVC10_SHORT);
                        USB_HostVideoSetProbe(videoAppInstance->classHandle, USB_HOST_VIDEO_SET_CUR,
                                             (void *)&s_VideoProbe, USB_HostVideoCommandCallback, videoAppInstance);
                        videoAppInstance->step++;
                    }
                    break;
                case 8U:
                    if (videoAppInstance->isControlTransferring == 0U)
                    {
                        videoAppInstance->isControlTransferring = 1U;
                        USB_HostVideoGetProbe(videoAppInstance->classHandle, USB_HOST_VIDEO_GET_CUR,
                                             (void *)&s_Probe[0], USB_HostVideoCommandCallback, videoAppInstance);
                        videoAppInstance->step++;
                    }
                    break;
                case 9U:
                    if (videoAppInstance->isControlTransferring == 0U)
                    {
                        videoAppInstance->isControlTransferring = 1U;
                        memcpy((void *)&s_VideoProbe, (void *)&s_Probe[0], USB_HOST_VIDEO_PROBE_CTRL_SIZE_UVC10);
                        USB_HostVideoSetProbe(videoAppInstance->classHandle, USB_HOST_VIDEO_SET_CUR,
                                             (void *)&s_VideoProbe, USB_HostVideoCommandCallback, videoAppInstance);
                        videoAppInstance->step++;
                    }
                    break;
                case 10U:
                    if (videoAppInstance->isControlTransferring == 0U)
                    {
                        videoAppInstance->isControlTransferring = 1U;
                        USB_HostVideoGetProbe(videoAppInstance->classHandle, USB_HOST_VIDEO_GET_CUR,
                                             (void *)&s_Probe[0], USB_HostVideoCommandCallback, videoAppInstance);
                        videoAppInstance->step++;
                    }
                    break;
                case 11U:
                    if (videoAppInstance->isControlTransferring == 0U)
                    {
                        videoAppInstance->isControlTransferring = 1U;
                        memcpy((void *)&s_VideoProbe, (void *)&s_Probe[0], USB_HOST_VIDEO_PROBE_CTRL_SIZE_UVC10);
                        SDK_DelayAtLeastUs(20000U, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
                        USB_HostVideoSetCommit(videoAppInstance->classHandle, USB_HOST_VIDEO_SET_CUR,
                                              (void *)&s_VideoProbe, USB_HostVideoCommandCallback, videoAppInstance);
                        videoAppInstance->step++;
                    }
                    break;
                case 12U:
                    if (videoAppInstance->isControlTransferring == 0U)
                    {
                        USB_HostHelperGetPeripheralInformation(videoAppInstance->deviceHandle,
                                                               kUSB_HostGetDeviceSpeed, &speed);
                        videoAppInstance->unMultipleIsoMaxPacketSize = 0U;
                        if (USB_SPEED_FULL == speed)
                        {
                            USB_HostGetProperEndpointInterfaceInfo(
                                videoAppInstance->classHandle, FULL_SPEED_ISO_MAX_PACKET_SIZE,
                                &videoAppInstance->currentStreamInterfaceAlternate,
                                &videoAppInstance->unMultipleIsoMaxPacketSize);
                        }
                        else if (USB_SPEED_HIGH == speed)
                        {
                            USB_HostGetProperEndpointInterfaceInfo(
                                videoAppInstance->classHandle, HIGH_SPEED_ISO_MAX_PACKET_SIZE_ZERO_ADDITION,
                                &videoAppInstance->currentStreamInterfaceAlternate,
                                &videoAppInstance->unMultipleIsoMaxPacketSize);
                        }

                        if (videoAppInstance->currentStreamInterfaceAlternate)
                        {
                            videoAppInstance->runWaitState = kUSB_HostVideoRunWaitGetSetProbeCommit;
                            videoAppInstance->runState     = kUSB_HostVideoRunIdle;
                            if (USB_HostVideoStreamSetInterface(
                                    videoAppInstance->classHandle, videoAppInstance->streamIntfHandle,
                                    videoAppInstance->currentStreamInterfaceAlternate,
                                    USB_HostVideoControlCallback, videoAppInstance) != kStatus_USB_Success)
                            {
                                HAL_LOGE("set stream interface error\r\n");
                            }
                        }
                        else
                        {
                            videoAppInstance->runWaitState = kUSB_HostVideoRunIdle;
                            videoAppInstance->runState     = kUSB_HostVideoRunIdle;
                            HAL_LOGE("no proper stream interface alternate found\r\n");
                        }
                    }
                    break;
                default:
                    break;
            }
            break;

        case kUSB_HostVideoRunGetSetProbeCommitDone:
        {
            /* Log the selected camera format, resolution and fps */
            if (videoAppInstance->cameraDeviceFormatType == USB_HOST_DESC_SUBTYPE_VS_FORMAT_MJPEG)
            {
                usb_echo("Selected: %ux%u @ %u fps (MJPEG)\r\n",
                    (unsigned)USB_SHORT_FROM_LITTLE_ENDIAN_ADDRESS(
                        videoAppInstance->videoStreamMjpegFrameDescriptor->wWitd),
                    (unsigned)USB_SHORT_FROM_LITTLE_ENDIAN_ADDRESS(
                        videoAppInstance->videoStreamMjpegFrameDescriptor->wHeight),
                    (unsigned)(10000000U / USB_LONG_FROM_LITTLE_ENDIAN_ADDRESS(s_VideoProbe.dwFrameInterval)));
                videoAppInstance->videoCameraPictureBufferSize =
                    ((uint32_t)USB_SHORT_FROM_LITTLE_ENDIAN_ADDRESS(
                        videoAppInstance->videoStreamMjpegFrameDescriptor->wHeight)) *
                    ((uint32_t)USB_SHORT_FROM_LITTLE_ENDIAN_ADDRESS(
                        videoAppInstance->videoStreamMjpegFrameDescriptor->wWitd));
                videoAppInstance->videoCameraPictureBufferSize =
                    (videoAppInstance->videoCameraPictureBufferSize / 100U) * USB_MJPEG_COMPRESSION_RATIO;
            }
            else if (videoAppInstance->cameraDeviceFormatType == USB_HOST_DESC_SUBTYPE_VS_FORMAT_UNCOMPRESSED)
            {
                const char *pixFmt = "unknown";
                if (memcmp(&videoAppInstance->videoStreamUncompressedFormatDescriptor->guidFormat[0],
                           k_YUY2FormatGUID, 16U) == 0)
                {
                    pixFmt = "YUY2";
                }
                else if (memcmp(&videoAppInstance->videoStreamUncompressedFormatDescriptor->guidFormat[0],
                                k_NV12FormatGUID, 16U) == 0)
                {
                    pixFmt = "NV12";
                }
                usb_echo("Selected: %ux%u @ %u fps (%s)\r\n",
                    (unsigned)USB_SHORT_FROM_LITTLE_ENDIAN_ADDRESS(
                        videoAppInstance->videoStreamUncompressedFrameDescriptor->wWitd),
                    (unsigned)USB_SHORT_FROM_LITTLE_ENDIAN_ADDRESS(
                        videoAppInstance->videoStreamUncompressedFrameDescriptor->wHeight),
                    (unsigned)(10000000U / USB_LONG_FROM_LITTLE_ENDIAN_ADDRESS(s_VideoProbe.dwFrameInterval)),
                    pixFmt);
                videoAppInstance->videoCameraPictureBufferSize =
                    (uint32_t)(videoAppInstance->videoStreamUncompressedFormatDescriptor->bBitsPerPixel / 8U) *
                    ((uint32_t)USB_SHORT_FROM_LITTLE_ENDIAN_ADDRESS(
                        videoAppInstance->videoStreamUncompressedFrameDescriptor->wHeight)) *
                    ((uint32_t)USB_SHORT_FROM_LITTLE_ENDIAN_ADDRESS(
                        videoAppInstance->videoStreamUncompressedFrameDescriptor->wWitd));
                videoAppInstance->videoCameraPictureBufferSize +=
                    videoAppInstance->unMultipleIsoMaxPacketSize;
            }

            /* Initialise the ping-pong picture buffers */
            if (USB_HostVideoInstanceInitBuffer(videoAppInstance) != 0)
            {
                videoAppInstance->runState     = kUSB_HostVideoRunIdle;
                videoAppInstance->runWaitState = kUSB_HostVideoRunIdle;
                return;
            }

            /* Allow camera to stabilise */
            vTaskDelay(pdMS_TO_TICKS(2U));

            taskENTER_CRITICAL();
            /* Prime all stream receive slots */
            for (i = 0U; i < USB_VIDEO_STREAM_BUFFER_COUNT; i++)
            {
                USB_HosVideoStreamRecv(
                    videoAppInstance->classHandle,
                    &videoAppInstance->streamBuffer[i * HOST_VIDEO_USB_RX_BUF_SIZE],
                    (HOST_VIDEO_USB_RX_BUF_SIZE / videoAppInstance->unMultipleIsoMaxPacketSize) *
                        videoAppInstance->unMultipleIsoMaxPacketSize,
                    USB_HostVideoStreamDataInCallback,
                    videoAppInstance);
            }
            taskEXIT_CRITICAL();

            videoAppInstance->runState = kUSB_HostVideoStart;
            break;
        }

        case kUSB_HostVideoStart:
#if ENABLE_FPS_LOG
            /*
             * FPS logging: compare the ISR frame counter (incremented in
             * USB_HostVideoStreamDataInCallback on each successfully queued frame)
             * against the last seen value to detect new frames without touching
             * the mppQueue.  Print a summary every 30 seconds.
             */
            {
                uint32_t curIsrCount = s_isrFrameCount[idx]; /* read volatile once */
                uint32_t newFrames   = curIsrCount - s_lastIsrFrameCount[idx];

                if (newFrames > 0U)
                {
                    /* Snapshot ISR length accumulators before updating frame count
                     * so avgFLen and maxFLen are consistent with the frame count. */
                    uint32_t snapTotalLen = s_isrTotalLen[idx];
                    uint32_t snapMaxLen   = s_isrMaxLen[idx];

                    s_lastIsrFrameCount[idx] = curIsrCount;
                    s_frameNumbers[idx]     += newFrames;
                    s_totalLen4Frames[idx]   = snapTotalLen;
                    if (snapMaxLen > s_maxFrameLen[idx])
                    {
                        s_maxFrameLen[idx] = snapMaxLen;
                    }

                    if (s_curTime[idx] == 0U)
                    {
                        /* First frame seen - start the measurement window */
                        s_curTime[idx]          = getTime();
                        s_frameNumbers[idx]     = 0U;
                        s_totalTime4Frames[idx] = 0U;
                        s_totalLen4Frames[idx]  = 0U;
                        s_maxFrameLen[idx]      = 0U;
                    }
                    else
                    {
                        uint32_t now  = getTime();
                        uint32_t diff = now - s_curTime[idx];
                        s_curTime[idx]           = now;
                        s_totalTime4Frames[idx] += diff;

                        if (s_totalTime4Frames[idx] >= 30U * 1000U)
                        {
                            usb_echo("Cam[%u]: %u fps  dropped:%u  avgFLen:%u  maxFLen:%u\r\n",
                                     (unsigned)idx,
                                     (unsigned)(s_frameNumbers[idx] * 1000U / s_totalTime4Frames[idx]),
                                     (unsigned)videoAppInstance->discardPictureCount,
                                     (s_frameNumbers[idx] > 0U) ?
                                         (unsigned)(s_totalLen4Frames[idx] / s_frameNumbers[idx]) : 0U,
                                     (unsigned)s_maxFrameLen[idx]);
                            s_frameNumbers[idx]     = 0U;
                            s_totalTime4Frames[idx] = 0U;
                            s_totalLen4Frames[idx]  = 0U;
                            s_maxFrameLen[idx]      = 0U;
                            /* Reset ISR accumulators to match the new window */
                            s_isrTotalLen[idx] = 0U;
                            s_isrMaxLen[idx]   = 0U;
                        }
                    }
                }
            }
#endif /* ENABLE_FPS_LOG */
            break;

        default:
            break;
    }
}

/*
 * USB host event callback.
 *
 * Called by the USB middleware on device attach/detach/enumeration events.
 * Assigns USB interface handles to a free g_Videos[] instance.
 */
usb_status_t USB_HostVideoEvent(usb_device_handle deviceHandle,
                                usb_host_configuration_handle configurationHandle,
                                uint32_t eventCode)
{
    usb_status_t    status = kStatus_USB_Success;
    uint8_t         id;
    usb_host_configuration_t *configuration;
    uint8_t         interface_index;
    usb_host_interface_t *hostInterface;
    uint32_t        info_value = 0U;
    usb_host_video_camera_instance_t *inst;

    static usb_device_handle            g_VideoDeviceHandle           = NULL;
    static usb_host_interface_handle    g_VideoStreamInterfaceHandle  = NULL;
    static usb_host_interface_handle    g_VideoControlInterfaceHandle = NULL;

    HAL_LOGD("USB_HostVideoEvent: eventCode=0x%x devH=%p\r\n", (unsigned)eventCode, deviceHandle);

    switch (eventCode)
    {
        case kUSB_HostEventAttach:
            configuration                 = (usb_host_configuration_t *)configurationHandle;
            g_VideoDeviceHandle           = NULL;
            g_VideoControlInterfaceHandle = NULL;
            g_VideoStreamInterfaceHandle  = NULL;
            HAL_LOGD("configuration: if#=%d\r\n", (int)configuration->interfaceCount);
            for (interface_index = 0U; interface_index < configuration->interfaceCount; ++interface_index)
            {
                hostInterface = &configuration->interfaceList[interface_index];
                id            = hostInterface->interfaceDesc->bInterfaceClass;
                if (id != USB_HOST_VIDEO_CLASS_CODE)
                {
                    continue;
                }
                id = hostInterface->interfaceDesc->bInterfaceSubClass;
                if (id == USB_HOST_VIDEO_SUBCLASS_CODE_CONTROL)
                {
                    g_VideoDeviceHandle           = deviceHandle;
                    g_VideoControlInterfaceHandle = hostInterface;
                }
                else if (id == USB_HOST_VIDEO_SUBCLASS_CODE_STREAM)
                {
                    g_VideoDeviceHandle          = deviceHandle;
                    g_VideoStreamInterfaceHandle = hostInterface;
                }
            }
            if (g_VideoDeviceHandle != NULL)
            {
                return kStatus_USB_Success;
            }
            status = kStatus_USB_NotSupported;
            break;

        case kUSB_HostEventNotSupported:
            break;

        case kUSB_HostEventEnumerationDone:
            inst = USB_HostVideoDeviceHandle2VideoInstance(g_VideoDeviceHandle);
            if (inst == NULL)
            {
                inst = USB_HostVideoGetFreeVideoInstance();
            }
            if (inst != NULL)
            {
                if ((g_VideoDeviceHandle != NULL) && (g_VideoControlInterfaceHandle != NULL))
                {
                    inst->devState          = kStatus_DEV_Attached;
                    inst->deviceHandle      = g_VideoDeviceHandle;
                    inst->controlIntfHandle = g_VideoControlInterfaceHandle;
                    inst->streamIntfHandle  = g_VideoStreamInterfaceHandle;

                    USB_HostHelperGetPeripheralInformation(deviceHandle, kUSB_HostGetDevicePID, &info_value);
                    HAL_LOGD("video attached: pid=0x%x", (unsigned)info_value);
                    USB_HostHelperGetPeripheralInformation(deviceHandle, kUSB_HostGetDeviceVID, &info_value);
                    HAL_LOGD(" vid=0x%x", (unsigned)info_value);
                    USB_HostHelperGetPeripheralInformation(deviceHandle, kUSB_HostGetDeviceAddress, &info_value);
                    HAL_LOGD(" addr=%u\r\n", (unsigned)info_value);
                }
            }
            else
            {
                HAL_LOGI("no free video instance\r\n");
            }
            break;

        case kUSB_HostEventDetach:
            inst = USB_HostVideoDeviceHandle2VideoInstance(deviceHandle);
            if ((inst != NULL) && (inst->devState != kStatus_DEV_Idle))
            {
                inst->devState = kStatus_DEV_Detached;
            }
            break;

        default:
            break;
    }
    return status;
}

/*
 * Stop the ISO stream for a single instance by switching the stream interface
 * back to alternate setting 0 (zero bandwidth). This closes the ISO pipe and
 * allows USB_HostDeinit to drain the device list without blocking.
 *
 * Must be called while the USB host task is still running.
 * devState on the instance must already be set to kStatus_DEV_Idle by the
 * caller BEFORE calling this function so that the ITD cancellation callbacks
 * do not re-queue new transfers onto the now-closing pipe.
 *
 * Safe to call even when no device is attached.
 */
static volatile bool s_streamStopDone[USB_HOST_VIDEO_INST_NUM];

static void USB_HostVideoStopStreamCallback(void *param, uint8_t *data, uint32_t dataLen, usb_status_t status)
{
    usb_host_video_camera_instance_t *inst = (usb_host_video_camera_instance_t *)param;
    uint32_t idx = USB_HostVideoInstance2Index(inst);
    if (idx < (uint32_t)USB_HOST_VIDEO_INST_NUM)
    {
        s_streamStopDone[idx] = true;
    }
}

void USB_HostVideoAppStopStream(usb_host_video_camera_instance_t *inst)
{
    uint32_t timeout_ms = 500U;
    uint32_t idx;

    if (inst == NULL)
    {
        return;
    }

    idx = USB_HostVideoInstance2Index(inst);
    if (idx >= (uint32_t)USB_HOST_VIDEO_INST_NUM)
    {
        return;
    }

    if (inst->classHandle == NULL)
    {
        return; /* nothing to stop - class not open */
    }

    s_streamStopDone[idx] = false;
    if (USB_HostVideoStreamSetInterface(inst->classHandle, inst->streamIntfHandle, 0U,
                                        USB_HostVideoStopStreamCallback, inst) != kStatus_USB_Success)
    {
        usb_echo("stop stream[%u]: set interface error\r\n", (unsigned)idx);
        return;
    }

    /* Wait for SET_INTERFACE(alt=0) to complete (closes the ISO pipe). */
    while ((!s_streamStopDone[idx]) && (timeout_ms > 0U))
    {
        vTaskDelay(pdMS_TO_TICKS(1U));
        timeout_ms--;
    }

    if (!s_streamStopDone[idx])
    {
        usb_echo("stop stream[%u]: timeout waiting for interface close\r\n", (unsigned)idx);
    }
    else
    {
        usb_echo("stop stream[%u]: ISO pipe closed\r\n", (unsigned)idx);
    }
}

/*
 * SD card initialization stub.
 * Implement USE_SD-specific logic here if required.
 */
usb_status_t USB_HostVideoAppSDcardInit(void)
{
#if USE_SD
    FATFS *fs;
    FIL   fileObj;
    uint32_t freeClusterNumber;
    uint32_t freeMemorySizeMB;
    char fileName[25];

    HAL_LOGD("please insert SD card\r\n");
    if (f_mount(&g_fileSystem, g_DriverNumberBuffer, 1U))
    {
        HAL_LOGE("f_mount failed.\r\n");
        return kStatus_USB_Error;
    }
    HAL_LOGD("sdcard inserted\r\n");

    if (f_getfree(&g_DriverNumberBuffer[0], (DWORD *)&freeClusterNumber, &fs))
    {
        HAL_LOGE("f_getfree failed.\r\n");
        return kStatus_USB_Error;
    }
    freeMemorySizeMB = freeClusterNumber * (fs->csize) / 2048U;
    HAL_LOGD("sdcard free: %u MB\r\n", (unsigned)freeMemorySizeMB);

    sprintf(fileName, "%svideo.txt", &g_DriverNumberBuffer[0]);
    if (f_open(&fileObj, _T(fileName), FA_WRITE | FA_CREATE_ALWAYS) == FR_OK)
    {
        f_close(&fileObj);
        (void)f_unlink(fileName);
    }
#endif
    return kStatus_USB_Success;
}

#endif /* USE_USB_CAMERA */
