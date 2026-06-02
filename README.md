# CameraWSLVision

OpenCV 기반 카메라 모니터링 앱입니다. 이 저장소에는 `src/main.cpp`만 있으며, OpenCV를 설치하고 빌드하기 위한 환경 구성을 추가했습니다.

## 요구 사항

- Linux (Ubuntu / WSL / WSLg 권장)
- C++17 컴파일러
- CMake 3.16 이상
- OpenCV 개발 라이브러리

## OpenCV 설치 (Ubuntu / Debian)

```bash
sudo apt update
sudo apt install -y build-essential cmake libopencv-dev pkg-config
```

WSL에서 GUI 창을 표시하려면 WSLg 또는 X 서버가 필요합니다.

### OpenCV 경로 문제 해결

CMake가 OpenCV를 찾지 못할 경우, OpenCV 설치 위치를 직접 지정할 수 있습니다.

```bash
export OpenCV_DIR=/path/to/opencv/build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$OpenCV_DIR" ..
```

또는 OpenCV가 `pkg-config`에 등록되어 있다면 다음 명령으로 확인할 수 있습니다.

```bash
pkg-config --modversion opencv4
```

## 빌드

```bash
cd /home/chois/Camera
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

빌드가 완료되면 `build/CameraApp` 실행 파일이 생성됩니다.

## 실행

```bash
cd build
./CameraApp [cameraIndex]
```

- `cameraIndex`는 선택 사항입니다. 생략하거나 `auto`를 입력하면 사용 가능한 카메라를 자동 탐색합니다.
- 특정 장치를 열려면 `./CameraApp 0`, `./CameraApp 1`처럼 숫자 인덱스를 지정합니다.
- Windows에서 카메라를 스트리밍하는 경우 `./CameraApp http://<windows-host-ip>:<port>/<path>`처럼 URL을 지정합니다.
- `q` 또는 `ESC`로 종료합니다.
- `m`으로 모션 감지 오버레이 토글
- `s`로 현재 프레임을 `capture.png`로 저장

### WSL에서 Windows 카메라 스트림 사용

WSL2에서 USB 카메라가 `/dev/video*`로 보이지만 프레임 수신이 timeout 되는 경우, Windows에서 카메라를 열고 WSL 앱은 HTTP/RTSP/MJPEG URL을 읽는 방식이 더 안정적입니다.

WSL에서 Windows 호스트 IP를 확인합니다.

```bash
ip route | awk '/default/ {print $3}'
```

Windows에서 카메라 스트리밍 앱을 실행한 뒤, WSL에서 URL을 지정해 실행합니다.

```bash
./CameraApp http://<windows-host-ip>:8080/video
```

## WSL / WSLg 참고

- `DISPLAY` 또는 `WAYLAND_DISPLAY` 환경 변수가 설정되어 있어야 합니다.
- 카메라 장치를 연결했다면 `/dev/video*`가 있는지 확인하세요.
- `/dev/video*`가 없다면 Windows에서 `usbipd-win`으로 카메라가 WSL에 attach되어 있는지 확인하세요.

## CMake 설정

이 프로젝트는 `CMakeLists.txt`를 사용해 OpenCV를 찾고 `CameraApp` 실행 파일을 생성합니다.
