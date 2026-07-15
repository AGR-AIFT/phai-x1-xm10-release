# API 참고서

> 상위: [문서 인덱스](../README.md) · [예제 가이드](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/README.md)

알고리즘을 작성하면서 옆에 두고 보는 함수 명세입니다. 처음부터 다 읽을 필요는 없고, 필요할 때 해당 그룹만 펴보세요.

---

## 이 문서를 보는 법 — 두 개의 레이어

API 문서는 **역할이 다른 두 레이어**로 나뉘어 있습니다. 무엇을 찾고 있는지에 따라 맞는 쪽을 펴보세요.

| 레이어 | 위치 | 다루는 것 | 이럴 때 펴보세요 |
|--------|------|-----------|------------------|
| **① 개념 가이드** | 본 문서 (01~09, 아래 [함수 그룹](#함수-그룹) 표) | **왜** 이 API 가 있고 **어떻게** 알고리즘에 녹이는지 — 배경 지식, 흔한 실수, 예제 코드 흐름 | 처음 써보는 API 를 어떻게 조합해야 할지 모를 때 |
| **② 함수 레퍼런스** | [`ref/`](ref/README.md) — 헤더 1개당 페이지 1개 | **정확한** 함수 시그니처 · 파라미터 타입/범위 · 반환값 · 구조체 필드 | 함수 이름/파라미터 순서/반환값 의미를 정확히 확인해야 할 때 |

두 레이어는 서로를 대체하지 않습니다. 개념 가이드 각 항목 끝에는 대응하는 `ref/` 페이지가 링크되어 있고, `ref/` 각 페이지 상단에도 대응하는 개념 가이드가 링크되어 있습니다 — 막히면 반대쪽으로 건너가세요.

→ **함수 레퍼런스 전체 인덱스**: [`ref/README.md`](ref/README.md)

---

## 함수 그룹

| # | 문서 | 내용 | 자주 쓰는 함수 |
|---|------|------|----------|
| 01 | [상태 머신 (TSM)](01-task-state-machine.md) | 상태별 동작 분리 + 전이 | `XM_TSM_Create`, `XM_TSM_AddState`, `XM_TSM_Run`, `XM_TSM_TransitionTo` |
| 02 | [KIT H10 제어 + 데이터](02-h10-control-n-data.md) | 외골격 센서 읽기 + 토크/위치 명령 | `XM.status.h10.*`, `XM_SetAssistTorque`, `XM_SendPVector`, `XM_SendIVector` |
| 03 | [LED + 버튼](03-led-btn-control.md) | 보드 LED 효과, 버튼 이벤트 | `XM_SetLedEffect`, `XM_SetLedState`, `XM_GetButtonEvent`, `XM_SetChannelLedRGB` |
| 04 | [외부 IO](04-external-io.md) | 확장 포트 GPIO/ADC | `XM_SetPinMode`, `XM_DigitalRead/Write`, `XM_AnalogReadMillivolts` |
| 05 | [USB 시리얼 통신](05-usb-connectivity.md) | PC 로 텍스트/바이너리 전송 | `XM_SendUsbDebugMessage`, `XM_SendUsbDataWithId`, `XM_SetUsbCustomMeta` |
| 06 | [USB 메모리 로깅](06-usb-data-logging.md) | USB 메모리에 자동 저장 | `XM_SetUsbLogSource`, `XM_StartUsbDataLog`, `XM_StopUsbDataLog` |
| 07 | [메모리 영역](07-memory-management.md) | 빠른 메모리 + 비휘발성 저장소 | `XM_GetUserWorkspace`, `XM_GetUserPSRAM`, `XM_UserNV_Read/Write` |
| 08 | [실시간 시계](08-rtc-clock.md) | 날짜/시간 읽기·쓰기 | `XM_RTC_SetDateTime`, `XM_RTC_GetDateTime`, `XM_RTC_IsRunning` |
| 09 | [보조 task + 데이터 공유](09-task-creation.md) | 1 kHz 제어 루프와 별도 작업 + 안전한 데이터 공유 | `XM_Task_CreateOneShot/Periodic`, `XM_Mutex_Lock/Unlock` |

---

## Body Data — 보행 분석 데이터를 쓰려면 먼저 읽어주세요

KIT H10 중앙 모듈은 1 kHz 로 사용자 보행을 분석해서 다음과 같은 데이터를 추정합니다. 그런데 이 분석은 사용자 신체 정보 (체중, 신장, 다리 길이) 가 있어야 정확합니다. 이 데이터를 쓰는 예제 (특히 Ex.23 이상) 에서는 `Control_Setup()` 에 `XM_SendUserBodyData()` 호출이 반드시 필요합니다.

### 어떤 데이터?

| 데이터 | API 경로 | 설명 |
|--------|----------|------|
| `isRightFootContact` | `XM.status.h10.isRightFootContact` | 우측 족저 접촉 여부 |
| `isLeftFootContact` | `XM.status.h10.isLeftFootContact` | 좌측 족저 접촉 여부 |
| `forwardVelocity` | `XM.status.h10.forwardVelocity` | 전진 속도 (m/s) — Body Data 설정 시 정확도 향상 |

> **보행 위상(gaitCycle / gaitState)은 현재 공개 구조체 `XmH10Data_t` 에 직접 노출되지 않습니다.** 보행 주기가 필요하면 `forwardVelocity` 와 발 접지(`isLeftFootContact` / `isRightFootContact`)로 직접 산출합니다 (Ex.23 방식).

### 미설정 시 증상

- `forwardVelocity` 기반 보행 위상 추정이 부정확해서 위상 동기가 어긋남
- `footContact` 이 항상 0 — 입각/유각 구분 불가
- `forwardVelocity` 값이 부정확

### 설정 방법

```c
void Control_Setup(void)
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

### 예제별 필요 여부

| 예제 | Body Data | 이유 |
|------|:---------:|------|
| 00~19 | 대부분 불필요 | 기본 입출력, USB, 단순 제어 |
| 20 임피던스 | ✗ | 관절각·토크 피드백만 사용 |
| 21 중력 보상 | ✗ | 관절각 기반 중력 계산 |
| 22 CPG 진동자 | △ | `gaitCycle` 모드 시 필요, 각도 피드백 모드로 대체 가능 |
| 23 보행 위상 적응 | ✔ 필수 | `gaitCycle` 이 토크 프로파일의 위상 소스 |
| 24 가상 구속 (HZD) | ✔ 필수 | `gaitCycle` 이 위상 변수의 유일한 소스 |
| 25 입각기 가변 강성 | ✔ 필수 | `footContact` 없이 입각/유각 구분 불가 |
| 26 ILC | ✔ 필수 | `gaitCycle` 이 학습 인덱스 소스 |
| 27 MRAC | ✗ | 관절각 직접 피드백 |
| 28 어드미턴스 | ✗ | 측정 토크 피드백만 사용 |
| 29 좌우 협응 | △ | 각도 기반 동작, Body Data 로 정확도 향상 |
| 30 FF+FB 혼합 | ✗ | 모델 파라미터 매크로로 직접 설정 |

> `gaitCycle` 이나 `footContact` 없이 보행 위상이 필요하면 IMU Hub 나 GRF 슈즈 같은 외부 센서로 독립 계측해서 쓸 수도 있습니다.

---

## 자주 쓰는 함수 빠른 참조

### 외골격 센서 데이터

```c
// 고관절 각도
float angle_r = XM.status.h10.rightHipMotorAngle;   // 우측 (deg)
float angle_l = XM.status.h10.leftHipMotorAngle;    // 좌측 (deg)

// 고관절 모터 전류 (필드명은 Torque 지만 단위는 A — 토크 환산 ×1.594 ≈ Nm)
float cur_r = XM.status.h10.rightHipTorque;         // 우측 (A)
float cur_l = XM.status.h10.leftHipTorque;          // 좌측 (A)

// 보행 분석 데이터 (Body Data 설정 시 정확도 향상)
bool    right_gnd = XM.status.h10.isRightFootContact;
bool    left_gnd  = XM.status.h10.isLeftFootContact;
float   fwd_vel   = XM.status.h10.forwardVelocity;  // m/s
// gaitCycle 은 공개 필드가 아닙니다 — fwd_vel + footContact 로 직접 산출 (Ex.23)

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

### 사전 정의 움직임 명령 (P-Vector / I-Vector)

직접 토크 계산 대신 "이 위치로 이 시간 안에 가" 같은 한 줄 명령으로 외골격을 움직이는 방식입니다.

```c
// 위치 명령 — 목표 각도까지 정해진 시간 안에 이동
PVector_t pv = { .yd = -250, .L = 1000, .s0 = 4, .sd = 4 };
// yd = 목표각 × 10 (deg × 10),  L = 소요 시간 (ms)
XM_SendPVector(SYS_NODE_ID_RH, &pv);

// 임피던스 명령 — 가상 스프링-댐퍼처럼 동작
IVector_t iv = { .epsilon = 0, .kp = 80, .kd = 1, .lambda = 0, .duration = 50 };
// kp / kd: 0~100 (%)
XM_SendIVector(SYS_NODE_ID_RH, &iv);
```

---

## 제어 시스템 한눈에

```
┌─────────────────────────────────────────────────────┐
│  KIT H10 중앙 모듈                                   │
│  실시간 보행 분석 (1 kHz)                            │
│  ┌──────────────┐   Body Data 설정 필요             │
│  │ gaitCycle    │← XM_SendUserBodyData()            │
│  │ footContact  │                                    │
│  │ forwardVel   │                                    │
│  └──────────────┘                                   │
│  고관절 모터 각도 / 추정 토크                          │
└──────────────────┬──────────────────────────────────┘
                   │ CAN-FD (1 ms 주기 센서 데이터)
┌──────────────────▼──────────────────────────────────┐
│  XM10                                               │
│  ┌───────────────────────────────────────────────┐  │
│  │  Control_Loop() — 매 1 ms 호출                    │  │
│  │                                               │  │
│  │  XM.status.h10.* 읽기                         │  │
│  │      ↓                                        │  │
│  │  내 알고리즘 계산 (예: τ = K·e + B·ė)         │  │
│  │      ↓                                        │  │
│  │  XM_SetAssistTorque(RH, LH) 전송              │  │
│  └───────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────┘
```

---

## 토크 제어 예제 한눈 정리

### 외골격 기본 모드 (Ex.11~13)

| 예제 | 제어 방식 | Body Data |
|------|----------|:---------:|
| 11 Passive | 사전 정의 움직임 명령 (P/I-Vector) | ✗ |
| 12 Active Assist | 의도 감지 + 단계 토크 | ✗ |
| 13 Resistive | τ = −B·ω (속도 저항) | ✗ |

### 토크 제어 입문 (Ex.14~15)

| 예제 | 제어 방식 | Body Data |
|------|----------|:---------:|
| 14 PD 제어 | τ = Kp·e + Kd·ė | ✗ |
| 15 역진자 모델 | τ = Mgl·sin(θ) + PD | ✗ |

### 제어 연구 시리즈 (Ex.20~30)

| 예제 | 제어 방식 | Body Data | 논문 |
|------|----------|:---------:|------|
| 20 임피던스 | τ = K·(θ_d−θ) + B·θ̇ | ✗ | Hogan 1985 |
| 21 중력 보상 | τ = α·(Mgl·cos θ + B_f·θ̇) | ✗ | Just 2018 |
| 22 CPG 진동자 | φ̇ = ω + ε·F·cosφ | △ | Ronsse 2011 |
| 23 보행 위상 적응 | τ(φ) = A·sin(π·φ) 구간별 | ✔ | Quinlivan 2017 |
| 24 가상 구속 (HZD) | θ_d(s) = Bézier(s) | ✔ | Westervelt 2003 |
| 25 입각기 가변 강성 | K_stance / K_swing 전환 | ✔ | Collins 2015 |
| 26 ILC | τ_{k+1} = τ_k + L·e_k | ✔ | Emken 2007 |
| 27 MRAC | MIT Rule 적응 게인 | ✗ | Slotine 1991 |
| 28 어드미턴스 | τ_ext → 가상 동역학 → θ_ref | ✗ | Keemink 2018 |
| 29 좌우 협응 | τ_R = −K_c·(θ_R+θ_L) | △ | Duschau-Wicke 2010 |
| 30 FF+FB 혼합 | τ = τ_ff(모델) + τ_fb(PD) | ✗ | Slotine 1991 Ch.6 |

---

## 관련 문서

- [Getting Started](../getting-started/) — 환경 구축 + 첫 빌드
- [Architecture](../architecture/) — 시스템 큰 그림
- [KIT H10 Firmware](../kit-h10-firmware/) — 펌웨어 호환성
- [Tutorials](../tutorials/) — 예제 학습 경로
