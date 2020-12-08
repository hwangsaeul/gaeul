![Build Status](https://github.com/hwangsaeul/gaeul/workflows/CI/badge.svg?branch=master)

# Gaeul
*Gaeul* agents are a set of services that constitue a *Hwangsaeul* (H8L)
deployment. Each deployment may contain several instances of each type of agent.

Currently supported agents are:

## Source agent

Installed on the camera endpoints, source agent provides H.264 or H.265-encoded
live video stream from a compatible capture device.

In order to start streaming, a configuration files for the agent and at least
one channel (video stream) need to be created.

See [org.hwangsaeul.Gaeul2.Source.gschema.xml](gaeul/source/org.hwangsaeul.Gaeul2.Source.gschema.xml)
for all available configuration options. Example configuration for a source
agent node:

```ini
[org/hwangsaeul/Gaeul2/Source]
uid="testinstance"
channel-configs=["/etc/gaeul2/conf.d/channel1.ini"]
```

And the contents of `channel1.ini`, defining a streaming channel:

```ini
[org/hwangsaeul/Gaeul2/Source/Channel]
name="channel1"
device="/dev/video4"
bitrate=4000000
bitrate-control="CBR"
fps=24
resolution="1920x1080"
target-uri="srt://127.0.0.1:8888"
codec="vaapih264"
passphrase="mypassphrase"
prefer-hw-decoding=false
```

Then, to launch the agent:

```console
./gaeul2-source-agent --config source-config.ini
```

### D-Bus API

A running source agent can be controlled through D-Bus API whose interfaces are
documented in [org.hwangsaeul.Gaeul2.Source.xml](gaeul/source/org.hwangsaeul.Gaeul2.Source.xml).

A command line option `--dbus-type` controls to which bus the agent will connect.
The options are `session`, `system`, or `none`, which disables the D-Bus API.

```console
./gaeul2-source-agent --config source-config.ini --bus-type session
```

When using the system bus, the user account that the agent process is running
under must have permission to own `org.hwangsaeul.Gaeul2.Source` name on the
bus. See [org.hwangsaeul.Gaeul2.Source.conf](gaeul/source/org.hwangsaeul.Gaeul2.Source.conf),
which grants the appropriate permissions to user `gaeul`.

## Relay agent

Forwards video streams from M source agents to N receivers, provides
endpoint authentication and controls access to individual video streams.

Configuration options of the relay agent are documented in [org.hwangsaeul.Gaeul2.Relay.gschema.xml](gaeul/relay/org.hwangsaeul.Gaeul2.Source.gschema.xml).
The config file is created and passed to the relay agent in the same manner as
with the source agent.

```ini
[org/hwangsaeul/Gaeul2/Relay]
authentication=true
sink-port=1234
source-port=5678
```

```console
./gaeul2-relay-agent --config relay-config.ini
```

### D-Bus API

Relay agent D-Bus API is described in [org.hwangsaeul.Gaeul2.Relay.xml](gaeul/source/org.hwangsaeul.Gaeul2.Relay.xml).

The agent executable also accepts `--dbus-type` command line option that works
the same way as in the case of `gaeul2-source-agent`.

## PPA nightly builds

Experimental versions of Gaeul are daily generated in [launchpad](https://launchpad.net/~hwangsaeul/+archive/ubuntu/nightly).

```console
$ sudo add-apt-repository ppa:hwangsaeul/nightly
$ sudo apt-get update
$ sudo apt-get install libgaeul2 gaeul2-agent \
                       libgaeul2-source-agent gaeul2-source-agent \
                       libgaeul2-relay-agent gaeul2-relay-agent \
                       libgaeul2-mjpeg-agent gaeul2-mjpeg-agent
```
