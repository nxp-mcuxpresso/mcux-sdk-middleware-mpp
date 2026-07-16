# RTSP/RTP Client

## Overview

This document describes how to use the RTSP/RTP client functionality in the eIQ Media Processing Pipeline to receive video streams over the network.

The RTSP/RTP client allows the MCU to connect to a remote RTSP server and receive a video stream for decoding and display.

## Features

- Real-time video reception over RTSP/RTP protocol
- Support for JPEG/MJPEG streams (RFC 2435 RTP payload format)
- Automatic reconnection on server loss or stream interruption
- Integration with MPP pipeline elements

## Architecture

### High-level description

```
RTSP Server (PC)
+------------------+
|                  |
|    MediaMTX      | <-- FFmpeg (publisher)
|                  |
+------------------+
        |
        | Network
        v
+----------------+     +---------+     +-------------+     +---------+
|                |     |         |     |             |     |         |
|  RTSP/RTP      | --> |  JPEG   | --> |  2D Convert | --> | Display |
|  Source        |     | Decoder |     |             |     |         |
+----------------+     +---------+     +-------------+     +---------+
```

### Pipeline Integration

The RTSP/RTP source can be integrated as a source element in the MPP pipeline, similar to the Camera element.

### RTSP/RTP Client Implementation

The RTSP/RTP client is implemented as a separate HAL component in:
- `hal/rtsp_client/rtsp_client.c` - Client implementation
- `hal/rtsp_client/rtsp_client.h` - Client interface and configuration

The client handles:
- **RTSP Protocol**: Session management (OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN, keepalive)
- **RTP Reassembly**:
  - JPEG: Frame reconstruction from RTP packets according to RFC 2435
- **Network Tasks**:
  - RTSP client task for session management over TCP
  - RTP receiver task for media data over UDP

The client is integrated into the MPP pipeline through the HAL and MPP RTSP source element.

## Configuration

### Network Configuration

Configure the network settings for your board in the test_config.h file:

```c
/* RTSP Client Configuration */
#define RTSP_SERVER_PORT    8554

/* IP Address Configuration */
#define configIP_ADDR0      192
#define configIP_ADDR1      168
#define configIP_ADDR2      0
#define configIP_ADDR3      102

/* Network Mask */
#define configNET_MASK0     255
#define configNET_MASK1     255
#define configNET_MASK2     255
#define configNET_MASK3     0

/* Gateway Address */
#define configGW_ADDR0      192
#define configGW_ADDR1      168
#define configGW_ADDR2      0
#define configGW_ADDR3      100

/* RTSP stream path published on the server (without leading slash). */
#define RTSP_STREAM_PATH    "photo"
```

The RTSP URL is assembled at runtime in the application code from the macros above:

```c
char rtsp_url[64];
snprintf(rtsp_url, sizeof(rtsp_url), "rtsp://%d.%d.%d.%d:%d/" RTSP_STREAM_PATH,
         configGW_ADDR0, configGW_ADDR1, configGW_ADDR2, configGW_ADDR3,
         RTSP_SERVER_PORT);
```

### Linux PC Configuration

The board should be connected directly to the PC via Ethernet cable (or WiFi if supported).

#### 1. Assign Static IP to Network Interface

First, identify your network interface name (e.g. `eth0`):

```bash
ip link show
```

Configure the host PC IP address to `192.168.0.100`:

```bash
sudo ip addr add 192.168.0.100/24 dev eth0
```

**Note:** Ensure the IP addresses don't conflict with your existing network configuration. The PC IP address must be in the same subnet as the board's IP address.

#### 2. Bring the Interface Up

```bash
sudo ip link set eth0 up
```

#### 3. Verify Configuration

```bash
ip addr show eth0
```

The output should be similar to:
```
2: eth0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UP group default qlen 1000
    link/ether 00:0a:cd:2e:39:2e brd ff:ff:ff:ff:ff:ff
    inet 192.168.0.100/24 scope global eth0
       valid_lft forever preferred_lft forever
```

#### 4. Test Network Connection

If the board is connected and booted with the RTSP/RTP test running, verify the network connection with ping:

```bash
ping -I eth0 192.168.0.102
```

You should see successful ping responses:
```
PING 192.168.0.102 (192.168.0.102) from 192.168.0.100 eth0: 56(84) bytes of data.
64 bytes from 192.168.0.102: icmp_seq=1 ttl=255 time=0.625 ms
64 bytes from 192.168.0.102: icmp_seq=2 ttl=255 time=0.577 ms
64 bytes from 192.168.0.102: icmp_seq=3 ttl=255 time=0.640 ms
^C
--- 192.168.0.102 ping statistics ---
3 packets transmitted, 3 received, 0% packet loss, time 2077ms
rtt min/avg/max/mdev = 0.577/0.614/0.640/0.026 ms
```

## Usage Example

### JPEG RTSP Client Pipeline

The complete implementation can be found in `tests/test_image_jpeg_rtsp_client/test_image_jpeg_rtsp_client.c`.

## Setting up an RTSP/RTP Server on a Linux PC

Before starting the test, an RTSP server and a publisher must be running on the connected PC.

**Note:** FFmpeg cannot act as an RTSP server on its own (it cannot respond to RTSP commands
such as DESCRIBE, SETUP, PLAY). It must publish to a dedicated RTSP server such as MediaMTX,
which handles the RTSP session and relays the RTP stream to clients.

### Using MediaMTX and FFmpeg

#### Step 1 - Start the RTSP server (MediaMTX)

Download MediaMTX from https://github.com/bluenviron/mediamtx and run it:

```bash
./mediamtx
```

Expected output:
```
2026/08/05 16:09:06 INF MediaMTX v1.19.2, linux, amd64
2026/08/05 16:09:06 INF configuration loaded from /home/alexf/mediamtx.yml
2026/08/05 16:09:06 INF [RTSP] started with listeners on :8554 (TCP/RTSP), :8000 (UDP/RTP), :8001 (UDP/RTCP)
2026/08/05 16:09:06 INF [RTMP] started with listener on :1935 (TCP/RTMP)
2026/08/05 16:09:06 INF [HLS] started with listener on :8888 (TCP/HTTP)
2026/08/05 16:09:06 INF [WebRTC] started with listeners on :8889 (TCP/HTTP), :8189 (UDP/ICE)
2026/08/05 16:09:06 INF [SRT] started with listener on :8890 (UDP/SRT)
2026/08/05 16:09:06 INF [MoQ] started with listeners on :8892 (TCP/HTTP2), :8892 (UDP/HTTP3)
```

Once a publisher connects (Step 2 below), MediaMTX will log:
```
2026/08/05 16:09:07 INF [RTSP] [conn 192.168.0.100:34948] opened
2026/08/05 16:09:07 INF [RTSP] [session 950a4d7b] created by 192.168.0.100:34948
2026/08/05 16:09:07 INF [path photo] stream is available and online, 1 track (M-JPEG)
2026/08/05 16:09:07 INF [RTSP] [session 950a4d7b] is publishing to path 'photo'
```

#### Step 2 - Publish a JPEG image as an MJPEG stream via FFmpeg

```bash
ffmpeg -loop 1 -re -i <IMAGE_FILE> -vcodec mjpeg -c:v copy -pkt_size 1400 \
       -f rtsp rtsp://<SERVER_IP>:8554/<STREAM_PATH>
```

Where `<SERVER_IP>` is `configGW_ADDR0.configGW_ADDR1.configGW_ADDR2.configGW_ADDR3`
and `<STREAM_PATH>` matches `RTSP_STREAM_PATH` defined in test_config.h.

**Important - pixel format requirement:** RFC 2435 only supports YUV 4:2:0 (Type 0) and
YUV 4:2:2 (Type 1). Images encoded with YUV 4:4:4 subsampling (e.g., `images/tiger.jpg`)
are not compatible and will not stream correctly. Use `images/tiger_rtp.jpg` which is a
pre-converted YUV 4:2:0 (yuvj420p) version, or convert your own image first:

```bash
ffmpeg -loop 1 -re -i images/tiger_rtp.jpg \
       -vcodec mjpeg -c:v copy -pkt_size 1400 \
       -f rtsp rtsp://192.168.0.100:8554/photo
```

Expected output:
```
ffmpeg version 4.4.2-0ubuntu0.22.04.1 Copyright (c) 2000-2021 the FFmpeg developers
Input #0, image2, from 'images/tiger_rtp.jpg':
  Duration: 00:00:00.04, start: 0.000000, bitrate: 32300 kb/s
  Stream #0:0: Video: mjpeg (Baseline), yuvj420p(pc, bt470bg/unknown/unknown), 1024x822 [SAR 1:1 DAR 512:411], 25 fps, 25 tbr, 25 tbn, 25 tbc
Output #0, rtsp, to 'rtsp://192.168.0.100:8554/photo':
  Stream #0:0: Video: mjpeg (Baseline), yuvj420p(pc, bt470bg/unknown/unknown), 1024x822 [SAR 1:1 DAR 512:411], q=2-31, 25 fps, 25 tbr, 90k tbn, 25 tbc
Stream mapping:
  Stream #0:0 -> #0:0 (copy)
Press [q] to stop, [?] for help
[rtp @ 0x...] RFC 2435 suggests two quantization tables, 1 provided
```

**Note:** The `RFC 2435 suggests two quantization tables` warning is expected for JPEG images
encoded with a single quantization table. The stream is still received correctly by the board.

## Supported Boards

- EVKB-MIMXRT1170

## Network Requirements

- Ethernet or WiFi connectivity
- TCP/IP stack (lwIP)
- Sufficient network bandwidth for video streaming

## Test Applications

See the following tests for complete implementations:

- `tests/test_image_jpeg_rtsp_client/` - JPEG image reception and display

## References

- [RFC 2326 - RTSP Protocol](https://tools.ietf.org/html/rfc2326)
- [RFC 3550 - RTP Protocol](https://tools.ietf.org/html/rfc3550)
- [RFC 2435 - RTP Payload Format for JPEG](https://tools.ietf.org/html/rfc2435)
- [MPP API Documentation](mpp_api/mpp_api.md)
- [HAL API Documentation](hal_api/hal_api.md)

## Limitations

- Supported stream format: MJPEG (JPEG over RTP, RFC 2435)
- Maximum concurrent streams: 1
- Supported protocols: UDP for RTP transport

### RTSP URL format constraints

The URL parser accepts only `rtsp://<IPv4-dotted-decimal>:<port>/<path>`.
Hostnames, IPv6 addresses, and URLs without an explicit port number are not supported.
Example: `rtsp://192.168.0.100:8554/photo`

### RFC 2435 JPEG type constraints

Only RFC 2435 types 0 and 1 (YUV 4:2:0 / 4:2:2, no restart interval) are supported.
Frames with type >= 64 (DRI required) are discarded with a log message.

## Future Enhancements

- H.264 stream reception support
- Adding support for more data formats
- Multiple stream support
