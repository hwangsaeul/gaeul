# Gaeul
*Gaeul* is streaming server for Edge devices based on SRT protocol

## Overview
Gaeul Agent is the streaming server of the project. It uses
* [**chamge**](https://github.com/hwangsaeul/chamge): to register to the network and receive messages

* [**gaeugli**](https://github.com/hwangsaeul/gaeguli): to handle the SRT streaming

At start, `Gaeul Agent` registers itself in the network using `chamge` and `AMQP` with its `edge_id` identifier. At this point it is able to receive commands from others devices to handle streaming. Using `gaeugli` it configures the streaming parameters and sends it to the requested URI.

## Settings
Schema: org.hwangsaeul.Gaeul

```console
$ gsettings list-recursively org.hwangsaeul.Gaeul
org.hwangsaeul.Gaeul default-srt-target-uri 'srt://127.0.0.1:8888?mode=listener'
org.hwangsaeul.Gaeul edge-id '63f88d8379243eb5f6e0efc22f3f0edf6dabbb630656532aac8e06d7bcab764e'
org.hwangsaeul.Gaeul video-device '/dev/video0'
org.hwangsaeul.Gaeul video-source 'v4l2src'
org.hwangsaeul.Gaeul encoding-method 'general'
```

*   **default-srt-target-uri**: URI to use when starting streaming when it is no specified
*   **edge-id**: Unique Edge device identifier, this id is auto generated.
*   **video-device**: Video device used as source
*   **video-source**: Valid gstreamer video source type
*   **encoding-method**: Encoding method [general|nvidia-tx1]

## Chamge API
### streamingStart
Starts the streaming of the Edge device.

*Arguments*
*   url: URI to send streaming to
*   resolution: Streaming resolution
*   fps: Streaming FPS
*   bitrates: Streaming bitrate

### streamingStop
Stops the streaming of the Edge device.

### streamingChangeParameters
Change the streaming parameters.

*Arguments*
*   resolution: Streaming resolution
*   fps: Streaming FPS
*   bitrates: Streaming bitrate

## D-BUS API
### State
Retrieves the agent state, possible values are:
* 0x0 = PAUSED
* 0x1 = PLAYING
* 0x2 = RECORDING

### GetEdgeId
Retrieves the edge\_id of the device

*Return*
*   edge\_id (s): Unique indentifier of the Edge device.

## Build from sources
To build the from sources follow the procedure described in

[Build from sources](https://github.com/hwangsaeul/hwangsaeul.github.io/blob/master/build_from_sources.md)

## Run
After building from sources Gaeul Agent can be run with

```console
/usr/local/bin/gaeul-agent
```

## PPA nightly builds

Experimental versions of Gaeul are daily generated in [launchpad](https://launchpad.net/~hwangsaeul/+archive/ubuntu/nightly).

```console
$ sudo add-apt-repository ppa:hwangsaeul/nightly
$ sudo apt-get update
$ sudo apt-get install gaeul
