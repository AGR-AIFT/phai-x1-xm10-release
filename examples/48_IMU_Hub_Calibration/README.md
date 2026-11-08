# Ex.48 — IMU Hub Calibration (IMU Hub 센서를 제자리에서 보정하기)

> 🎯 **학습 목표**:
> - CAN-FD 로 연결된 IMU Hub 의 센서를 케이스에 넣은 채로 보정합니다 (`XM_ImuCalibStart` · `XM_ImuCalibFinish` · `XM_ImuCalibAbort`).
> - 기다리지 않는 함수로 요청하고, 상태만 살펴봅니다 (`XM_ImuCalibGetStatus`).
> - 채널마다 지원하는 보정 항목을 확인합니다 (`XM_ImuCalibQuerySupported` · `XM_ImuCalibGetSupported`).
>
> ⏱️ 권장 시간: 40분 | 🔧 난이도: ⭐⭐⭐ | 🟢 **Rev 2.0 전용**
> 🧰 사전 예제: [Ex.41 IMU Hub Dashboard](../41_IMU_Hub_Dashboard/) | 📚 관련 docs: [IMU Hub 센서 보정](../../docs/api-reference/13-imu-calibration.md) · [`xm_api_imu_calib.h` 레퍼런스](../../docs/api-reference/ref/xm-api-imu-calib.md)

> 🟢 **Rev 2.0 전용** — Rev 1.1 SDK 에는 이 예제가 들어 있지 않습니다.
>
> ℹ️ IMU Hub 는 내부에서 개발 중인 모듈입니다. 사용하려면 https://huphailab.com/contact 로 문의해 주세요.

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

| 버튼 | 하는 일 |
|:---:|------|
| 버튼 1 | 연결된 모든 센서의 자이로 영점 — 센서를 가만히 두세요 |
| 버튼 2 | 지자기 보정 시작. 센서를 장착한 그대로 돌린 뒤 **한 번 더 누르면** 끝냅니다(저장) |
| 버튼 3 | 지자기 보정 중단 — 저장하지 않습니다 |

| LED | 뜻 |
|:---:|------|
| LED 1 느린 깜빡임 · 켜짐 | IMU Hub 없음 · 연결됨 |
| LED 2 빠른 깜빡임 | 보정 진행 중 |
| LED 2 느린 깜빡임 | 센서를 움직일 단계(지자기) |
| LED 2 켜짐 | 모두 끝남 |
| LED 3 켜짐 | 실패한 채널이 있음 — PC 메시지에서 이유를 보세요 |

PC — `xm10` 도구 메시지 창에 채널마다 상태가 바뀔 때 `[IMU] …` 메시지가 나옵니다. (보드 확인 전) <!-- board-check -->

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

- **시작은 한 번만** — `XM_ImuCalibStart()` 는 버튼을 누른 순간처럼 한 번만 부릅니다. 매 주기 부르면 끝날 때마다 다시 시작됩니다. 앞 요청을 처리하는 중이면 `XM_IMU_CALIB_REQ_BUSY` 이고, 이 예제는 요청을 기억해 두었다가 다음 주기에 다시 보냅니다.
- **제어 모드에서는 시작하지 않습니다** — `XM_CTRL_MONITOR`(기본값)에서 시작합니다. 끝내기 · 중단은 언제나 됩니다.
- **보정 중인 채널의 값은 쓰지 마세요** — 그 채널의 `XM.status.imu_hub.connected_mask` 비트가 꺼집니다. 끝난 뒤 새 데이터가 들어오면 다시 켜집니다.
- **항목과 센서** — 자이로 영점 · 지자기 · 가속도 간이 · 가속도 정밀. MTi-630 은 자이로 영점 · 지자기만 됩니다. 지원 항목은 `XM_ImuCalibGetSupported()` 로 확인합니다.
- **중단의 영향** — EBIMU 의 지자기 · 가속도 정밀을 중단하면 모든 채널의 전원이 약 2 초 꺼졌다 켜집니다. 다른 센서가 값을 저장하는 중이면 중단 결과까지 약 17 초가 걸릴 수 있습니다.
- **움직이는 시간의 상한** — 지자기 120 초, 가속도 정밀 600 초. 넘으면 `XM_IMU_CALIB_RESULT_SESSION_TIMEOUT` 으로 끝납니다.
- **PC 프로그램과 함께 쓰지 마세요** — 나중 요청이 앞 요청을 지웁니다.
- **IMU Hub 펌웨어** — 보정 기능이 없는 옛 펌웨어면 '지원하지 않음' 또는 '응답 없음' 으로 끝납니다.

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
void Control_Loop(void)
{
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        XM_ImuCalibStart(mask, XM_IMU_CALIB_OP_MAG);           // ① 시작 — 한 번만
    }

    XmImuCalibStatus_t st;
    if (XM_ImuCalibGetStatus(ch, &st)) {                       // ② 상태만 봅니다(기다리지 않음)
        if (st.state == XM_IMU_CALIB_WAIT_USER) {
            /* 센서를 돌린 뒤 버튼으로 XM_ImuCalibFinish(mask) */   // ③ 끝내기(저장)
        } else if (st.state == XM_IMU_CALIB_FAILED) {
            /* st.result 로 이유를 봅니다 */
        }
    }
}
```

전체 코드: [`imu_hub_calibration.c`](imu_hub_calibration.c)

> 🧒 ① 의 결과 `XM_IMU_CALIB_REQ_OK` 는 '요청을 받았다' 는 뜻입니다. 보정이 끝났는지는 ② 로 봅니다.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

아래 ✅ 는 코드가 의도한 동작이며, 보드에서는 아직 확인하지 못했습니다. (보드 확인 전) <!-- board-check -->

1. **IMU Hub 를 CAN-FD #2 포트에 연결하고 빌드 + 플래시** → ✅ LED 1 켜짐, PC 메시지에 채널별 지원 항목
2. **센서를 가만히 두고 버튼 1** → ✅ LED 2 빠른 깜빡임 → 켜짐
3. **버튼 2 → 센서를 장착한 그대로 여러 방향으로 돌리기 → 버튼 2** → ✅ LED 2 느린 깜빡임(움직일 단계) → 켜짐
4. **버튼 2 → 버튼 3(중단)** → ✅ 결과가 `ABORTED` 로 나옵니다. EBIMU 면 모든 채널이 약 2 초 끊겼다 돌아옵니다
5. **변형 — 가속도 정밀**: `XM_IMU_CALIB_OP_ACCEL_FULL` 로 바꾸고 여러 자세를 취한 뒤 끝냅니다

---

## 5️⃣ 다음 단계

- 보정 뒤 자세 보기: [Ex.41 IMU Hub Dashboard](../41_IMU_Hub_Dashboard/)
- 상태 · 결과 값을 자세히: [IMU Hub 센서 보정](../../docs/api-reference/13-imu-calibration.md) · [`xm_api_imu_calib.h` 레퍼런스](../../docs/api-reference/ref/xm-api-imu-calib.md)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| 눌러도 시작하지 않음(`REQ_CONTROL_ACTIVE`) | 제어 모드입니다 | `XM_CTRL_MONITOR` 로 바꾼 뒤 시작합니다 |
| 끝날 때마다 다시 시작함 | `XM_ImuCalibStart()` 를 매 주기 부름 | 버튼 이벤트처럼 한 번만 부릅니다 |
| `NOT_SUPPORTED` | 그 센서가 그 항목을 지원하지 않음 | `XM_ImuCalibGetSupported()` 로 확인합니다 |
| `NO_RESPONSE` · `HUB_NOT_SUPPORTED` | IMU Hub 펌웨어가 오래됨 · 통신 문제 | IMU Hub 펌웨어를 업데이트합니다 |
| 중단했더니 다른 채널까지 끊김 | EBIMU 중단은 모든 채널 전원을 약 2 초 끕니다 | 정상입니다. 새 데이터가 들어오면 다시 연결됩니다 |
| 보정 중 그 채널 값으로 제어함 | `connected_mask` 를 보지 않음 | 보정 중인 채널은 쓰지 않습니다 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)

> ℹ️ **아직 보드에서 확인되지 않은 예제입니다** (v2.10.0 시점). (보드 확인 전) <!-- board-check -->
