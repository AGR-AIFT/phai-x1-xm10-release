# XM10 예제 가이드 — Physical AI 5단계 학습 여정

XM10 Extension Module의 **41개 실습 예제**입니다.
각 예제 폴더에는 소스 코드(`.c`)와 상세 설명(`README.md`)이 포함되어 있습니다.

> 📖 API 전체 명세: [docs/api-reference/](../docs/api-reference/)
> 📘 단계별 학습 가이드: [docs/tutorials/](../docs/tutorials/)
> 🌐 온라인 학습 플랫폼: [onephai.com](https://onephai.com)

---

## Physical AI 5단계 학습 여정

이 예제들은 하나의 철학적 여정을 따릅니다:
**"로봇이 어떻게 인간을 이해하고, 인간과 함께 성장하는가"**

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Foundation   Platform & Tools
             (00~10c, 18, 19 — 플랫폼 기반, 언제든 참고)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Stage 1      Physical Transparency   [System 1]
             로봇이 "보이지 않는 존재"가 된다
             (20 임피던스 · 21 중력보상 · 30 FF+FB · 31 DOB★)
                         ↓
Stage 2      Intent Sensing          [S1 → S2 전환]
             센서 신호에서 인간 의도를 추출한다
             (16 TinyAI · 17 FSM Gait · 22 CPG · 32 GRF★)
                         ↓
Stage 3      Adaptive Assistance     [S1 + S2 협응]
             의도를 읽고 실시간으로 보조한다
             (12 Active Assist · 14 PD · 23 Gait Adaptive · 25 Stance · 28 Admittance)
                         ↓
Stage 4      Machine Learning        [System 2 강화]
             경험에서 학습하여 더 나은 제어기가 된다
             (15 Inverted Pendulum · 24 Virtual Constraint · 26 ILC · 27 MRAC)
                         ↓
Stage 5      Shared Autonomy         [S1 + S2 + Human]
             인간과 AI가 제어권을 나눈다 — 장인 스킬 캡처
             (11 Passive · 13 Resistive · 29 Bilateral · 33 Kinesthetic★)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
★ 신규 예제 (Ex.31~33)
```

> **왜 5단계인가?**
> 각 단계는 물리적 조건을 선행합니다:
> - Stage 2는 Stage 1(투명 모드)이 없으면 의도 신호가 노이즈에 묻힙니다.
> - Stage 3는 Stage 2(의도 감지)가 없으면 보조 타이밍이 맞지 않습니다.
> - Stage 5(Kinesthetic Teaching)는 Stage 1의 완벽한 투명성 위에서만 가능합니다.
>
> **System 1 / System 2 (Kahneman 적용)**:
> System 1 = 빠른 반응 제어 (1kHz 토크 루프) — 외골격의 반사적 행동.
> System 2 = 느린 추론 AI (VLM, 강화학습) — 고차원 의사결정.
> 이 예제들은 S1(임베디드)에서 S2(AI)로 연결되는 다리를 구축합니다.

---

## 단계별 예제 맵

### Foundation — 플랫폼 & 도구 (언제든 참고)

```
00  Quick Start              ← 보드 동작 확인 (외부 HW 불필요)
01~03  Button & LED          ← 기본 UI 제어
04~06  External I/O          ← GPIO, ADC, FSR 센서
07~09  USB CDC               ← 텍스트 → Total Data → User Custom 스트리밍
10~10c USB MSC               ← 파일 로깅 (초급→중급→고급)
18  Debug Monitor            ← 루프 타이밍, Health 대시보드
19  Memory Aware Design      ← 링 버퍼, 풀 할당자
```

---

### Stage 1: Physical Transparency — 로봇을 투명하게 (System 1)

> **목표**: 착용자가 로봇의 무게와 저항을 느끼지 못하도록 만든다.
> **원칙**: Hogan(1985) — 이상적인 투명 모드에서 로봇의 임피던스는 0이다.
> **연결**: Stage 2에서 의도 감지를 위한 `τ_ext_est` 신호가 여기서 생성됩니다.

```
20  Impedance Control        ← 가상 스프링-댐퍼 (투명 모드 입문)
21  Gravity Compensation     ← 중력+마찰 보상 (공칭 모델)
30  FF+FB Hybrid Control     ← 모델 기반 FF + PD FB
31  DOB Friction Comp ★      ← 외란 관측기로 Stage 1 완성 (τ_ext_est 생성)
```

---

### Stage 2: Intent Sensing — 신호에서 의도를 읽다 (S1 → S2)

> **목표**: 원시 센서 신호(GRF, IMU, 각도)에서 인간의 다음 동작 의도를 추출한다.
> **원칙**: Ronsse(2011) — 리드믹 동기화; Gervasi(2020) — 연속 보행 위상 추정.
> **연결**: 여기서 추출한 의도 신호가 Stage 3의 보조 트리거가 됩니다.

```
16  TinyAI Sensor Fusion     ← IMU + 온디바이스 추론
17  FSM Gait Intent          ← 7-phase 보행 FSM, 단계별 의도 인식
22  CPG Oscillator           ← 리드믹 센서 동기화 (보행 주기 추적)
32  GRF Gait Intent ★        ← 발 접촉 이벤트 → 연속 보행 위상 추정
```

---

### Stage 3: Adaptive Assistance — 의도에 맞춰 보조하다 (S1 + S2)

> **목표**: 감지된 의도에 실시간으로 동기화하여 보조 토크를 생성한다.
> **원칙**: Quinlivan(2017) — 보행 위상 동기화 보조; Collins(2015) — 최적 강성.
> **연결**: Stage 4에서 이 보조 전략을 학습 알고리즘으로 자동 최적화합니다.

```
12  Active Assist Mode       ← 의도 인식 + 실시간 보조 (계층적 FSM)
14  PD Realtime Control      ← 사용자 정의 PD 토크 제어
23  Gait Phase Adaptive      ← 보행 위상 기반 적응 토크
25  Stance Stiffness         ← 입각기 가변 강성 (에너지 최적화)
28  Admittance Control       ← 힘→위치 변환 (교시 기반 보조)
```

---

### Stage 4: Machine Learning — 경험에서 학습하다 (System 2)

> **목표**: 반복 경험을 통해 제어기가 스스로 파라미터를 최적화한다.
> **원칙**: Emken(2007) — 반복 학습 제어; Slotine(1991) — 적응 제어.
> **연결**: Stage 5의 Kinesthetic Teaching이 수집한 전문가 데이터가 이 단계의 학습 입력이 됩니다.

```
15  Inverted Pendulum        ← 물리 모델 기반 제어 (학습 전 기준선)
24  Virtual Constraint       ← Bézier 궤적 구속 (HZD — 불변 궤도)
26  ILC                      ← 반복 학습 제어 (trial-by-trial 최적화)
27  MRAC                     ← 온라인 모델 참조 적응 제어
```

---

### Stage 5: Shared Autonomy — 인간과 AI가 함께 (S1 + S2 + Human)

> **목표**: 인간과 AI가 제어권을 분담하고, 전문가 스킬을 데이터로 포착한다.
> **원칙**: Dragan(2013) — Shared Autonomy; Polanyi(1966) — 암묵지(Tacit Knowledge).
> **Physical AI 연결**: 전문가가 착용하고 교시한 궤적 데이터 → PhAI Studio → π0 VLA 모델 학습.

```
11  Passive Mode             ← 궤적 제어 (P/I-Vector, 수동 보조)
13  Resistive Mode           ← 운동 저항 보조 (재활)
29  Bilateral Coordination   ← 좌우 협응 제어 (재활 + 협응)
33  Kinesthetic Teaching ★   ← 전문가 스킬 캡처 → AI 학습 데이터 (장인 암묵지)
```

> **Ex.33 핵심**: 운동감각 교시(Kinesthetic Teaching) = 전문가가 투명 로봇을 착용하고
> 직접 움직여 동작을 교시 → 100Hz 궤적 기록 → π0 스타일 VLA 학습 데이터.
> *"암묵지를 데이터로"* — 말로 표현할 수 없는 장인 스킬이 AI의 학습 소스가 됩니다.

---

## 학습 경로 추천

| 목표 | 추천 경로 |
|------|----------|
| **입문** (임베디드 처음) | 00 → 01 → 04 → 07 → 09 → 10a → 11 |
| **중급** (임베디드 기초) | 02 → 05a~d → 08 → 10b → 12 → 14 |
| **고급** (제어 알고리즘) | 03 → 06 → 10c → 13 → 15 → 16 → 17 |
| **Physical AI 입문** | 20 → 21 → 31 → 32 → 33 (Stage 1→2→5) |
| **Physical AI 심화** | 22 → 23 → 26 → 27 → 28 → 29 (Stage 2→3→4→5) |
| **유틸리티** (언제든) | 18 (디버깅) · 19 (메모리) |

---

## Part 1: 기본 I/O 제어

### Button & LED (01~03)

| 예제 | 제목 | 난이도 | 핵심 API |
| :---: | :--- | :---: | :--- |
| [00](00_Quick_Start/) | 보드 동작 확인 | 입문 | TSM + LED + USB CDC |
| [01](01_Button_LED_Basic/) | 버튼→LED 미러링 | 초급 | `XM_GetButtonState`, `XM_SetLedState` |
| [02](02_Button_LED_Event/) | 이벤트와 LED 효과 | 초급 | `XM_GetButtonEvent`, `XM_SetLedEffect` |
| [03](03_Button_LED_FSM/) | 상태 머신 모드 전환 | 중급 | `XM_TSM_*`, `XM_BTN_LONG_PRESS` |

### External I/O (04~06)

| 예제 | 제목 | 난이도 | 핵심 API |
| :---: | :--- | :---: | :--- |
| [04](04_Ext_IO_Basic/) | 외부 스위치 & LED | 초급 | `XM_SetPinMode`, `XM_DigitalRead/Write` |
| [05](05_Ext_IO_analog/) | 고정 ADC 전압 읽기 | 초급 | `XM_AnalogReadMillivolts` |
| [05a](05a_Ext_IO_DIO_to_ADC/) | DIO→ADC 전환 | 초급 | `XM_SwitchDioToAdc` |
| [05b](05b_Ext_IO_FSR_8ch/) | FSR 8채널 일괄 전환 | 중급 | `XM_SwitchAllDioToAdc`, Resolution |
| [05c](05c_Ext_IO_Mixed_ADC/) | 고정+동적 ADC 혼합 | 중급 | 12채널, `XM_GetAnalogResolution` |
| [05d](05d_Ext_IO_DIO_ADC_Hybrid/) | GPIO+ADC 혼합 모드 | 응용 | Guard 메커니즘, Edge Detection |
| [06](06_Ext_IO_Safety_Switch/) | 안전 스위치 인터록 | 중급 | 3상태 FSM + 비상 정지 |

---

## Part 2: USB 통신 & 데이터 수집

> **PhAI Studio 연결 모델**: USB-CDC를 통해 XM10 ↔ PhAI Studio 실시간 채널 연결.
> Module ID `0x20` (Total Data)은 System 자동 스트리밍, `0xF0~0xFE`는 User Custom.

### USB-CDC (07~09)

| 예제 | 제목 | 난이도 | 핵심 API |
| :---: | :--- | :---: | :--- |
| [07](07_CDC_Basic_Print/) | 텍스트 메시지 전송 | 초급 | `XM_SendUsbDebugMessage` |
| [08](08_CDC_Sensor_Print/) | sprintf 센서 모니터링 | 초급 | `sprintf` + 논블로킹 타이머 |
| [09](09_CDC_Stream/) | **PhAI Studio 실시간 스트리밍** | 중급 | `XM_SetUsbCustomMeta`, `XM_SendUsbDataWithId` |

> **Ex.09 핵심**: Total Data(0x20) 425B 자동 스트리밍 구조 이해 + User Custom(0xF0) 추가 채널 등록 방법

### USB-MSC (10~10c)

| 예제 | 제목 | 난이도 | 핵심 API |
| :---: | :--- | :---: | :--- |
| [10](10_MSC_Manual_log/) | 수동 로깅 (레거시) | — | 10a/10b/10c 사용 권장 |
| [10a](10a_MSC_Basic_Log/) | 자동 로깅 기초 | 초급 | `XM_SetUsbLogSource`, 자동 타임스탬프 |
| [10b](10b_MSC_Custom_Struct/) | 커스텀 구조체 | 중급 | 수동 타임스탬프, 4-byte 정렬 |
| [10c](10c_MSC_Advanced_Log/) | TSM + 에러 복구 | 고급 | 파일 롤링, `XM_GetUsbLogStatus` |

---

## Part 3: KIT H10 로봇 제어

| 예제 | 제목 | 난이도 | 핵심 API |
| :---: | :--- | :---: | :--- |
| [11](11_Passive_Mode/) | 패시브 모드 (자동 왕복) | 고급 | `XM_SendPVector`, `XM_SendIVector` |
| [12](12_Active_Assist_Mode/) | 액티브 어시스트 (의도 인식) | 고급 | `XM_SetAssistTorqueRH/LH`, 계층적 FSM |
| [13](13_Resistive_Mode/) | 저항 모드 (운동 저항) | 중급 | `XM_SetResistiveCompGain` |

---

## Part 4: 심화 프로젝트

### 제어 이론

| 예제 | 제목 | 난이도 | 핵심 개념 |
| :---: | :--- | :---: | :--- |
| [14](14_PD_Realtime_Control/) | PD 실시간 토크 제어 | 중급 | PD 제어 수식, 이산 미분, 토크 포화 |
| [15](15_Inverted_Pendulum_Control/) | 역진자 모델 보행 보조 | 고급 | 중력 보상 MgL·sin(θ), Lyapunov 안정성 |

### AI & 의도 인식

| 예제 | 제목 | 난이도 | 핵심 개념 |
| :---: | :--- | :---: | :--- |
| [16](16_TinyAI_Sensor_Fusion/) | Tiny AI 센서 퓨전 | 고급 | 상보 필터, 3-layer NN, MCU 추론 |
| [17](17_FSM_Gait_Intent/) | FSM 보행 의도 인식 | 고급 | 7-phase 보행 FSM, 단계별 보조 토크 |

### 유틸리티

| 예제 | 제목 | 난이도 | 핵심 개념 |
| :---: | :--- | :---: | :--- |
| [18](18_Debug_Monitor/) | 시스템 디버깅 모니터 | 중급 | 루프 프로파일링, Health 대시보드 |
| [19](19_Memory_Aware_Design/) | 메모리 인식 설계 | 중급 | 링 버퍼, 풀 할당자, 이동 평균 |

---

## Part 5: Physical AI 연구 시리즈 — 5단계 학습 여정

> ⚠️ **Body Data 전제조건** — `gaitCycle`, `footContact`, `forwardVelocity` 등 H10 CM
> 보행 분석 데이터를 사용하는 예제는 `XM_SendUserBodyData()` 설정이 필수입니다.
> 상세: [API Reference — Body Data 전제조건](../docs/api-reference/README.md#-body-data-전제조건--반드시-읽으세요)
>
> 💡 **PhAI Studio 연동**: 모든 예제에 `XM_SetUsbCustomMeta` + `XM_SendUsbDataWithId`가
> 적용되어 있어 알고리즘 출력을 실시간 모니터링 가능합니다.

---

### Stage 1: Physical Transparency — 로봇을 투명하게 (Ex.20, 21, 30, 31)

> Hogan(1985): "이상적인 투명 모드에서 로봇의 임피던스는 0이다."
> Ex.21(공칭 모델) → Ex.31(DOB)로 진행하며 진정한 투명성을 완성합니다.

| 예제 | 제목 | Body Data | 난이도 | 논문 |
| :---: | :--- | :---: | :---: | :--- |
| [20](20_Impedance_Control/) | 임피던스 제어 | ✗ | ★★☆ | Hogan 1985 (ASME) |
| [21](21_Gravity_Compensation/) | 중력 보상 / 투명 모드 | ✗ | ★★☆ | Just 2018 (JNER) |
| [30](30_FF_FB_Hybrid_Control/) | FF+FB 혼합 제어 | ✗ | ★★★ | Slotine & Li 1991 |
| [31](31_Friction_Comp_DOB/) ★ | **외란 관측기 투명 모드** | ✗ | ★★★ | Ohnishi 1996 (IEEE IE) |

> **Ex.31 핵심**: DOB Q-filter로 잔류 외란 추정 → `τ_ext_est` ≈ 인간 의도 힘 → Stage 2 연결 고리

---

### Stage 2: Intent Sensing — 신호에서 의도를 읽다 (Ex.16, 17, 22, 32)

> 원시 센서 신호가 어떻게 "인간 의도"로 변환되는지를 보여줍니다.
> Ex.32는 GRF 이벤트 기반의 연속 보행 위상 추정을 구현합니다.

| 예제 | 제목 | Body Data | 난이도 | 논문 |
| :---: | :--- | :---: | :---: | :--- |
| [16](16_TinyAI_Sensor_Fusion/) | Tiny AI 센서 퓨전 | ✗ | ★★★ | — (온디바이스 추론) |
| [17](17_FSM_Gait_Intent/) | FSM 보행 의도 인식 | ✔ **필수** | ★★★ | — (7-phase 보행 FSM) |
| [22](22_CPG_Oscillator/) | CPG 적응 주파수 진동자 | △ | ★★★ | Ronsse 2011 (MBEC) |
| [32](32_GRF_Gait_Intent/) ★ | **GRF 보행 위상 추정** | ✔ **필수** | ★★★ | Gervasi 2020 (IROS) |

> **Ex.32 핵심**: Heel Strike → 위상 리셋 → `phase += dt/T` → sin 프로파일 보조 토크

---

### Stage 3: Adaptive Assistance — 의도에 맞춰 보조하다 (Ex.12, 14, 23, 25, 28)

> 감지된 의도에 실시간으로 동기화하여 최적 보조 토크를 생성합니다.

| 예제 | 제목 | Body Data | 난이도 | 논문 |
| :---: | :--- | :---: | :---: | :--- |
| [12](12_Active_Assist_Mode/) | 액티브 어시스트 | △ | ★★★ | — (의도 인식 + FSM) |
| [14](14_PD_Realtime_Control/) | PD 실시간 제어 | ✗ | ★★☆ | — (PD 기초) |
| [23](23_Gait_Phase_Adaptive_Torque/) | 보행 위상 적응 토크 | ✔ **필수** | ★★★ | Quinlivan 2017 (Science Robotics) |
| [25](25_Stance_Stiffness_Modulation/) | 입각기 가변 강성 | ✔ **필수** | ★★★ | Collins 2015 (Nature) |
| [28](28_Admittance_Control/) | 어드미턴스 제어 | ✗ | ★★★ | Keemink 2018 (IJRR) |

---

### Stage 4: Machine Learning — 경험에서 학습하다 (Ex.15, 24, 26, 27)

> 반복 경험과 온라인 적응으로 제어기가 스스로 최적화됩니다.

| 예제 | 제목 | Body Data | 난이도 | 논문 |
| :---: | :--- | :---: | :---: | :--- |
| [15](15_Inverted_Pendulum_Control/) | 역진자 모델 보행 보조 | ✗ | ★★★ | — (물리 모델 기준선) |
| [24](24_Virtual_Constraint/) | 가상 구속 / HZD | ✔ **필수** | ★★★★ | Westervelt 2003 (IEEE TAC) |
| [26](26_Iterative_Learning_Control/) | 반복 학습 제어 (ILC) | ✔ **필수** | ★★★★ | Emken 2007 (ICORR) |
| [27](27_MRAC/) | 모델 참조 적응 제어 (MRAC) | ✗ | ★★★★★ | Slotine & Li 1991 |

---

### Stage 5: Shared Autonomy — 인간과 AI가 함께 (Ex.11, 13, 29, 33)

> **암묵지(Tacit Knowledge, Polanyi 1966)**: 전문가가 말로 표현하지 못하는 장인의 솜씨를
> 외골격 착용 교시(Kinesthetic Teaching)로 데이터화합니다.
> 이것이 π0 스타일 VLA 모델의 학습 데이터가 됩니다.

| 예제 | 제목 | Body Data | 난이도 | 논문 |
| :---: | :--- | :---: | :---: | :--- |
| [11](11_Passive_Mode/) | 패시브 모드 | ✗ | ★★★ | — (P/I-Vector) |
| [13](13_Resistive_Mode/) | 저항 모드 (재활) | ✗ | ★★☆ | — |
| [29](29_Bilateral_Coordination/) | 좌우 협응 제어 | △ | ★★★ | Duschau-Wicke 2010 (TNSRE) |
| [33](33_Kinesthetic_Teaching/) ★ | **운동감각 교시 + 재생** | ✗ | ★★★★ | Billard 2008 · Chi 2023 |

> **Ex.33 Physical AI 파이프라인**:
> ```
> [투명 교시] → [100Hz 궤적 기록] → [PD 재생 검증]
>      ↓ Ex.10b SD카드 저장
> [PhAI Studio 라벨링] → [VLA 학습] → [배포]
> ```

---

> **난이도 기준**: ★☆☆ 초급 | ★★☆ 중급 | ★★★ 고급 | ★★★★ 연구 | ★★★★★ 고급 연구
> **Body Data**: ✔ 필수 | △ 권장 | ✗ 불필요

---

## 예제 사용법

1. 원하는 예제의 `.c` 파일을 확인합니다.
2. 코드를 `XM_Apps/User_Algorithm/user_app.c`에 복사합니다.
3. 프로젝트를 빌드하고 XM10에 업로드합니다.
4. 각 예제의 `README.md`에서 동작 원리와 실행 방법을 확인합니다.
5. PhAI Studio에서 USB 연결 → Total Data(0x20) 및 User Custom(`0xF0~0xF3`) 채널 확인.

> 상세 가이드: [Getting Started — 첫 빌드 & 실행](../docs/getting-started/03-first-build.md)
