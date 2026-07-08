# MCU Media Processing Pipeline

This is the documentation for the MCU Media Processing Pipeline API.

## Features Overview

The Media Processing Pipeline for MCUs is a software library for constructing graphs of media-handling
components for Vision-specific applications.

This is a clean and simple API which makes it easy to build and prototype vision-based applications

⚠️ **AI Components Disclaimer**

The AI components are provided solely for demonstration, evaluation, and benchmarking purposes and are not designed, licensed, or authorized for production use, deployment, or reliance in any operational setting or safety-related environment. Any use beyond NXP's expressly defined intended purpose is undertaken entirely at the Licensee's own risk and responsibility, including all applicable regulatory, compliance, and liability obligations. Nothing herein shall be construed as a representation or warranty that the AI components are production-ready or suitable for use in real-world or regulated environments. The AI components may not be used to train, fine tune, adapt, or create derivative AI models, nor to extract or reuse model artefacts, except as expressly permitted for the stated demonstration purposes.

## Concept

The concept behind the API is to create a Media Processing Pipeline (MPP) based on processing elements.
The basic pipeline structure - the _mpp_ in the API context - has a chain/queue structure which begins
with a **source element**:
- Camera (MIPI CSI, FlexIO, USB/UVC)
- Static image (from memory buffer)
- File source (from SD card storage)
- Multicore source (receive frames from a paired core over RPMsg)

The pipeline continues with multiple **processing elements** having a single input and a single output:
- Image format conversion (color space conversion, scaling, cropping, rotation, flip)
- Image decoder/decompressor (JPEG SW/HW, H.264 SW)
- Labeled rectangle and landmarks drawing
- Machine learning inference using the following frameworks:
   - TensorFlow Lite Micro
   - ExecuTorch

The pipeline can be closed by adding a **sink element**:
- Display panel
- File sink (write to SD card storage)
- RTSP/RTP network streaming sink
- Null sink
- Multicore sink (send frames to a secondary core over RPMsg)

## Multicore Pipelines

On multicore SoCs (e.g. i.MX RT1170 with Cortex-M7 + M4), the MPP framework supports
distributing pipeline processing across cores using a **multicore source** and a
**multicore sink** element pair connected over RPMsg/MCMGR.

```
Core 0 (CM7)                         Core 1 (CM4)
-----------                          -----------
Camera -> Convert -> MC Sink  ====>  MC Source -> Inference -> Display
```

- **`mpp_mc_sink_add()`** -- closes a pipeline branch on one core and transmits frames
  to the other core via shared memory and RPMsg signalling.
- **`mpp_mc_source_add()`** -- opens a pipeline branch on the receiving core that
  consumes frames sent by the paired MC sink.

The underlying HAL multicore device (`hal_mc`) handles RPMsg endpoint setup,
buffer negotiation between cores, synchronization locks, and the enqueue/dequeue
of shared frame buffers. Both elements must be configured with matching
`mpp_mc_params_t` (local/remote RPMsg endpoint addresses and the MCMGR event data
pointer used for inter-core signalling).

Key constraints:
- Each MC sink/source pair must use unique RPMsg endpoint addresses
  (up to `MPP_MAX_RPMSG_EPT_PER_CORE` endpoints per core).
- Call `mpp_boot_secondary_core()` on the primary core before starting the pipeline
  to load and launch the secondary core firmware.
- The secondary core must call `mpp_secondary_core_rpmsg_init()` once it is ready.

An _mpp_ can be **split** when the same media stream must follow different processing paths.
When a **processing element** has longer execution time than the others (for example the ML inference),
the user may create a branch of the pipeline that will run in **background** its processing elements;
meaning that this background branch will start executing when other branches of pipeline are done,
and run until pipeline cyclic period (set by user) is elapsed.

Compatibility of elements and supplied parameters are checked at each step and only compatible elements
can be added in an unequivocal way.

After the construction is complete, each _mpp_ must be started for all hardware and software required
to run the pipeline to initialize. Pipeline processing begins as soon as the last start call is flagged.

Each pipeline branch can be stopped individually. This involves stopping the execution and the hardware
peripherals of the branch. After being stopped, each branch can be started again.
To stop the whole pipeline you need to stop each of its branches separately.

At runtime the application receives events from the pipeline processing and may use these events to
update element parameters. For example, in object detection when the label of a bounding box must be
updated whenever a new object is detected.

Summarizing, the application controls:
- Creation of the pipeline
- Instantiation of processing elements
- Connection of elements to each other
- Prioritizing processing elements
- Reception of callbacks based on specific events
- Updating specific elements (not all elements can be updated)
- Stop of the pipeline (includes stop of the hardware peripherals)

Application does not control:
- Memory management
- Data structures management

The order in which an element is added to the pipeline defines its position within this pipeline,
therefore the order is important.

## Supported Examples

| Example | Description |
|---------|-------------|
| camera_view | Basic camera preview pipeline |
| camera_mobilenet_view | Image classification using MobileNet (TFLite) |
| camera_ultraface_view | Face detection using UltraFace slim (TFLite) |
| camera_persondetect_view | Person detection using FastestDet (TFLite) |
| camera_nanodet_view | Object detection using NanoDet-m (TFLite) |
| camera_mobilefacenet_view | Face recognition using MobileFaceNet (TFLite) |
| camera_ultraface_mobilefacenet_view | Face detection + recognition combined pipeline |
| camera_gesture_recognition_view | Hand gesture recognition using Blaze + landmark + classifier (TFLite) |
| camera_usb_final_fr_app_view | USB camera face recognition application |
| static_image_mobilenet_view | Image classification from static image |
| static_image_ultraface_view | Face detection from static image |
| static_image_persondetect_view | Person detection from static image |
| static_image_nanodet_view | Object detection from static image |
| static_image_mobilefacenet_view | Face recognition from static image |
| static_image_nanodet_persondetect_view | Combined object + person detection from static image |
| static_image_switch_ultraface_antispoofing_mobilefacenet_view | Face detection + anti-spoofing + recognition pipeline |

## Supported Boards

| Board | Camera | Display |
|-------|--------|---------|
| EVKB-MIMXRT1170 | OV5640 | RK055AHD091, RK055MHD091 |
| FRDM-MCX N947 | OV7670 | Mikroe TFT Proto 5", NXP LCD-PAR-S035 |
| FRDM-IMXRT1152 | OV5640 | RK055AHD091, RK055MHD091 |
| MIMXRT700-EVK | OV7670, USB UVC | RK055AHD091, RK055MHD091 |
| FRDM-IMXRT700 | OV7670, USB UVC | Raspberry Pi 7" |

# How To
See the provided examples and reference documentation for practical examples using the MPP API.

To build and run an example:
```
$ python3 ./build_mpp.py -b evkbmimxrt1170 -e camera_view
```

For the full list of build options:
```
$ python3 ./build_mpp.py -h
```
