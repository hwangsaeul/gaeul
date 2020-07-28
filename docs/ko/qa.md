# QA 점검 리스트

영상 전송 소프트웨어(코드네임: 개울)의 릴리즈 전의 동작을 테스트하기 위한 환경 구축 및 주요 테스트 항목을 기술한 문서 입니다.

## 설치하기

### 동작 환경
하드웨어 구성 환경은 다음과 같습니다.
* NVIDIA TX1

소프트웨어 구동을 위한 환경은 다음과 같습니다.
* Ubuntu 16.04

### 패키지 설치
1. PPA 저장소 추가 하기
```
$ sudo add-apt-repository ppa:hwangsaeul/stable
$ sudo apt-get update
```
2. 영상 전송 에이전트 패키지 설치
```
$ sudo apt-get install libgaeguli1 gaeul2-source-agent
```
3. 에이전트 실행 사용자 등록하기
```
$ sudo usermod -a -G video gaeul
```
### 설정파일 생성
1. 메인 설정 파일 생성 (`/etc/gaeul2/gaeul2.ini`)
다음은 NVIDIA TX1을 사용하는 경우의 예제입니다. *uid* 는 영상 전송 장치의 고유 ID를 의미하며 사용하는 시스템 내에서 유일한 임의의 값을 사용할 수 있습니다. 

```
[org/hwangsaeul/Gaeul2/Source]
uid="your own unique id"
platform="nvidia"
autoclean=true
source-conf-dir="/etc/gaeul2/conf.d"
```
2. 비디오 채널 설정 파일 생성 (`/etc/gaeul2/conf.d/ch0.ini`)
다음은 장치 카메라 정보를 활용한 예제입니다. *name*은 카메라에 부여하는 고유 ID 로 사용자가 임의로 설정할 수 있습니다.
```
[org/hwangsaeul/Gaeul2/Source/Node]
name="ch0"
device="/dev/video1"
bitrate=1000000
fps=15
resolution="1920x1080"
target-uri="srt://host:port?mode=caller"
```
**주의 사항**: 스트리밍 에이전트가 구동하면서 `/etc/gaeul2/conf.d` 디렉토리 내의 모든 파일에 대해서 설정 정보 분석을 시도하므로 해당 디렉토리에는 유효한 비디오 채널 설정 파일만 생성해야합니다. 

## 테스트용 재생기
영상 전송 장치가 정상적으로 구동하는지 확인하기 위한 테스트 환경은 Ubuntu 리눅스를 활용하여 구성할 수 있습니다. Ubuntu 18.04 혹은 20.04 버전이 설치된 PC를 활용하여 영상 재생을 확인 할 수 있습니다.

### 테스트용 재생기 준비하기
준비된 PC에 필요 패키지들을 설치합니다.

```
$ sudo apt-get install gstreamer1.0-tools libgstreamer-plugins-bad1.0-0 
libgstreamer-plugins-good1.0-0
```

### 테스트용 재생기 동작 확인

테스트 재생기을 위한 패키지가 정상 설치 되어있는지 확인합니다.
```
$ gst-inspect-1.0 --exists srtsrc ; echo $?
0
```
만약 위 결과가 `0`이 아닌 다른 값을 출력한다면 설치된 ubuntu 버전이 18.04 혹은 20.04가 아닌지 확인해야 합니다.

다음 명령을 수행하였을 때, 새로운 윈도우에서 테스트 영상이 출력되면 정상적으로 준비가 된것입니다. 

```
$ gst-launch-1.0 videotestsrc ! autovideosink
```
![](./img/videotestsrc.png)

### 테스트용 재생기 구동
영상 수신을 할 수 있도록 다음 명령을 통해 영상 수신을 대기 하도록합니다.
```
$ gst-player-1.0 srt://:50010?mode=listener
```
시험 장치에서 영상 송출하는 경우 새로운 창에서 수신된 영상이 표시됩니다.

## 체크리스트

영상 전송 장치 (NVIDIA TX1) 내의 다음 항목을 확인 해야합니다.

<table>
<tr>
  <th>항목</th>
  <th>내용</th>
  <th>예제</th>
  <th>Pass/Fail</th>
</tr>
<tr>
  <td rowspan="3"> 1. 메인 설정 파일 (`/etc/gaeul2/gaeul2.ini`) </td>
  <td>1.1 *uid*가 장치별로 고유한 값을 가지고 있는가?  </td>
  <td>4a7R8hqb</td>
  <td></td>
</tr>
<tr>
  <td> 1.2 *platform*의 값이 테스트하는 장치와 일치하는가?  </td>
  <td>nvidia</td>
  <td></td>
</tr>
<tr>
  <td> 1.3 *source-conf-dir*의 값이 절대 경로이며 해당 디렉토리가 존재하는가?  </td>
  <td>`/etc/gaeul2/conf.d`</td>
  <td></td>
</tr>
<tr>
  <td rowspan="4"> 2. 비디오 채널 설정 (`/etc/gaeul2/conf.d/`) </td>
  <td> 2.1 메인 설정 파일의 *source-conf-dir*에 설정한 디렉토리에 비디오 채널 파일이 생성되었는가?</td>
  <td></td>
  <td></td>
</tr>
<tr>
  <td> 2.2 비디오 채널 설정 파일은 1개의 파일만 생성하였는가? </td>
  <td>ch0.ini</td>
  <td></td>
</tr>
<tr>
  <td> 2.3 해당 파일내의 *name* 항목은 카메라의 고유 ID로 설정하였는가?</td>
  <td>ch0</td>
  <td></td>
</tr>
<tr>
  <td> 2.4 *target-uri*의 값은 영상을 수신하는 장치의 주소로 올바르게 설정되었는가?</td>
  <td> `srt://172.16.0.1:50010?mode=caller` </td>
  <td></td>
</tr>
<tr>
  <td rowspan="3"> 3. 영상 전송 에이전트 구동</td>
  <td> 3.1 `sudo systemctl start gaeul2-source-agent` 실행 후 `systemctl is-active gaeul2-source-agent`의 결과가 `active` 인가? </td>
  <td></td>
  <td></td>
</tr>
<tr>
  <td> 3.2 테스트 재생기에서 3.1 명령 수행후 10초 이내에 영상이 출력되는가?</td>
  <td></td>
  <td></td>
</tr>
<tr>
  <td> 3.3 `sudo systemctl stop gaeul2-source-agent` 실행 시 1초 이내에 테스트 재생기에서 영상출력이 종료 되는가?</td>
  <td></td>
  <td></td>
</tr>
</table>
