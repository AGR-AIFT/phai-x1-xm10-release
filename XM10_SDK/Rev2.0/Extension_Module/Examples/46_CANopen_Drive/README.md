# Ex.46 — CANopen Drive (CANopen 모터 드라이버를 SDO 로 설정하고 명령하기)

> 🎯 **학습 목표**:
> - CAN-FD #2 포트를 클래식 CAN 1 Mbit/s 로 정합니다 (`XM_CanBusMode`).
> - CANopen 드라이버의 상태를 하트비트로 보고, NMT 로 상태를 바꿉니다 (`XM_CanOpenWatchNode` · `XM_CanOpenSendNmt`).
> - SDO 로 드라이버를 설정하고, 켜고, 100 Hz 로 속도를 명령합니다 (`XM_CanOpenSdoWrite` · `XM_CanOpenSdoRead`).
> - 통신이 끊기면 드라이버가 스스로 멈추게 하는 방법(하트비트 소비)을 익힙니다 (`XM_CanEnableHeartbeat`).
>
> ⏱️ 권장 시간: 90분 | 🔧 난이도: ⭐⭐⭐⭐ | 🟢 **Rev 2.0 전용**
> 🧰 사전 예제: [Ex.45 CAN-FD My Sensor](../45_CANFD_My_Sensor/) | 📚 관련 docs: [CANopen 장치 다루기](../../docs/api-reference/12-canopen.md) · [`xm_api_canopen.h` 레퍼런스](../../docs/api-reference/ref/xm-api-canopen.md) · [CAN 통신](../../docs/api-reference/11-canbus.md)

> 🟢 **Rev 2.0 전용** — Rev 1.1 은 CAN-FD #2 포트로 외부 장치와 주고받는 기능을 지원하지 않습니다.

---

## ⚠️ 먼저 읽기 — 상대는 PC 시뮬레이터입니다

- 이 예제의 상대는 [CANopen 드라이브 PC 시뮬레이터](../../device-examples/canopen-pc-sim/)입니다. 실제 모터 없이 PC 가 CANopen 속도 모드 드라이브를 흉내 냅니다.
- 설정 단계에서 시뮬레이터 표시 객체(`0x2F00` = `"XSIM"`)를 확인하지 못하면 **켜지 않고 멈춥니다**.
- 실제 드라이버 · 모터로 돌리려면 먼저 드라이버 설명서로 **정지 방법 · 하트비트 소비 시간 · 통신 상실 반응**을 정하고, 속도는 낮게, 버튼 정지를 확인한 뒤에 이 확인을 바꾸세요.
- 이 포트를 클래식 CAN 으로 정하므로 **CAN-FD 장치(센서 허브 등)를 같은 포트에 연결하지 마세요.**

## 🧰 준비물

| 준비물 | 설명 |
|------|------|
| XM10 (Rev 2.0) | 이 예제를 올릴 보드 |
| 클래식 CAN USB 어댑터 + PC | [PC 시뮬레이터](../../device-examples/canopen-pc-sim/) 실행 (Python · python-can) |
| 배선 | CAN_H · CAN_L · GND. 버스 양 끝에 120 Ω — XM10 보드에는 있으므로 어댑터 쪽에 하나 |

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

버튼 1 을 누르면 XM10 이 드라이버를 찾아 설정하고 켠 뒤, 100 Hz 로 목표 속도를 보내고 실제 속도를 읽습니다. 버튼 1 을 다시 누르면 멈춥니다. (보드 확인 전) <!-- board-check -->

| 순서 | 하는 일 |
|:---:|------|
| ① 시작 판정 | 첫 하트비트로 드라이버 상태를 보고 PRE_OPERATIONAL 로 데려옵니다. 3 초 동안 하트비트가 없으면 통신 재설정을 한 번 보내 부팅 알림을 기다립니다 |
| ② 설정 (SDO) | 시뮬레이터 표시 · 장치 종류(`0x1000`) · 하트비트 송신 주기 100 ms(`0x1017`) · XM10 하트비트 시작 · 하트비트 소비(`0x1016:01`) · 프로파일 속도 모드(`0x6060` = 3). 드라이버의 소비 감시가 켜진 것을 확인한 뒤에만 다음으로 |
| ③ 켜기 | NMT 시작 → 하트비트 `0x05` 확인 → 상태 단어(`0x6041`)를 보며 제어 단어(`0x6040`) `0x06` → `0x0F` (고장이면 먼저 `0x00` → `0x80`) |
| ④ 100 Hz | 목표 속도(`0x60FF`) 쓰기 · 실제 속도(`0x606C`) 읽기 |
| ⑤ 정지 | 버튼이면 `0x6040` = `0x06`. 통신 이상이면 명령을 멈추고, 정지 쓰기를 한 번 시도하고, XM10 하트비트를 멈춥니다 — 드라이버가 XM10 을 잃은 것을 보고 스스로 멈추게 하는 것이 안전 정지의 핵심입니다 |
| ⑥ 다시 시작 | 통신이 돌아와도 스스로 다시 움직이지 않습니다. 버튼 1 로 ① 부터 |

| LED | 뜻 |
|:---:|------|
| LED 1 느린 깜빡임 | 버튼 대기 |
| LED 1 빠른 깜빡임 | ① ~ ③ 진행 중 |
| LED 1 켜짐 | ④ 동작 중 |
| LED 1 아주 빠른 깜빡임 | 정지하는 중 |
| LED 1 꺼짐 | 버튼으로 정지 |
| LED 1 두근 | 멈춤 — 이유는 PC 메시지 |
| LED 2 켜짐 | XM10 하트비트를 내는 중 |

PC — `xm10` 도구 `0xF0` 탭에 단계 · 목표 속도 · 실제 속도 · 상태 단어, 단계가 바뀔 때마다 `[Ex46] …` 메시지.

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

- **클래식 모드** — `XM_CanBusMode()` 를 내 코드에 만들어 `XM_CAN_BUS_CLASSIC` 을 돌려주면 전원을 켤 때 포트가 클래식 CAN(기본 1 Mbit/s)이 됩니다. 속도를 바꾸려면 `XM_CanClassicBitTiming()` 도 만듭니다.
- **SDO 는 한 번에 하나** — `XM_CanOpenSdoRead` · `Write` 는 기다리지 않습니다. `XM_CanOpenSdoGetResult()` 로 결과를 본 뒤 다음 요청을 보냅니다. 4 바이트 이하만 됩니다. 응답이 500 ms 안에 없으면 시간 초과이고, 그 뒤 1.5 초 동안 새 요청을 받지 않습니다.
- **하트비트 두 방향** — 드라이버 → XM10: `XM_CanOpenWatchNode()` 로 감시합니다(300 ms 동안 없으면 LOST). XM10 → 드라이버: `XM_CanEnableHeartbeat()` 로 보내고, 드라이버는 하트비트 소비(`0x1016`)로 XM10 을 감시합니다.
- **소비 감시는 첫 하트비트부터** — 드라이버는 설정 뒤 XM10 하트비트를 처음 받아야 감시를 시작합니다. 그래서 이 예제는 감시가 켜진 것을 확인한 뒤에 켭니다.
- **1 kHz 제어는 PDO 로** — SDO 는 느립니다. 1 kHz 로 명령하려면 [Ex.47](../47_CANopen_Drive_PDO/) 을 보세요.

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
XmCanBusMode_t XM_CanBusMode(void)                       // ① 포트를 클래식 CAN 으로 — 전원을 켤 때 한 번
{
    return XM_CAN_BUS_CLASSIC;
}

void Control_Setup(void)
{
    XM_CanOpenWatchNode(EX46_NODE_ID, EX46_WATCH_TIMEOUT_MS);   // ② 드라이버 하트비트 감시
}

void Control_Loop(void)
{
    if (XM_CanOpenSdoWrite(EX46_NODE_ID, 0x60FF, 0, (uint32_t)target, 4) == XM_CAN_OK) {   // ③ 쓰기 시작
        /* … 다음 주기부터 결과를 봅니다 … */
    }
    XmCanOpenSdoResult_t res;
    if (XM_CanOpenSdoGetResult(&res) == XM_CANOPEN_SDO_DONE) {  // ④ 끝났으면 다음 요청
        /* … */
    }
}
```

전체 코드: [`canopen_drive.c`](canopen_drive.c) — 파일 위쪽의 `EX46_*` 상수(노드 ID · 목표 속도 · 하트비트 시간)를 상대와 맞추세요.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

아래 ✅ 는 코드가 의도한 동작이며, 보드에서는 아직 확인하지 못했습니다. (보드 확인 전) <!-- board-check -->

1. **PC 시뮬레이터 켜기** — `python cia402_drive_sim.py --interface pcan --channel PCAN_USBBUS1` ([안내](../../device-examples/canopen-pc-sim/))
2. **빌드 + 플래시** → ✅ `0 errors`, LED 1 느린 깜빡임
3. **버튼 1** → ✅ LED 1 빠른 깜빡임 → 켜짐, 시뮬레이터 화면에 `OPERATION_ENABLED`
4. **`xm10` 도구** → ✅ 실제 속도가 목표 속도(100)로 올라갑니다
5. **버튼 1** → ✅ 멈춤(LED 1 꺼짐), 시뮬레이터는 `READY_TO_SWITCH_ON`
6. **통신 이상 해 보기** — 동작 중에 CAN 선을 뽑으면 ✅ XM10 은 LED 1 두근, 시뮬레이터는 0.3 초 뒤 `고장 0x8130` 으로 멈춥니다. 다시 꽂아도 스스로 움직이지 않습니다
7. **변형 — 속도 바꾸기**: `EX46_TARGET_VELOCITY` 를 바꿔 봅니다. 실제 드라이버라면 낮게 시작하세요

---

## 5️⃣ 다음 단계

- 1 kHz 로 명령하기(PDO + SYNC): [Ex.47 CANopen Drive PDO](../47_CANopen_Drive_PDO/)
- 함수 · 결과 값을 자세히: [CANopen 장치 다루기](../../docs/api-reference/12-canopen.md) · [`xm_api_canopen.h` 레퍼런스](../../docs/api-reference/ref/xm-api-canopen.md)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| `장치 없음` 으로 멈춤 | 노드 ID 가 다름 · 속도가 다름 · 종단 저항 없음 | `EX46_NODE_ID` 와 시뮬레이터 `--node`, 버스 속도, 배선 |
| 설정 단계에서 멈춤(시뮬레이터 아님) | 상대가 PC 시뮬레이터가 아님 | 머리말의 '먼저 읽기' — 실제 드라이버는 설명서로 정한 뒤 확인을 바꿉니다 |
| `SDO` 시간 초과 · 중단으로 멈춤 | 드라이버가 답하지 않음 · 그 객체를 지원하지 않음 | 시뮬레이터 `--verbose` 로 어느 객체에서 멈췄는지 봅니다 |
| 켜다가 멈춤 | 고장 상태가 풀리지 않음 | 시뮬레이터 화면의 고장 코드, 드라이버 설명서 |
| 같은 포트의 센서 허브가 끊김 | 클래식 모드에서는 허브가 동작하지 않습니다 | 이 예제를 쓰는 동안에는 허브를 연결하지 않습니다(허브는 기본 FD 모드에서 씁니다) |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)

> ℹ️ **아직 보드에서 확인되지 않은 예제입니다** (v2.10.0 시점). (보드 확인 전) <!-- board-check -->
