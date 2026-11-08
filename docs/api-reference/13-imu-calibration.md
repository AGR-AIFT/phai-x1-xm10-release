# API Reference: IMU Hub 센서 보정

> 📌 **이 페이지를 읽고 나면**: CAN-FD 로 연결된 IMU Hub 의 센서를 케이스나 옷에 넣은 채로 보정할 수 있습니다.
> ⏱️ 예상 학습 시간: 20분
> 🧰 사전 지식: [Ex.41](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/41_IMU_Hub_Dashboard/) IMU Hub 데이터 보기
> 🎯 핵심 함수: `XM_ImuCalibStart` / `XM_ImuCalibGetStatus` / `XM_ImuCalibFinish` / `XM_ImuCalibAbort`
>
> 🟢 **Rev 2.0 전용** *(v2.10.0 신규)*: Rev 1.1 SDK 에는 `xm_api_imu_calib.h` 가 없습니다.
>
> ℹ️ IMU Hub 는 내부에서 개발 중인 모듈입니다. 사용하려면 https://huphailab.com/contact 로 문의해 주세요.

`xm_api_imu_calib.h` 에 정의된 **IMU Hub 센서 제자리 보정 API** 안내입니다. 함수 하나하나는 [`xm_api_imu_calib.h` 레퍼런스](ref/xm-api-imu-calib.md) 에 있습니다.

> ℹ️ **보드 확인 안내**: 아래 동작은 API 가 정한 것이며, 보드에서는 아직 확인하지 못했습니다. (보드 확인 전) <!-- board-check -->

---

## 1. 왜 제자리에서 보정하나

센서는 케이스나 옷에 넣은 채로 보정해야 맞습니다. 특히 지자기는 장착한 물체째 돌려야 합니다. 이 API 로 XM10 사용자 코드에서 보정을 시작하고 끝낼 수 있습니다.

---

## 2. 쓰는 순서

```c
// ① 시작 — 버튼을 누른 순간처럼 한 번만
if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
    (void)XM_ImuCalibStart(0x01, XM_IMU_CALIB_OP_MAG);       // 채널 0 의 지자기
}

// ② 상태 보기 — 기다리지 않으므로 Control_Loop 에서
XmImuCalibStatus_t st;
if (XM_ImuCalibGetStatus(0, &st)) {
    if (st.state == XM_IMU_CALIB_WAIT_USER) {
        /* ③ 센서를 장착한 그대로 돌린 뒤 XM_ImuCalibFinish(0x01) — 값을 저장합니다 */
    } else if (st.state == XM_IMU_CALIB_DONE) {
        /* ④ 끝 */
    } else if (st.state == XM_IMU_CALIB_FAILED) {
        /* st.result 로 이유를 봅니다 */
    }
}
```

- **시작은 한 번만** 부릅니다. 매 주기 부르면 끝날 때마다 보정이 다시 시작됩니다.
- 채널은 비트 마스크(bit0 ~ 5 = 채널 0 ~ 5)로 여러 개를 한 번에 줄 수 있습니다. 이미 진행 중인 채널은 그대로 둡니다.
- 시작 요청은 IMU Hub 에 **한 번만** 보냅니다 — 같은 보정이 두 번 도는 일을 막기 위해서입니다. 답이 오지 않으면 `XM_IMU_CALIB_PENDING` 에 머물고, 그동안 다음 요청은 `XM_IMU_CALIB_REQ_BUSY` 입니다. IMU Hub 가 상태를 다시 알려 주거나 연결이 끊기면 풀립니다.
- 모든 함수는 기다리지 않습니다. `Control_Setup` · `Control_Loop` · 백그라운드 태스크에서 부를 수 있고, 인터럽트에서는 부르지 마세요.

---

## 3. 보정 항목

| 항목 | 하는 일 | 끝내기 |
|------|------|------|
| `XM_IMU_CALIB_OP_GYRO` | 자이로 영점 — 센서를 가만히 둡니다 | 저절로 끝남 |
| `XM_IMU_CALIB_OP_MAG` | 지자기 — 장착한 그대로 여러 방향으로 돌립니다 | `XM_ImuCalibFinish` (최대 120 초) |
| `XM_IMU_CALIB_OP_ACCEL_SIMPLE` | 가속도 간이 — 수평으로 가만히 둡니다 | 저절로 끝남 |
| `XM_IMU_CALIB_OP_ACCEL_FULL` | 가속도 정밀 — 여러 자세를 취합니다 | `XM_ImuCalibFinish` (최대 600 초) |

- MTi-630 은 자이로 영점 · 지자기만 됩니다.
- 채널별 지원 항목은 `XM_ImuCalibQuerySupported()` 로 물은 뒤 `XM_ImuCalibGetSupported(ch, &mask)` 로 읽습니다. `mask & XM_IMU_CALIB_OP_BIT(op)` 로 확인하고, `mask` 가 0 이면 그 채널에 센서가 없습니다.
- 움직이는 시간이 상한을 넘으면 `XM_IMU_CALIB_RESULT_SESSION_TIMEOUT` 으로 끝납니다.

---

## 4. 요청 결과 · 상태 · 결과

| 요청 결과 (`XmImuCalibRequest_t`) | 뜻 |
|------|------|
| `XM_IMU_CALIB_REQ_OK` | 받았습니다 — 끝난 것은 아닙니다. 결과는 상태로 봅니다 |
| `XM_IMU_CALIB_REQ_BUSY` | 앞 요청을 처리하는 중 — 잠시 뒤 다시 |
| `XM_IMU_CALIB_REQ_NOT_CONNECTED` | IMU Hub 가 연결돼 있지 않음 |
| `XM_IMU_CALIB_REQ_CONTROL_ACTIVE` | 제어 모드 — `XM_CTRL_MONITOR` 로 바꾼 뒤 시작 |
| `XM_IMU_CALIB_REQ_INVALID` | 채널 · 항목 값이 틀림 |

| 상태 (`XmImuCalibState_t`) | 뜻 |
|------|------|
| `XM_IMU_CALIB_IDLE` | 보정한 적 없음 |
| `XM_IMU_CALIB_PENDING` | 요청을 보내는 중 — 아직 IMU Hub 가 확인하기 전 |
| `XM_IMU_CALIB_RUNNING` | 진행 중 |
| `XM_IMU_CALIB_WAIT_USER` | 센서를 움직이는 단계 — 끝나면 `XM_ImuCalibFinish` |
| `XM_IMU_CALIB_DONE` | 끝 — 센서 데이터가 다시 들어오는 것까지 확인했습니다 |
| `XM_IMU_CALIB_FAILED` | 실패 · 중단 — `result` 를 봅니다 |

| 결과 (`XmImuCalibResult_t`) | 뜻 · 할 일 |
|------|------|
| `OK` | 성공 |
| `NO_SENSOR` · `NOT_SUPPORTED` | 이 채널에 센서가 없음 · 이 센서는 이 항목을 지원하지 않음 |
| `SENSOR_TIMEOUT` · `SENSOR_REJECTED` | 센서가 답하지 않음 · 거절함 — `info` 에 단계 · 센서 상태 값 |
| `STREAM_NOT_RESUMED` | 보정 뒤 센서 데이터가 다시 들어오지 않음 |
| `HUB_BUSY` | IMU Hub 가 지금 처리할 수 없음 — 잠시 기다리고 연결을 확인한 뒤 다시 |
| `ABORTED` | 중단 — 값은 저장되지 않았습니다 |
| `SESSION_TIMEOUT` | 움직이는 단계의 시간 상한을 넘김 |
| `NO_RESPONSE` | IMU Hub 가 답하지 않음 — FAILED 면 요청이 닿지 않았으니 다시 시작, PENDING 이면 이미 돌고 있을 수 있으니 이 채널을 쓰지 말고 기다림 |
| `HUB_NOT_SUPPORTED` | IMU Hub 가 거절 — 펌웨어가 오래되었을 수 있으니 업데이트 |
| `LINK_LOST` | 보정 중 연결이 끊겼거나 IMU Hub 가 다시 시작함 |

(표의 이름은 `XM_IMU_CALIB_RESULT_` 뒤 부분입니다.)

---

## 5. 알아 둘 것

- **제어 모드에서는 시작할 수 없습니다.** `XM_CTRL_MONITOR` 로 바꾼 뒤 시작하세요. 끝내기 · 중단은 제어 모드에서도 됩니다.
- **보정 중인 채널의 값으로 제어하지 마세요.** 그동안 그 채널의 `XM.status.imu_hub.connected_mask` 비트가 꺼지고, 끝난 뒤 새 데이터가 들어오면 다시 켜집니다. 보정 중 연결이 끊겼다 돌아오면 IMU Hub 상태를 확인할 때까지 꺼져 있습니다. 외부 드라이버를 `XM_CanSend` 로 직접 움직인다면 이 비트를 꼭 확인하세요.
- **중단은 지자기 · 가속도 정밀을 끝내기 전까지만** 됩니다. 자이로 영점 · 가속도 간이는 금방 끝나서 중단되지 않습니다.
- **EBIMU 의 지자기 · 가속도 정밀을 중단하거나 움직이는 시간 상한을 넘기면** 모든 채널의 전원이 약 2 초 꺼졌다 켜집니다. 그동안 다른 채널도 끊기고, 채널마다 새 데이터가 들어와야 다시 연결된 것으로 보입니다. 다른 센서가 값을 저장하는 중이면 먼저 기다리므로 중단 결과까지 약 17 초가 걸릴 수 있습니다.
- **PC 프로그램에서 보정하는 동안 이 함수를 함께 쓰지 마세요.** 나중 요청이 앞 요청을 지웁니다.
- **IMU Hub 펌웨어** — 보정 기능이 없는 옛 펌웨어면 '지원하지 않음' 또는 '응답 없음' 으로 끝납니다.

---

## 6. 예제

| 무엇 | 링크 |
|------|------|
| 버튼으로 자이로 영점 · 지자기 보정 · 중단 | [Ex.48 IMU Hub Calibration](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/48_IMU_Hub_Calibration/) |
| 보정 뒤 자세 보기 | [Ex.41 IMU Hub Dashboard](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/41_IMU_Hub_Dashboard/) |
| 함수 하나하나 | [`xm_api_imu_calib.h` 레퍼런스](ref/xm-api-imu-calib.md) |
