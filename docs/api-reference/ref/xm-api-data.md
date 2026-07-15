# `xm_api_data.h` — XM10 통합 데이터 및 제어 인터페이스

> **대상 헤더**: `XM_FW/XM_API/xm_api_data.h` (Rev1.1 / Rev2.0 공통 — GRF 확장 필드만 🟢 Rev 2.0 전용)
> **관련 개념 문서**: [02. KIT H10 제어 + 데이터](../02-h10-control-n-data.md) (IPO 사이클, Body Data, 토크 부호 규약 설명)
> **관련 예제**: [11 Passive Mode](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/11_Passive_Mode/) · [12 Active Assist](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/12_Active_Assist_Mode/) · [13 Resistive](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/13_Resistive_Mode/) · [14 PD Realtime](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/14_PD_Realtime_Control/) · [16 TinyAI Sensor Fusion](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/16_TinyAI_Sensor_Fusion/) · [32 GRF Gait Intent](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/32_GRF_Gait_Intent/) · [37 FES Hub Ctrl](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/37_FES_Hub_Module_Ctrl/) 🛑 Rev 2.0 전용 · [41 IMU Hub Dashboard](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/41_IMU_Hub_Dashboard/) 🛑 Rev 2.0 전용 · [42 EMG Hub Biofeedback](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/42_EMG_Hub_Biofeedback/) 🛑 Rev 2.0 전용

---

## 언제 사용하나

`xm_api_data.h`는 XM10 펌웨어에서 **가장 자주 열어보게 되는 파사드**입니다. `Control_Loop()` 안에서 `KIT H10`과 각종 센서 허브(GRF, External IMU, IMU Hub, EMG Hub, FES Hub)의 최신 상태를 읽고(`XM.status`), 계산한 제어 명령을 내리는(`XM_Set*` / `XM_Send*`) 함수가 모두 여기 선언되어 있습니다.

전역 객체 `XM`이 채워지고 비워지는 **IPO(Input-Process-Output) 사이클** 자체의 동작 원리, 토크 부호 규약, Body Data 전제조건 같은 배경 지식은 이 페이지에서 다시 설명하지 않습니다. 처음이라면 [02. KIT H10 제어 + 데이터](../02-h10-control-n-data.md)를 먼저 읽고, 이 페이지는 함수 시그니처·파라미터·구조체 필드를 빠르게 찾아보는 용도로 사용하세요.

> ℹ️ **`nodeId` 파라미터 공통 안내**: 이 헤더의 관절 제어 함수 대부분은 `SystemNodeID_t nodeId`를 받습니다. 실제 사용 값은 `SYS_NODE_ID_RH`(오른쪽 고관절) / `SYS_NODE_ID_LH`(왼쪽 고관절) 두 가지이며, `data_object_dictionaries.h`(다른 헤더)에 정의되어 있습니다. 본 페이지는 `xm_api_data.h` 범위만 다루므로 `SystemNodeID_t` 자체의 전체 열거값 목록은 다루지 않습니다.

---

## 함수 목록

### 제어 모드 · 실시간 토크

| 함수 | 한 줄 설명 |
|---|---|
| [`XM_SetControlMode()`](#xm_setcontrolmode) | 토크 명령 송신 여부(모니터링/토크 제어)를 전환합니다 |
| [`XM_SetAssistTorque()`](#xm_setassisttorque) | 양쪽 고관절 보조 토크를 동시에 설정합니다 |
| [`XM_SetAssistTorqueRH()` / `XM_SetAssistTorqueLH()`](#xm_setassisttorquerh--xm_setassisttorquelh) | 한쪽 고관절 보조 토크만 설정합니다 |

### 시스템 · 연결 상태

| 함수 | 한 줄 설명 |
|---|---|
| [`XM_IsCmConnected()`](#xm_iscmconnected) | CM과의 통신이 정상(Operational) 상태인지 확인합니다 |
| [`XM_GetXMNmtState()`](#xm_getxmnmtstate) | CM과의 PnP(NMT) 상태를 세부 값으로 가져옵니다 |

### 데이터 송신 (Body Data · PIF-Vector)

| 함수 | 한 줄 설명 |
|---|---|
| [`XM_SendUserBodyData()`](#xm_senduserbodydata) | 사용자 신체 정보(체중/키/분절 길이)를 CM으로 전송합니다 |
| [`XM_SendPVector()`](#xm_sendpvector) | 위치 기반 궤적(P-Vector)을 전송합니다 |
| [`XM_SendPVectorReset()`](#xm_sendpvectorreset) | 진행 중인 P-Vector 궤적을 즉시 취소합니다 |
| [`XM_ClearPVectorDoneFlag()`](#xm_clearpvectordoneflag) | P-Vector 완료 플래그를 수동으로 클리어합니다 |
| [`XM_SendIVector()`](#xm_sendivector) | 임피던스 제어 파라미터(I-Vector)를 전송합니다 |
| [`XM_SendFVector()`](#xm_sendfvector) | 힘 기반 궤적(F-Vector)을 전송합니다 |
| [`XM_SendIVectorKpKdMax()`](#xm_sendivectorkpkdmax) | I-Vector의 Kp/Kd 상한(100% 기준값)을 설정합니다 |

### 루틴 · 파라미터

| 함수 | 한 줄 설명 |
|---|---|
| [`XM_SetDegreeLimitRoutine()` / `XM_SetDegreeLimit()`](#xm_setdegreelimitroutine--xm_setdegreelimit) | 각도 제한 루틴 on/off + 상·하한 설정 |
| [`XM_SetVelocityLimitRoutine()` / `XM_SetVelocityLimit()`](#xm_setvelocitylimitroutine--xm_setvelocitylimit) | 각속도 제한 루틴 on/off + 상·하한 설정 |
| [`XM_SetDOBRoutine()`](#xm_setdobroutine) | 외란 관측기(DOB) 루틴을 on/off 합니다 |
| [`XM_SetNormalCompGain()` / `XM_SetResistiveCompGain()`](#xm_setnormalcompgain--xm_setresistivecompgain) | 중력 보상 / 저항 보상 게인을 설정합니다 |
| [`XM_SetH10AssistExistingMode()`](#xm_seth10assistexistingmode) | H10 내장(기존) 보조 알고리즘을 활성/비활성화합니다 |

### 유틸리티

| 함수 | 한 줄 설명 |
|---|---|
| [`XM_CaptureLoopCountBase()` / `XM_GetRelativeLoopCount()`](#xm_captureloopcountbase--xm_getrelativeloopcount) | Assist Loop Count의 기준점을 캡처하고, 그 이후의 상대 카운트를 반환합니다 |

총 **23개 함수** + 매크로 3종 + 구조체/열거형 13종을 포함합니다 (아래 [타입 · 매크로](#타입--매크로) 참고).

---

## 함수 상세

### `XM_SetControlMode()`

```c
void XM_SetControlMode(XmControlMode_t mode);
```

로봇의 **출력(구동) ON/OFF**를 결정하는 안전 스위치입니다. `XM_CTRL_MONITOR`/`XM_CTRL_TORQUE` 두 모드 모두에서 사용자의 `Control_Loop()` 알고리즘 자체는 매 tick 그대로 실행됩니다 — 차이는 "계산된 토크 명령을 CM으로 실제 전송하느냐"뿐입니다. 즉 알고리즘을 끄는 스위치가 아니라 **출력만** 끄고 켜는 스위치입니다.

| 파라미터 | 타입 | 설명 |
|---|---|---|
| `mode` | `XmControlMode_t` | `XM_CTRL_MONITOR`(기본값, 출력 없음) 또는 `XM_CTRL_TORQUE`(실제 구동) |

**반환값**: 없음

**안전 로직**: 모드가 바뀌는 순간(특히 MONITOR → TORQUE) 급발진 방지를 위해 **모든 토크 명령이 내부적으로 0으로 초기화**됩니다.

⚠️ **호출 컨텍스트**: `Control_Setup()`/`Control_Loop()` 컨텍스트 기준 (헤더에 별도 ISR 안전성 명시 없음). 알고리즘 진입(Entry) 시 `XM_CTRL_TORQUE`, 종료(Exit) 시 `XM_CTRL_MONITOR`로 되돌리는 것이 일반적인 패턴입니다.

**예제**
```c
void Active_Entry(void) {
    XM_SetControlMode(XM_CTRL_TORQUE);   // 실제 구동 시작
}

void Active_Exit(void) {
    XM_SetControlMode(XM_CTRL_MONITOR);  // 안전하게 모니터링으로 복귀
}
```

**참고**: [`XmControlMode_t`](#xmcontrolmode_t)

---

### `XM_SetAssistTorque()`

```c
void XM_SetAssistTorque(float rh, float lh);
```

양쪽 고관절의 보조 토크를 한 번에 설정합니다. 실제 CAN 전송은 이 함수가 아니라 IPO 사이클의 Output 단계에서 자동으로 이루어집니다.

| 파라미터 | 타입 | 설명 |
|---|---|---|
| `rh` | `float` | 오른쪽(RH) 고관절 보조 토크, 단위 **Nm** — 첫 번째 인자 |
| `lh` | `float` | 왼쪽(LH) 고관절 보조 토크, 단위 **Nm** — 두 번째 인자 |

**반환값**: 없음

> ⚠️ 인자 순서는 **(오른쪽 rh, 왼쪽 lh)** 입니다. 헷갈리면 [`XM_SetAssistTorqueRH()` / `XM_SetAssistTorqueLH()`](#xm_setassisttorquerh--xm_setassisttorquelh)로 한쪽씩 지정하세요.
> 부호·단위·하드 리미트(±10 Nm) 등 상세 규약은 [02문서 §실시간 제어](../02-h10-control-n-data.md#xm_setassisttorque)를 참고하세요 — 이 헤더 자체에는 스케일/클램프 값이 정의되어 있지 않습니다(다른 내부 모듈에서 적용).

⚠️ **호출 컨텍스트**: `Control_Loop()` 기준. `XM_SetControlMode(XM_CTRL_TORQUE)`가 설정되어 있어야 실제로 전송됩니다.

**예제**
```c
void Active_Loop(void) {
    float cmd_R = XM.status.h10.rightHipAngle * 0.5f;   // 간단한 P 제어
    float cmd_L = XM.status.h10.leftHipAngle  * 0.5f;
    XM_SetAssistTorque(cmd_R, cmd_L);
}
```

---

### `XM_SetAssistTorqueRH()` / `XM_SetAssistTorqueLH()`

```c
void XM_SetAssistTorqueRH(float rh);
void XM_SetAssistTorqueLH(float lh);
```

한쪽 다리의 보조 토크만 개별적으로 설정합니다. 반대쪽 다리의 토크 값은 이전에 설정된 값을 그대로 유지합니다.

| 파라미터 | 타입 | 설명 |
|---|---|---|
| `rh` (RH 버전) | `float` | 오른쪽 고관절 보조 토크, 단위 Nm |
| `lh` (LH 버전) | `float` | 왼쪽 고관절 보조 토크, 단위 Nm |

**반환값**: 없음

⚠️ **호출 컨텍스트**: `Control_Loop()` 기준. `XM_SetAssistTorque()`와 동일하게 내부 Dirty Flag(`XmOutput_t._dirty_flags`)를 세팅하는 방식으로 동작합니다.

**예제**
```c
XM_SetAssistTorqueRH(5.0f);   // 오른쪽만 5.0 Nm, 왼쪽은 기존 값 유지
```

---

### `XM_IsCmConnected()`

```c
bool XM_IsCmConnected(void);
```

제어 모듈(CM)과의 통신 연결 상태를 확인합니다.

**반환값**

| 값 | 의미 |
|---|---|
| `true` | 연결이 정상(Operational) 상태 |
| `false` | 연결이 끊겼거나 아직 준비되지 않은 상태 |

⚠️ **호출 컨텍스트**: `Control_Setup()`/`Control_Loop()` 컨텍스트 기준.

**예제**
```c
if (XM_IsCmConnected()) {
    // 알고리즘 시작 조건으로 사용
}
```

**참고**: [`XM_GetXMNmtState()`](#xm_getxmnmtstate) — 더 세밀한 상태 분기가 필요할 때 사용

---

### `XM_GetXMNmtState()`

```c
CM_NmtState_t XM_GetXMNmtState(void);
```

CM과의 DOP V3 PnP(NMT) 상태를 세부 값으로 반환합니다. `XM_IsCmConnected()`는 내부적으로 `XM_GetXMNmtState() == CM_NMT_OPERATIONAL`을 확인하는 것과 같습니다.

**반환값**: `CM_NmtState_t` (`cm_drv.h`에 정의 — 이 헤더가 include 하는 다른 헤더 소속)

| 상태 | 값 | 설명 |
|---|---|---|
| `CM_NMT_INITIALISING` | 0 | 부팅 중 (Boot-up 메시지 미수신) |
| `CM_NMT_PRE_OPERATIONAL` | 1 | SDO 통신 가능, PDO 비활성 |
| `CM_NMT_OPERATIONAL` | 2 | 모든 통신 활성 (정상 상태) |
| `CM_NMT_STOPPED` | 3 | 통신 중단됨 |

⚠️ **호출 컨텍스트**: `Control_Setup()`/`Control_Loop()` 컨텍스트 기준. SDK 예제 중 이 함수를 직접 호출하는 사례는 아직 없습니다(모두 `XM_IsCmConnected()`로 충분).

---

### `XM_SendUserBodyData()`

```c
void XM_SendUserBodyData(const uint32_t bodyData[8]);
```

사용자 신체 정보(체중, 키, 분절 길이 등)를 CM으로 전송합니다. CM은 이 값을 바탕으로 실시간 보행 분석 정확도를 높입니다.

| 파라미터 | 타입 | 설명 |
|---|---|---|
| `bodyData` | `const uint32_t[8]` | 아래 표의 8개 값을 담은 배열 |

**`bodyData` 인덱스별 의미** (단위: header 주석 기준):

| 인덱스 | 설명 | 단위 |
|---|---|---|
| 0 | 착용자 몸무게 | g |
| 1 | 착용자 키 | mm |
| 2 | 오른쪽 허벅지 분절 길이 | mm |
| 3 | 왼쪽 허벅지 분절 길이 | mm |
| 4 | 오른쪽 종아리 분절 길이 | mm |
| 5 | 왼쪽 종아리 분절 길이 | mm |
| 6 | 오른쪽 발목 분절 길이 | mm |
| 7 | 왼쪽 발목 분절 길이 | mm |

**반환값**: 없음

⚠️ **호출 컨텍스트**: `Control_Setup()` 권장(1회 호출). 신체 정보는 보통 세션 중 변하지 않으므로 매 tick 반복 호출할 필요가 없습니다 — 미호출 시 `forwardVelocity`/`leftKneeAngle` 등 추정 데이터가 부정확해집니다 ([README §Body Data](../README.md) 참고).

**예제**
```c
void Control_Setup(void) {
    uint32_t body_data[8] = {
        70000, 1750, 450, 450, 420, 420, 60, 60,
    };
    XM_SendUserBodyData(body_data);
}
```

---

### `XM_SendPVector()`

```c
void XM_SendPVector(SystemNodeID_t nodeId, const PVector_t* pVector);
```

지정된 관절에 **위치 기반 궤적(P-Vector)**을 전송합니다. 모터드라이버가 5차 다항식(polynomial) 궤적을 생성해 지정 시간(`L`) 안에 목표 위치(`yd`)까지 부드럽게 움직입니다.

| 파라미터 | 타입 | 설명 |
|---|---|---|
| `nodeId` | `SystemNodeID_t` | 명령을 전달할 관절 (`SYS_NODE_ID_RH` / `SYS_NODE_ID_LH`) |
| `pVector` | `const PVector_t*` | 전송할 P-Vector 데이터 구조체 포인터 |

**반환값**: 없음

⚠️ 반드시 사전에 [`XM_SendIVector()`](#xm_sendivector)로 임피던스 파라미터가 설정되어 있어야 합니다.

⚠️ **호출 컨텍스트**: `Control_Loop()` 기준.

**예제**
```c
PVector_t pv = { .yd = 250, .L = 1000, .s0 = 4, .sd = 4 };  // 25.0deg 목표, 1초간 이동
XM_SendPVector(SYS_NODE_ID_RH, &pv);
```

**참고**: [`PVector_t`](#pvector_t)

---

### `XM_SendPVectorReset()`

```c
void XM_SendPVectorReset(SystemNodeID_t nodeId);
```

현재 실행 중이거나 대기 중인 P-Vector 명령을 즉시 초기화(취소)합니다.

| 파라미터 | 타입 | 설명 |
|---|---|---|
| `nodeId` | `SystemNodeID_t` | P-Vector를 리셋할 관절 |

**반환값**: 없음

⚠️ **호출 컨텍스트**: `Control_Loop()` 기준. 모드 전환/비상 정지 시 사용.

---

### `XM_ClearPVectorDoneFlag()`

```c
void XM_ClearPVectorDoneFlag(SystemNodeID_t nodeId);
```

`XM.status.h10.isPVectorRHDone` / `isPVectorLHDone` 플래그가 `true`가 된 것을 확인하고 관련 로직 처리를 마친 뒤, 이벤트를 '소비(consume)'했음을 알리기 위해 수동으로 호출합니다.

| 파라미터 | 타입 | 설명 |
|---|---|---|
| `nodeId` | `SystemNodeID_t` | 완료 플래그를 클리어할 관절 |

**반환값**: 없음

⚠️ **주의**: 호출하지 않으면 플래그가 계속 `true`로 남아 동일한 완료 이벤트가 반복 처리될 수 있습니다.

⚠️ **호출 컨텍스트**: `Control_Loop()` 기준.

---

### `XM_SendIVector()`

```c
void XM_SendIVector(SystemNodeID_t nodeId, const IVector_t* iVector);
```

지정된 관절에 **임피던스 제어 파라미터(I-Vector)**를 전송하여, 관절이 가상의 스프링-댐퍼처럼 동작하도록 설정합니다.

| 파라미터 | 타입 | 설명 |
|---|---|---|
| `nodeId` | `SystemNodeID_t` | 명령을 전달할 관절 |
| `iVector` | `const IVector_t*` | 임피던스 파라미터 구조체 포인터 |

**반환값**: 없음

⚠️ **호출 컨텍스트**: `Control_Loop()` 기준. `kp`/`kd`는 [`XM_SendIVectorKpKdMax()`](#xm_sendivectorkpkdmax)로 설정한 최댓값 대비 백분율(%)로 해석됩니다.

**예제**
```c
IVector_t iv = { .epsilon = 0, .kp = 80, .kd = 1, .lambda = 0, .duration = 50 };
XM_SendIVector(SYS_NODE_ID_RH, &iv);
```

**참고**: [`IVector_t`](#ivector_t), [`XM_SendIVectorKpKdMax()`](#xm_sendivectorkpkdmax)

---

### `XM_SendFVector()`

```c
void XM_SendFVector(SystemNodeID_t nodeId, const FVector_t* fVector);
```

지정된 관절에 **힘 기반 궤적(F-Vector)**을 전송하여, 사전 정의된 토크 프로파일을 생성하도록 명령합니다.

| 파라미터 | 타입 | 설명 |
|---|---|---|
| `nodeId` | `SystemNodeID_t` | 명령을 전달할 관절 |
| `fVector` | `const FVector_t*` | 목표 토크·모드 정보를 담은 구조체 포인터 |

**반환값**: 없음

⚠️ **호출 컨텍스트**: `Control_Loop()` 기준. 현재 SDK 예제 중 이 함수를 사용하는 사례는 없습니다 — 아래 예제는 구조체 필드 정의만으로 구성한 참고용 초기화 코드입니다.

**예제**
```c
FVector_t fv = { .modeIdx = 1, .tauMax = 500, .delay = 0, .zero = 0 };  // tauMax=5.00A (scaled by 100)
XM_SendFVector(SYS_NODE_ID_RH, &fv);
```

**참고**: [`FVector_t`](#fvector_t)

---

### `XM_SendIVectorKpKdMax()`

```c
void XM_SendIVectorKpKdMax(SystemNodeID_t nodeId, const float kpMax, const float kdMax);
```

지정된 관절의 임피던스 제어에서 `XM_SendIVector()`의 `kp`/`kd`(%)가 100%일 때 적용될 **최댓값**을 설정합니다.

| 파라미터 | 타입 | 설명 |
|---|---|---|
| `nodeId` | `SystemNodeID_t` | 파라미터를 설정할 관절 |
| `kpMax` | `const float` | `kp`가 100%일 때 적용될 최대 가상 스프링 강성 |
| `kdMax` | `const float` | `kd`가 100%일 때 적용될 최대 가상 댐퍼 강성 |

**반환값**: 없음

⚠️ **호출 컨텍스트**: `Control_Setup()` 또는 상태 진입(Entry) 시점 권장 — `XM_SendIVector()`보다 먼저 설정되어 있어야 백분율 해석이 의미를 갖습니다.

**예제**
```c
XM_SendIVectorKpKdMax(SYS_NODE_ID_RH, 6.0f, 1.0f);
XM_SendIVectorKpKdMax(SYS_NODE_ID_LH, 6.0f, 1.0f);
```

---

### `XM_SetDegreeLimitRoutine()` / `XM_SetDegreeLimit()`

```c
void XM_SetDegreeLimitRoutine(SystemNodeID_t nodeId, bool isSet);
void XM_SetDegreeLimit(SystemNodeID_t nodeId, float upperLimit, float lowerLimit);
```

지정된 관절의 **각도 제한 루틴**을 켜고 끄고(`Routine`), 그 상·하한 값을 설정합니다(`Limit`).

| 파라미터 | 타입 | 설명 |
|---|---|---|
| `nodeId` | `SystemNodeID_t` | 제어할 관절 |
| `isSet` (Routine) | `bool` | `true`: 루틴 활성화 / `false`: 비활성화 |
| `upperLimit` (Limit) | `float` | 가동범위 상한, 단위 degree |
| `lowerLimit` (Limit) | `float` | 가동범위 하한, 단위 degree |

**반환값**: 없음

⚠️ **호출 컨텍스트**: `Control_Setup()`/`Control_Loop()` 컨텍스트 기준. 현재 SDK 예제 중 직접 사용하는 사례는 없습니다.

**예제**
```c
XM_SetDegreeLimitRoutine(SYS_NODE_ID_RH, true);
XM_SetDegreeLimit(SYS_NODE_ID_RH, 30.0f, -30.0f);
```

---

### `XM_SetVelocityLimitRoutine()` / `XM_SetVelocityLimit()`

```c
void XM_SetVelocityLimitRoutine(SystemNodeID_t nodeId, bool isSet);
void XM_SetVelocityLimit(SystemNodeID_t nodeId, float upperLimit, float lowerLimit);
```

지정된 관절의 **각속도 제한 루틴**을 켜고 끄고, 그 상·하한 값을 설정합니다.

| 파라미터 | 타입 | 설명 |
|---|---|---|
| `nodeId` | `SystemNodeID_t` | 제어할 관절 |
| `isSet` (Routine) | `bool` | `true`: 루틴 활성화 / `false`: 비활성화 |
| `upperLimit` (Limit) | `float` | 가동속도범위 상한, 단위 deg/s |
| `lowerLimit` (Limit) | `float` | 가동속도범위 하한, 단위 deg/s |

**반환값**: 없음

⚠️ **호출 컨텍스트**: `Control_Setup()`/`Control_Loop()` 컨텍스트 기준. 현재 SDK 예제 중 직접 사용하는 사례는 없습니다.

**예제**
```c
XM_SetVelocityLimitRoutine(SYS_NODE_ID_LH, true);
XM_SetVelocityLimit(SYS_NODE_ID_LH, 100.0f, -100.0f);
```

---

### `XM_SetDOBRoutine()`

```c
void XM_SetDOBRoutine(SystemNodeID_t nodeId, bool isSet);
```

지정된 관절의 **외란 관측기(DOB, Disturbance Observer)** 루틴을 활성화/비활성화합니다.

| 파라미터 | 타입 | 설명 |
|---|---|---|
| `nodeId` | `SystemNodeID_t` | 제어할 관절 |
| `isSet` | `bool` | `true`: 활성화 / `false`: 비활성화 |

**반환값**: 없음

⚠️ **호출 컨텍스트**: `Control_Setup()`/`Control_Loop()` 컨텍스트 기준. 현재 SDK 예제 중 직접 사용하는 사례는 없습니다. 헤더 주석 기준으로 DOB 사용을 위해서는 모터드라이버 측 System Identification 기록이 필요합니다.

---

### `XM_SetNormalCompGain()` / `XM_SetResistiveCompGain()`

```c
void XM_SetNormalCompGain(SystemNodeID_t nodeId, uint8_t gain);
void XM_SetResistiveCompGain(SystemNodeID_t nodeId, float gain);
```

각각 **일반 보상(중력 보상 등)**과 **저항 보상(저항 훈련 모드)**의 강도를 조절합니다.

| 파라미터 | 타입 | 설명 |
|---|---|---|
| `nodeId` | `SystemNodeID_t` | 제어할 관절 |
| `gain` (`Normal`) | `uint8_t` | 일반 보상 게인 |
| `gain` (`Resistive`) | `float` | 저항 보상 게인 |

**반환값**: 없음

⚠️ **주의**: 두 함수의 `gain` 파라미터 타입이 다릅니다(`uint8_t` vs `float`). 헤더에 값의 범위·스케일이 별도로 정의되어 있지 않으므로, 시그니처 그대로 정수/실수 구분에 유의하세요.

⚠️ **호출 컨텍스트**: `Control_Loop()` 기준. `XM_SetNormalCompGain()`은 현재 SDK 예제 중 직접 사용하는 사례가 없습니다.

**예제**
```c
float strongResistance = 0.8f;
XM_SetResistiveCompGain(SYS_NODE_ID_RH, strongResistance);
```

---

### `XM_SetH10AssistExistingMode()`

```c
void XM_SetH10AssistExistingMode(bool isSet);
```

H10에 내장된 **기존 보조 알고리즘**의 활성화 여부를 설정합니다. XM10과 H10이 연결되면 기본값은 **비활성화**(XM10이 직접 제어)입니다.

| 파라미터 | 타입 | 설명 |
|---|---|---|
| `isSet` | `bool` | `true`: H10 기존 보조 알고리즘 활성화 / `false`: 비활성화 |

**반환값**: 없음

⚠️ **호출 컨텍스트**: `Control_Setup()` 기준.

**예제**
```c
void Off_Entry(void) {
    XM_SetH10AssistExistingMode(true);  // H10 내장 보조로 전환
}
```

---

### `XM_CaptureLoopCountBase()` / `XM_GetRelativeLoopCount()`

```c
void XM_CaptureLoopCountBase(void);
uint32_t XM_GetRelativeLoopCount(void);
```

호출 시점의 `h10AssistModeLoopCnt`를 기준점(0)으로 저장하고(`Capture`), 이후 그 기준점 대비 상대 카운트를 반환합니다(`GetRelative`). 데이터 로깅 세션을 시작할 때 호출하면, 저장 데이터의 카운트가 항상 0부터 시작합니다.

**반환값** (`XM_GetRelativeLoopCount`): `h10AssistModeLoopCnt - 기준점`. `XM_CaptureLoopCountBase()`를 호출한 적이 없으면 절대값을 그대로 반환합니다.

⚠️ **호출 컨텍스트**: `Control_Setup()`/상태 진입(Entry) 시점에 `Capture`, `Control_Loop()`에서 `GetRelative` 조회. 현재 SDK 예제 중 직접 사용하는 사례는 없습니다.

**예제**
```c
void Active_Entry(void) {
    XM_CaptureLoopCountBase();
}

void Active_Loop(void) {
    uint32_t elapsed = XM_GetRelativeLoopCount();  // 0부터 시작하는 상대 카운트
    myLogData.loopCnt = elapsed;
}
```

---

## 타입 · 매크로

### 매크로

| 매크로 | 값 | 설명 |
|---|---|---|
| `XM_GRF_CHANNEL_SIZE` | `14` | 레거시 GRF(FSR) 슈즈 센서의 채널 수 |
| `XM_IMU_HUB_SENSOR_COUNT` | `6` | IMU Hub Module의 센서(포트) 개수 |
| `XM_FES_HUB_CH_COUNT` | `2` | FES Hub Module의 채널 수 |
| `XM_GRF_FSR_CH_TOTAL` 🟢 Rev 2.0 전용 | `24` | SM-GRF 고정프레임 모듈의 FSR 채널 수(ADC1 15ch + ADC3 9ch). `module.h`에 정의되며 `xm_api_data.h`가 include — Rev1.1에는 해당 include 자체가 없음 |

---

### `XmControlMode_t`

XM10의 제어 권한(출력 ON/OFF) 모드입니다.

```c
typedef enum {
    XM_CTRL_MONITOR = 0,  // 모니터링 모드 (기본값) — 알고리즘은 실행되나 토크 명령 미전송
    XM_CTRL_TORQUE  = 1   // 토크 제어 모드 — 계산된 토크 명령을 주기적으로 전송 (실제 구동)
} XmControlMode_t;
```

| 값 | 이름 | 설명 |
|---|---|---|
| 0 | `XM_CTRL_MONITOR` | 기본값. 알고리즘은 매 tick 실행되지만 출력(구동)은 차단됩니다 |
| 1 | `XM_CTRL_TORQUE` | 계산된 토크 명령을 실제로 CM에 전송합니다 |

**참고**: [`XM_SetControlMode()`](#xm_setcontrolmode)

---

### `XmH10Mode_t`

H10 로봇의 현재 동작 모드입니다 (`XM.status.h10.h10Mode`로 읽기 전용 확인).

```c
typedef enum {
    XM_H10_MODE_STANDBY = 0,  // 대기 모드 (무출력)
    XM_H10_MODE_ASSIST  = 1,  // 보조 모드 (토크 출력)
    XM_H10_MODE_UNKNOWN
} XmH10Mode_t;
```

---

### `PVector_t`

위치 기반 궤적 명령 벡터입니다. [`XM_SendPVector()`](#xm_sendpvector)의 파라미터.

```c
typedef struct {
    int16_t  yd; // 목표 위치, deg×10 스케일
    uint16_t L;  // 이동 시간 (ms)
    uint8_t  s0; // 시작 가속도 프로파일 (deg/s^2)
    uint8_t  sd; // 종료 감속도 프로파일 (deg/s^2)
} PVector_t;
```

| 필드 | 타입 | 단위 | 설명 |
|---|---|---|---|
| `yd` | `int16_t` | deg × 10 | 목표 위치. 헤더 주석에 실동작 검증값 명시: **25.0deg → 250** |
| `L` | `uint16_t` | ms | 궤적 이동 시간 |
| `s0` | `uint8_t` | deg/s² | 시작 가속도 프로파일 |
| `sd` | `uint8_t` | deg/s² | 종료 감속도 프로파일 |

> ⚠️ **스케일 표기 차이 주의**: [02. KIT H10 제어 + 데이터](../02-h10-control-n-data.md) 문서의 예제 텍스트에는 `yd`가 "scaled by 100"으로 남아있는 곳이 있으나, 본 헤더(`xm_api_data.h`, ground truth)의 주석은 **"scaled by 10 — 실동작 검증값"**으로 명시합니다. 코드 작성 시 본 페이지(헤더 기준)를 따르세요.

---

### `IVector_t`

임피던스 제어 파라미터 벡터입니다. [`XM_SendIVector()`](#xm_sendivector)의 파라미터.

```c
typedef struct {
    uint8_t  epsilon; // Corridor 절반 폭 (deg×10)
    uint8_t  kp;      // 가상 스프링 강도 (%)
    uint8_t  kd;      // 가상 댐퍼 강도 (%)
    uint8_t  lambda;  // 임피던스 비율 (×100 스케일)
    uint16_t duration;// 전환 시간 (ms)
} IVector_t;
```

| 필드 | 타입 | 단위 | 설명 |
|---|---|---|---|
| `epsilon` | `uint8_t` | deg × 10 | Corridor(코리더)의 절반 폭 |
| `kp` | `uint8_t` | % | 가상 스프링 강도 — `XM_SendIVectorKpKdMax()`의 `kpMax` 대비 백분율 |
| `kd` | `uint8_t` | % | 가상 댐퍼 강도 — `kdMax` 대비 백분율 |
| `lambda` | `uint8_t` | ratio × 100 | 임피던스 비율 |
| `duration` | `uint16_t` | ms | 파라미터 전환 시간 |

---

### `FVector_t`

힘 기반 궤적 명령 벡터입니다. [`XM_SendFVector()`](#xm_sendfvector)의 파라미터.

```c
typedef struct {
    uint16_t modeIdx; // 토크 프로파일 인덱스, 0.1~10
    int16_t  tauMax;  // 최대 토크 (A × 100 스케일)
    uint16_t delay;   // 초기 지연 시간 (ms)
    uint16_t zero;    // 리셋용 더미 데이터
} FVector_t;
```

| 필드 | 타입 | 단위 | 설명 |
|---|---|---|---|
| `modeIdx` | `uint16_t` | Index (0.1 ~ 10) | 토크 프로파일 인덱스(Tp) |
| `tauMax` | `int16_t` | A × 100 | 최대 토크 |
| `delay` | `uint16_t` | ms | 초기 지연 시간 |
| `zero` | `uint16_t` | - | 리셋을 위한 더미 데이터 (헤더 주석: "Dummy data for reset") |

---

### `XmH10Data_t`

`XM.status.h10`으로 접근하는 **KIT H10 로봇 본체 데이터**(DOP V1)입니다. 엔코더, 관절 각도, 보행 상태 등을 포함합니다.

| 필드 | 타입 | 단위 | 설명 |
|---|---|---|---|
| `is_connected` | `bool` | - | H10과의 통신 연결 상태 |
| `h10AssistModeLoopCnt` | `uint32_t` | - | H10 보조 모드 루프 카운트 (Assist Mode 시작 시 count) |
| `h10PostProcessingCnt` | `uint32_t` | - | GaitAnalysis Post-Processing 샘플 카운트 (0~30000, 5m 분석용) |
| `h10Mode` | `XmH10Mode_t` | - | H10 동작 모드 (Assist ↔ Standby) |
| `h10AssistLevel` | `uint8_t` | 0~10 | H10 보조 레벨 |
| `h10FSMcurrentState` | `uint8_t` | - | H10 현재 FSM 상태 |
| `isPVectorRHDone` | `bool` | - | RH P-Vector 완료 플래그 |
| `isPVectorLHDone` | `bool` | - | LH P-Vector 완료 플래그 |
| `h10IsNeutralPosSet` | `bool` | - | H10 중립각도 설정 완료 상태 |
| `leftHipAngle` / `rightHipAngle` | `float` | deg | 좌/우 고관절 각도 |
| `leftThighAngle` / `rightThighAngle` | `float` | deg | 좌/우 허벅지 절대각 |
| `leftKneeAngle` / `rightKneeAngle` | `float` | deg | 좌/우 무릎 각도 (추정치) |
| `pelvicAngle` | `float` | deg | 골반 각도 (Tilt) |
| `isLeftFootContact` / `isRightFootContact` | `bool` | - | 좌/우 발 착지 여부 |
| `forwardVelocity` | `float` | m/s | 전방 보행 속도 |
| `leftHipTorque` / `rightHipTorque` | `float` | **A** | ⚠️ 필드명과 달리 **모터 전류**입니다. 관절 토크 환산: `τ_joint[Nm] = 0.085 × 18.75 × 전류[A] ≈ 1.594 × 전류` (실제 토크 센서 없음, 전류 기반 추정) |
| `leftHipMotorAngle` / `rightHipMotorAngle` | `float` | deg | 좌/우 모터 엔코더 각도 (감속기 비율 때문에 관절 각도와 다를 수 있음) |
| `left/rightHipImuFrontalRoll` | `float` | deg | 고관절 IMU Frontal Roll |
| `left/rightHipImuSagittalPitch` | `float` | deg | 고관절 IMU Sagittal Pitch |
| `left/rightHipImuTransverseYaw` | `float` | deg | 고관절 IMU Transverse Yaw |
| `left/rightHipImuGlobalAccX/Y/Z` | `float` | m/s² | 고관절 IMU Global 가속도 |
| `left/rightHipImuGlobalGyrX/Y/Z` | `float` | deg/s | 고관절 IMU Global 자이로 |

---

### `XM_GRF_SPACE_e`

레거시 GRF(FSR) 모듈의 좌/우 구분 값입니다.

```c
typedef enum {
    XM_SPACE_LEFT = 1,
    XM_SPACE_RIGHT,
    XM_SPACE_UNKNOWN,
} XM_GRF_SPACE_e;
```

---

### `XmGrfData_t`

`XM.status.grf`로 접근하는 **지면 반발력(족압) 센서 데이터**입니다. 레거시 14채널 MarvelDex FSR 슈즈 필드는 항상 존재하고, 🟢 **Rev 2.0 전용** 24채널 고정프레임(SM-GRF) 필드는 `XM_GRF_FIXED_FRAME_MODULE` 매크로가 켜져 있을 때만(Rev 2.0) 구조체에 포함됩니다.

| 필드 | 타입 | 단위 | 설명 |
|---|---|---|---|
| `is_left_grf_connected` / `is_right_grf_connected` | `bool` | - | 좌/우 GRF 모듈 연결 상태 |
| `leftLastUpdateTick` / `rightLastUpdateTick` | `uint32_t` | ms | 좌/우 데이터 수신 시각 |
| `leftSensorSpace` / `rightSensorSpace` | `XM_GRF_SPACE_e` | - | 1=왼발, 2=오른발 |
| `leftRollingIndex` / `rightRollingIndex` | `uint8_t` | - | 패킷 시퀀스 (0~199) |
| `leftSensorData[14]` / `rightSensorData[14]` | `uint8_t[XM_GRF_CHANNEL_SIZE]` | 0~255 Raw | 14채널 FSR 값 |
| `leftBatteryLevel` / `rightBatteryLevel` | `uint8_t` | 0~100 | 배터리 잔량 |
| `leftStatusFlags` / `rightStatusFlags` | `uint8_t` | - | 상태 플래그 |
| `leftFsr[24]` / `rightFsr[24]` 🟢 Rev 2.0 전용 | `uint16_t[XM_GRF_FSR_CH_TOTAL]` | ADC LSB | SM-GRF 24ch FSR raw (ADC1 15ch + ADC3 9ch) |
| `leftImu[7]` / `rightImu[7]` 🟢 Rev 2.0 전용 | `int16_t[7]` | raw | `acc[3], gyr[3], temp` 순서의 6축 IMU raw 값 |
| `leftGrfTick` / `rightGrfTick` 🟢 Rev 2.0 전용 | `uint32_t` | ms | SM-GRF 모듈 제어틱 — 연속 수신 gap/freshness 판단용 |

> ℹ️ **L/R은 물리 포트 기준**입니다 (헤더 주석: 오른발 GRF → XM 오른쪽 포트=UART8). 레거시 필드의 "1=왼발,2=오른발" 판별과는 별개로, Rev 2.0 확장 필드는 포트 자체가 좌/우에 고정 배선되어 있습니다.

---

### `XmExtImuData_t`

`XM.status.ext_imu`로 접근하는 **External UART 정밀 IMU**(현재 Xsens MTi-630) 데이터입니다. "ext_imu" 접두사는 CAN-FD 기반 IMU Hub와 구분하기 위함이며, 다른 UART IMU로 교체되어도 이름이 유지됩니다.

| 필드 | 타입 | 단위 | 설명 |
|---|---|---|---|
| `is_connected` | `bool` | - | External IMU 연결 상태 |
| `lastUpdateTick` | `uint32_t` | ms | 데이터 수신 시각 |
| `q_w, q_x, q_y, q_z` | `float` | - | Orientation (Quaternion) |
| `acc_x, acc_y, acc_z` | `float` | m/s² | Calibrated Acceleration |
| `gyr_x, gyr_y, gyr_z` | `float` | deg/s 또는 rad/s | Calibrated Gyroscope (헤더 주석에 두 단위 모두 언급 — 장치별 확인 필요) |

---

### `XmImuHubSensor_t` / `XmImuHubData_t`

`XM.status.imu_hub`로 접근하는 **IMU Hub Module**(EBIMU-9DOFV6 × 6, DOP V3) 데이터입니다.

```c
typedef struct {
    float q_w, q_x, q_y, q_z;           // Orientation (Quaternion)
    float roll, pitch, yaw;             // Orientation (Euler, deg)
    float acc_x, acc_y, acc_z;          // Calibrated Acceleration (g)
    float gyr_x, gyr_y, gyr_z;          // Calibrated Gyroscope (deg/s)
    float mag_x, mag_y, mag_z;          // Calibrated Magnetometer (uT)
} XmImuHubSensor_t;
```

| 필드 | 타입 | 단위 | 설명 |
|---|---|---|---|
| `q_w, q_x, q_y, q_z` | `float` | - | Orientation (Quaternion) |
| `roll, pitch, yaw` | `float` | deg | Orientation (Euler Angles) |
| `acc_x, acc_y, acc_z` | `float` | g | Calibrated Acceleration |
| `gyr_x, gyr_y, gyr_z` | `float` | deg/s | Calibrated Gyroscope |
| `mag_x, mag_y, mag_z` | `float` | uT | Calibrated Magnetometer |

`XmImuHubData_t` (센서 6개를 배열로 감싼 최상위 구조체):

| 필드 | 타입 | 설명 |
|---|---|---|
| `is_connected` | `bool` | IMU Hub Module 연결 상태 |
| `lastUpdateTick` | `uint32_t` | 데이터 수신 시각 (ms) |
| `sensor[6]` | `XmImuHubSensor_t[XM_IMU_HUB_SENSOR_COUNT]` | 포트 0~5의 IMU 센서 데이터 |
| `connected_mask` | `uint8_t` | 각 센서의 연결 상태 비트마스크 (bit0~5) |

**예제** (Ex.41 IMU Hub Dashboard 패턴)
```c
const XmImuHubData_t* hub = &XM.status.imu_hub;
if (hub->is_connected) {
    float roll0 = hub->sensor[0].roll;   // 포트 0 IMU의 Roll (deg)
    bool port2_ok = (hub->connected_mask & (1u << 2)) != 0;
}
```

---

### `XmEmgHubData_t`

`XM.status.emg_hub`로 접근하는 **EMG Hub Module**(sEMG 센서 허브, DOP V3) 데이터입니다. 원본은 EMG Hub TPDO1(CAN ID `0x18F`), 1kHz 샘플링에 HPF 20Hz → 정류 → RMS 200ms → Envelope 8Hz → MVC → Activation 파이프라인 결과가 포함됩니다.

| 필드 | 타입 | 단위 | 설명 |
|---|---|---|---|
| `is_connected` | `bool` | - | EMG Hub Module 연결 상태 |
| `lastUpdateTick` | `uint32_t` | ms | Slave 제어 틱(OD `0x6050 ctrl_tick_ms`, 32-bit, 약 49.7일 wrap). 연속 수신 간 delta가 1이 아니면 gap. 구 14B TPDO 수신 시 24-bit Metadata timestamp로 fallback |
| `raw_adc` | `uint16_t` | 12-bit (HW OVS 16×) | ADC 원시값 |
| `voltage_uv` | `float` | µV | EMG 전압 (AFE 환산) |
| `rms_uv` | `float` | µV | RMS 값 (200ms 슬라이딩 윈도우) |
| `envelope_uv` | `float` | µV | Envelope 값 (EMA fc≈8Hz) |
| `mvc_percent` | `uint8_t` | 0~100% | MVC 정규화 |
| `is_active` | `bool` | - | 근수축 감지 (Schmitt trigger) |
| `status_flags` | `uint8_t` | bitfield | 아래 표 참고 |

**`status_flags` 비트 의미:**

| 비트 | 이름 | 의미 |
|---|---|---|
| bit0 | `ADC_OK` | ADC 정상 |
| bit1 | `IS_ACTIVE` | 근수축 감지 상태 |
| bit2 | `SATURATED` | 신호 포화 |
| bit3 | `CALIB_VALID` | MVC 캘리브레이션 유효 |

**예제** (Ex.42 EMG Hub Biofeedback 패턴)
```c
const XmEmgHubData_t* emg = &XM.status.emg_hub;
if (emg->is_connected) {
    bool calib_valid = (emg->status_flags & (1u << 3)) != 0U;
    uint8_t mvc = emg->mvc_percent;
}
```

---

### `XmFesHubData_t`

`XM.status.fes_hub`로 접근하는 **FES Hub Module**(기능적 전기 자극, DOP V3 ES-vector, Node `0x0C`) 피드백 데이터입니다. 원본은 FES Hub TPDO1(CAN ID `0x18C`, 37B, 10ms 주기) — Legacy 16B(채널 상태/전류/HV 등) + KHJ 확장 21B(FSM/ISI/타깃 진폭 등)로 구성됩니다. 명령은 이 구조체가 아니라 SDO(`0x6300` ES-vector, `0x6310` Master Command)로 전송합니다.

| 필드 | 타입 | 단위 | 설명 |
|---|---|---|---|
| `is_connected` | `bool` | - | FES Hub Module 연결 상태 |
| `lastUpdateTick` | `uint32_t` | ms | 데이터 수신 시각 (FES Slave 24-bit timestamp, LE) |
| `ch_state[2]` | `uint8_t[XM_FES_HUB_CH_COUNT]` | 0~3 | 채널 상태: 0=IDLE, 1=READY, 2=STIMULATING, 3=FAULT |
| `ch_current_mA[2]` | `float[2]` | mA | 전류 피드백 (PID output) |
| `ch_fault_code[2]` | `uint8_t[2]` | - | 채널별 Fault 코드 |
| `hv_voltage_V` | `float` | V | HV 부스트 컨버터 출력 전압 |
| `digipot_pos` | `uint8_t` | 0~127 | Digipot 위치 (진폭 제어) |
| `es_state_packed` | `uint8_t` | bitfield | `[3:0]`=CH0 ESState, `[7:4]`=CH1 ESState |
| `error_register` | `uint8_t` | - | Error Register |
| `fsm_state` / `fsm_state_prev` | `uint8_t` | - | 현재/직전 FSM state (KHJ Control Task) |
| `isi_packed` | `uint8_t` | bitmap | `bit[N]`=ISI[N]. EXT7(bit7)/EXT8(bit8)=Master Command 동작 증적 |
| `ch_es_error_lo[2]` | `uint8_t[2]` | - | ES-vector error code low byte (채널별, CiA 301 Abort와 별개) |
| `ch_target_amplitude_mA[2]` | `float[2]` | mA | Master가 지시한 목표 진폭 (setpoint) |
| `ch_impedance[2]` | `float[2]` | ohm | 필터링된 임피던스 — 전극 접촉 판단 지표 |
| `ch_pulse_cnt[2]` | `uint16_t[2]` | - | 누적 자극 펄스 카운터 |
| `ch_voltage_diff_V[2]` | `float[2]` | V | 실제 자극 차동 전압 |

**참고**: 채널 인덱스(0/1)와 배열 크기는 [`XM_FES_HUB_CH_COUNT`](#매크로)로 정의됩니다.

---

### `XmInput_t`

`XM.status`로 접근하는 **로봇 상태 통합 구조체**입니다. 모든 읽기 전용 센서 데이터가 여기 모입니다.

```c
typedef struct {
    XmH10Data_t     h10;      // H10 로봇 본체 데이터
    XmGrfData_t     grf;      // GRF 족압 센서 데이터
    XmExtImuData_t  ext_imu;  // External UART IMU (Xsens MTi-630)
    XmImuHubData_t  imu_hub;  // IMU Hub 센서 데이터 (DOP V3)
    XmEmgHubData_t  emg_hub;  // EMG Hub 센서 데이터 (DOP V3)
    XmFesHubData_t  fes_hub;  // FES Hub 자극 피드백 (DOP V3)
} XmInput_t;
```

---

### `XmOutput_t`

`XM.command`로 접근하는 **로봇 제어 명령 구조체**입니다.

```c
typedef struct {
    XmControlMode_t control_mode;
    float assist_torque_rh;
    float assist_torque_lh;
    struct {
        uint8_t torque_rh_updated : 1;
        uint8_t torque_lh_updated : 1;
    } _dirty_flags;
} XmOutput_t;
```

| 필드 | 타입 | 설명 |
|---|---|---|
| `control_mode` | `XmControlMode_t` | 현재 제어 모드 |
| `assist_torque_rh` / `assist_torque_lh` | `float` | 우/좌 보조 토크 (Nm) |
| `_dirty_flags` | 익명 bitfield struct | ⚠️ **내부 전용** — 헤더 주석: "User는 몰라도 됨, Helper 함수가 관리". 직접 쓰지 말고 반드시 `XM_SetAssistTorque()` 계열 함수를 사용하세요 |

> ⚠️ **직접 쓰기 금지**: `XM.command`는 Staging 영역입니다. 이 구조체 필드를 직접 대입해도 Dirty Flag가 세팅되지 않아 실제로 전송되지 않습니다. 반드시 [`XM_SetAssistTorque()`](#xm_setassisttorque) 등 setter 함수를 사용하세요.

---

### `XmRobot_t` (전역 인스턴스 `XM`)

```c
typedef struct {
    XmInput_t  status;  // [Read] 센서값 (Input)
    XmOutput_t command; // [Write] 명령값 (Output)
} XmRobot_t;

extern XmRobot_t XM;
```

| 필드 | 타입 | 설명 |
|---|---|---|
| `status` | `XmInput_t` | 읽기 전용 센서 데이터 (H10/GRF/ExtIMU/IMU Hub/EMG Hub/FES Hub) |
| `command` | `XmOutput_t` | 쓰기 전용 제어 명령 Staging 영역 — 직접 쓰지 말고 `XM_Set*` 함수 사용 |

**예제**
```c
float angle = XM.status.h10.leftHipAngle;  // 왼쪽 고관절 각도 읽기
```

---

## 관련 문서

- [02. KIT H10 제어 + 데이터](../02-h10-control-n-data.md) — IPO 사이클, 토크 부호 규약, Body Data 상세, 흔한 실수
- [README — API 참고서 인덱스](../README.md) — 함수 그룹 전체 목차, Body Data 예제별 필요 여부
