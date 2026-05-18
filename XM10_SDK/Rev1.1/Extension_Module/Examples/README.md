# XM10 예제

41 개의 실습 예제예요. 각 폴더에 소스 코드 `.c` 와 그 예제만의 설명서 `README.md` 가 함께 들어있어요.

- API 함수 명세: [docs/api-reference/](../docs/api-reference/)
- 단계별 학습 안내: [docs/tutorials/](../docs/tutorials/)
- 온라인 학습 플랫폼: [onephai.com](https://onephai.com)

---

## 처음 오신 분 — 첫 30 분 동선

| 순서 | 예제 | 난이도 | 학습 포인트 | 권장 시간 |
|:---:|------|:---:|------------|:---:|
| 1 | [Ex.00 Quick Start](00_Quick_Start/) | ⭐ | 상태 머신 + LED + 버튼 + USB 시리얼 기초 | 30 분 |
| 2 | [Ex.01 Button & LED Basic](01_Button_LED_Basic/) | ⭐ | 폴링 입출력 (`GetButtonState`) | 20 분 |
| 3 | [Ex.02 Button & LED Event](02_Button_LED_Event/) | ⭐⭐ | 이벤트 + Toggle + Oneshot 효과 | 25 분 |
| 4 | [Ex.03 Button & LED FSM](03_Button_LED_FSM/) | ⭐⭐ | 멀티 상태 머신 + 롱 프레스 | 30 분 |

여기까지 약 2 시간이에요. 외부 부품 없이 보드 하나로 다 됩니다. Ex.04 부터 외부 입출력과 센서로 넓혀가요.

**보드 리비전 호환성** — 41 개 예제는 Rev 1.1 / Rev 2.0 양쪽 모두 빌드·실행됩니다 (SDK 코드가 동일해요). 다만 외부 GPIO 를 직접 다루는 예제 (Ex.04~06, Ex.05a~05d, Ex.16 외부 IMU 모드) 는 보드의 커넥터 위치와 핀 라벨이 리비전마다 달라요. 시작 전에 내 보드의 핀맵을 펴두세요 — [Rev 1.1 핀맵](../docs/hardware/external-gpio-rev1.1.md) / [Rev 2.0 핀맵](../docs/hardware/external-gpio-rev2.0.md).

각 예제의 README 는 다섯 단계로 정리되어 있어요: ① 목표 / ② 사전 지식 / ③ 핵심 코드 / ④ 실험과 변형 / ⑤ 다음 단계. 마지막에 "흔한 실수" 도 함께.

막혔다면 Claude Code 에서 `"Ex.XX 막혔어"` 라고 말해보세요. `example-helper` 가 해당 예제의 흔한 실수 섹션을 찾아 알려드려요.

---

## 큰 그림 — 5 단계 학습 흐름

기본기를 다진 다음, 41 개 예제는 다섯 단계의 흐름을 따라가요. "로봇이 인간을 어떻게 이해하고, 함께 성장하는가" 라는 한 줄로 묶을 수 있어요.

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Foundation   기본 입출력과 도구
             (00~10c, 18, 19 — 언제든 돌아와 참고)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Stage 1      투명한 로봇 만들기
             착용자가 로봇의 무게·저항을 못 느끼게
             (20 임피던스 · 21 중력보상 · 30 FF+FB · 31 DOB★)
                         ↓
Stage 2      의도 읽기
             센서 신호에서 다음 동작 의도 뽑아내기
             (16 TinyAI · 17 FSM Gait · 22 CPG · 32 GRF★)
                         ↓
Stage 3      맞춤 보조
             읽은 의도에 맞춰 실시간으로 거들기
             (12 Active Assist · 14 PD · 23 Gait Adaptive · 25 Stance · 28 Admittance)
                         ↓
Stage 4      경험에서 학습
             반복하면서 제어기가 스스로 좋아지기
             (15 Inverted Pendulum · 24 Virtual Constraint · 26 ILC · 27 MRAC)
                         ↓
Stage 5      사람과 AI 가 함께
             제어권 분담 + 전문가 스킬 데이터화
             (11 Passive · 13 Resistive · 29 Bilateral · 33 Kinesthetic★)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
★ 신규 예제 (Ex.31~33)
```

이 순서에는 이유가 있어요. 로봇이 투명하지 않으면 (Stage 1) 인간 의도 신호가 마찰·중력 노이즈에 묻혀 안 보여요 (Stage 2). 의도를 못 읽으면 보조 타이밍이 빗나가고요 (Stage 3). 그래서 한 단계씩 쌓아 올리는 거예요.

처음부터 5 단계를 다 가야 하는 건 아니에요. **임베디드 처음이면 Foundation 만 한 학기 충분**, **제어 알고리즘에 관심 있으면 Stage 1~3**, **AI 응용이 목표면 Stage 4~5** 로 갈라져요. 추천 경로는 아래 표.

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

### Stage 1: 투명한 로봇 만들기

목표는 착용자가 로봇의 무게·마찰을 못 느끼게 하는 거예요. Hogan(1985) 의 고전적인 정의로 "투명할수록 로봇의 임피던스가 0 에 가깝다". 여기서 만들어지는 외란 추정값이 다음 단계의 의도 감지 신호로 이어져요.

```
20  Impedance Control        ← 가상 스프링-댐퍼 (투명 모드 입문)
21  Gravity Compensation     ← 중력+마찰 보상 (공칭 모델)
30  FF+FB Hybrid Control     ← 모델 기반 FF + PD FB
31  DOB Friction Comp ★      ← 외란 관측기로 Stage 1 완성 (τ_ext_est 생성)
```

---

### Stage 2: 의도 읽기

원시 센서 신호 (GRF, IMU, 관절각) 에서 사람이 다음에 하려는 동작을 추출해요. Ronsse(2011) 의 리드믹 동기화, Gervasi(2020) 의 연속 보행 위상 추정 같은 고전 방법부터 시작해서, 여기서 뽑은 의도 신호가 Stage 3 의 보조 트리거가 돼요.

```
16  TinyAI Sensor Fusion     ← IMU + 온디바이스 추론
17  FSM Gait Intent          ← 7-phase 보행 FSM, 단계별 의도 인식
22  CPG Oscillator           ← 리드믹 센서 동기화 (보행 주기 추적)
32  GRF Gait Intent ★        ← 발 접촉 이벤트 → 연속 보행 위상 추정
```

---

### Stage 3: 맞춤 보조

읽어낸 의도에 실시간으로 동기화해 보조 토크를 만들어내요. Quinlivan(2017) 의 보행 위상 동기화, Collins(2015) 의 최적 강성 같은 방법들. 이 보조 전략을 다음 단계에서 학습 알고리즘으로 자동 최적화해요.

```
12  Active Assist Mode       ← 의도 인식 + 실시간 보조 (계층적 FSM)
14  PD Realtime Control      ← 사용자 정의 PD 토크 제어
23  Gait Phase Adaptive      ← 보행 위상 기반 적응 토크
25  Stance Stiffness         ← 입각기 가변 강성 (에너지 최적화)
28  Admittance Control       ← 힘→위치 변환 (교시 기반 보조)
```

---

### Stage 4: 경험에서 학습

반복 경험을 통해 제어기가 자기 파라미터를 스스로 조정해요. Emken(2007) 의 반복 학습 제어 (ILC), Slotine(1991) 의 적응 제어 같은 방법들. Stage 5 에서 전문가 교시로 모은 데이터가 이 단계의 학습 입력으로 들어와요.

```
15  Inverted Pendulum        ← 물리 모델 기반 제어 (학습 전 기준선)
24  Virtual Constraint       ← Bézier 궤적 구속 (HZD — 불변 궤도)
26  ILC                      ← 반복 학습 제어 (trial-by-trial 최적화)
27  MRAC                     ← 온라인 모델 참조 적응 제어
```

---

### Stage 5: 사람과 AI 가 함께

사람과 AI 가 제어권을 분담하고, 말로 표현할 수 없는 전문가의 몸 감각을 데이터로 포착해요. Dragan(2013) 의 공유 자율성, Polanyi(1966) 의 암묵지 개념. 전문가가 투명 로봇을 착용한 채 직접 움직여 만든 궤적 데이터가 다음 세대 AI 모델의 학습 입력이 돼요.

```
11  Passive Mode             ← 궤적 제어 (P/I-Vector, 수동 보조)
13  Resistive Mode           ← 운동 저항 보조 (재활)
29  Bilateral Coordination   ← 좌우 협응 제어 (재활 + 협응)
33  Kinesthetic Teaching ★   ← 전문가 스킬 캡처 → AI 학습 데이터 (장인 암묵지)
```

**Ex.33 한 줄 요약** — 전문가가 투명 모드로 만든 로봇을 직접 착용하고 동작을 보여주면, 100 Hz 로 궤적이 기록돼요. 그 데이터가 VLA 같은 차세대 AI 모델의 학습 소스가 됩니다. 말로 못 가르치는 장인의 손맛을 데이터로 옮기는 거예요.

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

> 외부 GPIO 헤더의 커넥터 위치·핀 라벨은 보드 리비전마다 다릅니다. 시작 전 사용 중인 보드의 [Rev 1.1 핀맵](../docs/hardware/external-gpio-rev1.1.md) 또는 [Rev 2.0 핀맵](../docs/hardware/external-gpio-rev2.0.md) 을 펴두세요.

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

## Part 5: 제어 알고리즘 심화 — 5 단계 흐름 상세

위 큰 그림을 단계별로 표로 풀어둔 거예요. 논문 출처와 난이도가 필요할 때 참고하세요.

**Body Data 전제** — `gaitCycle`, `footContact`, `forwardVelocity` 같은 H10 보행 분석 값을 쓰는 예제는 `XM_SendUserBodyData()` 호출이 먼저 필요해요. 자세한 건 [API Reference — Body Data 전제조건](../docs/api-reference/README.md#-body-data-전제조건--반드시-읽으세요).

**PhAI Studio 연동** — 모든 예제에 USB 실시간 스트리밍 코드가 들어있어요. 보드 동작과 동시에 PhAI Studio 에서 그래프로 확인할 수 있어요.

---

### Stage 1: 투명한 로봇 만들기 (Ex.20, 21, 30, 31)

Hogan(1985) 의 정의대로 임피던스를 0 에 가깝게. Ex.21 의 공칭 모델 보상에서 시작해 Ex.31 의 외란 관측기 (DOB) 까지 가면 진짜 투명 모드가 완성돼요.

| 예제 | 제목 | Body Data | 난이도 | 논문 |
| :---: | :--- | :---: | :---: | :--- |
| [20](20_Impedance_Control/) | 임피던스 제어 | ✗ | ★★☆ | Hogan 1985 (ASME) |
| [21](21_Gravity_Compensation/) | 중력 보상 / 투명 모드 | ✗ | ★★☆ | Just 2018 (JNER) |
| [30](30_FF_FB_Hybrid_Control/) | FF+FB 혼합 제어 | ✗ | ★★★ | Slotine & Li 1991 |
| [31](31_Friction_Comp_DOB/) ★ | **외란 관측기 투명 모드** | ✗ | ★★★ | Ohnishi 1996 (IEEE IE) |

**Ex.31 한 줄 요약** — DOB 의 Q-filter 로 잔류 외란을 추정해요. 그 외란값이 사실상 "사람이 가하는 힘" 이라서, 다음 단계 (의도 감지) 의 핵심 신호가 됩니다.

---

### Stage 2: 의도 읽기 (Ex.16, 17, 22, 32)

원시 센서 신호가 어떻게 "인간 의도" 로 변환되는지를 다뤄요. Ex.32 는 발 접지 이벤트 기반으로 연속 보행 위상을 추정해요.

| 예제 | 제목 | Body Data | 난이도 | 논문 |
| :---: | :--- | :---: | :---: | :--- |
| [16](16_TinyAI_Sensor_Fusion/) | Tiny AI 센서 퓨전 | ✗ | ★★★ | — (온디바이스 추론) |
| [17](17_FSM_Gait_Intent/) | FSM 보행 의도 인식 | ✔ **필수** | ★★★ | — (7-phase 보행 FSM) |
| [22](22_CPG_Oscillator/) | CPG 적응 주파수 진동자 | △ | ★★★ | Ronsse 2011 (MBEC) |
| [32](32_GRF_Gait_Intent/) ★ | **GRF 보행 위상 추정** | ✔ **필수** | ★★★ | Gervasi 2020 (IROS) |

**Ex.32 한 줄 요약** — 발 뒤꿈치 접지에서 위상을 리셋하고, `phase += dt / T` 로 연속 위상을 업데이트해요. sin 프로파일에 맞춰 보조 토크를 내보내면 보행에 자연스럽게 동기화됩니다.

---

### Stage 3: 맞춤 보조 (Ex.12, 14, 23, 25, 28)

감지한 의도에 실시간으로 동기화해 최적 보조 토크를 만들어내요.

| 예제 | 제목 | Body Data | 난이도 | 논문 |
| :---: | :--- | :---: | :---: | :--- |
| [12](12_Active_Assist_Mode/) | 액티브 어시스트 | △ | ★★★ | — (의도 인식 + FSM) |
| [14](14_PD_Realtime_Control/) | PD 실시간 제어 | ✗ | ★★☆ | — (PD 기초) |
| [23](23_Gait_Phase_Adaptive_Torque/) | 보행 위상 적응 토크 | ✔ **필수** | ★★★ | Quinlivan 2017 (Science Robotics) |
| [25](25_Stance_Stiffness_Modulation/) | 입각기 가변 강성 | ✔ **필수** | ★★★ | Collins 2015 (Nature) |
| [28](28_Admittance_Control/) | 어드미턴스 제어 | ✗ | ★★★ | Keemink 2018 (IJRR) |

---

### Stage 4: 경험에서 학습 (Ex.15, 24, 26, 27)

반복 경험과 온라인 적응으로 제어기가 스스로 최적화돼요.

| 예제 | 제목 | Body Data | 난이도 | 논문 |
| :---: | :--- | :---: | :---: | :--- |
| [15](15_Inverted_Pendulum_Control/) | 역진자 모델 보행 보조 | ✗ | ★★★ | — (물리 모델 기준선) |
| [24](24_Virtual_Constraint/) | 가상 구속 / HZD | ✔ **필수** | ★★★★ | Westervelt 2003 (IEEE TAC) |
| [26](26_Iterative_Learning_Control/) | 반복 학습 제어 (ILC) | ✔ **필수** | ★★★★ | Emken 2007 (ICORR) |
| [27](27_MRAC/) | 모델 참조 적응 제어 (MRAC) | ✗ | ★★★★★ | Slotine & Li 1991 |

---

### Stage 5: 사람과 AI 가 함께 (Ex.11, 13, 29, 33)

말로 옮길 수 없는 전문가의 몸 감각을 외골격 교시로 데이터화해요. Polanyi(1966) 가 "암묵지" 라고 불렀던 그 영역. 이 데이터가 차세대 VLA 모델의 학습 입력이 됩니다.

| 예제 | 제목 | Body Data | 난이도 | 논문 |
| :---: | :--- | :---: | :---: | :--- |
| [11](11_Passive_Mode/) | 패시브 모드 | ✗ | ★★★ | — (P/I-Vector) |
| [13](13_Resistive_Mode/) | 저항 모드 (재활) | ✗ | ★★☆ | — |
| [29](29_Bilateral_Coordination/) | 좌우 협응 제어 | △ | ★★★ | Duschau-Wicke 2010 (TNSRE) |
| [33](33_Kinesthetic_Teaching/) ★ | **운동감각 교시 + 재생** | ✗ | ★★★★ | Billard 2008 · Chi 2023 |

**Ex.33 전체 파이프라인**

```
[투명 교시] → [100 Hz 궤적 기록] → [PD 재생으로 검증]
     ↓  Ex.10b SD 카드 저장
[PhAI Studio 라벨링] → [VLA 학습] → [배포]
```

---

> **난이도 기준**: ★☆☆ 초급 | ★★☆ 중급 | ★★★ 고급 | ★★★★ 연구 | ★★★★★ 고급 연구
> **Body Data**: ✔ 필수 | △ 권장 | ✗ 불필요

---

## 예제 사용법

1. 원하는 예제 폴더의 `.c` 파일과 `README.md` 를 열어요.
2. `.c` 코드를 `XM_Apps/User_Algorithm/user_app.c` 에 복사 후 빌드해서 보드에 업로드.
3. README 의 4 단계 "실험" 을 그대로 따라가며 동작을 확인.
4. 변형도 시도해보세요 — 값을 바꾸거나 LED 를 다른 핀으로 옮겨봐요.
5. PhAI Studio 를 연결하면 보드 데이터를 실시간 그래프로 볼 수 있어요 (`0x20` 채널 기본, `0xF0~0xF3` 채널 커스텀).

자세한 빌드·업로드 방법은 [Getting Started — 첫 빌드](../docs/getting-started/03-first-build.md).
