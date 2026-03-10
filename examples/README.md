# XM10 예제 가이드

XM10 Extension Module의 **38개 실습 예제**입니다.
각 예제 폴더에는 소스 코드(`.c`)와 상세 설명(`README.md`)이 포함되어 있습니다.

> 📖 API 전체 명세: [docs/api-reference/](../docs/api-reference/)
> 📘 단계별 학습 가이드: [docs/tutorials/](../docs/tutorials/)

---

## 예제 전체 맵

```
[입문]  00  Quick Start              ← 보드 동작 확인 (외부 HW 불필요)
         │
[기초]  ├── 01~03  Button & LED      ← 디지털 I/O, 이벤트, FSM
        ├── 04~06  External I/O      ← 외부 회로, ADC, 안전 스위치
        ├── 07~09  USB CDC           ← 텍스트 → 포맷팅 → 바이너리 스트리밍
        ├── 10~10c USB MSC           ← 파일 로깅 (초급→중급→고급)
         │
[제어]  ├── 11  Passive Mode         ← P/I-Vector 궤적 제어
        ├── 12  Active-Assist Mode   ← 의도 인식 + 실시간 토크
        ├── 13  Resistive Mode       ← 파라미터 설정형 제어
         │
[응용]  ├── 14  PD Realtime Control  ← PD 제어 수식 직접 구현
        ├── 15  Inverted Pendulum    ← 역진자 모델 기반 보행 보조
        ├── 16  TinyAI Sensor Fusion ← 상보 필터 + 신경망 자세 분류
        ├── 17  FSM Gait Intent      ← 7단계 보행 FSM + 단계별 보조
         │
[유틸]  ├── 18  Debug Monitor        ← 루프 타이밍, Health 대시보드
        ├── 19  Memory Aware Design  ← 링 버퍼, 풀 할당자, 이동 평균
         │
[연구]  ├── 20  Impedance Control    ← 가상 스프링-댐퍼 (Hogan 1985)
        ├── 21  Gravity Comp.        ← 투명 모드, α-점진 증가 (Just 2018)
        ├── 22  CPG Oscillator       ← 적응 주파수 진동자 (Ronsse 2011)
        ├── 23  Gait-Phase Adaptive  ← 위상별 정현파 보조 (Quinlivan 2017)
        ├── 24  Virtual Constraint   ← Bézier + HZD (Westervelt 2003)
        ├── 25  Stance Stiffness     ← 입각기 가변 강성 (Collins 2015)
        ├── 26  ILC                  ← 반복 학습 제어 (Emken 2007)
        ├── 27  MRAC                 ← MIT Rule 적응 (Slotine & Li 1991)
        ├── 28  Admittance           ← 힘→위치 가상 동역학 (Keemink 2018)
        ├── 29  Bilateral Coord.     ← 좌우 역위상 커플링 (Duschau-Wicke 2010)
        └── 30  FF+FB Hybrid         ← 모델 기반 FF + PD FB (Slotine & Li 1991)
```

---

## 난이도별 학습 경로

| 대상 | 추천 경로 |
|------|----------|
| **입문** (임베디드 경험 없음) | 00 → 01 → 04 → 05 → 07 → 10a → 11 |
| **중급** (임베디드 기초 있음) | 02 → 05a~d → 08 → 10b → 12 → 14 |
| **고급** (제어 알고리즘 설계) | 03 → 06 → 09 → 10c → 13 → 15 → 16 → 17 |
| **유틸리티** (언제든 참고) | 18 (디버깅) · 19 (메모리 패턴) |

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

## Part 2: USB 통신 & 데이터 로깅

### USB-CDC (07~09)

| 예제 | 제목 | 난이도 | 핵심 API |
| :---: | :--- | :---: | :--- |
| [07](07_CDC_Basic_Print/) | 텍스트 메시지 전송 | 초급 | `XM_SendUsbDebugMessage` |
| [08](08_CDC_Sensor_Print/) | sprintf 센서 모니터링 | 초급 | `sprintf` + 논블로킹 타이머 |
| [09](09_CDC_Stream/) | PhAI V2 바이너리 스트리밍 | 중급 | `XM_SetUsbStreamSource`, `PHAI_MODULE_COMBINED` |

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

## Part 5: 연구 시리즈 (Ex.20~30) — 논문 레퍼런스 기반

> ⚠️ **Body Data 전제조건** — `gaitCycle`, `footContact`, `forwardVelocity` 등 H10 CM 보행 분석 데이터를 사용하는 예제는 `XM_SendUserBodyData()` 설정이 필수입니다. 상세 내용: [API Reference — Body Data 전제조건](../docs/api-reference/README.md#-body-data-전제조건--반드시-읽으세요)

### 기본 상호작용 제어 (Ex.20~21)

| 예제 | 제목 | Body Data | 난이도 | 논문 |
| :---: | :--- | :---: | :---: | :--- |
| [20](20_Impedance_Control/) | 임피던스 제어 | ✗ | ★★☆ | Hogan 1985 (ASME) |
| [21](21_Gravity_Compensation/) | 중력 보상 / 투명 모드 | ✗ | ★★☆ | Just 2018 (JNER) |

### 리드믹·위상 기반 제어 (Ex.22~25)

| 예제 | 제목 | Body Data | 난이도 | 논문 |
| :---: | :--- | :---: | :---: | :--- |
| [22](22_CPG_Oscillator/) | CPG 적응 주파수 진동자 | △ | ★★★ | Ronsse 2011 (MBEC) |
| [23](23_Gait_Phase_Adaptive_Torque/) | 보행 위상 적응 토크 | ✔ **필수** | ★★★ | Quinlivan 2017 (Science Robotics) |
| [24](24_Virtual_Constraint/) | 가상 구속 / HZD | ✔ **필수** | ★★★★ | Westervelt 2003 (IEEE TAC) |
| [25](25_Stance_Stiffness_Modulation/) | 입각기 가변 강성 | ✔ **필수** | ★★★ | Collins 2015 (Nature) |

### 적응·학습 제어 (Ex.26~27)

| 예제 | 제목 | Body Data | 난이도 | 논문 |
| :---: | :--- | :---: | :---: | :--- |
| [26](26_Iterative_Learning_Control/) | 반복 학습 제어 (ILC) | ✔ **필수** | ★★★★ | Emken 2007 (ICORR) |
| [27](27_MRAC/) | 모델 참조 적응 제어 (MRAC) | ✗ | ★★★★★ | Slotine & Li 1991 |

### 고급 상호작용·협응 제어 (Ex.28~30)

| 예제 | 제목 | Body Data | 난이도 | 논문 |
| :---: | :--- | :---: | :---: | :--- |
| [28](28_Admittance_Control/) | 어드미턴스 제어 | ✗ | ★★★ | Keemink 2018 (IJRR) |
| [29](29_Bilateral_Coordination/) | 좌우 협응 제어 | △ | ★★★ | Duschau-Wicke 2010 (TNSRE) |
| [30](30_FF_FB_Hybrid_Control/) | FF+FB 혼합 제어 | ✗ | ★★★ | Slotine & Li 1991 Ch.6 |

> **난이도 기준**: ★☆☆ 초급 | ★★☆ 중급 | ★★★ 고급 | ★★★★ 연구 | ★★★★★ 고급 연구
> **Body Data**: ✔ 필수 | △ 권장 | ✗ 불필요

---

## 예제 사용법

1. 원하는 예제의 `.c` 파일을 확인합니다
2. 코드를 `XM_Apps/User_Algorithm/user_app.c`에 복사합니다
3. 프로젝트를 빌드하고 XM10에 업로드합니다
4. 각 예제의 `README.md`에서 동작 원리와 실행 방법을 확인합니다

> 상세 가이드: [Getting Started — 첫 빌드 & 실행](../docs/getting-started/03-first-build.md)
