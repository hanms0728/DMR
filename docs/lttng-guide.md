# LTTng Tracing Guide

DMR 시스템의 함수 호출 흐름을 LTTng로 추적하는 방법을 설명합니다.

## 사전 준비

### LTTng 설치

```bash
sudo apt update
sudo apt install -y lttng-tools lttng-modules-dkms liblttng-ust-dev
sudo apt install -y python3-setuptools python3-yaml
```

### lttng-utils 설치

```bash
git clone https://github.com/tahini/lttng-utils.git
cd lttng-utils
sudo python3 setup.py install
```

### 컴파일 옵션

추적 대상 프로그램은 반드시 다음 옵션으로 컴파일해야 합니다:

```bash
gcc program.c -g -O2 -finstrument-functions -w -o program
```

## 방법 1: lttng-record-trace (권장)

가장 간단한 방법입니다. 프로그램 실행과 동시에 자동으로 트레이싱을 수행합니다.

```bash
# User event만 추적
sudo lttng-record-trace -p cyg_profile ./Primary 100 1 -p basicmath_large

# User event + Kernel event 동시 추적
# (user-event가 많으면 일부 누락될 수 있음)
sudo lttng-record-trace -p cyg_profile,kernel ./Primary 100 1 -p basicmath_large
```

실행이 완료되면 trace 데이터가 담긴 폴더가 자동 생성됩니다.
예: `Primary-u-20260220-165840`

## 방법 2: lttng 수동 제어

세밀한 제어가 필요할 때 사용합니다.

```bash
# 1. 세션 생성
sudo lttng create DMR --output=./trace

# 2. 추적 대상 설정
sudo lttng enable-event -k -a    # kernel 전체 추적
sudo lttng enable-event -u -a    # user 전체 추적

# 3. 녹화 시작
sudo lttng start

# 4. 프로그램 실행 (LD_PRELOAD 필수)
# ARM64 (Raspberry Pi)
LD_PRELOAD=/usr/lib/aarch64-linux-gnu/liblttng-ust-cyg-profile.so ./Primary 100 1 -p basicmath_large

# x86_64
LD_PRELOAD=/usr/lib/liblttng-ust-cyg-profile.so ./Primary 100 1 -p basicmath_large

# 5. 녹화 중지 및 세션 해제
sudo lttng stop
sudo lttng destroy
```

## Symbol Mapping 파일 생성

Trace Compass에서 함수 이름을 표시하려면 symbol mapping 파일이 필요합니다.
프로그램을 **실행한 보드**에서 다음 명령어를 수행합니다:

```bash
# Primary 보드
nm --demangle Primary > Primary.txt
nm --demangle basicmath_large > basicmath_large_P.txt

# Secondary 보드
nm --demangle Secondary > Secondary.txt
nm --demangle basicmath_large > basicmath_large_S.txt
```

생성된 trace 폴더와 `.txt` 파일을 분석 컴퓨터로 복사한 후 Trace Compass로 분석합니다.

> Trace Compass 분석 방법은 [Trace Compass Analysis Guide](trace-compass-guide.md)를 참고하세요.
