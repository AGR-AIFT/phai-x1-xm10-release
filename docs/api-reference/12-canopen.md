# API Reference: CANopen 장치 다루기

> 📌 **이 페이지를 읽고 나면**: CANopen(CiA 301) 모터 드라이버를 XM10 에서 NMT 로 깨우고, 하트비트로 지켜보고, SDO 로 설정 · 명령할 수 있습니다. 1 kHz 명령은 PDO 와 SYNC 로 합니다.
> ⏱️ 예상 학습 시간: 40분
> 🧰 사전 지식: [CAN 통신](11-canbus.md) 의 클래식 모드 · 자유 송수신
> 🎯 핵심 함수: `XM_CanOpenSendNmt` / `XM_CanOpenWatchNode` / `XM_CanOpenSdoRead` / `XM_CanOpenSdoWrite` / `XM_CanOpenSdoGetResult`
>
> 🟢 **Rev 2.0 전용** *(v2.10.0 신규)*: Rev 1.1 SDK 에는 `xm_api_canopen.h` 가 없습니다. **클래식 모드에서만** 동작합니다.

`xm_api_canopen.h` 에 정의된 **CANopen 도움 함수** 안내입니다. 함수 하나하나는 [`xm_api_canopen.h` 레퍼런스](ref/xm-api-canopen.md) 에 있습니다.

> ⚠️ **모터를 돌리는 코드입니다.** 실제 드라이버로 돌리기 전에 아래 '6. 실제 드라이버로 바꿀 때' 를 꼭 읽으세요.
>
> ℹ️ **보드 확인 안내**: 아래 흐름은 API 와 예제가 정한 것이며, 보드에서는 아직 확인하지 못했습니다. (보드 확인 전) <!-- board-check -->

---

## 1. 무엇을 돕나

| 기능 | 함수 | 메시지 |
|------|------|------|
| NMT 보내기 | `XM_CanOpenSendNmt(command, node_id)` | `0x000` `[명령, 노드]` |
| 노드 하트비트 감시 (8 노드까지) | `XM_CanOpenWatchNode` · `XM_CanOpenGetNodeState` · `XM_CanOpenGetBootupCount` | `0x700 + 노드` |
| SDO 빠른 읽기 · 쓰기 (4 B 이하) | `XM_CanOpenSdoRead` · `XM_CanOpenSdoWrite` · `XM_CanOpenSdoGetResult` | `0x600 + 노드` → `0x580 + 노드` |

포트를 클래식 모드로 정해야 합니다(`XM_CanBusMode()` 가 `XM_CAN_BUS_CLASSIC` — [CAN 통신 — 포트와 버스 모드](11-canbus.md)). FD 모드에서는 보내기 · 등록이 `XM_CAN_REJECTED` / `false` 입니다. SYNC 는 `XM_CanEnableSync`, PDO 같은 그 밖의 메시지는 `XM_CanSendFrame` · `XM_CanReadFrame` · `XM_CanSubscribe` 를 씁니다. 호출 자리와 결과 확인(`== XM_CAN_OK`) 규칙은 CAN 통신과 같습니다.

---

## 2. NMT 와 하트비트 감시

| NMT 명령 | 값 | 뜻 |
|------|:---:|------|
| `XM_CANOPEN_NMT_START` | `0x01` | OPERATIONAL 로 — PDO 가 오갑니다 |
| `XM_CANOPEN_NMT_STOP` | `0x02` | STOPPED 로 — SDO 에 답하지 않습니다 |
| `XM_CANOPEN_NMT_PRE_OPERATIONAL` | `0x80` | PRE_OPERATIONAL 로 — 설정(SDO)하는 상태 |
| `XM_CANOPEN_NMT_RESET_NODE` | `0x81` | 노드 재설정 |
| `XM_CANOPEN_NMT_RESET_COMM` | `0x82` | 통신 재설정 — 통신 설정이 기본값으로 돌아갑니다 |

`Control_Setup()` 에서 `XM_CanOpenWatchNode(node, timeout_ms)` 로 등록하면 `XM_CanOpenGetNodeState()` 가 마지막 하트비트로 상태를 알려 줍니다.

| 상태 | 뜻 |
|------|------|
| `XM_CANOPEN_NODE_UNKNOWN` | 아직 하트비트를 못 받음 |
| `XM_CANOPEN_NODE_BOOTUP` | 방금 켜짐(부팅 알림 `0x00`) |
| `XM_CANOPEN_NODE_STOPPED` · `OPERATIONAL` · `PRE_OPERATIONAL` | 드라이버가 보낸 상태 `0x04` · `0x05` · `0x7F` |
| `XM_CANOPEN_NODE_LOST` | `timeout_ms` 동안 못 받음 — XM10 의 판단이며 다음 하트비트까지 유지 |
| `XM_CANOPEN_NODE_NOT_WATCHED` | 감시 등록 안 함 · FD 모드 · 다른 곳에서 부름 |

- 드라이버가 하트비트를 주기적으로 내도록 설정돼 있어야 LOST 판단에 뜻이 있습니다(드라이버의 `0x1017`).
- `XM_CanOpenGetBootupCount()` 는 부팅 알림을 받은 수입니다. 통신 재설정 뒤 이 수가 늘면 드라이버가 있다는 뜻입니다.

---

## 3. SDO — 한 번에 하나, 기다리지 않음

```c
// 쓰기 시작 — 바로 돌아옵니다
if (XM_CanOpenSdoWrite(node, 0x6040, 0, 0x0006, 2) == XM_CAN_OK) { s_waiting = true; }

// 다음 주기들에서 결과를 봅니다
XmCanOpenSdoResult_t r;
switch (XM_CanOpenSdoGetResult(&r)) {
case XM_CANOPEN_SDO_DONE:    /* 끝 — 읽기면 r.value · r.size */ break;
case XM_CANOPEN_SDO_ABORTED: /* 드라이버가 거절 — r.abort_code */ break;
case XM_CANOPEN_SDO_TIMEOUT: /* 500 ms 안에 응답 없음 */ break;
default: break;              /* IDLE · BUSY — 계속 기다림 */
}
```

| 규칙 | 내용 |
|------|------|
| 한 번에 하나 | 모든 노드 공통으로 요청은 하나씩입니다. 진행 중이거나 결과를 아직 안 가져갔으면 `XM_CAN_BUSY` |
| 크기 | 4 바이트 이하 빠른 전송만. 드라이버가 분할 전송으로 답하면 `XM_CANOPEN_SDO_UNSUPPORTED` |
| 시간 초과 | 500 ms(`XM_CANOPEN_SDO_TIMEOUT_MS`). 시간 초과 · 분할 전송으로 끝나면 XM10 이 중단 프레임을 한 번 보내고, 1.5 초(`XM_CANOPEN_SDO_QUARANTINE_MS`) 동안 새 요청을 받지 않습니다 — 늦은 응답을 다음 요청의 결과로 잘못 읽지 않기 위해서입니다 |
| 결과는 한 번 | 끝난 결과(DONE · ABORTED · TIMEOUT · UNSUPPORTED)는 한 번 돌려준 뒤 IDLE 이 됩니다 |
| 응답 ID | `0x580 + 노드` 를 `XM_CanSubscribe()` 로 등록하면 SDO 함수가 거부합니다 |

---

## 4. 통신이 끊기면 드라이버가 스스로 멈추게 — 하트비트 소비

XM10 이 멈추거나 선이 빠지면 XM10 은 "멈춰" 를 보낼 수 없습니다. 그래서 **드라이버가 XM10 의 하트비트를 지켜보다가 끊기면 스스로 멈추게** 하는 것이 안전 정지의 핵심입니다.

1. XM10 이 하트비트를 보냅니다 — `XM_CanEnableHeartbeat(xm_node, 100)`. `Control_Loop()` 가 멈추면 새로 보내지 않습니다.
2. 드라이버가 그 하트비트를 지켜보게 설정합니다 — SDO 로 하트비트 소비(`0x1016:01` = (XM10 하트비트 번호 << 16) | 시간 ms).
3. 소비 감시는 **설정 뒤 첫 하트비트를 받아야** 시작합니다. 감시가 켜진 것을 확인한 뒤에 모터를 켜세요(확인 방법은 드라이버마다 다릅니다 — 예제는 PC 시뮬레이터 전용 객체로 확인합니다).
4. 통신 이상을 알게 되면 명령을 멈추고, 정지 쓰기를 한 번 시도하고(닿았다고 가정하지 않습니다), XM10 하트비트를 끕니다.

---

## 5. 1 kHz 명령 — PDO 와 SYNC

SDO 는 느립니다. 1 kHz 로 명령하려면 SDO 로 PDO 를 설정한 뒤 PDO 와 SYNC 로 주고받습니다([Ex.47](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/47_CANopen_Drive_PDO/)).

| 단계 | 하는 일 |
|------|------|
| 매핑 바꾸기 (CiA 301 순서) | 통신 파라미터로 PDO 끄기(COB-ID bit 31) → 매핑 개수 0 → 항목 → 개수 → 전송 형식(1 = SYNC 마다) → 다시 켜기 |
| 받기 준비 | TPDO ID 를 `XM_CanSubscribe()` 로 등록 → `XM_CanReadLatestById()` |
| 시작 | NMT 시작 → `XM_CanEnableSync(true)` |
| 주고받기 | 새 TPDO 를 받은 주기에 RPDO 하나(`XM_CanSendFrame`) |
| 끊김 판단 | SYNC · PDO 는 버스 정리 · 복구로 잠깐 쉴 수 있습니다. 1 kHz 를 가정하지 말고, TPDO 가 일정 시간(예제 50 ms) 끊길 때 통신 이상으로 봅니다 |

---

## 6. 실제 드라이버로 바꿀 때

예제 Ex.46 · 47 의 상대는 [PC 시뮬레이터](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/device-examples/canopen-pc-sim/)이고, 시뮬레이터 표시 객체를 확인하지 못하면 켜지 않습니다. 실제 드라이버 · 모터로 바꾸려면:

- 드라이버 설명서로 **프로파일 · 객체 · 정지 방법 · 하트비트 소비 시간 · 통신 상실 반응**을 먼저 정합니다.
- 속도는 낮게 시작하고, 버튼 정지가 되는지 먼저 확인합니다.
- 그 뒤에 예제의 시뮬레이터 확인을 바꿉니다.
- 이 포트를 클래식 모드로 쓰는 동안 센서 허브는 동작하지 않습니다. 같은 포트에 CAN-FD 장치를 연결하지 마세요.

---

## 7. 통계

`XM_CanOpenGetStats(&s)` — 각 값은 온전하지만 서로 같은 순간의 값은 아닙니다.

| 값 | 뜻 |
|------|------|
| `sdo_late_response_count` | 시간 초과 뒤 늦게 와서 버린 응답 + 기다리던 요청과 맞지 않는 응답 |
| `sdo_rx_busy_drop_count` | 응답을 넣는 중 겹쳐 놓친 수(대개 TIMEOUT 으로 끝남) |
| `sdo_clear_hold_count` · `sdo_last_clear_wait_ticks` | 시간 초과 뒤 송신 대기를 비우는 동안 새 송신을 막은 tick 수 · 마지막으로 비기까지 걸린 tick 수 |

---

## 8. 예제와 시뮬레이터

| 무엇 | 링크 |
|------|------|
| SDO 로 설정 · 100 Hz 명령 | [Ex.46 CANopen Drive](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/46_CANopen_Drive/) |
| PDO + SYNC 로 1 kHz | [Ex.47 CANopen Drive PDO](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/47_CANopen_Drive_PDO/) |
| CANopen 드라이브 PC 시뮬레이터 | [canopen-pc-sim](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/device-examples/canopen-pc-sim/) |
| 함수 하나하나 | [`xm_api_canopen.h` 레퍼런스](ref/xm-api-canopen.md) |
