# Tutorials — 단계별 학습 가이드

XM10 의 기능을 단계적으로 마스터할 수 있도록 **41 개의 실습 예제** 를 제공합니다. 각 예제 폴더에는 소스 코드와 5-step Lab Manual 형식의 README 가 포함되어 있습니다.

> 📖 예제 전체 인덱스: [examples/README.md](../../examples/README.md)

> 📋 **각 예제 README 통일 포맷 (5-step Lab Manual)**:
> ① 목표 → ② 사전 지식 → ③ 핵심 코드 → ④ 실험 (체크포인트 ✅) → ⑤ 다음 단계 + ⚠️ 흔한 실수
>
> 각 예제는 **30 분 이내 학습 목표 도달** 을 기준으로 설계. 막히면 ⚠️ 섹션 먼저 확인.

> 🤖 **AI 트러블슈팅**: Claude Code 에서 `"Ex.XX 가 안 돼"` 라고 말하면 `example-helper` 스킬이
> 해당 예제 README 의 ⚠️ 섹션 + [docs/troubleshooting.md](../troubleshooting.md) 를 인용해 답합니다.

---

## 추천 학습 경로

| 경로 | 대상 | 권장 순서 |
|------|------|----------|
| **🚀 입문 (3시간)** | 임베디드 처음 — 빠르게 동작 확인까지 | Ex.00 → 01 → 04 → 07 → 10a → 11 |
| **🛠️ 중급 (1주)** | 임베디드 기초 있음 — 통신/로깅 + 실시간 제어 | Ex.02 → 05b → 08 → 10b → 12 → 14 → 18 |
| **🧠 고급 (수업/연구)** | 제어 알고리즘 + AI 설계 | Ex.03 → 09 → 10c → 15 → 16 → 17 → 19 → 20+ |
| **🤖 응용 프로젝트** | Physical AI / 재활 / 학습 제어 | Ex.21 → 31 → 32 → 33 → 36 |

---

## Part 0 — Quick Start

| 예제 | 제목 | 난이도 | 학습 내용 |
| :---: | :--- | :---: | :--- |
| [00](../../examples/00_Quick_Start/) | 보드 동작 확인 | ⭐ | TSM + LED + USB CDC — 외부 HW 불필요 |

---

## Part 1 — XM10 기본 I/O 제어

기초 입출력부터 상태 기반 프로그래밍 (FSM) 까지, 임베디드 제어의 핵심.

### Button & LED (Ex.01 ~ 03)

| 예제 | 제목 | 난이도 | 학습 내용 |
| :---: | :--- | :---: | :--- |
| [01](../../examples/01_Button_LED_Basic/) | 버튼 + LED 기초 | ⭐ | 폴링 방식 디지털 I/O |
| [02](../../examples/02_Button_LED_Event/) | 이벤트 + 특수 효과 | ⭐⭐ | 클릭 이벤트, Oneshot LED |
| [03](../../examples/03_Button_LED_FSM/) | FSM 상태 머신 | ⭐⭐ | LongPress 모드 전환, Task State Machine |

### External I/O — GPIO & ADC (Ex.04 ~ 06)

| 예제 | 제목 | 난이도 | 학습 내용 |
| :---: | :--- | :---: | :--- |
| [04](../../examples/04_Ext_IO_Basic/) | 외부 디지털 제어 | ⭐⭐ | 확장 포트 외부 스위치/LED |
| [05](../../examples/05_Ext_IO_analog/) | 아날로그 센서 입문 | ⭐⭐ | 고정 ADC 핀 전압 측정 |
| [05a](../../examples/05a_Ext_IO_DIO_to_ADC/) | DIO→ADC 동적 전환 | ⭐⭐ | DIO 핀을 ADC 모드로 FSR 읽기 |
| [05b](../../examples/05b_Ext_IO_FSR_8ch/) | FSR 8채널 일괄 | ⭐⭐ | 8 채널 + Resolution 설정 |
| [05c](../../examples/05c_Ext_IO_Mixed_ADC/) | 혼합 ADC 12채널 | ⭐⭐⭐ | 고정 4 + DIO→ADC 8 |
| [05d](../../examples/05d_Ext_IO_DIO_ADC_Hybrid/) | DIO/ADC 하이브리드 | ⭐⭐⭐ | 디지털 + 아날로그 동시 |
| [06](../../examples/06_Ext_IO_Safety_Switch/) | 안전 상태 머신 | ⭐⭐ | 리미트 스위치 기반 FSM |

> **ADC 시리즈 권장 순서:** Ex.05 → 05a → 05b → 05c → 05d (점진 난이도)

---

## Part 2 — USB 통신 + 데이터 로깅

PC ↔ 보드 실시간 통신과 USB 메모리 로깅 — 디버깅 + 데이터 수집의 핵심.

> ⚠️ **USB-CDC 단일 점유**: Ex.07~09 는 시리얼 터미널 또는 PhAI Studio **둘 중 하나만** 사용. 동시 점유 시 충돌.

### USB-CDC 시리얼 통신 (Ex.07 ~ 09)

| 예제 | 제목 | 난이도 | 학습 내용 |
| :---: | :--- | :---: | :--- |
| [07](../../examples/07_CDC_Basic_Print/) | USB 시리얼 기초 | ⭐⭐ | PC 터미널 텍스트 메시지 |
| [08](../../examples/08_CDC_Sensor_Print/) | 센서 데이터 모니터링 | ⭐⭐ | sprintf + 실시간 출력 |
| [09](../../examples/09_CDC_Stream/) | 고속 바이너리 스트리밍 | ⭐⭐⭐ | PhAI V2 프로토콜, 500 Hz |

### USB-MSC 데이터 로깅 (Ex.10 ~ 10c)

| 예제 | 제목 | 난이도 | 학습 내용 |
| :---: | :--- | :---: | :--- |
| [10](../../examples/10_MSC_Manual_log/) | 수동 로깅 (legacy) | ⭐⭐ | TSM 연동 (10a/10b/10c 권장) |
| [10a](../../examples/10a_MSC_Basic_Log/) | 등록 기반 자동 로깅 | ⭐⭐ | 구조체 등록만으로 자동 저장 |
| [10b](../../examples/10b_MSC_Custom_Struct/) | 커스텀 구조체 | ⭐⭐⭐ | 사용자 정의 + 수동 timestamp |
| [10c](../../examples/10c_MSC_Advanced_Log/) | 고급 로깅 시스템 | ⭐⭐⭐ | 롤링 파일, 에러 모니터링, LED 피드백 |

> **MSC 시리즈 권장 순서:** Ex.10a → 10b → 10c (10 은 legacy).

---

## Part 3 — KIT H10 외골격 모드

실제 웨어러블 로봇 (KIT H10) 의 3 가지 기본 동작 모드.

| 예제 | 제목 | 난이도 | 학습 내용 |
| :---: | :--- | :---: | :--- |
| [11](../../examples/11_Passive_Mode/) | Passive Mode | ⭐⭐⭐ | P/I-Vector 기반 자동 왕복 |
| [12](../../examples/12_Active_Assist_Mode/) | Active Assist Mode | ⭐⭐⭐ | 의도 감지 + 보조 토크 |
| [13](../../examples/13_Resistive_Mode/) | Resistive Mode | ⭐⭐ | H10 내장 모드 + 물속 걷기 저항감 |

---

## Part 4 — 제어 알고리즘 기초

학생이 직접 구현하는 첫 사용자 정의 제어 알고리즘.

| 예제 | 제목 | 난이도 | 학습 내용 |
| :---: | :--- | :---: | :--- |
| [14](../../examples/14_PD_Realtime_Control/) | PD 실시간 토크 제어 | ⭐⭐⭐ | 이산 PD + 토크 포화 |
| [15](../../examples/15_Inverted_Pendulum_Control/) | 역진자 모델 보행 보조 | ⭐⭐⭐ | 중력 보상 MgL·sin(θ) + Lyapunov |
| [16](../../examples/16_TinyAI_Sensor_Fusion/) | Tiny AI 센서 퓨전 | ⭐⭐⭐ | 상보 필터 + 3-layer NN MCU 추론 |
| [17](../../examples/17_FSM_Gait_Intent/) | FSM 보행 의도 인식 | ⭐⭐⭐ | 7-phase 보행 FSM + 단계별 토크 |
| [18](../../examples/18_Debug_Monitor/) | 시스템 디버깅 모니터 | ⭐⭐ | 루프 프로파일링, Health 대시보드 |
| [19](../../examples/19_Memory_Aware_Design/) | 메모리 인식 설계 | ⭐⭐⭐ | Ring Buffer, Pool, sizeof |

---

## Part 5 — 제어 알고리즘 심화 (Algorithm Classic)

상호작용 역학 + 투명 모드 + 보행 위상 기반 제어 — 외골격 제어의 정통 기법.

| 예제 | 제목 | 난이도 | 학습 내용 |
| :---: | :--- | :---: | :--- |
| [20](../../examples/20_Impedance_Control/) | Hogan Impedance | ⭐⭐⭐ | 가상 스프링-댐퍼 상호작용 (Hogan 1985) |
| [21](../../examples/21_Gravity_Compensation/) | Gravity Compensation | ⭐⭐⭐ | Mgl·sin(θ) + 마찰 보상 + α 점진 활성화 |
| [22](../../examples/22_CPG_Oscillator/) | CPG Adaptive Oscillator | ⭐⭐⭐ | 적응 주파수 φ·ω (Ronsse 2011) |
| [23](../../examples/23_Gait_Phase_Adaptive_Torque/) | Gait Phase Adaptive | ⭐⭐⭐ | 4 구간 정현파 토크 (Quinlivan 2017) |
| [24](../../examples/24_Virtual_Constraint/) | Virtual Constraint / HZD | ⭐⭐⭐ | 5 차 Bézier (Westervelt 2003) |
| [25](../../examples/25_Stance_Stiffness_Modulation/) | Stance Stiffness | ⭐⭐⭐ | 입각/유각 LPF blending (Collins 2015) |

---

## Part 6 — 학습 + 적응 제어 (Learning / Adaptive)

매 보행 주기마다 학습하거나 사용자 변화에 자동 적응하는 제어.

| 예제 | 제목 | 난이도 | 학습 내용 |
| :---: | :--- | :---: | :--- |
| [26](../../examples/26_Iterative_Learning_Control/) | Iterative Learning Control | ⭐⭐⭐ | P-type ILC + 위상 인덱싱 (Emken 2007) |
| [27](../../examples/27_MRAC/) | Model Reference Adaptive | ⭐⭐⭐ | MIT Rule + Projection (Slotine 1991) |
| [28](../../examples/28_Admittance_Control/) | Admittance Control | ⭐⭐⭐ | 힘 → 위치 (Impedance 쌍대, Keemink 2018) |
| [29](../../examples/29_Bilateral_Coordination/) | Bilateral Coordination | ⭐⭐⭐ | 역위상 대칭 + 약측 강화 (재활) |
| [30](../../examples/30_FF_FB_Hybrid_Control/) | FF + FB Hybrid | ⭐⭐⭐ | Computed Torque + BTN3 비교 토글 |

---

## Part 7 — Advanced Applications (Physical AI)

투명성 → 의도 감지 → 학습 → 자율 재생 — Physical AI 의 핵심 stage.

| 예제 | 제목 | 난이도 | 학습 내용 |
| :---: | :--- | :---: | :--- |
| [31](../../examples/31_Friction_Comp_DOB/) | Friction Comp DOB | ⭐⭐⭐ | 외란 관측기 — Physical Transparency 완성 |
| [32](../../examples/32_GRF_Gait_Intent/) | GRF Gait Intent | ⭐⭐⭐ | Stage 2 의도 감지 (Heel Strike) |
| [33](../../examples/33_Kinesthetic_Teaching/) | Kinesthetic Teaching | ⭐⭐⭐ | 전문가 시연 캡처 + 재생 (Stage 5) |
| [34](../../examples/34_MSC_GaitAnalysis_Log/) | MSC GaitAnalysis Log | ⭐⭐ | H10 → USB → Python → MATLAB |
| [35](../../examples/35_MultiLayer_Transparent_Control/) | MultiLayer Transparent | ⭐⭐⭐ | Zero-Imp / Virtual Wall / Bilateral 전환 |
| [36](../../examples/36_OnDevice_Kinesthetic_Learning/) | On-Device Learning | ⭐⭐⭐ | MCU 위 Tiny NN 학습 + LQR 재생 |

---

## 예제 사용 방법

1. `examples/` 폴더에서 원하는 예제의 `.c` 파일 확인.
2. 해당 파일 내용을 `XM_Apps/User_Algorithm/user_app.c` 에 복사.
3. STM32CubeIDE 에서 빌드 → XM10 에 업로드.
4. 각 예제 폴더의 `README.md` 5-step 가이드 따라가기.

> 첫 빌드/실행 상세: [Getting Started — 첫 빌드 & 실행](../getting-started/03-first-build.md)
> 🤖 첫 환경 구축: [Claude Code Quick Start](../getting-started/00-claude-code-quickstart.md)
