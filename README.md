# LTTNG 사용법

# 1.  lttng-record-trace

[https://github.com/dorsal-lab/lttng-utils](https://github.com/dorsal-lab/lttng-utils)

내가 추적하고자 하는 프로그램을 컴파일 할때는 다음 옵션을 추가하여 컴파일한다.

```bash
-g -o2 -finstrument-functions
```

추적 프로그램 수행 방법

```bash
 #프로그램이 동작하는 기간동안 myapp프로그램의 user evevt 부분을 추적한다
 lttng-record-trace -p cyg_profile ./myapp
 
 #프로그램이 동작하는 기간동안 user event 및 kernel 부분을 추적한다.
 #다만 이런 경우 user-event 부분에 추적할게 너무 많다면 일부분은 녹화가 안되어 있을 수도 있다.
 lttng-record-trace -p cyg_profile,kernel ./myapp

	#Primary프로그램의 주소별 함수에 대한 정보를 저장(mapping)
	#프로그램을 실행시킨 HW랑 Tracecompass를 실행시킨 HW가 다를때 필요.
	nm --demangle Primary > Primary.txt
```

# 2. lttng

```bash
sudo lttng create DMR #새로운 녹화 파일을 만든다.
sudo lttng enable-event -k -a #kernel의 모든 부분을 추적한다.
sudo lttng enable-event -u -a #user의 모든 부분을 추적한다.
sudo lttng start #녹화를 시작한다.

#프로그램을 실행하기 전에 이 프로그램을 preload한 뒤 실행시킨다.
#라즈베리파이와 같은 arm기반 ubuntu는 이걸 사용.
LD_PRELOAD=/usr/lib/aarch64-linux-gnu/liblttng-ust-cyg-profile.so ./myapp
#x86기반 ubuntu는 이걸 사용
LD_PRELOAD=/usr/lib/liblttng-ust-cyg-profile.so ./myprogram

sudo lttng stop #녹화 중지
sudo lttng destroy #녹화 파일 연결 해제
```
