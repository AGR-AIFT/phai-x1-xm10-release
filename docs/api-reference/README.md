# API Reference

> **상위 문서**: [Documentation Home](../README.md) | [Examples Guide](../../examples/README.md)

XM10 Extension Module SDK에서 사용 가능한 모든 API의 레퍼런스 문서 목록입니다.

---

## 📚 API 문서 목록

| # | 문서 | 내용 | 핵심 API |
|---|------|------|----------|
| **01** | [Task State Machine](01-task-state-machine.md) | TSM 생성·상태 관리 | `XM_TSM_Create`, `XM_TSM_AddState`, `XM_TSM_Run`, `XM_TSM_TransitionTo` |
| **02** | [KIT H10 Control & Data](02-h10-control-n-data.md) | H10 센서 데이터·토크 제어·벡터 명령 | `XM.status.h10.*`, `XM_SetAssistTorque`, `XM_SendPVector`, `XM_SendIVector` |
| **03** | [LED & Button Control](03-led-btn-control.md) | LED 효과·버튼 이벤트·채널 LED | `XM_SetLedEffect`, `XM_SetLedState`, `XM_GetButtonEvent`, `XM_SetChannelLedRGB` |
| **04** | [External I/O](04-external-io.md) | 확장 포트 DIO·ADC | `XM_SetPinMode`, `XM_DigitalRead/Write`, `XM_AnalogReadMillivolts` |
| **05** | [USB Connectivity](05-usb-connectivity.md) | CDC 디버그·스트리밍 | `XM_SendUsbDebugMessage`, `XM_SendUsbDataWithId`, `XM_SetUsbCustomMeta` |
| **06** | [USB Data Logging](06-usb-data-logging.md) | MSC 데이터 로깅 | `XM_SetUsbLogSource`, `XM_StartUsbDataLog`, `XM_StopUsbDataLog` |
| **07** | [Memory Management](07-memory-management.md) | 메모리 영역 접근·NV Flash | `XM_GetUserWorkspace`, `XM_GetUserPSRAM`, `XM_UserNV_Read/Write` |
| **08** | [RTC Clock](08-rtc-clock.md) | 실시간 시계 | `XM_RTC_SetDateTime`, `XM_RTC_GetDateTime`, `XM_RTC_IsRunning` |

---

## ⚠️ Body Data 전제조건 — 반드시 읽으세요

> **H10 CM 보행 분석 데이터를 사용하는 예제에 필수입니다.**

### 보행 분석 데이터란?

H10 CM(Central Module)은 실시간 보행 분석(1kHz)으로 다음 데이터를 추정합니다:

| 데이터 | API 경로 | 설명 |
|--------|----------|------|
| `gaitCycle` | `XM.status.h10.gaitCycle` | 보행 주기 (0~100%) |
| `isRightFootContact` | `XM.status.h10.isRightFootContact` | 우측 족저 접촉 여부 |
| `isLeftFootContact` | `XM.status.h10.isLeftFootContact` | 좌측 족저 접촉 여부 |
| `forwardVelocity` | `XM.status.h10.forwardVelocity` | 전진 속도 (m/s) |
| `gaitState` | `XM.status.h10.gaitState` | 보행 상태 비트마스크 |

### 왜 Body Data가 필요한가?

이 보행 분석은 **사용자 신체 정보(체중, 신장, 다리 길이)**를 기반으로 동작합니다.
`XM_SendUserBodyData()` 없이는:

- `gaitCycle` 부정확 → 보행 위상 추정 오류
- `footContact` 미검출 → 입각/유각 구분 불가
- `forwardVelocity` 부정확

### 설정 방법

```c
void User_Setup(void)
{
    // ⚠️ H10 보행 분석 정확도를 위해 반드시 설정
    // 실측값 우선. 불가 시 표준체형 근사치 사용 가능.
    uint32_t body_data[8] = {
        70000,  // [0] 체중 (g)           — 예: 70 kg
        1750,   // [1] 신장 (mm)          — 예: 175 cm
        450,    // [2] 우측 대퇴 길이 (mm)
        450,    // [3] 좌측 대퇴 길이 (mm)
        420,    // [4] 우측 하퇴 길이 (mm)
        420,    // [5] 좌측 하퇴 길이 (mm)
        60,     // [6] 우측 발목 높이 (mm)
        60,     // [7] 좌측 발목 높이 (mm)
    };
    XM_SendUserBodyData(body_data);
    // ...
}
```

### Body Data 필요 여부 (예제별)

| 예제 | Body Data | 이유 |
|------|:---------:|------|
| 00~19 | 대부분 불필요 | 기본 I/O, USB, 단순 제어 |
| **20** Impedance | ✗ | 관절각·토크 피드백만 사용 |
| **21** Gravity Comp | ✗ | 관절각 기반 중력 계산 |
| **22** CPG Oscillator | △ | gaitCycle 사용 시 필요, 각도 피드백 모드로 대체 가능 |
| **23** Gait-Phase Adaptive | ✔ **필수** | gaitCycle이 토크 프로파일의 위상 소스 |
| **24** Virtual Constraint | ✔ **필수** | gaitCycle이 위상 변수 s의 유일한 소스 |
| **25** Stance Stiffness | ✔ **필수** | footContact 없이 입각/유각 구분 불가 |
| **26** ILC | ✔ **필수** | gaitCycle이 학습 인덱스 소스 |
| **27** MRAC | ✗ | 관절각 직접 피드백 |
| **28** Admittance | ✗ | 측정 토크 피드백만 사용 |
| **29** Bilateral | △ | 각도 기반 동작, Body Data로 정확도 향상 |
| **30** FF+FB Hybrid | ✗ | 모델 파라미터 매크로로 직접 설정 |

> **대안**: `gaitCycle`이나 `footContact` 없이 보행 위상이 필요하면 **IMU Hub**, **GRF 슈즈** 등 외부 센서 모듈로 독립 계측하여 사용할 수 있습니다.

---

## 🔧 자주 쓰는 API 빠른 참조

### 센서 데이터 (H10)

```c
// 고관절 각도
float angle_r = XM.status.h10.rightHipMotorAngle;   // 우측 (deg)
float angle_l = XM.status.h10.leftHipMotorAngle;    // 좌측 (deg)

// 고관절 추정 토크
float torque_r = XM.status.h10.rightHipTorque;      // 우측 (Nm)
float torque_l = XM.status.h10.leftHipTorque;       // 좌측 (Nm)

// 보행 분석 데이터 (Body Data 설정 필요)
uint8_t gait_pct  = XM.status.h10.gaitCycle;        // 0~100 (%)
bool    right_gnd = XM.status.h10.isRightFootContact;
bool    left_gnd  = XM.status.h10.isLeftFootContact;
float   fwd_vel   = XM.status.h10.forwardVelocity;  // m/s

// IMU (전처리된 자세 각도)
float pitch_r = XM.status.h10.rightHipImuSagittalPitch; // 시상면 피치 (deg)
float roll_r  = XM.status.h10.rightHipImuFrontalRoll;   // 전두면 롤 (deg)

// H10 동작 모드
bool is_assist = (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST);
```

### 토크 제어

```c
// 제어 모드 설정
XM_SetControlMode(XM_CTRL_MONITOR);  // 모니터링 전용 (기본)
XM_SetControlMode(XM_CTRL_TORQUE);   // 직접 토크 제어 활성화

// 토크 명령 (CTRL_TORQUE 모드에서만 유효)
XM_SetAssistTorqueRH(float torque_nm);          // 우측
XM_SetAssistTorqueLH(float torque_nm);          // 좌측
XM_SetAssistTorque(float rh_nm, float lh_nm);   // 양측

// 안전 해제 패턴 (Active_Exit에서 항상 수행)
XM_SetAssistTorqueRH(0.0f);
XM_SetAssistTorqueLH(0.0f);
XM_SetControlMode(XM_CTRL_MONITOR);
```

### 벡터 명령 (궤도·임피던스·힘)

```c
// P-Vector: 위치 궤도
PVector_t pv = { .yd = -250, .L = 1000, .s0 = 4, .sd = 4 };
// yd = 목표각 ×10 (deg×10), L = 소요시간 (ms)
XM_SendPVector(SYS_NODE_ID_RH, &pv);

// I-Vector: 임피던스 (가상 스프링-댐퍼)
IVector_t iv = { .epsilon = 0, .kp = 80, .kd = 1, .lambda = 0, .duration = 50 };
// kp/kd: 0~100 (%)
XM_SendIVector(SYS_NODE_ID_RH, &iv);
```

---

## 🏗️ 제어 시스템 아키텍처

```
┌─────────────────────────────────────────────────────┐
│  H10 CM (Central Module)                            │
│  실시간 보행 분석 (1kHz)                              │
│  ┌──────────────┐   ⚠️ Body Data 필수               │
│  │ gaitCycle    │← XM_SendUserBodyData()             │
│  │ footContact  │                                    │
│  │ forwardVel   │                                    │
│  └──────────────┘                                   │
│  고관절 모터 각도 / 추정 토크                          │
└──────────────────┬──────────────────────────────────┘
                   │ CAN-FD (1ms PDO)
┌──────────────────▼──────────────────────────────────┐
│  XM10 Extension Module                              │
│  ┌───────────────────────────────────────────────┐  │
│  │  User_Loop() — 1kHz 제어 루프                  │  │
│  │                                               │  │
│  │  XM.status.h10.* 읽기                         │  │
│  │      ↓                                        │  │
│  │  제어 법칙 계산 (예: τ = K·e + B·ė)           │  │
│  │      ↓                                        │  │
│  │  XM_SetAssistTorque(RH, LH) 전송              │  │
│  └───────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────┘
```

---

## 📖 토크 제어 예제 가이드 (Ex.11~30)

### 기본 보행 제어 모드 (Ex.11~13)

| 예제 | 제어 방식 | Body Data | 난이도 |
|------|----------|:---------:|:------:|
| **11** Passive Mode | P-Vector·I-Vector 궤도 | ✗ | ★★★★ |
| **12** Active Assist | 의도 감지 + 단계 토크 | ✗ | ★★★★ |
| **13** Resistive Mode | τ = −B·ω (속도 저항) | ✗ | ★★★ |

### 토크 제어 심화 (Ex.14~15)

| 예제 | 제어 방식 | Body Data | 논문 |
|------|----------|:---------:|------|
| **14** PD Control | τ = Kp·e + Kd·ė | ✗ | - |
| **15** Inverted Pendulum | τ = Mgl·sin(θ) + PD | ✗ | - |

### 토크 제어 연구 시리즈 (Ex.20~30)

| 예제 | 제어 방식 | Body Data | 논문 레퍼런스 | 난이도 |
|------|----------|:---------:|--------------|:------:|
| **20** Impedance | τ = K·(θ_d−θ) + B·θ̇ | ✗ | Hogan 1985 (ASME) | ★★☆ |
| **21** Gravity Comp | τ = α·(Mgl·cos θ + B_f·θ̇) | ✗ | Just 2018 (JNER) | ★★☆ |
| **22** CPG Oscillator | φ̇ = ω + ε·F·cosφ | △ | Ronsse 2011 (MBEC) | ★★★ |
| **23** Gait-Phase Adaptive | τ(φ) = A·sin(π·φ) 구간별 | ✔ | Quinlivan 2017 (SciRobot) | ★★★ |
| **24** Virtual Constraint | θ_d(s) = Bézier(s) | ✔ | Westervelt 2003 (IEEE TAC) | ★★★★ |
| **25** Stance Stiffness | K_stance/K_swing 전환 | ✔ | Collins 2015 (Nature) | ★★★ |
| **26** ILC | τ_{k+1} = τ_k + L·e_k | ✔ | Emken 2007 (ICORR) | ★★★★ |
| **27** MRAC | MIT Rule 적응 게인 | ✗ | Slotine & Li 1991 | ★★★★★ |
| **28** Admittance | τ_ext → 가상 동역학 → θ_ref | ✗ | Keemink 2018 (IJRR) | ★★★ |
| **29** Bilateral | τ_R = −K_c·(θ_R+θ_L) | △ | Duschau-Wicke 2010 (TNSRE) | ★★★ |
| **30** FF+FB Hybrid | τ = τ_ff(모델) + τ_fb(PD) | ✗ | Slotine & Li 1991 Ch.6 | ★★★ |

> **난이도 기준**: ★☆☆ 초급 | ★★☆ 중급 | ★★★ 고급 | ★★★★ 연구 | ★★★★★ 고급 연구

---

## 🔗 관련 문서

- [Getting Started](../getting-started/) — 개발환경 설정 및 첫 빌드
- [Architecture Overview](../architecture/) — XM10 시스템 아키텍처
- [KIT H10 Firmware Compatibility](../kit-h10-firmware/) — 펌웨어 호환성 매트릭스
- [Tutorials](../tutorials/) — 단계별 학습 경로
