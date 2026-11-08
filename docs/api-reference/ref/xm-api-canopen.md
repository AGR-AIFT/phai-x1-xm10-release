# `xm_api_canopen.h` — CANopen 장치 도움 함수

> 🟢 **Rev 2.0 전용** — 본 헤더는 XM10 Rev 2.0 용이며, 포트를 **클래식 모드**로 정했을 때만 동작합니다. Rev 1.1 프로젝트(`XM10_SDK/Rev1.1/`)에서는 아래 함수를 쓸 수 없습니다. 보드 리비전 확인은 [보드 리비전 비교](../../hardware/README.md#보드-리비전-비교) 참고.
>
> - **대상 헤더**: `XM_FW/XM_API/xm_api_canopen.h` (`xm_api.h` 를 include 하면 함께 쓸 수 있습니다. 이 헤더는 `xm_api_canbus.h` 를 include 합니다)
> - **관련 개념 문서**: [12. CANopen 장치 다루기](../12-canopen.md)
> - **관련 예제**: [46_CANopen_Drive](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/46_CANopen_Drive/) · [47_CANopen_Drive_PDO](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/47_CANopen_Drive_PDO/)
> - **상대 장치 예제**: [`canopen-pc-sim/`](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/device-examples/canopen-pc-sim/) — CANopen 속도 모드 드라이브를 PC 로 흉내 내는 모의 장치 ([device-examples](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/device-examples/))
> - **보드 확인**: 아래 동작은 API 가 정한 것이며, 보드에서는 아직 확인하지 못했습니다. (보드 확인 전) <!-- board-check -->

이 헤더는 CANopen(CiA 301) 모터 드라이버 등을 XM10 이 직접 다루도록 돕는 함수를 정의합니다 — **NMT 보내기 · 노드 하트비트 감시 · SDO 빠른 읽기/쓰기(4 B 이하)**. 포트를 클래식 모드로 정했을 때만 동작합니다(`xm_api_canbus.h` 의 `XM_CanBusMode`). FD 모드에서는 보내기와 등록이 각각 `XM_CAN_REJECTED` 와 `false` 입니다.

---

## 언제 사용하나

CANopen 모터 드라이버 같은 장치를 XM10 의 클래식 CAN 포트에 연결해, 상태를 바꾸고(NMT) · 살아있는지 보고(하트비트 감시) · 설정값을 읽고 쓰고 싶을 때(SDO) 사용합니다. 쓰는 순서 · 배선 · 흔한 실수는 [12. CANopen 장치 다루기](../12-canopen.md) 을 먼저 읽어 보는 것을 권장합니다 — 이 페이지는 헤더에 선언된 **모든 함수 · 타입 · 상수의 시그니처와 파라미터 상세**만 다룹니다.

이 헤더에 없는 것은 `xm_api_canbus.h` 로 합니다.

| 하고 싶은 일 | 쓰는 것 |
|------|------|
| SYNC 보내기 | [`XM_CanEnableSync`](xm-api-canbus.md#xm_canenablesync) (클래식 모드에서는 길이 0) |
| PDO 같은 그 밖의 메시지 | [`XM_CanSendFrame`](xm-api-canbus.md#xm_cansendframe) · [`XM_CanReadFrame`](xm-api-canbus.md#xm_canreadframe) · [`XM_CanSubscribe`](xm-api-canbus.md#xm_cansubscribe) |

> ⚠️ **보내기 결과는 꼭 `== XM_CAN_OK` 로 확인하세요.** `XM_CAN_OK` 가 0 이라 `if (XM_CanOpenSendNmt(...))` 는 실패를 성공으로 읽습니다.

---

## 호출 규칙

호출 자리와 결과 확인 규칙은 [`xm_api_canbus.h`](xm-api-canbus.md#호출-규칙) 와 같습니다. 어느 함수도 기다리지 않습니다.

| 종류 | 함수 | 부르는 곳 |
|------|------|-----------|
| 등록 (1개) | `XM_CanOpenWatchNode` | `Control_Setup()` 에서만 |
| 그 밖의 함수 (7개) | `XM_CanOpenSendNmt` · `XM_CanOpenGetNodeState` · `XM_CanOpenGetBootupCount` · `XM_CanOpenSdoRead` · `XM_CanOpenSdoWrite` · `XM_CanOpenSdoGetResult` · `XM_CanOpenGetStats` | `Control_Setup()` · `Control_Loop()` 에서만 |

다른 태스크나 인터럽트에서 부르면 거부됩니다.

| 함수 | 밖에서 부르면 |
|------|------|
| `XM_CanOpenSendNmt` · `XM_CanOpenSdoRead` · `XM_CanOpenSdoWrite` | `XM_CAN_NOT_OWNER` |
| `XM_CanOpenGetNodeState` | `XM_CANOPEN_NODE_NOT_WATCHED` |
| `XM_CanOpenSdoGetResult` | `XM_CANOPEN_SDO_IDLE` (`out` 은 그대로) |

---

## 함수 목록

**NMT**

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_CanOpenSendNmt`](#xm_canopensendnmt) | NMT 명령 보내기 |

**하트비트 감시**

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_CanOpenWatchNode`](#xm_canopenwatchnode) | 노드의 하트비트 감시 등록 (`Control_Setup()` 에서만) |
| [`XM_CanOpenGetNodeState`](#xm_canopengetnodestate) | 감시 노드의 상태 |
| [`XM_CanOpenGetBootupCount`](#xm_canopengetbootupcount) | 감시 노드의 부팅 알림을 받은 수 |

**SDO**

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_CanOpenSdoRead`](#xm_canopensdoread) | SDO 빠른 읽기 시작 |
| [`XM_CanOpenSdoWrite`](#xm_canopensdowrite) | SDO 빠른 쓰기 시작 |
| [`XM_CanOpenSdoGetResult`](#xm_canopensdogetresult) | SDO 결과 보기 |

**통계**

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_CanOpenGetStats`](#xm_canopengetstats) | CANopen 도움 함수 통계 |

**타입 · 상수**: [`XmCanOpenNodeState_t`](#xmcanopennodestate_t) · [`XmCanOpenSdoStatus_t`](#xmcanopensdostatus_t) · [`XmCanOpenSdoResult_t`](#xmcanopensdoresult_t) · [`XmCanOpenStats_t`](#xmcanopenstats_t) · [상수 8개](#상수)

---

## SDO 쓰는 순서

SDO 는 **시작하고, 결과를 가져오는** 두 걸음입니다. 두 함수 모두 기다리지 않습니다.

1. `XM_CanOpenSdoRead()` 또는 `XM_CanOpenSdoWrite()` 로 시작합니다. `XM_CAN_OK` 면 요청을 받아들였다는 뜻입니다.
2. `Control_Loop()` 에서 `XM_CanOpenSdoGetResult()` 로 결과를 봅니다.
3. 결과(`DONE` · `ABORTED` · `TIMEOUT` · `UNSUPPORTED`)는 **한 번** 돌려준 뒤 `IDLE` 이 됩니다.

한 번에 한 요청입니다(모든 노드 공통). 진행 중이거나, 결과를 아직 가져가지 않았거나, 아래 쉬는 시간이면 새 요청은 `XM_CAN_BUSY` 입니다.

| 끝난 모습 | `XM_CanOpenSdoGetResult()` | 다음 요청 전 쉬는 시간 |
|------|------|:---:|
| 정상 | `XM_CANOPEN_SDO_DONE` | 없음 |
| 장치가 중단을 보냄 | `XM_CANOPEN_SDO_ABORTED` | 없음 |
| `XM_CANOPEN_SDO_TIMEOUT_MS` (500 ms) 안에 응답 없음 | `XM_CANOPEN_SDO_TIMEOUT` | `XM_CANOPEN_SDO_QUARANTINE_MS` (1500 ms) |
| 장치가 분할 전송으로 답함 | `XM_CANOPEN_SDO_UNSUPPORTED` | `XM_CANOPEN_SDO_QUARANTINE_MS` (1500 ms) |

### 시간 초과나 분할 전송 뒤의 쉬는 시간

`TIMEOUT` 이나 `UNSUPPORTED` 로 끝나면 XM10 은 중단 프레임을 한 번 보내고, 그 요청이 송신 대기를 떠난 것을 확인한 뒤 1.5 초 동안 새 SDO 요청을 받지 않습니다(`XM_CAN_BUSY`).

- 그사이 그 노드의 늦은 응답은 버립니다. 다음 요청의 결과로 잘못 읽지 않기 위해서입니다.
- 이 약속은 장치가 요청을 받고 1.4 초 안에 응답을 내보낼 때만 성립합니다. 장치 설명서로 확인하세요.
- 송신 대기가 빌 때까지는 이 포트의 새 송신이 잠깐 `XM_CAN_BUSY` 이고 SYNC 가 빠질 수 있습니다. 가벼운 부하면 수 ms 입니다. 길어도 연속 20 ms 뒤 80 ms 는 정상입니다.
- 다른 장치가 버스를 쉬지 않고 쓰면 다음 SDO 요청이 오래 `XM_CAN_BUSY` 일 수 있습니다. 버스 부하를 줄이면 풀립니다.

---

## 함수 상세

### NMT

#### `XM_CanOpenSendNmt`

```c
XmCanResult_t XM_CanOpenSendNmt(uint8_t command, uint8_t node_id);
```

NMT 명령을 보냅니다 — ID `0x000`, 데이터 `[command, node_id]` 2 바이트.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `command` | `uint8_t` | [`XM_CANOPEN_NMT_*`](#상수) 다섯 값 중 하나. 그 밖의 값은 `XM_CAN_REJECTED` |
| `node_id` | `uint8_t` | `0` = 모든 노드, `1` ~ `127` = 그 노드 |

**반환값**: [`XmCanResult_t`](xm-api-canbus.md#xmcanresult_t) — `XM_CAN_OK`: 보내기 큐에 실렸습니다. 상대가 받았다는 뜻은 아닙니다. 나머지 값의 뜻은 표를 보세요.

**⚠️ 호출 컨텍스트**: `Control_Setup()` · `Control_Loop()` 에서 부릅니다. FD 모드에서는 `XM_CAN_REJECTED` 입니다.

**예제**

```c
if (XM_CanOpenSendNmt(XM_CANOPEN_NMT_START, 1U) == XM_CAN_OK) {
    /* 노드 1 에 Start 를 보냈습니다 */
}
```

**참고**: [`XM_CanOpenGetNodeState`](#xm_canopengetnodestate)

---

### 하트비트 감시

#### `XM_CanOpenWatchNode`

```c
bool XM_CanOpenWatchNode(uint8_t node_id, uint16_t timeout_ms);
```

노드의 하트비트(ID `0x700` + 노드 ID)를 감시합니다. 최대 `XM_CANOPEN_MAX_WATCH_NODES` (8) 노드입니다.

장치가 하트비트를 주기적으로 내도록 설정돼 있어야 뜻이 있습니다. 장치의 하트비트 송신 주기는 장치 설명서나 SDO `0x1017` 로 확인하세요.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `node_id` | `uint8_t` | 감시할 노드 번호 |
| `timeout_ms` | `uint16_t` | 이만큼 하트비트가 없으면 `XM_CANOPEN_NODE_LOST` [ms] |

**반환값**: `bool` — `true`: 등록했습니다. `false`: 등록하지 못했습니다(FD 모드 등).

**⚠️ 호출 컨텍스트**: `Control_Setup()` 에서만 부릅니다.

**예제**

```c
void Control_Setup(void) {
    XM_CanOpenWatchNode(1U, 1500U);    /* 노드 1 — 1.5 초 동안 하트비트가 없으면 LOST */
}
```

**참고**: 같은 ID(`0x700` + 노드 ID)를 `XM_CanAttachDevice` · `XM_CanSubscribe` · `XM_CanEnableHeartbeat` 로도 쓰는 경우 [`XM_CanEnableHeartbeat`](xm-api-canbus.md#xm_canenableheartbeat) 를 보세요.

---

#### `XM_CanOpenGetNodeState`

```c
XmCanOpenNodeState_t XM_CanOpenGetNodeState(uint8_t node_id);
```

감시 노드의 상태를 돌려줍니다. `XM_CANOPEN_NODE_LOST` 는 XM10 의 판단이지 장치가 보낸 값이 아닙니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `node_id` | `uint8_t` | `XM_CanOpenWatchNode` 로 등록한 노드 번호 |

**반환값**: [`XmCanOpenNodeState_t`](#xmcanopennodestate_t). 감시를 등록하지 않았거나, FD 모드이거나, 다른 태스크에서 불렀으면 `XM_CANOPEN_NODE_NOT_WATCHED` 입니다.

**⚠️ 호출 컨텍스트**: `Control_Setup()` · `Control_Loop()` 에서 부릅니다.

**예제**

```c
if (XM_CanOpenGetNodeState(1U) == XM_CANOPEN_NODE_LOST) {
    /* 하트비트가 끊겼습니다 — 다음 하트비트가 올 때까지 이 상태입니다 */
}
```

---

#### `XM_CanOpenGetBootupCount`

```c
uint32_t XM_CanOpenGetBootupCount(uint8_t node_id);
```

감시 노드의 부팅 알림(`0x00`)을 받은 수를 돌려줍니다. 장치가 켜지거나 재설정될 때마다 늘어납니다.

받은 알림의 수일 뿐 특정 NMT 요청의 응답이 아닙니다. 장치가 있는지 확인하는 데 쓰세요.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `node_id` | `uint8_t` | `XM_CanOpenWatchNode` 로 등록한 노드 번호 |

**반환값**: `uint32_t` — 부팅 알림을 받은 수. 감시를 등록하지 않았으면 0 입니다.

**⚠️ 호출 컨텍스트**: `Control_Setup()` · `Control_Loop()` 에서 부릅니다.

---

### SDO

#### `XM_CanOpenSdoRead`

```c
XmCanResult_t XM_CanOpenSdoRead(uint8_t node_id, uint16_t index, uint8_t subindex);
```

SDO 빠른 읽기를 시작합니다. 기다리지 않습니다. `Control_Loop()` 에서 [`XM_CanOpenSdoGetResult`](#xm_canopensdogetresult) 로 결과를 보세요.

한 번에 한 요청입니다(모든 노드 공통). 진행 중이거나, 결과를 아직 가져가지 않았거나, [쉬는 시간](#시간-초과나-분할-전송-뒤의-쉬는-시간)이면 `XM_CAN_BUSY` 입니다. 이 노드의 응답 ID(`0x580` + 노드 ID)를 `XM_CanSubscribe` 로 등록했으면 `XM_CAN_REJECTED` 입니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `node_id` | `uint8_t` | 읽을 노드 번호 |
| `index` | `uint16_t` | 오브젝트 인덱스 |
| `subindex` | `uint8_t` | 서브인덱스 |

**반환값**: [`XmCanResult_t`](xm-api-canbus.md#xmcanresult_t)

- `XM_CAN_OK`: 요청을 받아들였습니다. 결과는 `XM_CanOpenSdoGetResult()` 로 봅니다.
- `XM_CAN_BUSY`: 진행 중이거나, 결과를 아직 가져가지 않았거나, 쉬는 시간입니다.
- `XM_CAN_REJECTED`: 응답 ID 를 `XM_CanSubscribe` 로 등록했거나, FD 모드이거나, 인자가 잘못됐습니다.
- `XM_CAN_NOT_OWNER`: `Control_Setup()` · `Control_Loop()` 밖에서 불렀습니다.
- 그 밖의 값은 [`XmCanResult_t`](xm-api-canbus.md#xmcanresult_t) 표를 보세요.

**⚠️ 호출 컨텍스트**: `Control_Setup()` · `Control_Loop()` 에서 부릅니다. 한 번만 일어나는 곳(예: 버튼을 누른 순간)에서 시작하고, 결과는 `Control_Loop()` 마다 확인하세요.

**예제**

```c
static bool s_sdo_pending;

/* 한 번만 일어나는 곳에서 시작합니다 */
if (!s_sdo_pending && (XM_CanOpenSdoRead(1U, 0x1017U, 0U) == XM_CAN_OK)) {
    s_sdo_pending = true;                       /* 0x1017: 장치의 하트비트 송신 주기 */
}

/* Control_Loop() 마다 결과를 확인합니다 */
if (s_sdo_pending) {
    XmCanOpenSdoResult_t r;
    XmCanOpenSdoStatus_t st = XM_CanOpenSdoGetResult(&r);
    if (st == XM_CANOPEN_SDO_DONE) {
        /* r.value, r.size 사용 */
        s_sdo_pending = false;
    } else if ((st != XM_CANOPEN_SDO_BUSY) && (st != XM_CANOPEN_SDO_IDLE)) {
        s_sdo_pending = false;                  /* ABORTED · TIMEOUT · UNSUPPORTED */
    }
}
```

**참고**: [SDO 쓰는 순서](#sdo-쓰는-순서), [`XmCanOpenSdoResult_t`](#xmcanopensdoresult_t)

---

#### `XM_CanOpenSdoWrite`

```c
XmCanResult_t XM_CanOpenSdoWrite(uint8_t node_id, uint16_t index, uint8_t subindex,
                                 uint32_t value, uint8_t size);
```

SDO 빠른 쓰기를 시작합니다. 규칙은 [`XM_CanOpenSdoRead`](#xm_canopensdoread) 와 같습니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `node_id` | `uint8_t` | 쓸 노드 번호 |
| `index` | `uint16_t` | 오브젝트 인덱스 |
| `subindex` | `uint8_t` | 서브인덱스 |
| `value` | `uint32_t` | 쓸 값 (작은 쪽 바이트부터 보냅니다) |
| `size` | `uint8_t` | 쓸 바이트 수, 1 ~ 4 |

**반환값**: [`XmCanResult_t`](xm-api-canbus.md#xmcanresult_t) — 뜻은 [`XM_CanOpenSdoRead`](#xm_canopensdoread) 와 같습니다.

**⚠️ 호출 컨텍스트**: `Control_Setup()` · `Control_Loop()` 에서 부릅니다.

**예제**

```c
/* index · subindex · 값은 장치 설명서를 보세요 */
XM_CanOpenSdoWrite(1U, MY_OD_INDEX, MY_OD_SUBINDEX, 100U, 2U);
```

---

#### `XM_CanOpenSdoGetResult`

```c
XmCanOpenSdoStatus_t XM_CanOpenSdoGetResult(XmCanOpenSdoResult_t* out);
```

SDO 결과를 봅니다. 끝난 결과(`DONE` · `ABORTED` · `TIMEOUT` · `UNSUPPORTED`)는 **한 번** 돌려준 뒤 `IDLE` 이 됩니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `out` | [`XmCanOpenSdoResult_t*`](#xmcanopensdoresult_t) | 끝난 결과를 받을 구조체 |

**반환값**: [`XmCanOpenSdoStatus_t`](#xmcanopensdostatus_t). `IDLE` 이나 `BUSY` 면 `out` 은 그대로입니다. FD 모드이거나 다른 태스크에서 부르면 `IDLE` 입니다(`out` 그대로).

**⚠️ 호출 컨텍스트**: `Control_Setup()` · `Control_Loop()` 에서 부릅니다.

---

### 통계

#### `XM_CanOpenGetStats`

```c
bool XM_CanOpenGetStats(XmCanOpenStats_t* out);
```

CANopen 도움 함수 통계를 읽습니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `out` | [`XmCanOpenStats_t*`](#xmcanopenstats_t) | 통계를 받을 구조체 |

**반환값**: `bool` — `true`: 채웠습니다. `false`: `out` 이 `NULL` 입니다.

**⚠️ 호출 컨텍스트**: `Control_Setup()` · `Control_Loop()` 에서 부릅니다.

---

## 타입 / 상수

### `XmCanOpenNodeState_t`

감시 노드의 상태입니다. [`XM_CanOpenGetNodeState()`](#xm_canopengetnodestate) 로 읽습니다.

| 값 | 이름 | 뜻 |
|----|------|------|
| 0 | `XM_CANOPEN_NODE_UNKNOWN` | 아직 하트비트를 받지 못함 |
| 1 | `XM_CANOPEN_NODE_BOOTUP` | `0x00` — 방금 켜짐(부팅 알림) |
| 2 | `XM_CANOPEN_NODE_STOPPED` | `0x04` |
| 3 | `XM_CANOPEN_NODE_OPERATIONAL` | `0x05` |
| 4 | `XM_CANOPEN_NODE_PRE_OPERATIONAL` | `0x7F` |
| 5 | `XM_CANOPEN_NODE_LOST` | `timeout_ms` 동안 받지 못함 — 다음 하트비트까지 유지 |
| 6 | `XM_CANOPEN_NODE_NOT_WATCHED` | 감시 등록 안 함 · FD 모드 · 다른 태스크에서 부름 |

---

### `XmCanOpenSdoStatus_t`

SDO 진행 상태입니다. [`XM_CanOpenSdoGetResult()`](#xm_canopensdogetresult) 가 돌려줍니다.

| 값 | 이름 | 뜻 |
|----|------|------|
| 0 | `XM_CANOPEN_SDO_IDLE` | 진행 중인 요청 없음 |
| 1 | `XM_CANOPEN_SDO_BUSY` | 응답을 기다리는 중 |
| 2 | `XM_CANOPEN_SDO_DONE` | 끝남 — 읽기면 `value` · `size` 에 값 |
| 3 | `XM_CANOPEN_SDO_ABORTED` | 장치가 중단(SDO abort)을 보냄 — `abort_code` 에 그 코드 |
| 4 | `XM_CANOPEN_SDO_TIMEOUT` | `XM_CANOPEN_SDO_TIMEOUT_MS` 안에 응답이 없음 |
| 5 | `XM_CANOPEN_SDO_UNSUPPORTED` | 장치가 분할 전송(4 B 초과 등)으로 답함 — 이 API 는 지원하지 않습니다 |

---

### `XmCanOpenSdoResult_t`

끝난 SDO 요청의 결과입니다. [`XM_CanOpenSdoGetResult()`](#xm_canopensdogetresult) 로 읽습니다.

```c
typedef struct {
    uint8_t  node_id;
    uint16_t index;
    uint8_t  subindex;
    uint32_t value;
    uint8_t  size;
    uint32_t abort_code;
    bool     abort_queued;
} XmCanOpenSdoResult_t;
```

| 필드 | 타입 | 설명 |
|------|------|------|
| `node_id` | `uint8_t` | 요청한 노드 번호 |
| `index` | `uint16_t` | 요청한 오브젝트 인덱스 |
| `subindex` | `uint8_t` | 요청한 서브인덱스 |
| `value` | `uint32_t` | 읽기 결과 (작은 쪽 바이트부터) |
| `size` | `uint8_t` | 읽은 바이트 수 1 ~ 4. 장치가 크기를 알려 주지 않으면 4 |
| `abort_code` | `uint32_t` | `ABORTED` 일 때 장치가 보낸 중단 코드 |
| `abort_queued` | `bool` | `TIMEOUT` · `UNSUPPORTED` 때 XM10 이 보낸 중단 프레임이 실렸는지 (한 번만 시도) |

---

### `XmCanOpenStats_t`

CANopen 도움 함수 통계입니다. [`XM_CanOpenGetStats()`](#xm_canopengetstats) 로 읽습니다. 각 값은 온전하지만 서로 같은 순간의 값은 아닙니다.

| 필드 | 타입 | 설명 |
|------|------|------|
| `sdo_late_response_count` | `uint32_t` | 시간 초과 뒤 늦게 와서 버린 응답 + 기다리던 요청과 맞지 않는 응답(종류가 다르거나 길이가 모자람)의 수 |
| `sdo_rx_busy_drop_count` | `uint32_t` | 응답을 넣는 중 겹쳐 놓친 수 (대개 `TIMEOUT` 으로 끝남) |
| `sdo_clear_hold_count` | `uint32_t` | 시간 초과 뒤 송신 대기를 비우려고 새 송신을 막은 tick 수 |
| `sdo_last_clear_wait_ticks` | `uint32_t` | 마지막 시간 초과에서 송신 대기가 비기까지 걸린 tick 수 |

---

### 상수

**NMT 명령** — ID `0x000`, 데이터 `[명령, 노드 ID]` 2 B

| 이름 | 값 | 설명 |
|------|----|------|
| `XM_CANOPEN_NMT_START` | `0x01U` | Start |
| `XM_CANOPEN_NMT_STOP` | `0x02U` | Stop |
| `XM_CANOPEN_NMT_PRE_OPERATIONAL` | `0x80U` | Pre-operational |
| `XM_CANOPEN_NMT_RESET_NODE` | `0x81U` | Reset node |
| `XM_CANOPEN_NMT_RESET_COMM` | `0x82U` | Reset communication |

**그 밖의 상수**

| 이름 | 값 | 설명 |
|------|----|------|
| `XM_CANOPEN_MAX_WATCH_NODES` | `8U` | 하트비트를 감시할 수 있는 노드 수 |
| `XM_CANOPEN_SDO_TIMEOUT_MS` | `500U` | SDO 응답을 기다리는 시간 [ms]. 이 API 의 정책이며, 모든 장치의 응답 시간을 보장하는 값이 아닙니다 |
| `XM_CANOPEN_SDO_QUARANTINE_MS` | `1500U` | 시간 초과 뒤 새 SDO 요청을 받지 않는 시간 [ms] ([쉬는 시간](#시간-초과나-분할-전송-뒤의-쉬는-시간)) |

---

## Rev1.1 / Rev2.0 차이 요약

| 항목 | Rev1.1 | Rev2.0 |
|------|--------|--------|
| CANopen 도움 함수 (`xm_api_canopen.h`) | ❌ 없음 | 🟢 전용 (클래식 모드에서만 동작) |

---

## 관련 문서

- [12. CANopen 장치 다루기 (개념)](../12-canopen.md) — 쓰는 순서, 배선, 흔한 실수
- [`xm_api_canbus.h` — CAN-FD · 클래식 CAN 으로 연결한 내 장치와 주고받기](xm-api-canbus.md) — 포트 모드 · 클래식 속도 · SYNC · 자유 송수신
- [device-examples](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/device-examples/) — [`canopen-pc-sim/`](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/device-examples/canopen-pc-sim/) 로 PC 에서 CANopen 드라이브를 흉내 낼 수 있습니다
