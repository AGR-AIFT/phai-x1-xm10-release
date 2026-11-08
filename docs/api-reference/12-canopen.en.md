# API Reference: Working with CANopen Devices

> 📌 **After reading this page** you will be able to wake a CANopen (CiA 301) motor driver with NMT from the XM10, watch it with its heartbeat, and configure and command it with SDO. For 1 kHz commands, you use PDO and SYNC.
> ⏱️ Estimated reading time: 40 minutes
> 🧰 Prerequisites: Classic mode and free send and receive in [CAN Communication](11-canbus.en.md)
> 🎯 Key functions: `XM_CanOpenSendNmt` / `XM_CanOpenWatchNode` / `XM_CanOpenSdoRead` / `XM_CanOpenSdoWrite` / `XM_CanOpenSdoGetResult`
>
> 🟢 **Rev 2.0 only** *(new in v2.10.0)*: The Rev 1.1 SDK does not have `xm_api_canopen.h`. It works **in classic mode only**.

This page is a guide to the **CANopen helper functions** defined in `xm_api_canopen.h`. For each function in detail, see the [`xm_api_canopen.h` reference](ref/xm-api-canopen.en.md).

> ⚠️ **This code turns a motor.** Before you run it with a real driver, be sure to read "6. Switching to a Real Driver" below.
>
> ℹ️ **Board status**: The flow below is what the API and the examples define; we have not yet confirmed it on a board. (not yet confirmed on the board) <!-- board-check -->

---

## 1. What It Helps With

| Feature | Function | Message |
|------|------|------|
| Send NMT | `XM_CanOpenSendNmt(command, node_id)` | `0x000` `[command, node]` |
| Monitor node heartbeats (up to 8 nodes) | `XM_CanOpenWatchNode` · `XM_CanOpenGetNodeState` · `XM_CanOpenGetBootupCount` | `0x700 + node` |
| SDO expedited read · write (4 B or less) | `XM_CanOpenSdoRead` · `XM_CanOpenSdoWrite` · `XM_CanOpenSdoGetResult` | `0x600 + node` → `0x580 + node` |

The port must be set to classic mode (`XM_CanBusMode()` returns `XM_CAN_BUS_CLASSIC` — [CAN Communication — Port and Bus Mode](11-canbus.en.md)). In FD mode, sending and registering return `XM_CAN_REJECTED` / `false`. For SYNC, use `XM_CanEnableSync`; for other messages such as PDO, use `XM_CanSendFrame` · `XM_CanReadFrame` · `XM_CanSubscribe`. The rules for where to call and for checking results (`== XM_CAN_OK`) are the same as in CAN Communication.

---

## 2. NMT and Heartbeat Monitoring

| NMT command | Value | Meaning |
|------|:---:|------|
| `XM_CANOPEN_NMT_START` | `0x01` | Go to OPERATIONAL — PDOs are exchanged |
| `XM_CANOPEN_NMT_STOP` | `0x02` | Go to STOPPED — it does not answer SDO |
| `XM_CANOPEN_NMT_PRE_OPERATIONAL` | `0x80` | Go to PRE_OPERATIONAL — the state in which you configure (SDO) |
| `XM_CANOPEN_NMT_RESET_NODE` | `0x81` | Reset the node |
| `XM_CANOPEN_NMT_RESET_COMM` | `0x82` | Reset communication — the communication settings go back to their defaults |

If you register a node with `XM_CanOpenWatchNode(node, timeout_ms)` in `Control_Setup()`, `XM_CanOpenGetNodeState()` tells you its state from the last heartbeat.

| State | Meaning |
|------|------|
| `XM_CANOPEN_NODE_UNKNOWN` | No heartbeat received yet |
| `XM_CANOPEN_NODE_BOOTUP` | Just powered on (boot-up notice `0x00`) |
| `XM_CANOPEN_NODE_STOPPED` · `OPERATIONAL` · `PRE_OPERATIONAL` | The state the driver sent: `0x04` · `0x05` · `0x7F` |
| `XM_CANOPEN_NODE_LOST` | Nothing received for `timeout_ms` — this is XM10's own judgment, and it is kept until the next heartbeat |
| `XM_CANOPEN_NODE_NOT_WATCHED` | Monitoring not registered · FD mode · called from somewhere else |

- The driver must be set to send its heartbeat periodically for the LOST judgment to mean anything (`0x1017` on the driver).
- `XM_CanOpenGetBootupCount()` is the number of boot-up notices received. If this number goes up after a communication reset, the driver is there.

---

## 3. SDO — One at a Time, No Waiting

```c
// start a write — returns right away
if (XM_CanOpenSdoWrite(node, 0x6040, 0, 0x0006, 2) == XM_CAN_OK) { s_waiting = true; }

// check the result in the following periods
XmCanOpenSdoResult_t r;
switch (XM_CanOpenSdoGetResult(&r)) {
case XM_CANOPEN_SDO_DONE:    /* done — for a read, r.value · r.size */ break;
case XM_CANOPEN_SDO_ABORTED: /* the driver refused — r.abort_code */ break;
case XM_CANOPEN_SDO_TIMEOUT: /* no response within 500 ms */ break;
default: break;              /* IDLE · BUSY — keep waiting */
}
```

| Rule | Details |
|------|------|
| One at a time | There is one request at a time, shared by all nodes. While one is in progress, or its result has not been fetched yet, you get `XM_CAN_BUSY` |
| Size | Expedited transfers of 4 bytes or less only. If the driver answers with a split transfer, you get `XM_CANOPEN_SDO_UNSUPPORTED` |
| Timeout | 500 ms (`XM_CANOPEN_SDO_TIMEOUT_MS`). When a request ends in a timeout or a split transfer, XM10 sends an abort frame once and accepts no new request for 1.5 seconds (`XM_CANOPEN_SDO_QUARANTINE_MS`, the quiet period) — so that a late response is not mistaken for the result of the next request |
| A result comes once | A finished result (DONE · ABORTED · TIMEOUT · UNSUPPORTED) is returned once, and then the status becomes IDLE |
| Response ID | If you register `0x580 + node` with `XM_CanSubscribe()`, the SDO functions refuse |

---

## 4. If Communication Drops, the Driver Stops by Itself — Heartbeat Consumer

If the XM10 stops or a wire comes off, XM10 cannot send "stop". So the key to a safe stop is to **make the driver watch the XM10 heartbeat and stop by itself when it is lost**.

1. XM10 sends the heartbeat — `XM_CanEnableHeartbeat(xm_node, 100)`. When `Control_Loop()` stops, no new heartbeat is sent.
2. Set the driver to watch that heartbeat — set the heartbeat consumer through SDO (`0x1016:01` = (XM10 heartbeat number << 16) | time in ms).
3. Consumer monitoring starts **only after the first heartbeat arrives following the setup**. Turn the motor on after you confirm that monitoring is on (how to confirm differs from driver to driver — the example confirms it with an object that exists only in the PC simulator).
4. When you learn that communication is abnormal, stop commanding, try one stop write (do not assume it arrived), and turn off the XM10 heartbeat.

---

## 5. 1 kHz Commands — PDO and SYNC

SDO is slow. To command at 1 kHz, set up the PDOs with SDO, then exchange data with PDO and SYNC ([Ex.47](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/47_CANopen_Drive_PDO/)).

| Step | What to do |
|------|------|
| Change the mapping (CiA 301 order) | Disable the PDO through the communication parameter (COB-ID bit 31) → set the number of mapped objects to 0 → entries → number of entries → transmission type (1 = every SYNC) → enable it again |
| Prepare to receive | Register the TPDO ID with `XM_CanSubscribe()` → `XM_CanReadLatestById()` |
| Start | NMT start → `XM_CanEnableSync(true)` |
| Exchange | Send one RPDO (`XM_CanSendFrame`) in each period in which a new TPDO arrived |
| Judging a drop | SYNC and PDO can pause for a moment during bus clearing and recovery. Do not assume 1 kHz; treat it as a communication failure when the TPDO stops for a set time (50 ms in the example) |

---

## 6. Switching to a Real Driver

The partner in Ex.46 and Ex.47 is the [PC simulator](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/device-examples/canopen-pc-sim/), and the examples do not turn the motor on if they cannot confirm the simulator's marker object. To switch to a real driver and motor:

- First decide the **profile · objects · stop method · heartbeat consumer time · reaction to lost communication** from the driver manual.
- Start with a low speed, and first check that the button stop works.
- Only after that, change the simulator check in the example.
- While this port is used in classic mode, the sensor hubs do not work. Do not connect a CAN-FD device to the same port.

---

## 7. Statistics

`XM_CanOpenGetStats(&s)` — Each value is consistent on its own, but the values are not from the same instant.

| Value | Meaning |
|------|------|
| `sdo_late_response_count` | Responses dropped because they came late after a timeout, plus responses that do not match the awaited request |
| `sdo_rx_busy_drop_count` | Responses missed because of an overlap while they were being stored (usually ends in TIMEOUT) |
| `sdo_clear_hold_count` · `sdo_last_clear_wait_ticks` | Number of ticks in which new sends were held back while the send queue was emptied after a timeout · number of ticks it took for the queue to empty the last time |

---

## 8. Examples and Simulator

| What | Link |
|------|------|
| Set up with SDO · 100 Hz commands | [Ex.46 CANopen Drive](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/46_CANopen_Drive/) |
| 1 kHz with PDO + SYNC | [Ex.47 CANopen Drive PDO](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/47_CANopen_Drive_PDO/) |
| PC simulator of a CANopen drive | [canopen-pc-sim](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/device-examples/canopen-pc-sim/) |
| Every function in detail | [`xm_api_canopen.h` reference](ref/xm-api-canopen.en.md) |
