# RTSP/RTP Server

## Overview

This document describes how to use the RTSP/RTP server functionality in the eIQ Media Processing Pipeline to stream video over the network.

The RTSP/RTP server allows you to stream camera or processed video data from the MCU to network clients such as ffplay, gstreamer, or other RTSP-compatible players.

## Features

- Real-time video streaming over RTSP/RTP protocol
- Support for encoded video formats like JPEG and H.264
- Integration with MPP pipeline elements

## Architecture

### High-level description

```
+-------------+     +-------------+     +--------------+     +----------------+
|             |     |             |     |              |     |                |
|   Camera    | --> | 2D Convert  | --> |  Encoder     | --> |  RTSP/RTP      |
|             |     |             |     | (e.g. H.264) |     |  Server        |
+-------------+     +-------------+     +--------------+     +----------------+
                                                                    |
                                                                    | Network
                                                                    v
                                                            RTSP Client (ffplay, etc.)
```

### Pipeline Integration

The RTSP/RTP server can be integrated as a sink element in the MPP pipeline, similar to the Display element.

### RTSP/RTP Server Implementation

The RTSP/RTP server is implemented as a separate HAL component in:
- `hal/rtsp_server/rtsp_server.c` - Server implementation
- `hal/rtsp_server/rtsp_server.h` - Server interface and configuration

The server handles:
- **RTSP Protocol**: Session management (OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN)
- **RTP Packetization**: 
  - H.264: NAL unit fragmentation according to RFC 6184 (FU-A packets)
  - JPEG: Frame fragmentation according to RFC 2435
- **Network Tasks**: 
  - RTSP server task for client connections
  - RTSP session task for protocol handling
  - RTP task for media streaming

The server is integrated into the MPP pipeline through the HAL and MPP RTSP sink element.

## Configuration

### Network Configuration

Configure the network settings for your board in the test_config.h file:


```c
/* RTSP Server Configuration */
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

### H.264 Video Encoding and RTSP Streaming Pipeline

The complete implementation can be found in `tests/test_camera_video_encode/test_camera_video_encode.c`.

## Connecting RTSP/RTP Clients

After the board is booted and the RTSP/RTP server is running, you can connect from a Linux PC.

### Using ffplay

#### For H.264 Video Streams

Basic playback:
```bash
ffplay -rtsp_transport udp -i rtsp://192.168.0.102:8554/
```

Low latency playback with no buffering:
```bash
ffplay -rtsp_transport udp -fflags nobuffer -flags low_delay -framedrop -i rtsp://192.168.0.102:8554/
```

#### For JPEG Streams

```bash
ffplay -rtsp_transport udp -fflags nobuffer -flags low_delay -framedrop -i rtsp://192.168.0.102:8554/
```

### Using GStreamer

#### For H.264 Video Streams

```bash
gst-launch-1.0 rtspsrc location=rtsp://192.168.0.102:8554/ protocols=udp ! \
rtph264depay ! h264parse ! avdec_h264 max-threads=1 ! autovideosink sync=false
```

#### For JPEG Streams

```bash
gst-launch-1.0 rtspsrc location=rtsp://192.168.0.102:8554/ protocols=udp ! \
rtpjpegdepay ! jpegparse ! jpegdec ! autovideosink
```

## Supported Boards

- EVKB-MIMXRT1170

## Network Requirements

- Ethernet or WiFi connectivity
- TCP/IP stack (lwIP)
- Sufficient network bandwidth for video streaming 

## Test Applications

See the following tests for complete implementations:

- `tests/test_camera_video_encode/` - Basic camera streaming
- `tests/test_image_jpeg_rtsp_server/` - Static JPEG image streaming

## References

- [RFC 2326 - RTSP Protocol](https://tools.ietf.org/html/rfc2326)
- [RFC 3550 - RTP Protocol](https://tools.ietf.org/html/rfc3550)
- [RFC 6184 - RTP Payload Format for H.264 Video](https://tools.ietf.org/html/rfc6184)
- [RFC 2435 - RTP Payload Format for JPEG](https://tools.ietf.org/html/rfc2435)
- [MPP API Documentation](mpp_api/mpp_api.md)
- [HAL API Documentation](hal_api/hal_api.md)

## Limitations

- Maximum supported resolution for H.264 real-time streaming (30 FPS): 100x100 (limited by high SW encoder processing times)
- Maximum concurrent clients: 1
- Supported protocols: UDP for RTP transport

## Future Enhancements

- H.264 hardware encoding support
- Adding support for more data formats
- Multiple stream support

