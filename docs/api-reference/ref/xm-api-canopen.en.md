# `xm_api_canopen.h` — CANopen Device Helper Functions

> 🟢 **Rev 2.0 only** — This header is for XM10 Rev 2.0, and it works only when the port is set to **classic mode**. You cannot use the functions below from a Rev 1.1 project (`XM10_SDK/Rev1.1/`). To check your board revision, see [Board Revision Comparison](../../hardware/README.en.md#board-revision-comparison).
>
> - **Header**: `XM_FW/XM_API/xm_api_canopen.h` (you get it together with `xm_api.h`. This header includes `xm_api_canbus.h`)
> - **Related concept doc**: [12. Working with CANopen Devices](../12-canopen.en.md)
> - **Related examples**: [46_CANopen_Drive](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/46_CANopen_Drive/) · [47_CANopen_Drive_PDO](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/47_CANopen_Drive_PDO/)
> - **Partner device example**: [`canopen-pc-sim/`](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/device-examples/canopen-pc-sim/) — a PC simulator of a CANopen velocity-mode drive ([device-examples](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/device-examples/))
> - **Board check**: The behavior below is what the API defines; it has not been confirmed on a board yet. (not yet confirmed on the board) <!-- board-check -->

This header defines the functions that help XM10 handle CANopen (CiA 301) motor drivers and similar devices directly — **sending NMT · monitoring node heartbeats · SDO expedited read/write (4 B or less)**. They work only when the port is set to classic mode (`XM_CanBusMode` in `xm_api_canbus.h`). In FD mode, sending returns `XM_CAN_REJECTED` and registering returns `false`.

---

## When to use it

Use it when you connect a device such as a CANopen motor driver to the XM10 classic CAN port, and you want to change its state (NMT), see whether it is alive (heartbeat monitoring), and read and write its settings (SDO). For the order of use, wiring and common mistakes, we recommend reading [12. Working with CANopen Devices](../12-canopen.en.md) first. This page only covers the signatures and parameters of **every function, type and constant** declared in the header.

What is not in this header is done with `xm_api_canbus.h`.

| What you want to do | What to use |
|------|------|
| Send SYNC | [`XM_CanEnableSync`](xm-api-canbus.en.md#xm_canenablesync) (length 0 in classic mode) |
| Other messages such as PDO | [`XM_CanSendFrame`](xm-api-canbus.en.md#xm_cansendframe) · [`XM_CanReadFrame`](xm-api-canbus.en.md#xm_canreadframe) · [`XM_CanSubscribe`](xm-api-canbus.en.md#xm_cansubscribe) |

> ⚠️ **Always check a send result with `== XM_CAN_OK`.** `XM_CAN_OK` is 0, so `if (XM_CanOpenSendNmt(...))` reads a failure as a success.

---

## Calling rules

Where to call and how to check results are the same as in [`xm_api_canbus.h`](xm-api-canbus.en.md#calling-rules). No function waits.

| Kind | Functions | Where to call |
|------|------|-----------|
| Registration (1) | `XM_CanOpenWatchNode` | Only in `Control_Setup()` |
| Other functions (7) | `XM_CanOpenSendNmt` · `XM_CanOpenGetNodeState` · `XM_CanOpenGetBootupCount` · `XM_CanOpenSdoRead` · `XM_CanOpenSdoWrite` · `XM_CanOpenSdoGetResult` · `XM_CanOpenGetStats` | Only in `Control_Setup()` and `Control_Loop()` |

Called from another task or an interrupt, a call is rejected.

| Function | When called from outside |
|------|------|
| `XM_CanOpenSendNmt` · `XM_CanOpenSdoRead` · `XM_CanOpenSdoWrite` | `XM_CAN_NOT_OWNER` |
| `XM_CanOpenGetNodeState` | `XM_CANOPEN_NODE_NOT_WATCHED` |
| `XM_CanOpenSdoGetResult` | `XM_CANOPEN_SDO_IDLE` (`out` unchanged) |

---

## Function list

**NMT**

| Function | One-line description |
|------|-----------|
| [`XM_CanOpenSendNmt`](#xm_canopensendnmt) | Sends an NMT command |

**Heartbeat monitoring**

| Function | One-line description |
|------|-----------|
| [`XM_CanOpenWatchNode`](#xm_canopenwatchnode) | Registers a node for heartbeat monitoring (only in `Control_Setup()`) |
| [`XM_CanOpenGetNodeState`](#xm_canopengetnodestate) | The state of a monitored node |
| [`XM_CanOpenGetBootupCount`](#xm_canopengetbootupcount) | Number of boot-up notices received from a monitored node |

**SDO**

| Function | One-line description |
|------|-----------|
| [`XM_CanOpenSdoRead`](#xm_canopensdoread) | Starts an SDO expedited read |
| [`XM_CanOpenSdoWrite`](#xm_canopensdowrite) | Starts an SDO expedited write |
| [`XM_CanOpenSdoGetResult`](#xm_canopensdogetresult) | Looks at the SDO result |

**Statistics**

| Function | One-line description |
|------|-----------|
| [`XM_CanOpenGetStats`](#xm_canopengetstats) | CANopen helper statistics |

**Types · constants**: [`XmCanOpenNodeState_t`](#xmcanopennodestate_t) · [`XmCanOpenSdoStatus_t`](#xmcanopensdostatus_t) · [`XmCanOpenSdoResult_t`](#xmcanopensdoresult_t) · [`XmCanOpenStats_t`](#xmcanopenstats_t) · [8 constants](#constants)

---

## How to use SDO

SDO takes **two steps: start, then fetch the result**. Neither function waits.

1. Start with `XM_CanOpenSdoRead()` or `XM_CanOpenSdoWrite()`. `XM_CAN_OK` means the request was accepted.
2. In `Control_Loop()`, look at the result with `XM_CanOpenSdoGetResult()`.
3. A finished result (`DONE` · `ABORTED` · `TIMEOUT` · `UNSUPPORTED`) is returned **once**, and then the status becomes `IDLE`.

Only one request runs at a time (shared by all nodes). A new request returns `XM_CAN_BUSY` while one is in progress, while a result has not been fetched yet, or during the quiet period below.

| How it ended | `XM_CanOpenSdoGetResult()` | Quiet period before the next request |
|------|------|:---:|
| Normally | `XM_CANOPEN_SDO_DONE` | none |
| The device sent an abort | `XM_CANOPEN_SDO_ABORTED` | none |
| No response within `XM_CANOPEN_SDO_TIMEOUT_MS` (500 ms) | `XM_CANOPEN_SDO_TIMEOUT` | `XM_CANOPEN_SDO_QUARANTINE_MS` (1500 ms) |
| The device answered with a split transfer | `XM_CANOPEN_SDO_UNSUPPORTED` | `XM_CANOPEN_SDO_QUARANTINE_MS` (1500 ms) |

### Quiet period after a timeout or a split transfer

When a request ends in `TIMEOUT` or `UNSUPPORTED`, XM10 sends an abort frame once. After it confirms that the request has left the send queue, it accepts no new SDO request for 1.5 seconds (`XM_CAN_BUSY`).

- During that time, late responses from that node are dropped. This is so that they are not mistaken for the result of the next request.
- This promise holds only if the device sends its response within 1.4 seconds of receiving the request. Check the device manual.
- Until the send queue empties, new sends from this port are `XM_CAN_BUSY` for a moment and SYNC can be skipped. Under a light load it is a few ms. Even at worst, 80 ms after 20 ms in a row is normal.
- If another device keeps using the bus without a break, the next SDO request can stay `XM_CAN_BUSY` for a long time. Reducing the bus load clears it.

---

## Function details

### NMT

#### `XM_CanOpenSendNmt`

```c
XmCanResult_t XM_CanOpenSendNmt(uint8_t command, uint8_t node_id);
```

Sends an NMT command — ID `0x000`, data `[command, node_id]` (2 bytes).

**Parameters**

| Name | Type | Description |
|------|------|------|
| `command` | `uint8_t` | One of the five [`XM_CANOPEN_NMT_*`](#constants) values. Any other value gives `XM_CAN_REJECTED` |
| `node_id` | `uint8_t` | `0` = all nodes, `1` – `127` = that node |

**Return value**: [`XmCanResult_t`](xm-api-canbus.en.md#xmcanresult_t) — `XM_CAN_OK`: put in the send queue. It does not mean the other side received it. See the table for the other values.

**⚠️ Call context**: Call it in `Control_Setup()` or `Control_Loop()`. In FD mode it returns `XM_CAN_REJECTED`.

**Example**

```c
if (XM_CanOpenSendNmt(XM_CANOPEN_NMT_START, 1U) == XM_CAN_OK) {
    /* Start was sent to node 1 */
}
```

**See also**: [`XM_CanOpenGetNodeState`](#xm_canopengetnodestate)

---

### Heartbeat monitoring

#### `XM_CanOpenWatchNode`

```c
bool XM_CanOpenWatchNode(uint8_t node_id, uint16_t timeout_ms);
```

Monitors the heartbeat of a node (ID `0x700` + node ID). Up to `XM_CANOPEN_MAX_WATCH_NODES` (8) nodes.

It is meaningful only if the device is set to send its heartbeat periodically. For the device's heartbeat period, check the device manual or SDO `0x1017`.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `node_id` | `uint8_t` | The node number to monitor |
| `timeout_ms` | `uint16_t` | If no heartbeat arrives for this long, the state becomes `XM_CANOPEN_NODE_LOST` [ms] |

**Return value**: `bool` — `true`: registered. `false`: could not be registered (FD mode and so on).

**⚠️ Call context**: Call it only in `Control_Setup()`.

**Example**

```c
void Control_Setup(void) {
    XM_CanOpenWatchNode(1U, 1500U);    /* node 1 — LOST if no heartbeat for 1.5 seconds */
}
```

**See also**: If you also use the same ID (`0x700` + node ID) with `XM_CanAttachDevice` · `XM_CanSubscribe` · `XM_CanEnableHeartbeat`, see [`XM_CanEnableHeartbeat`](xm-api-canbus.en.md#xm_canenableheartbeat).

---

#### `XM_CanOpenGetNodeState`

```c
XmCanOpenNodeState_t XM_CanOpenGetNodeState(uint8_t node_id);
```

Returns the state of a monitored node. `XM_CANOPEN_NODE_LOST` is XM10's own judgment, not a value sent by the device.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `node_id` | `uint8_t` | A node number registered with `XM_CanOpenWatchNode` |

**Return value**: [`XmCanOpenNodeState_t`](#xmcanopennodestate_t). It is `XM_CANOPEN_NODE_NOT_WATCHED` if monitoring is not registered, the port is in FD mode, or it was called from another task.

**⚠️ Call context**: Call it in `Control_Setup()` or `Control_Loop()`.

**Example**

```c
if (XM_CanOpenGetNodeState(1U) == XM_CANOPEN_NODE_LOST) {
    /* the heartbeat is lost — it stays in this state until the next heartbeat arrives */
}
```

---

#### `XM_CanOpenGetBootupCount`

```c
uint32_t XM_CanOpenGetBootupCount(uint8_t node_id);
```

Returns how many boot-up notices (`0x00`) were received from a monitored node. It goes up each time the device is powered on or reset.

It is only a count of notices received, not the response to a particular NMT request. Use it to check that the device is there.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `node_id` | `uint8_t` | A node number registered with `XM_CanOpenWatchNode` |

**Return value**: `uint32_t` — number of boot-up notices received. 0 if the node is not monitored.

**⚠️ Call context**: Call it in `Control_Setup()` or `Control_Loop()`.

---

### SDO

#### `XM_CanOpenSdoRead`

```c
XmCanResult_t XM_CanOpenSdoRead(uint8_t node_id, uint16_t index, uint8_t subindex);
```

Starts an SDO expedited read. It does not wait. Look at the result in `Control_Loop()` with [`XM_CanOpenSdoGetResult`](#xm_canopensdogetresult).

Only one request runs at a time (shared by all nodes). It returns `XM_CAN_BUSY` while one is in progress, while a result has not been fetched yet, or during the [quiet period](#quiet-period-after-a-timeout-or-a-split-transfer). If you registered this node's response ID (`0x580` + node ID) with `XM_CanSubscribe`, it returns `XM_CAN_REJECTED`.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `node_id` | `uint8_t` | The node number to read from |
| `index` | `uint16_t` | Object index |
| `subindex` | `uint8_t` | Sub-index |

**Return value**: [`XmCanResult_t`](xm-api-canbus.en.md#xmcanresult_t)

- `XM_CAN_OK`: the request was accepted. Look at the result with `XM_CanOpenSdoGetResult()`.
- `XM_CAN_BUSY`: a request is in progress, a result has not been fetched yet, or it is the quiet period.
- `XM_CAN_REJECTED`: the response ID was registered with `XM_CanSubscribe`, the port is in FD mode, or an argument is invalid.
- `XM_CAN_NOT_OWNER`: called outside `Control_Setup()` / `Control_Loop()`.
- For the other values, see the [`XmCanResult_t`](xm-api-canbus.en.md#xmcanresult_t) table.

**⚠️ Call context**: Call it in `Control_Setup()` or `Control_Loop()`. Start it from a place where it happens only once (for example, the moment a button is pressed), and check the result every `Control_Loop()`.

**Example**

```c
static bool s_sdo_pending;

/* start it from a place where it happens only once */
if (!s_sdo_pending && (XM_CanOpenSdoRead(1U, 0x1017U, 0U) == XM_CAN_OK)) {
    s_sdo_pending = true;                       /* 0x1017: the device's heartbeat period */
}

/* check the result every Control_Loop() */
if (s_sdo_pending) {
    XmCanOpenSdoResult_t r;
    XmCanOpenSdoStatus_t st = XM_CanOpenSdoGetResult(&r);
    if (st == XM_CANOPEN_SDO_DONE) {
        /* use r.value, r.size */
        s_sdo_pending = false;
    } else if ((st != XM_CANOPEN_SDO_BUSY) && (st != XM_CANOPEN_SDO_IDLE)) {
        s_sdo_pending = false;                  /* ABORTED · TIMEOUT · UNSUPPORTED */
    }
}
```

**See also**: [How to use SDO](#how-to-use-sdo), [`XmCanOpenSdoResult_t`](#xmcanopensdoresult_t)

---

#### `XM_CanOpenSdoWrite`

```c
XmCanResult_t XM_CanOpenSdoWrite(uint8_t node_id, uint16_t index, uint8_t subindex,
                                 uint32_t value, uint8_t size);
```

Starts an SDO expedited write. The rules are the same as [`XM_CanOpenSdoRead`](#xm_canopensdoread).

**Parameters**

| Name | Type | Description |
|------|------|------|
| `node_id` | `uint8_t` | The node number to write to |
| `index` | `uint16_t` | Object index |
| `subindex` | `uint8_t` | Sub-index |
| `value` | `uint32_t` | The value to write (sent starting from the least significant byte) |
| `size` | `uint8_t` | Number of bytes to write, 1 – 4 |

**Return value**: [`XmCanResult_t`](xm-api-canbus.en.md#xmcanresult_t) — the meaning is the same as [`XM_CanOpenSdoRead`](#xm_canopensdoread).

**⚠️ Call context**: Call it in `Control_Setup()` or `Control_Loop()`.

**Example**

```c
/* see the device manual for the index, sub-index and value */
XM_CanOpenSdoWrite(1U, MY_OD_INDEX, MY_OD_SUBINDEX, 100U, 2U);
```

---

#### `XM_CanOpenSdoGetResult`

```c
XmCanOpenSdoStatus_t XM_CanOpenSdoGetResult(XmCanOpenSdoResult_t* out);
```

Looks at the SDO result. A finished result (`DONE` · `ABORTED` · `TIMEOUT` · `UNSUPPORTED`) is returned **once**, and then the status becomes `IDLE`.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `out` | [`XmCanOpenSdoResult_t*`](#xmcanopensdoresult_t) | Struct that receives the finished result |

**Return value**: [`XmCanOpenSdoStatus_t`](#xmcanopensdostatus_t). With `IDLE` or `BUSY`, `out` is unchanged. In FD mode, or when called from another task, it returns `IDLE` (`out` unchanged).

**⚠️ Call context**: Call it in `Control_Setup()` or `Control_Loop()`.

---

### Statistics

#### `XM_CanOpenGetStats`

```c
bool XM_CanOpenGetStats(XmCanOpenStats_t* out);
```

Reads the CANopen helper statistics.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `out` | [`XmCanOpenStats_t*`](#xmcanopenstats_t) | Struct that receives the statistics |

**Return value**: `bool` — `true`: filled. `false`: `out` is `NULL`.

**⚠️ Call context**: Call it in `Control_Setup()` or `Control_Loop()`.

---

## Types / constants

### `XmCanOpenNodeState_t`

The state of a monitored node. Read it with [`XM_CanOpenGetNodeState()`](#xm_canopengetnodestate).

| Value | Name | Meaning |
|----|------|------|
| 0 | `XM_CANOPEN_NODE_UNKNOWN` | No heartbeat received yet |
| 1 | `XM_CANOPEN_NODE_BOOTUP` | `0x00` — just powered on (boot-up notice) |
| 2 | `XM_CANOPEN_NODE_STOPPED` | `0x04` |
| 3 | `XM_CANOPEN_NODE_OPERATIONAL` | `0x05` |
| 4 | `XM_CANOPEN_NODE_PRE_OPERATIONAL` | `0x7F` |
| 5 | `XM_CANOPEN_NODE_LOST` | Nothing received for `timeout_ms` — kept until the next heartbeat |
| 6 | `XM_CANOPEN_NODE_NOT_WATCHED` | Monitoring not registered · FD mode · called from another task |

---

### `XmCanOpenSdoStatus_t`

The SDO progress status. Returned by [`XM_CanOpenSdoGetResult()`](#xm_canopensdogetresult).

| Value | Name | Meaning |
|----|------|------|
| 0 | `XM_CANOPEN_SDO_IDLE` | No request in progress |
| 1 | `XM_CANOPEN_SDO_BUSY` | Waiting for the response |
| 2 | `XM_CANOPEN_SDO_DONE` | Finished — for a read, the value is in `value` · `size` |
| 3 | `XM_CANOPEN_SDO_ABORTED` | The device sent an abort (SDO abort) — its code is in `abort_code` |
| 4 | `XM_CANOPEN_SDO_TIMEOUT` | No response within `XM_CANOPEN_SDO_TIMEOUT_MS` |
| 5 | `XM_CANOPEN_SDO_UNSUPPORTED` | The device answered with a split transfer (more than 4 B and so on) — this API does not support it |

---

### `XmCanOpenSdoResult_t`

The result of a finished SDO request. Read it with [`XM_CanOpenSdoGetResult()`](#xm_canopensdogetresult).

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

| Field | Type | Description |
|------|------|------|
| `node_id` | `uint8_t` | The node number of the request |
| `index` | `uint16_t` | The object index of the request |
| `subindex` | `uint8_t` | The sub-index of the request |
| `value` | `uint32_t` | The read result (starting from the least significant byte) |
| `size` | `uint8_t` | Number of bytes read, 1 – 4. 4 if the device does not tell the size |
| `abort_code` | `uint32_t` | The abort code the device sent, when the status is `ABORTED` |
| `abort_queued` | `bool` | Whether the abort frame that XM10 sends on `TIMEOUT` · `UNSUPPORTED` was queued (tried only once) |

---

### `XmCanOpenStats_t`

CANopen helper statistics. Read them with [`XM_CanOpenGetStats()`](#xm_canopengetstats). Each value is consistent on its own, but the values are not from the same instant.

| Field | Type | Description |
|------|------|------|
| `sdo_late_response_count` | `uint32_t` | Responses dropped because they came late after a timeout, plus responses that do not match the awaited request (a different kind or too short) |
| `sdo_rx_busy_drop_count` | `uint32_t` | Responses missed because of an overlap while they were being stored (usually ends in `TIMEOUT`) |
| `sdo_clear_hold_count` | `uint32_t` | Number of ticks in which new sends were held back to empty the send queue after a timeout |
| `sdo_last_clear_wait_ticks` | `uint32_t` | Number of ticks it took for the send queue to empty after the last timeout |

---

### Constants

**NMT commands** — ID `0x000`, data `[command, node ID]` (2 B)

| Name | Value | Description |
|------|----|------|
| `XM_CANOPEN_NMT_START` | `0x01U` | Start |
| `XM_CANOPEN_NMT_STOP` | `0x02U` | Stop |
| `XM_CANOPEN_NMT_PRE_OPERATIONAL` | `0x80U` | Pre-operational |
| `XM_CANOPEN_NMT_RESET_NODE` | `0x81U` | Reset node |
| `XM_CANOPEN_NMT_RESET_COMM` | `0x82U` | Reset communication |

**Other constants**

| Name | Value | Description |
|------|----|------|
| `XM_CANOPEN_MAX_WATCH_NODES` | `8U` | Number of nodes whose heartbeat can be monitored |
| `XM_CANOPEN_SDO_TIMEOUT_MS` | `500U` | How long to wait for an SDO response [ms]. It is this API's policy, not a guarantee of the response time of every device |
| `XM_CANOPEN_SDO_QUARANTINE_MS` | `1500U` | How long no new SDO request is accepted after a timeout [ms] ([quiet period](#quiet-period-after-a-timeout-or-a-split-transfer)) |

---

## Rev1.1 / Rev2.0 Difference Summary

| Item | Rev1.1 | Rev2.0 |
|------|--------|--------|
| CANopen helper functions (`xm_api_canopen.h`) | ❌ Absent | 🟢 Exclusive (works in classic mode only) |

---

## Related documents

- [12. Working with CANopen Devices (concept)](../12-canopen.en.md) — order of use, wiring, common mistakes
- [`xm_api_canbus.h` — Talk to Your Own Device over CAN-FD or Classic CAN](xm-api-canbus.en.md) — port mode · classic speed · SYNC · free send and receive
- [device-examples](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/device-examples/) — with [`canopen-pc-sim/`](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/device-examples/canopen-pc-sim/) you can simulate a CANopen drive on a PC
