# Trace Compass Analysis Guide

LTTng로 수집한 trace 데이터를 Trace Compass로 시각적으로 분석하는 방법을 설명합니다.

## Trace Compass 설치

[Eclipse Trace Compass 공식 사이트](https://eclipse.dev/tracecompass/)에서 다운로드합니다.

## 분석 파일 준비

다음 파일들을 분석 컴퓨터에 모아둡니다:

```
analysis/
├── Primary-u-YYYYMMDD-HHMMSS/    # Primary trace 데이터
├── Secondary-u-YYYYMMDD-HHMMSS/  # Secondary trace 데이터
├── Primary.txt                    # Primary symbol mapping
├── basicmath_large_P.txt          # Primary mission symbol mapping
├── Secondary.txt                  # Secondary symbol mapping
└── basicmath_large_S.txt          # Secondary mission symbol mapping
```

> trace 폴더는 root 권한으로 생성되므로 권한을 변경해야 합니다:
> ```bash
> sudo chmod -R 777 *
> ```

## Trace 파일 로드

1. Trace Compass 실행
2. **File > Open Trace** 에서 trace 폴더를 선택하여 로드
3. 로드된 trace를 더블클릭하여 **Flame Chart** 표시

## Symbol Mapping 적용

Flame Chart에서 함수 이름이 16진수로 표시되는 경우 Symbol Mapping이 필요합니다.

1. Flame Chart 뷰에서 **Configure Symbol** 버튼 클릭
2. 앞서 생성한 `.txt` 파일 (nm --demangle 출력) 을 선택
3. 적용하면 함수 이름이 정상적으로 표시됨

## 분석 방법

### Flame Chart 분석

Flame Chart를 통해 각 프로세스 내 함수가 **어느 시간에 얼마만큼 동작했는지** 확인할 수 있습니다.

확인 가능한 항목:
- **CBIT Loop**: 한 loop 내에서 여러 고장에 대한 판별이 수행되는지
- **Heartbeat Loop**: 설정한 주기(예: 100ms)대로 정확히 동작하는지
- **Mission Process**: 임무가 정상적으로 수행되고 있는지

### 고장 복구 흐름 분석

Flame Chart와 시스템 FlowChart를 비교하여 고장 복구 프로세스가 설계대로 동작하는지 검증합니다.

#### 정의되지 않은 고장 (전원 절체) - 복구 시간 695ms

1. Primary 보드의 모든 프로세스가 동작 정지
2. Secondary 보드가 Heartbeat 3회 미수신 감지
3. Secondary 보드에서 Mission 대체 수행 시작

<p align="center">
  <img src="images/trace-power-fault.png" alt="Power Fault Recovery Trace" width="700"/>
</p>

- 고장 발생 시각: 42.082s
- Secondary 고장 인지 시각: 42.607s (Heartbeat 3회 미수신 대기)
- Secondary Mission 시작 시각: 42.777s
- **총 복구 시간: 695ms**

#### 사전에 정의된 고장 (미션 동작 중지) - 복구 시간 470ms

1. Primary 보드의 CBIT이 메모리 고장으로 인한 미션 중지를 감지
2. Alert 신호가 Heartbeat 메시지를 통해 Secondary로 즉시 전송
3. Secondary 보드가 Alert 수신 즉시 Mission 대체 수행 시작

<p align="center">
  <img src="images/trace-mission-fault.png" alt="Mission Fault Recovery Trace" width="700"/>
</p>

- 메모리 고장 발생 시각: 29.482s
- Primary 고장 검출 시각: 29.754s (CBIT 감지, 272ms)
- Secondary 고장 인지 시각: 29.793s (Alert 수신, 39ms)
- Secondary Mission 시작 시각: 29.952s (미션 시작, 159ms)
- **총 복구 시간: 470ms** (Heartbeat 기반 대비 약 32% 단축)
