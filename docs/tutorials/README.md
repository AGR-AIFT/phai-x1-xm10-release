# Tutorials — 단계별 학습 가이드

XM10의 기능을 단계적으로 마스터할 수 있도록 **27개의 실습 예제**를 제공합니다. 각 예제 폴더에는 소스 코드와 상세한 README가 포함되어 있습니다.

> 📖 예제 전체 맵: [examples/README.md](../../examples/README.md)

> 📋 **각 예제 README 의 통일 포맷 (5-step Lab Manual)**:
> ① 목표 → ② 사전 지식 → ③ 핵심 코드 → ④ 실험 (체크포인트 ✅) → ⑤ 다음 단계 + ⚠️ 흔한 실수
>
> 각 예제는 30 분 안에 학습 목표 도달을 기준으로 설계되었습니다. 막히면 ⚠️ 섹션을 먼저 확인하세요.

> 🤖 **AI 트러블슈팅**: Claude Code 에서 `"Ex.XX 가 안 돼"` 라고 말하면 `example-helper` 스킬이
> 해당 예제 README 의 ⚠️ 섹션 + [docs/troubleshooting.md](../troubleshooting.md) 를 인용해 답합니다.

---

## 난이도별 학습 경로

자신의 수준에 맞는 경로를 선택하여 학습하세요.

**입문자 (임베디드 경험 없음)**
> Ex.00 → Ex.01 → Ex.04 → Ex.05 → Ex.07 → Ex.10a → Ex.11

**중급자 (임베디드 기초 있음)**
> Ex.02 → Ex.05a~d → Ex.08 → Ex.10b → Ex.12 → Ex.14

**고급자 (제어 알고리즘 설계)**
> Ex.03 → Ex.06 → Ex.09 → Ex.10c → Ex.13 → Ex.15 → Ex.16 → Ex.17

**유틸리티 (언제든 참고)**
> Ex.18 (디버깅) · Ex.19 (메모리 패턴)

---

## Part 0: Quick Start

| 예제 | 제목 | 난이도 | 학습 내용 |
| :---: | :--- | :---: | :--- |
| [00](../../examples/00_Quick_Start/) | 보드 동작 확인 | 입문 | TSM + LED + USB CDC — 외부 HW 불필요 |

---

## Part 1: XM10 기본 I/O 제어

가장 기초적인 입출력 제어부터 상태 기반 프로그래밍(FSM)까지, 임베디드 제어의 핵심을 익힙니다.

### Button & LED (Ex.01 ~ 03)

| 예제 | 제목 | 난이도 | 학습 내용 |
| :---: | :--- | :---: | :--- |
| [01](../../examples/01_Button_LED_Basic/) | 버튼과 LED 기초 | 초급 | 폴링(Polling) 방식 디지털 I/O 제어 |
| [02](../../examples/02_Button_LED_Event/) | 이벤트와 특수 효과 | 중급 | 클릭 이벤트 감지, 원샷(One-shot) LED 효과 |
| [03](../../examples/03_Button_LED_FSM/) | 상태 머신 제어 | 고급 | 롱 프레스 모드 전환, Task State Machine 구현 |

### External I/O — GPIO & ADC (Ex.04 ~ 06)

| 예제 | 제목 | 난이도 | 학습 내용 |
| :---: | :--- | :---: | :--- |
| [04](../../examples/04_Ext_IO_Basic/) | 외부 디지털 제어 | 초급 | 확장 포트의 외부 스위치/LED 제어 |
| [05](../../examples/05_Ext_IO_analog/) | 아날로그 센서 입문 | 초급 | 고정 ADC 핀으로 전압 측정 |
| [05a](../../examples/05a_Ext_IO_DIO_to_ADC/) | DIO→ADC 동적 전환 | 초급 | DIO 핀을 ADC 모드로 전환하여 FSR 읽기 |
| [05b](../../examples/05b_Ext_IO_FSR_8ch/) | FSR 8채널 읽기 | 중급 | 8채널 일괄 전환 + Resolution 설정 |
| [05c](../../examples/05c_Ext_IO_Mixed_ADC/) | 혼합 ADC 구성 | 중급 | 고정 ADC 4핀 + DIO→ADC 8핀 (최대 12채널) |
| [05d](../../examples/05d_Ext_IO_DIO_ADC_Hybrid/) | DIO/ADC 하이브리드 | 응용 | 디지털 입출력 + 아날로그 입력 동시 활용 |
| [06](../../examples/06_Ext_IO_Safety_Switch/) | 외부 안전 제어 | 고급 | 리미트 스위치 기반 안전 상태 머신 |

> **ADC 시리즈 (Ex.05 → 05a → 05b → 05c → 05d):** 아날로그 센서 활용을 기초부터 응용까지 순차적으로 다루는 튜토리얼입니다. 순서대로 학습하는 것을 권장합니다.

---

## Part 2: USB 통신 & 데이터 로깅

PC와의 실시간 통신 및 데이터 로깅 기능을 활용하여 개발 효율을 극대화합니다.

### USB-CDC 시리얼 통신 (Ex.07 ~ 09)

| 예제 | 제목 | 난이도 | 학습 내용 |
| :---: | :--- | :---: | :--- |
| [07](../../examples/07_CDC_Basic_Print/) | USB 시리얼 기초 | 초급 | PC 터미널로 텍스트 메시지 전송 |
| [08](../../examples/08_CDC_Sensor_Print/) | 센서 데이터 모니터링 | 중급 | sprintf를 활용한 실시간 데이터 출력 |
| [09](../../examples/09_CDC_Stream/) | 고속 바이너리 스트리밍 | 응용 | PhAI V2 프로토콜, 500Hz 고속 전송 |

### USB-MSC 데이터 로깅 (Ex.10 ~ 10c)

| 예제 | 제목 | 난이도 | 학습 내용 |
| :---: | :--- | :---: | :--- |
| [10](../../examples/10_MSC_Manual_log/) | 수동 바이너리 로깅 | 레거시 | TSM 연동 로깅 (10a/10b/10c 사용 권장) |
| [10a](../../examples/10a_MSC_Basic_Log/) | 등록 기반 자동 로깅 | 초급 | 구조체 등록만으로 2ms 자동 저장 |
| [10b](../../examples/10b_MSC_Custom_Struct/) | 커스텀 구조체 로깅 | 중급 | 사용자 정의 데이터 + 수동 타임스탬프 |
| [10c](../../examples/10c_MSC_Advanced_Log/) | 고급 로깅 시스템 | 고급 | 롤링 파일, 에러 모니터링, LED 피드백 |

> **MSC 시리즈 (Ex.10a → 10b → 10c):** USB 메모리 데이터 로깅을 단계별로 학습합니다. 10번은 레거시이므로 10a부터 시작하세요.

---

## Part 3: KIT H10 로봇 제어

실제 웨어러블 로봇(KIT H10)을 제어하는 고급 알고리즘을 구현합니다.

| 예제 | 제목 | 난이도 | 학습 내용 |
| :---: | :--- | :---: | :--- |
| [11](../../examples/11_Passive_Mode/) | 패시브 모드 | 응용 | P-Vector 기반 자동 왕복 운동 모드 |
| [12](../../examples/12_Active_Assist_Mode/) | 액티브 어시스트 | 응용 | 사용자 의도 감지 + 보행 보조 토크 제어 |
| [13](../../examples/13_Resistive_Mode/) | 저항 모드 | 응용 | 내장 모드 활용, 물속 걷기 저항감 구현 |

---

## Part 4: 심화 프로젝트

제어 이론, AI, 보행 의도 인식 등 고급 알고리즘을 XM10에 직접 구현합니다.

### 제어 이론

| 예제 | 제목 | 난이도 | 학습 내용 |
| :---: | :--- | :---: | :--- |
| [14](../../examples/14_PD_Realtime_Control/) | PD 실시간 토크 제어 | 중급 | PD 제어 수식, 이산 미분, 토크 포화 |
| [15](../../examples/15_Inverted_Pendulum_Control/) | 역진자 모델 보행 보조 | 고급 | 중력 보상 MgL·sin(θ), Lyapunov 안정성 |

### AI & 의도 인식

| 예제 | 제목 | 난이도 | 학습 내용 |
| :---: | :--- | :---: | :--- |
| [16](../../examples/16_TinyAI_Sensor_Fusion/) | Tiny AI 센서 퓨전 | 고급 | 상보 필터 + 3-layer NN MCU 추론 |
| [17](../../examples/17_FSM_Gait_Intent/) | FSM 보행 의도 인식 | 고급 | 7-phase 보행 FSM + 단계별 보조 토크 |

### 유틸리티

| 예제 | 제목 | 난이도 | 학습 내용 |
| :---: | :--- | :---: | :--- |
| [18](../../examples/18_Debug_Monitor/) | 시스템 디버깅 모니터 | 중급 | 루프 프로파일링, Health 대시보드 |
| [19](../../examples/19_Memory_Aware_Design/) | 메모리 인식 설계 | 중급 | 링 버퍼, 풀 할당자, 이동 평균 |

### 로드맵

| 주제 | 설명 |
| :--- | :--- |
| 강화학습 기반 제어 | AM(Jetson) + XM10 연동 RL 파이프라인 |
| PhAI Studio 연동 학습 | 데이터 수집 → 전처리 → 학습 → 배포 |

---

## 예제 사용 방법

1. `examples/` 폴더에서 원하는 예제의 `.c` 파일을 확인합니다.
2. 해당 파일의 코드를 `XM_Apps/User_Algorithm/user_app.c`에 복사합니다.
3. 프로젝트를 빌드하고 XM10에 업로드합니다.
4. 각 예제 폴더의 `README.md`에서 동작 원리와 실행 방법을 확인합니다.

> 예제 실행 상세 가이드: [Getting Started — 첫 빌드 & 실행](../getting-started/03-first-build.md)
