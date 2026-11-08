# `xm_api_imu_calib.h` — IMU Hub 센서 제자리 보정

> 🟢 **Rev 2.0 전용** — 본 헤더는 XM10 Rev 2.0 용입니다. Rev 1.1 프로젝트(`XM10_SDK/Rev1.1/`)에서는 아래 함수를 쓸 수 없습니다. 보드 리비전 확인은 [보드 리비전 비교](../../hardware/README.md#보드-리비전-비교) 참고.
>
> - **대상 헤더**: `XM_FW/XM_API/xm_api_imu_calib.h` (`xm_api.h` 를 include 하면 함께 쓸 수 있습니다)
> - **관련 개념 문서**: [13. IMU Hub 센서 보정](../13-imu-calibration.md)
> - **관련 예제**: [48_IMU_Hub_Calibration](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/48_IMU_Hub_Calibration/)
> - **보드 확인**: 아래 동작은 API 가 정한 것이며, 보드에서는 아직 확인하지 못했습니다. (보드 확인 전) <!-- board-check -->
>
> ℹ️ IMU Hub 는 내부에서 개발 중인 모듈입니다. 사용하려면 <https://huphailab.com/contact> 로 문의해 주세요.

이 헤더는 **CAN-FD 로 연결된 IMU Hub 의 센서를 제자리에서 보정하는** API 를 정의합니다. 케이스나 옷에 넣은 센서를 꺼내지 않고 보정합니다. 지자기 보정은 장착한 그대로 돌려야 맞습니다.

---

## 언제 사용하나

센서를 케이스나 옷에 장착한 채로 자이로 영점 · 지자기 · 가속도를 보정하고 싶을 때 사용합니다. 쓰는 순서 · 자세 잡는 법 · 흔한 실수는 [13. IMU Hub 센서 보정](../13-imu-calibration.md) 을 먼저 읽어 보는 것을 권장합니다 — 이 페이지는 헤더에 선언된 **모든 함수 · 타입 · 상수의 시그니처와 파라미터 상세**만 다룹니다.

---

## 쓰는 순서

1. `XM_ImuCalibStart(채널, 항목)` 을 **한 번만** 부릅니다. 버튼을 누른 순간처럼 한 번만 일어나는 곳에서 부르세요. 매 주기 부르면 끝날 때마다 보정이 다시 시작됩니다.
2. `XM_ImuCalibGetStatus()` 로 상태를 봅니다. 함수는 기다리지 않으므로 `Control_Loop()` 에서 불러도 됩니다.
3. 지자기 · 가속도 정밀은 상태가 `XM_IMU_CALIB_WAIT_USER` 가 되면 센서를 움직인 뒤 `XM_ImuCalibFinish()` 를 부릅니다.
4. `XM_IMU_CALIB_DONE` 이면 끝입니다. `XM_IMU_CALIB_FAILED` 이면 `result` 를 봅니다.

시작 요청은 IMU Hub 에 한 번만 보냅니다. 같은 보정이 두 번 도는 일을 막기 위해서입니다. 답이 오지 않으면 `XM_IMU_CALIB_PENDING` 에 머물고, 그동안 다음 요청은 `XM_IMU_CALIB_REQ_BUSY` 입니다. IMU Hub 가 상태를 다시 알려 주거나 연결이 끊기면 풀립니다.

---

## 알아 둘 것

- **제어 모드에서는 시작할 수 없습니다.** 제어 모드(`XM_CTRL_CONTROL`)에서 부르면 시작하지 않습니다. `XM_CTRL_MONITOR` 로 바꾼 뒤 시작하세요.
- **보정 중인 채널로 제어하지 마세요.** 보정 중인 채널은 그동안 `XM.status.imu_hub.connected_mask` 비트가 꺼집니다. 끝난 뒤에는 새 데이터가 들어오면 다시 켜집니다. 보정 중에 연결이 끊겼다 돌아오면 IMU Hub 상태를 확인할 때까지 꺼져 있습니다. 외부 드라이버를 `XM_CanSend` 로 직접 움직인다면 이 비트를 꼭 확인하세요.
- **중단은 `XM_ImuCalibFinish()` 하기 전까지만 됩니다.** 지자기 · 가속도 정밀만 해당합니다. 자이로 영점 · 가속도 간이는 금방 끝나므로 중단되지 않습니다.
- **EBIMU 는 중단할 때 전원이 잠깐 꺼집니다.** EBIMU 의 지자기 · 가속도 정밀을 중단하거나 움직이는 시간 상한을 넘기면 모든 채널의 전원이 약 2 초 꺼졌다 켜집니다. 그동안 다른 채널도 끊기고, 채널마다 새 데이터가 들어와야 다시 연결된 것으로 보입니다. 다른 센서가 값을 저장하는 중이면 먼저 기다리므로 중단 결과가 나올 때까지 약 17 초가 걸릴 수 있습니다.
- **움직이며 기다리는 시간에는 상한이 있습니다.** 지자기는 120 초, 가속도 정밀은 600 초까지입니다. 넘으면 `XM_IMU_CALIB_RESULT_SESSION_TIMEOUT` 으로 끝납니다.
- **MTi-630 은 자이로 영점 · 지자기만 됩니다.**
- **PC 프로그램과 함께 쓰지 마세요.** PC 프로그램에서 보정하는 동안 이 함수를 함께 쓰지 마세요. 나중 요청이 앞 요청을 지웁니다.

---

## 호출 규칙

모든 함수가 기다리지 않습니다.

| 종류 | 함수 | 부르는 곳 |
|------|------|-----------|
| 전체 (6개) | `XM_ImuCalibStart` · `XM_ImuCalibFinish` · `XM_ImuCalibAbort` · `XM_ImuCalibGetStatus` · `XM_ImuCalibQuerySupported` · `XM_ImuCalibGetSupported` | `Control_Setup()` · `Control_Loop()` · 백그라운드 태스크(`xm_api_freertos.h`). **인터럽트에서는 부르지 마세요** |

---

## 함수 목록

**시작 · 마무리 · 중단**

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_ImuCalibStart`](#xm_imucalibstart) | 보정 시작 (제어 모드에서는 시작할 수 없음) |
| [`XM_ImuCalibFinish`](#xm_imucalibfinish) | 센서를 움직이는 단계를 끝내고 값을 저장 |
| [`XM_ImuCalibAbort`](#xm_imucalibabort) | 보정 중단 |

**상태**

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_ImuCalibGetStatus`](#xm_imucalibgetstatus) | 채널의 보정 상태 읽기 |

**지원 항목**

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_ImuCalibQuerySupported`](#xm_imucalibquerysupported) | 채널별 지원 항목을 IMU Hub 에 묻기 |
| [`XM_ImuCalibGetSupported`](#xm_imucalibgetsupported) | 지원 항목 읽기 |

**타입 · 매크로**: [`XmImuCalibOp_t`](#xmimucalibop_t) · [`XmImuCalibRequest_t`](#xmimucalibrequest_t) · [`XmImuCalibState_t`](#xmimucalibstate_t) · [`XmImuCalibResult_t`](#xmimucalibresult_t) · [`XmImuCalibStatus_t`](#xmimucalibstatus_t) · [`XM_IMU_CALIB_OP_BIT`](#xm_imu_calib_op_bit)

---

## 함수 상세

### 시작 · 마무리 · 중단

#### `XM_ImuCalibStart`

```c
XmImuCalibRequest_t XM_ImuCalibStart(uint8_t ch_mask, XmImuCalibOp_t op);
```

보정을 시작합니다. **한 번만** 부르세요 — 버튼을 누른 순간처럼 한 번만 일어나는 곳에서 부릅니다. 매 주기 부르면 끝날 때마다 보정이 다시 시작됩니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `ch_mask` | `uint8_t` | bit0 ~ bit5 = 채널 0 ~ 5. 여러 채널을 한 번에 시작할 수 있습니다. 이미 진행 중인 채널은 그대로 둡니다 |
| `op` | [`XmImuCalibOp_t`](#xmimucalibop_t) | 보정 항목 |

**반환값**: [`XmImuCalibRequest_t`](#xmimucalibrequest_t) — 요청을 받았는지입니다. 받았다고 보정이 끝난 것은 아닙니다. 결과는 [`XM_ImuCalibGetStatus`](#xm_imucalibgetstatus) 로 봅니다.

**⚠️ 호출 컨텍스트**: `Control_Setup()` · `Control_Loop()` · 백그라운드 태스크에서 부릅니다. 인터럽트에서는 부르지 마세요. 제어 모드(`XM_CTRL_CONTROL`)에서는 시작할 수 없습니다(`XM_IMU_CALIB_REQ_CONTROL_ACTIVE`).

**예제**

```c
#define CALIB_CH0  (0x01U)    /* ch_mask: bit0 = 채널 0 */

/* 한 번만 일어나는 곳(예: 버튼을 누른 순간)에서 */
XmImuCalibRequest_t req = XM_ImuCalibStart(CALIB_CH0, XM_IMU_CALIB_OP_MAG);
if (req != XM_IMU_CALIB_REQ_OK) {
    /* 이유는 XmImuCalibRequest_t 표를 보세요 */
}
```

**참고**: [쓰는 순서](#쓰는-순서), [알아 둘 것](#알아-둘-것)

---

#### `XM_ImuCalibFinish`

```c
XmImuCalibRequest_t XM_ImuCalibFinish(uint8_t ch_mask);
```

센서를 움직이는 단계를 끝냅니다. 값을 저장합니다. 상태가 `XM_IMU_CALIB_WAIT_USER` 가 된 뒤, 센서를 움직인 다음에 부릅니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `ch_mask` | `uint8_t` | bit0 ~ bit5 = 채널 0 ~ 5 |

**반환값**: [`XmImuCalibRequest_t`](#xmimucalibrequest_t)

**⚠️ 호출 컨텍스트**: `Control_Setup()` · `Control_Loop()` · 백그라운드 태스크에서 부릅니다. 인터럽트에서는 부르지 마세요. 제어 모드에서도 부를 수 있습니다.

**참고**: [`XM_ImuCalibGetStatus`](#xm_imucalibgetstatus)

---

#### `XM_ImuCalibAbort`

```c
XmImuCalibRequest_t XM_ImuCalibAbort(uint8_t ch_mask);
```

보정을 중단합니다. 중단은 지자기 · 가속도 정밀을 `XM_ImuCalibFinish()` 하기 전까지만 됩니다. 자세한 내용은 [알아 둘 것](#알아-둘-것)을 보세요. 중단하면 값은 저장되지 않습니다(`XM_IMU_CALIB_RESULT_ABORTED`).

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `ch_mask` | `uint8_t` | bit0 ~ bit5 = 채널 0 ~ 5 |

**반환값**: [`XmImuCalibRequest_t`](#xmimucalibrequest_t)

**⚠️ 호출 컨텍스트**: `Control_Setup()` · `Control_Loop()` · 백그라운드 태스크에서 부릅니다. 인터럽트에서는 부르지 마세요. 제어 모드에서도 부를 수 있습니다.

**참고**: EBIMU 는 중단할 때 모든 채널의 전원이 약 2 초 꺼졌다 켜집니다 — [알아 둘 것](#알아-둘-것)

---

### 상태

#### `XM_ImuCalibGetStatus`

```c
bool XM_ImuCalibGetStatus(uint8_t ch, XmImuCalibStatus_t* out);
```

채널의 보정 상태를 읽습니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `ch` | `uint8_t` | 채널 0 ~ 5 |
| `out` | [`XmImuCalibStatus_t*`](#xmimucalibstatus_t) | 상태를 받을 구조체 |

**반환값**: `bool` — `true`: `out` 을 채웠습니다. `false`: 다음 주기에 다시 읽으세요.

**⚠️ 호출 컨텍스트**: `Control_Setup()` · `Control_Loop()` · 백그라운드 태스크에서 부릅니다. 인터럽트에서는 부르지 마세요.

**예제**

```c
static bool s_user_moved_sensor;   /* 센서를 다 돌렸을 때 true 로 바꿉니다 */

void Control_Loop(void) {
    XmImuCalibStatus_t st;
    if (XM_ImuCalibGetStatus(0U, &st)) {
        if ((st.state == XM_IMU_CALIB_WAIT_USER) && s_user_moved_sensor) {
            XM_ImuCalibFinish(CALIB_CH0);            /* 한 번만 */
            s_user_moved_sensor = false;
        } else if (st.state == XM_IMU_CALIB_DONE) {
            /* 끝 */
        } else if (st.state == XM_IMU_CALIB_FAILED) {
            /* st.result 를 보세요 */
        }
    }
}
```

**참고**: [`XmImuCalibStatus_t`](#xmimucalibstatus_t), [`XmImuCalibState_t`](#xmimucalibstate_t)

---

### 지원 항목

#### `XM_ImuCalibQuerySupported`

```c
XmImuCalibRequest_t XM_ImuCalibQuerySupported(void);
```

채널별로 어떤 항목을 지원하는지 IMU Hub 에 묻습니다. 결과는 [`XM_ImuCalibGetSupported`](#xm_imucalibgetsupported) 로 봅니다.

**파라미터**: 없음

**반환값**: [`XmImuCalibRequest_t`](#xmimucalibrequest_t)

**⚠️ 호출 컨텍스트**: `Control_Setup()` · `Control_Loop()` · 백그라운드 태스크에서 부릅니다. 인터럽트에서는 부르지 마세요.

---

#### `XM_ImuCalibGetSupported`

```c
XmImuCalibResult_t XM_ImuCalibGetSupported(uint8_t ch, uint8_t* out_op_mask);
```

채널이 지원하는 항목을 읽습니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `ch` | `uint8_t` | 채널 0 ~ 5 |
| `out_op_mask` | `uint8_t*` | (출력) 결과가 `XM_IMU_CALIB_RESULT_OK` 일 때 지원하는 항목의 비트 마스크([`XM_IMU_CALIB_OP_BIT`](#xm_imu_calib_op_bit)). `0` 이면 이 채널에 센서가 없습니다 |

**반환값**: [`XmImuCalibResult_t`](#xmimucalibresult_t)

| 값 | 뜻 |
|------|------|
| `XM_IMU_CALIB_RESULT_NONE` | 아직 답이 없습니다. 채널 값이 틀렸거나 `out_op_mask` 가 `NULL` 일 때도 이 값입니다 |
| `XM_IMU_CALIB_RESULT_OK` | `out_op_mask` 에 지원 항목이 있습니다 |
| `XM_IMU_CALIB_RESULT_NO_RESPONSE` · `XM_IMU_CALIB_RESULT_HUB_NOT_SUPPORTED` | 보정 기능이 없는 펌웨어일 수 있습니다 |
| `XM_IMU_CALIB_RESULT_LINK_LOST` | IMU Hub 와 연결이 끊겼습니다 |

**⚠️ 호출 컨텍스트**: `Control_Setup()` · `Control_Loop()` · 백그라운드 태스크에서 부릅니다. 인터럽트에서는 부르지 마세요.

**예제**

```c
XM_ImuCalibQuerySupported();                 /* 한 번 묻고 */

/* 이후 Control_Loop() 에서 */
uint8_t mask = 0U;
XmImuCalibResult_t r = XM_ImuCalibGetSupported(0U, &mask);
if ((r == XM_IMU_CALIB_RESULT_OK) && (mask & XM_IMU_CALIB_OP_BIT(XM_IMU_CALIB_OP_MAG))) {
    /* 채널 0 은 지자기 보정을 지원합니다 */
}
```

**참고**: [`XM_ImuCalibQuerySupported`](#xm_imucalibquerysupported)

---

## 타입 / 매크로

### `XM_IMU_CALIB_OP_BIT`

```c
#define XM_IMU_CALIB_OP_BIT(op)  (1U << ((unsigned)(op) - 1U))
```

[`XM_ImuCalibGetSupported`](#xm_imucalibgetsupported) 의 `out_op_mask` 에서 항목 하나의 비트를 고릅니다. 예: `mask & XM_IMU_CALIB_OP_BIT(XM_IMU_CALIB_OP_MAG)`.

| 항목 | 비트 |
|------|------|
| `XM_IMU_CALIB_OP_GYRO` | bit0 |
| `XM_IMU_CALIB_OP_MAG` | bit1 |
| `XM_IMU_CALIB_OP_ACCEL_SIMPLE` | bit2 |
| `XM_IMU_CALIB_OP_ACCEL_FULL` | bit3 |

---

### `XmImuCalibOp_t`

보정 항목입니다. [`XM_ImuCalibStart()`](#xm_imucalibstart) 에 넘깁니다.

| 값 | 이름 | 뜻 |
|----|------|------|
| 1 | `XM_IMU_CALIB_OP_GYRO` | 자이로 영점 — 센서를 가만히 둡니다 |
| 2 | `XM_IMU_CALIB_OP_MAG` | 지자기 — 장착한 그대로 돌린 뒤 `XM_ImuCalibFinish()` |
| 3 | `XM_IMU_CALIB_OP_ACCEL_SIMPLE` | 가속도 간이 — 수평으로 가만히 둡니다 |
| 4 | `XM_IMU_CALIB_OP_ACCEL_FULL` | 가속도 정밀 — 여러 자세를 취한 뒤 `XM_ImuCalibFinish()` |

MTi-630 은 자이로 영점 · 지자기만 됩니다.

---

### `XmImuCalibRequest_t`

요청을 받았는지입니다. 받았다고 보정이 끝난 것은 아닙니다 — 결과는 [`XM_ImuCalibGetStatus()`](#xm_imucalibgetstatus) 로 봅니다.

| 값 | 이름 | 뜻 |
|----|------|------|
| 0 | `XM_IMU_CALIB_REQ_OK` | 받았습니다 |
| 1 | `XM_IMU_CALIB_REQ_BUSY` | 앞 요청을 처리하는 중입니다. 잠시 뒤 다시 부르세요 |
| 2 | `XM_IMU_CALIB_REQ_NOT_CONNECTED` | IMU Hub 가 연결되어 있지 않습니다 |
| 3 | `XM_IMU_CALIB_REQ_CONTROL_ACTIVE` | 제어 모드입니다. `XM_CTRL_MONITOR` 로 바꾼 뒤 시작하세요 |
| 4 | `XM_IMU_CALIB_REQ_INVALID` | 채널이나 항목 값이 틀렸습니다 |

---

### `XmImuCalibState_t`

채널 상태입니다.

| 값 | 이름 | 뜻 |
|----|------|------|
| 0 | `XM_IMU_CALIB_IDLE` | 보정한 적 없음 |
| 1 | `XM_IMU_CALIB_RUNNING` | 진행 중 |
| 2 | `XM_IMU_CALIB_WAIT_USER` | 센서를 움직이는 단계 — 끝나면 `XM_ImuCalibFinish()` |
| 3 | `XM_IMU_CALIB_DONE` | 끝. 센서 데이터가 다시 들어오는 것까지 확인했습니다 |
| 4 | `XM_IMU_CALIB_FAILED` | 실패 · 중단 — `result` 를 보세요 |
| 5 | `XM_IMU_CALIB_PENDING` | 요청을 보내는 중 — 아직 IMU Hub 가 확인하기 전입니다 |

---

### `XmImuCalibResult_t`

결과입니다. 6 은 쓰지 않습니다.

| 값 | 이름 | 뜻 |
|----|------|------|
| 0 | `XM_IMU_CALIB_RESULT_NONE` | 아직 없음 |
| 1 | `XM_IMU_CALIB_RESULT_OK` | 성공 |
| 2 | `XM_IMU_CALIB_RESULT_NO_SENSOR` | 이 채널에 센서가 없습니다 |
| 3 | `XM_IMU_CALIB_RESULT_NOT_SUPPORTED` | 이 센서는 이 항목을 지원하지 않습니다 |
| 4 | `XM_IMU_CALIB_RESULT_SENSOR_TIMEOUT` | 센서가 답하지 않았습니다. `info` = 실패한 단계 |
| 5 | `XM_IMU_CALIB_RESULT_STREAM_NOT_RESUMED` | 보정 뒤 센서 데이터가 다시 들어오지 않았습니다 |
| 7 | `XM_IMU_CALIB_RESULT_HUB_BUSY` | IMU Hub 가 지금 요청을 처리할 수 없습니다. 잠시 기다리고 연결 상태를 확인한 뒤 다시 시도하세요 |
| 8 | `XM_IMU_CALIB_RESULT_ABORTED` | 중단했습니다. 값은 저장되지 않았습니다 |
| 9 | `XM_IMU_CALIB_RESULT_SENSOR_REJECTED` | 센서가 거절했습니다. `info` = 센서가 보낸 상태 값 |
| 10 | `XM_IMU_CALIB_RESULT_SESSION_TIMEOUT` | 움직이는 단계의 시간 상한을 넘겼습니다 |
| 11 | `XM_IMU_CALIB_RESULT_NO_RESPONSE` | IMU Hub 가 답하지 않습니다. 펌웨어가 오래되었거나 통신 문제입니다 (아래 설명) |
| 12 | `XM_IMU_CALIB_RESULT_HUB_NOT_SUPPORTED` | IMU Hub 가 요청을 거절했습니다. 펌웨어가 오래되었을 수 있습니다. 업데이트해 보세요 |
| 13 | `XM_IMU_CALIB_RESULT_LINK_LOST` | 보정 중 IMU Hub 와 연결이 끊겼거나 IMU Hub 가 다시 시작했습니다 |

**`XM_IMU_CALIB_RESULT_NO_RESPONSE` 일 때는 상태에 따라 다르게 하세요.**

| 상태 | 할 일 |
|------|------|
| `XM_IMU_CALIB_FAILED` | 요청이 닿지 않았습니다. 다시 시작하세요 |
| `XM_IMU_CALIB_PENDING` | 보정이 이미 돌고 있을 수 있습니다. 이 채널을 쓰지 말고 답을 기다리세요 |
| 그 밖의 진행 상태 | 답이 다시 오는지 보고, `XM_ImuCalibFinish()` · `XM_ImuCalibAbort()` 를 다시 부르세요 |

---

### `XmImuCalibStatus_t`

채널 하나의 보정 상태입니다. [`XM_ImuCalibGetStatus()`](#xm_imucalibgetstatus) 로 읽습니다.

```c
typedef struct {
    XmImuCalibState_t  state;
    XmImuCalibOp_t     op;
    XmImuCalibResult_t result;
    uint8_t            info;
} XmImuCalibStatus_t;
```

| 필드 | 타입 | 설명 |
|------|------|------|
| `state` | [`XmImuCalibState_t`](#xmimucalibstate_t) | 채널 상태 |
| `op` | [`XmImuCalibOp_t`](#xmimucalibop_t) | 이 상태의 항목. 보정한 적 없으면 0 |
| `result` | [`XmImuCalibResult_t`](#xmimucalibresult_t) | 결과 |
| `info` | `uint8_t` | 진행 중 = 단계 번호. 실패 = `result` 설명 참고. 그 밖 = 0 |

---

## Rev1.1 / Rev2.0 차이 요약

| 항목 | Rev1.1 | Rev2.0 |
|------|--------|--------|
| IMU Hub 센서 보정 API (`xm_api_imu_calib.h`) | ❌ 없음 | 🟢 전용 |

---

## 관련 문서

- [13. IMU Hub 센서 보정 (개념)](../13-imu-calibration.md) — 쓰는 순서, 자세 잡는 법, 흔한 실수
- [`xm_api_data.h` — XM10 통합 데이터 및 제어 인터페이스](xm-api-data.md) — 제어 모드(`XM_CTRL_*`)와 `XM.status`
- [`xm_api_canbus.h` — CAN-FD · 클래식 CAN 으로 연결한 내 장치와 주고받기](xm-api-canbus.md) — `XM_CanSend`
