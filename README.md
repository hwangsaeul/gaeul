Gaeul
-----

*개울*은 드론 프로젝트에서 사용하기 위한 드론용 어플리케이션 입니다.

## Getting started

### Install meson and ninja

Meson 0.40 and newer is required.

### Build Gaeul

You can get library and executables built runnning:

```
meson build
ninja -C build
```

### Run

#### Install

```
ninja -C build install
```

#### Compile gschema

```
glib-compile-schemas /usr/local/share/glib-2.0/schemas/
```
#### Run

```
/usr/local/bin/gaeul-agent
```

### Configurations

#### Get 

```
$ gsettings get org.hwangsaeul.Gaeul edge-id
'randomized-string'
$ gsettings get org.hwangsaeul.Gaeul video-source
'v4l2src'
$ gsettings get org.hwangsaeul.Gaeul video-device
'/dev/video0'
$ gsettings get org.hwangsaeul.Gaeul encoding-method
'general'
```

#### Set 

```
$ gsettings set org.hwangsaeul.Gaeul encoding-method nvidia-tx1
```
