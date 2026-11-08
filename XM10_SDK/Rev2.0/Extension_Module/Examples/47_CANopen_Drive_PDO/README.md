# Ex.47 — CANopen Drive PDO (CANopen 모터 드라이버를 PDO + SYNC 로 1 kHz 명령하기)

> 🎯 **학습 목표**:
> - SDO 로 PDO 매핑을 CiA 301 절차대로 바꿉니다 (끄기 → 개수 0 → 항목 → 개수 → 전송 형식 → 켜기).
> - 1 kHz SYNC 에 맞춰 RPDO 로 명령하고 TPDO 로 상태를 받습니다 (`XM_CanEnableSync` · `XM_CanSendFrame` · `XM_CanSubscribe` · `XM_CanReadLatestById`).
> - TPDO 가 끊기면 통신 이상으로 보고 멈추는 방법을 익힙니다.
>
> ⏱️ 권장 시간: 90분 | 🔧 난이도: ⭐⭐⭐⭐ | 🟢 **Rev 2.0 전용**
> 🧰 사전 예제: [Ex.46 CANopen Drive](../46_CANopen_Drive/) | 📚 관련 docs: [CANopen 장치 다루기](../../docs/api-reference/12-canopen.md) · [CAN 통신](../../docs/api-reference/11-canbus.md) · [`xm_api_canbus.h` 레퍼런스](../../docs/api-reference/ref/xm-api-canbus.md)

> 🟢 **Rev 2.0 전용** — Rev 1.1 은 CAN-FD #2 포트로 외부 장치와 주고받는 기능을 지원하지 않습니다.

---

## ⚠️ 먼저 읽기 — 상대는 PC 시뮬레이터입니다

- 이 예제의 상대도 [CANopen 드라이브 PC 시뮬레이터](../../device-examples/canopen-pc-sim/)입니다. 시뮬레이터 표시(`0x2F00`)를 확인하지 못하면 켜지 않고 멈춥니다.
- 실제 드라이버로 돌리려면 먼저 드라이버 설명서로 **프로파일 · 객체 · 정지 방법 · 하트비트 소비 시간 · 통신 상실 반응**을 정하고, 속도는 낮게, 버튼 정지를 확인한 뒤에 이 확인을 바꾸세요.
- 이 포트를 클래식 CAN 으로 정하므로 **CAN-FD 장치를 같은 포트에 연결하지 마세요.**
- 준비물과 배선은 [Ex.46](../46_CANopen_Drive/) 과 같습니다.

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

| 메시지 | ID | 내용 |
|------|:---:|------|
| RPDO1 (XM10 → 드라이버) | `0x200 + 노드` | 제어 단어 2 B + 목표 속도 4 B |
| TPDO1 (드라이버 → XM10) | `0x180 + 노드` | 상태 단어 2 B + 실제 속도 4 B |
| SYNC (XM10 → 모두) | `0x080` | 1 ms 마다. 드라이버는 SYNC 마다 받아 둔 RPDO 를 적용하고 TPDO 를 냅니다 |

| 순서 | 하는 일 |
|:---:|------|
| ① 시작 판정 | Ex.46 과 같습니다 — 첫 하트비트로 상태를 보고 PRE_OPERATIONAL 로 |
| ② 설정 (SDO) | 시뮬레이터 표시 → 하트비트 송신 주기 → XM10 하트비트 시작 → 하트비트 소비 → 동작 모드(`0x6060` = 9, 주기 동기 속도) → 통신 주기(`0x1006` = 1000 µs) → PDO 매핑(`0x1400` · `0x1600` · `0x1800` · `0x1A00`) |
| ③ 시작 | NMT 시작 → 하트비트 `0x05` → SYNC 시작 → TPDO 확인 |
| ④ 켜기 | RPDO 의 제어 단어로 `0x06` → `0x0F` (고장이면 `0x00` → `0x80` 한 번) |
| ⑤ 1 kHz | 새 TPDO 를 받은 주기에 RPDO 하나. 길이가 맞는 TPDO 가 50 ms 끊기면 통신 이상 |
| ⑥ 정지 | 버튼이면 RPDO `0x06` → 상태 확인 → SYNC 끔. 통신 이상이면 RPDO 를 멈추고 XM10 하트비트 · SYNC 를 끈 뒤 `0x06` 을 한 번 보내 봅니다 |
| ⑦ 다시 시작 | 스스로 다시 움직이지 않습니다. 버튼 1 로 ① 부터 |

LED 와 PC 메시지(`[Ex47] …`)는 [Ex.46](../46_CANopen_Drive/) 과 같은 뜻입니다. (보드 확인 전) <!-- board-check -->

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

- **PDO 매핑을 바꾸는 순서** — PDO 를 끄고(COB-ID bit 31) 개수를 0 으로 만든 뒤 항목을 쓰고, 개수를 다시 쓰고, 전송 형식을 정하고, 켭니다. 순서를 어기면 드라이버가 중단 코드로 거절합니다.
- **4 B 를 넘는 객체** — SDO 도움 함수는 4 바이트까지입니다. 그보다 큰 객체는 직접 짜야 합니다.
- **우편함** — TPDO ID 는 `XM_CanSubscribe()` 로 등록해 최신값으로 받습니다. SDO 응답 ID 는 등록하지 마세요(도움 함수가 거부합니다).
- **SYNC 는 잠깐 쉴 수 있습니다** — 버스 정리 · 복구로 SYNC 와 PDO 가 잠깐 멈출 수 있어 1 kHz 를 가정하지 않고, TPDO 가 50 ms 끊길 때만 통신 이상으로 봅니다.
- **`XM_CAN_OK` 는 '보내기 큐에 실림'** — 전달 확인은 TPDO 로 봅니다.

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
void Control_Setup(void)
{
    XM_CanOpenWatchNode(EX47_NODE_ID, EX47_WATCH_TIMEOUT_MS);    // ① 드라이버 하트비트 감시
    XM_CanSubscribe(ID_TPDO1);                                   // ② TPDO 는 최신값 우편함으로
}

void Control_Loop(void)
{
    XmCanFrame_t f;
    if (XM_CanReadLatestById(ID_TPDO1, &f) && f.seq != s_tpdo_seq && f.len >= PDO_LEN) {   // ③ 새 TPDO 만
        s_tpdo_seq = f.seq;
        uint8_t d[PDO_LEN] = { cw & 0xFF, cw >> 8, /* 목표 속도 4 B */ };
        XM_CanSendFrame(ID_RPDO1, d, PDO_LEN);                   // ④ 새 TPDO 하나에 RPDO 하나
    }
}
```

전체 코드: [`canopen_drive_pdo.c`](canopen_drive_pdo.c) — `EX47_*` 상수(노드 ID · 목표 속도 · 통신 주기)를 상대와 맞추세요.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

아래 ✅ 는 코드가 의도한 동작이며, 보드에서는 아직 확인하지 못했습니다. (보드 확인 전) <!-- board-check -->

1. **PC 시뮬레이터 켜기** ([안내](../../device-examples/canopen-pc-sim/)) → **빌드 + 플래시** → ✅ `0 errors`
2. **버튼 1** → ✅ 시뮬레이터 화면에 `RPDO1 켬` · `TPDO1 켬` → `OPERATIONAL` → `OPERATION_ENABLED`
3. **`xm10` 도구** → ✅ 실제 속도가 목표 속도를 빠르게 따라갑니다(주기 동기 속도)
4. **버튼 1** → ✅ 멈춤
5. **통신 이상 해 보기** — CAN 선을 뽑으면 ✅ XM10 은 TPDO 가 50 ms 끊긴 것을 보고 멈추고, 시뮬레이터는 0.3 초 뒤 `고장 0x8130`
6. **변형 — 매핑 실수 보기**: PDO 를 켠 채로 매핑 개수를 쓰도록 바꿔 보면 시뮬레이터가 `0x08000022` 로 거절합니다(`--verbose`)

---

## 5️⃣ 다음 단계

- 함수 · 결과 값을 자세히: [CANopen 장치 다루기](../../docs/api-reference/12-canopen.md) · [CAN 통신](../../docs/api-reference/11-canbus.md)
- 시뮬레이터가 흉내 내는 객체: [PC 시뮬레이터 안내](../../device-examples/canopen-pc-sim/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| 설정 단계의 PDO 매핑에서 멈춤 | 순서가 틀림 · 매핑할 수 없는 객체 · 64 비트를 넘음 | 시뮬레이터 `--verbose` 의 중단 코드 |
| `TPDO` 를 받지 못해 멈춤 | NMT 가 OPERATIONAL 이 아님 · TPDO 를 켜지 않음 · SYNC 가 나가지 않음 | 시뮬레이터 화면의 NMT · TPDO 상태 |
| 동작 중 자주 멈춤 | 버스가 바빠 TPDO 가 50 ms 넘게 끊김 | 같은 버스의 다른 장치 · 속도 |
| 같은 포트의 센서 허브가 끊김 | 클래식 모드에서는 허브가 동작하지 않습니다 | 함께 연결하지 않습니다 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)

> ℹ️ **아직 보드에서 확인되지 않은 예제입니다** (v2.10.0 시점). (보드 확인 전) <!-- board-check -->
