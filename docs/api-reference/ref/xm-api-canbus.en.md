# `xm_api_canbus.h` — Talk to Your Own Device over CAN-FD or Classic CAN

> 🟢 **Rev 2.0 only** — This header is for XM10 Rev 2.0. You cannot use the functions below from a Rev 1.1 project (`XM10_SDK/Rev1.1/`). To check your board revision, see [Board Revision Comparison](../../hardware/README.en.md#board-revision-comparison).
>
> - **Header**: `XM_FW/XM_API/xm_api_canbus.h` (you get it together with `xm_api.h`)
> - **Related concept doc**: [11. CAN Communication](../11-canbus.en.md)
> - **Related examples**: [45_CANFD_My_Sensor](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/45_CANFD_My_Sensor/) · [46_CANopen_Drive](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/46_CANopen_Drive/) · [47_CANopen_Drive_PDO](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/47_CANopen_Drive_PDO/)
> - **Partner device examples**: [device-examples](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/device-examples/) — the CAN-FD reference firmware [`canfd-sensor-stm32g474/`](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/device-examples/canfd-sensor-stm32g474/) and the PC simulators [`canfd-sensor-pc-sim/`](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/device-examples/canfd-sensor-pc-sim/) · [`canopen-pc-sim/`](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/device-examples/canopen-pc-sim/)
> - **Board check**: The behavior below is what the API defines; it has not been confirmed on a board yet. (not yet confirmed on the board) <!-- board-check -->

This header defines the API for **sending and receiving messages with your own device connected over CAN-FD or classic CAN**. There are two ways, and you may use both together.

- **① Device API** — Register a device number, and XM10 sorts the device's periodic data 1 and 2 and its alive signal for you (latest values). You can also send commands to the device.
- **② Free send and receive** — Send and receive with just an ID and data, no protocol (a receive-order queue and a per-ID latest-value mailbox).

For classic-CAN-only devices (such as motor drivers), set the port to classic mode with `XM_CanBusMode()`. The protocol, the device setup and the start-up order are decided in your own code. XM10 blocks only the IDs that have a reason to be blocked.

---

## When to use it

Use it when you want to connect your own sensor or actuator to the XM10 CAN port, read its values and send it commands. For the big picture — order of use, wiring, the message agreement on the device side — we recommend reading [11. CAN Communication](../11-canbus.en.md) first. This page only covers the signatures and parameters of **every function, type and constant** declared in the header.

For NMT, heartbeat monitoring and SDO of CANopen (CiA 301) devices, use [`xm_api_canopen.h`](xm-api-canopen.en.md) together with this header.

> ⚠️ **Always check a send result with `== XM_CAN_OK`.** `XM_CAN_OK` is 0, so `if (XM_CanSend(...))` reads a failure as a success.

---

## Rules for one port

- Do not connect classic devices and CAN-FD devices together.
- All devices must use the same speed.
- Two devices must not send the same ID — do not let the numbers overlap.
- There is a limit to how much you can send — add up the message times and calculate whether they fit within the bus time.
- The data rate is 5 Mbit/s at most. CAN XL is not supported.
- Only 11-bit IDs are supported. Remote frames are not supported.
- If you send the same ID while the earlier message is still waiting, the order can change — send after the earlier message has gone out.

---

## Calling rules

| Kind | Functions | Where to call |
|------|------|-----------|
| Functions you write (2) | `XM_CanBusMode` · `XM_CanClassicBitTiming` | The system calls them once at power-on. Other XM functions cannot be used yet at that point, so only return or fill in a value |
| Registration (2) | `XM_CanAttachDevice` · `XM_CanSubscribe` | Only in `Control_Setup()` |
| Send · receive · settings · statistics (13) | `XM_CanSetClassicBitTiming` · `XM_CanGetClassicBitTiming` · `XM_CanIsConnected` · `XM_CanReadLatest` · `XM_CanSend` · `XM_CanGetStats` · `XM_CanGetBusStats` · `XM_CanEnableSync` · `XM_CanEnableHeartbeat` · `XM_CanSendFrame` · `XM_CanReadFrame` · `XM_CanReadLatestById` · `XM_CanAgeMs` | Only in `Control_Setup()` and `Control_Loop()` |
| Anywhere (5) | `XM_CanGetBusMode` · `XM_CanIsIdAllowed` · `XM_CanIsDeviceIdAllowed` · `XM_CanFitLength` · `XM_CanSeqIsNewer` | Anywhere |

- Called outside `Control_Setup()` / `Control_Loop()` (from another task or an interrupt), a call is rejected. Functions that return `bool` return `false`, functions that return a result (`XmCanResult_t`) return `XM_CAN_NOT_OWNER`, and the call is counted in [`owner_violation_count`](#xmcanbusstats_t).
- No function waits.

---

## Function list

**Bus mode**

| Function | One-line description |
|------|-----------|
| [`XM_CanBusMode`](#xm_canbusmode) | Function that sets the port's bus mode — **you write it** (optional) |
| [`XM_CanGetBusMode`](#xm_cangetbusmode) | The current bus mode |
| [`XM_CanClassicBitTiming`](#xm_canclassicbittiming) | Function that sets the classic CAN speed — **you write it** (optional) |
| [`XM_CanSetClassicBitTiming`](#xm_cansetclassicbittiming) | Changes the classic CAN speed while running |
| [`XM_CanGetClassicBitTiming`](#xm_cangetclassicbittiming) | Reads the current classic CAN speed |

**Device API**

| Function | One-line description |
|------|-----------|
| [`XM_CanAttachDevice`](#xm_canattachdevice) | Registers a device number (only in `Control_Setup()`) |
| [`XM_CanIsConnected`](#xm_canisconnected) | Whether an alive signal arrived within 3 seconds |
| [`XM_CanReadLatest`](#xm_canreadlatest) | Reads the latest periodic data of a device |
| [`XM_CanSend`](#xm_cansend) | Sends a command to a device |
| [`XM_CanGetStats`](#xm_cangetstats) | Per-device statistics |
| [`XM_CanGetBusStats`](#xm_cangetbusstats) | Statistics for the whole port |
| [`XM_CanEnableSync`](#xm_canenablesync) | Sends the 1 kHz sync signal (SYNC) |
| [`XM_CanEnableHeartbeat`](#xm_canenableheartbeat) | Sends the XM10 heartbeat |

**Free send and receive**

| Function | One-line description |
|------|-----------|
| [`XM_CanSendFrame`](#xm_cansendframe) | Sends one message |
| [`XM_CanReadFrame`](#xm_canreadframe) | Takes out one message in receive order |
| [`XM_CanSubscribe`](#xm_cansubscribe) | Receives an ID into a latest-value mailbox (only in `Control_Setup()`) |
| [`XM_CanReadLatestById`](#xm_canreadlatestbyid) | Reads the latest value of a mailbox ID |
| [`XM_CanAgeMs`](#xm_canagems) | Time since a mailbox ID was last received [ms] |

**Helper functions** (anywhere)

| Function | One-line description |
|------|-----------|
| [`XM_CanIsIdAllowed`](#xm_canisidallowed) | Whether an ID can be sent in this mode |
| [`XM_CanIsDeviceIdAllowed`](#xm_canisdeviceidallowed) | Whether a number can be registered in the device API |
| [`XM_CanFitLength`](#xm_canfitlength) | Rounds a length up to one that fits on the bus |
| [`XM_CanSeqIsNewer`](#xm_canseqisnewer) | Whether an 8-bit sequence number is a new one |

**Types · constants**: [`XmCanResult_t`](#xmcanresult_t) · [`XmCanBusMode_t`](#xmcanbusmode_t) · [`XmCanBitTiming_t`](#xmcanbittiming_t) · [`XmCanFrame_t`](#xmcanframe_t) · [`XmCanDeviceStats_t`](#xmcandevicestats_t) · [`XmCanBusStats_t`](#xmcanbusstats_t) · [9 constants](#constants)

---

## Message IDs of the device API

When you register a number with `XM_CanAttachDevice()`, XM10 handles these messages for that number.

| Message | ID | Direction | Description |
|------|----|:---:|------|
| Periodic data 1 | `0x180` + number | device → XM10 | Received and kept as the latest value |
| Periodic data 2 | `0x280` + number | device → XM10 | Received and kept as the latest value |
| Alive | `0x700` + number | device → XM10 | One data byte: `0x05` means alive, `0x00` means a restart notice. Receiving `0x05` within 3 seconds (`XM_CAN_ALIVE_TIMEOUT_MS`) counts as connected |
| Command | `0x200` + number | XM10 → device | Sent with [`XM_CanSend`](#xm_cansend) |

- On one bus, every device must have a different number. XM10 cannot detect duplicates.
- A number you can register is any of 1 – 127 except the reserved numbers (`0x01` · `0x02` · `0x0C` · `0x0D` · `0x0F` · `0x10` · `0x11`) ([`XM_CanIsDeviceIdAllowed`](#xm_canisdeviceidallowed)).
- You can register at most `XM_CAN_MAX_DEVICES` (4) devices.

---

## Function details

### Bus mode

#### `XM_CanBusMode`

```c
XmCanBusMode_t XM_CanBusMode(void);
```

The function that sets the port's bus mode. **You write it yourself** (optional). If you do not write it, the port is in CAN-FD mode (`XM_CAN_BUS_FD`).

It is called once at power-on. Other XM functions are not ready yet during boot, so only return a value. If you choose classic, this port sends and receives classic CAN only, and the sensor hubs do not work on this port.

**Parameters**: none

**Return value**: [`XmCanBusMode_t`](#xmcanbusmode_t) — `XM_CAN_BUS_FD` or `XM_CAN_BUS_CLASSIC`

**⚠️ Call context**: The system calls it once at power-on. You do not call it yourself.

**Example**

```c
XmCanBusMode_t XM_CanBusMode(void) { return XM_CAN_BUS_CLASSIC; }
```

**See also**: [`XM_CanClassicBitTiming`](#xm_canclassicbittiming), [`XM_CanGetBusMode`](#xm_cangetbusmode)

---

#### `XM_CanGetBusMode`

```c
XmCanBusMode_t XM_CanGetBusMode(void);
```

Returns the current bus mode. It is `XM_CAN_BUS_UNAVAILABLE` if classic could not be set up or the port is broken.

**Parameters**: none

**Return value**: [`XmCanBusMode_t`](#xmcanbusmode_t)

**⚠️ Call context**: Can be called at any time.

---

#### `XM_CanClassicBitTiming`

```c
void XM_CanClassicBitTiming(XmCanBitTiming_t* timing);
```

The function that sets the classic CAN speed. **You write it yourself** (optional). If you do not write it, the speed is 1 Mbit/s.

If `XM_CanBusMode()` returns classic, it is called once at power-on. Other XM functions are not ready yet during boot, so only fill in the value. If XM10 starts with a wrong speed, it breaks the bus with error messages, so the speed is set before start. If the value is out of range, the port cannot be used (`XM_CAN_BUS_UNAVAILABLE`).

**Parameters**

| Name | Type | Description |
|------|------|------|
| `timing` | [`XmCanBitTiming_t*`](#xmcanbittiming_t) | Struct to fill in with the speed. The presets (`XM_CAN_TIMING_*`) are handy |

**Return value**: none

**⚠️ Call context**: The system calls it once at power-on. You do not call it yourself.

**Example**

```c
void XM_CanClassicBitTiming(XmCanBitTiming_t* timing)
{
    const XmCanBitTiming_t t = XM_CAN_TIMING_500K;
    *timing = t;
}
```

**See also**: [`XmCanBitTiming_t`](#xmcanbittiming_t), [`XM_CanSetClassicBitTiming`](#xm_cansetclassicbittiming)

---

#### `XM_CanSetClassicBitTiming`

```c
XmCanResult_t XM_CanSetClassicBitTiming(const XmCanBitTiming_t* t);
```

Changes the classic CAN speed while running. It does not wait. For most cases [`XM_CanClassicBitTiming`](#xm_canclassicbittiming) is enough.

Use it when a device with a different factory speed is set to the new speed and then moved along with the bus. Messages that were received but not yet read are lost during the change, and the error counters (`tec` · `rec`) stay as they are. If the value is the same, nothing stops and it returns `XM_CAN_OK`.

After you send a speed change to a device, do not send any other message — call this function right away. If the device moves first, an unanswered message can stay behind and the result can be `XM_CAN_BUSY` for a few seconds.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `t` | [`const XmCanBitTiming_t*`](#xmcanbittiming_t) | The new speed (`XM_CAN_TIMING_*` or a value you wrote) |

**Return value**: [`XmCanResult_t`](#xmcanresult_t)

- `XM_CAN_OK`: changed. The new speed applies from the next send.
- `XM_CAN_BUSY`: messages are still waiting to be sent (if you turned SYNC on, turn it off first), or `XM_CanOpenSdoRead` / `XM_CanOpenSdoWrite` is in progress, or there was a brief overlap. Call again next period.
- `XM_CAN_RECOVERY`: the bus is recovering.
- `XM_CAN_REJECTED`: `NULL`, a value out of range, not in classic mode, or the port is broken. If the port is broken, restart (`XM_CanGetBusMode()` returns `XM_CAN_BUS_UNAVAILABLE`).
- `XM_CAN_NOT_OWNER`: called outside `Control_Setup()` / `Control_Loop()`.

**⚠️ Call context**: Call it in `Control_Setup()` or `Control_Loop()`.

**See also**: The speed must be the same as every device on the bus. If it does not match, errors pile up and it looks like bus recovery (`tec` · `bus_status`).

---

#### `XM_CanGetClassicBitTiming`

```c
bool XM_CanGetClassicBitTiming(XmCanBitTiming_t* out);
```

Reads the current classic CAN speed.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `out` | [`XmCanBitTiming_t*`](#xmcanbittiming_t) | Struct that receives the speed |

**Return value**: `bool` — `true`: `out` is filled. `false`: not in classic mode, or the port is broken — `out` is unchanged.

**⚠️ Call context**: Call it in `Control_Setup()` or `Control_Loop()`.

---

### Device API

#### `XM_CanAttachDevice`

```c
bool XM_CanAttachDevice(uint8_t device_id);
```

Registers a device number. Once registered, XM10 receives and keeps that device's periodic data 1 (ID `0x180` + number), periodic data 2 (`0x280` + number) and alive signal (`0x700` + number). On one bus, every device must have a different number (XM10 cannot detect duplicates).

**Parameters**

| Name | Type | Description |
|------|------|------|
| `device_id` | `uint8_t` | Device number — a number for which [`XM_CanIsDeviceIdAllowed`](#xm_canisdeviceidallowed) is `true` |

**Return value**: `bool` — `true`: registered. `false`: the number cannot be used, it is already registered, there are no free slots (at most `XM_CAN_MAX_DEVICES`), or it was called outside `Control_Setup()`. In that case `bad_call_count` goes up by 1.

**⚠️ Call context**: Call it only in `Control_Setup()`.

**Example**

```c
#define MY_SENSOR_ID  (0x20U)

void Control_Setup(void) {
    XM_CanAttachDevice(MY_SENSOR_ID);
}
```

**See also**: [Message IDs of the device API](#message-ids-of-the-device-api), [`XM_CanReadLatest`](#xm_canreadlatest)

---

#### `XM_CanIsConnected`

```c
bool XM_CanIsConnected(uint8_t device_id);
```

Returns whether an alive signal (`0x05`) arrived within 3 seconds (`XM_CAN_ALIVE_TIMEOUT_MS`). After a restart notice (`0x00`), it stays `false` until the next `0x05`.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `device_id` | `uint8_t` | A registered device number |

**Return value**: `bool` — `true`: connected. `false`: the connection is lost, or no `0x05` has arrived since the restart notice.

**⚠️ Call context**: Call it in `Control_Setup()` or `Control_Loop()`.

---

#### `XM_CanReadLatest`

```c
bool XM_CanReadLatest(uint8_t device_id, uint8_t stream, XmCanFrame_t* out);
```

Reads the latest periodic data of that device.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `device_id` | `uint8_t` | A registered device number |
| `stream` | `uint8_t` | `1` = periodic data 1, `2` = periodic data 2 |
| `out` | [`XmCanFrame_t*`](#xmcanframe_t) | Struct that receives the value |

**Return value**: `bool`

- `true`: `out` has a value. Reading the same `seq` again also returns `true` — compare `seq` to know whether it is new. An old value is still read as `true` after the connection is lost, so judge with `tick_ms` and [`XM_CanIsConnected`](#xm_canisconnected).
- `false`: nothing received yet, no new value since the restart notice, it could not be read this time because a write was in progress, or an argument is invalid — `out` is unchanged.

**⚠️ Call context**: Call it in `Control_Setup()` or `Control_Loop()`.

**Example**

```c
static uint32_t s_last_seq;

void Control_Loop(void) {
    XmCanFrame_t f;
    if (XM_CanReadLatest(MY_SENSOR_ID, 1U, &f) && (f.seq != s_last_seq)) {
        s_last_seq = f.seq;                    /* a new message */
        if (XM_CanIsConnected(MY_SENSOR_ID)) {
            /* use f.data[0 .. f.len - 1] */
        }
    }
}
```

**See also**: [`XmCanFrame_t`](#xmcanframe_t)

---

#### `XM_CanSend`

```c
XmCanResult_t XM_CanSend(uint8_t device_id, const void* data, uint8_t len);
```

Sends a command to the device (ID `0x200` + number).

The length is 1 – 64 (1 – 8 in classic mode). If the length is not an allowed length (1 – 8 · 12 · 16 · 20 · 24 · 32 · 48 · 64), it is padded with zeros up to the next allowed length. It is accepted only while XM10 judges the device as "connected" from its recent alive signal (up to 3 seconds right after the connection drops, it can still be accepted).

An accepted command is retried by the hardware until it goes out, and XM10 does not resend it in software. A command that stayed in the hardware while the device was disconnected can go out when it reconnects. Use the first byte of the command as a sequence number so that the device can filter out duplicates and reordering ([`XM_CanSeqIsNewer`](#xm_canseqisnewer)). A sequence number is not an expiry time.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `device_id` | `uint8_t` | A registered device number |
| `data` | `const void*` | The data to send |
| `len` | `uint8_t` | Length [B], 1 – 64 (1 – 8 in classic mode) |

**Return value**: [`XmCanResult_t`](#xmcanresult_t)

- `XM_CAN_OK`: put in the send queue. It does not mean delivery is complete.
- `XM_CAN_NOT_CONNECTED` · `XM_CAN_BUSY` · `XM_CAN_RECOVERY` · `XM_CAN_REJECTED` · `XM_CAN_NOT_OWNER`: see the [`XmCanResult_t`](#xmcanresult_t) table for their meaning.

**⚠️ Call context**: Call it in `Control_Setup()` or `Control_Loop()`.

**Example**

```c
static uint8_t s_cmd_seq;

void Control_Loop(void) {
    uint8_t cmd[2] = { s_cmd_seq, 1U };       /* first byte = sequence number */
    if (XM_CanSend(MY_SENSOR_ID, cmd, sizeof(cmd)) == XM_CAN_OK) {
        s_cmd_seq++;
    }
}
```

**See also**: [`XM_CanIsConnected`](#xm_canisconnected), [`XM_CanSeqIsNewer`](#xm_canseqisnewer)

---

#### `XM_CanGetStats`

```c
bool XM_CanGetStats(uint8_t device_id, XmCanDeviceStats_t* out);
```

Reads the per-device statistics.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `device_id` | `uint8_t` | A registered device number |
| `out` | [`XmCanDeviceStats_t*`](#xmcandevicestats_t) | Struct that receives the statistics |

**Return value**: `bool` — `true`: filled. `false`: the number is not registered, or `out` is `NULL`.

**⚠️ Call context**: Call it in `Control_Setup()` or `Control_Loop()`.

---

#### `XM_CanGetBusStats`

```c
bool XM_CanGetBusStats(XmCanBusStats_t* out);
```

Reads the statistics for the whole port.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `out` | [`XmCanBusStats_t*`](#xmcanbusstats_t) | Struct that receives the statistics |

**Return value**: `bool` — `true`: filled. `false`: `out` is `NULL`.

**⚠️ Call context**: Call it in `Control_Setup()` or `Control_Loop()`.

---

#### `XM_CanEnableSync`

```c
void XM_CanEnableSync(bool on);
```

Sends the 1 kHz sync signal (SYNC, ID `0x080`) to your device too. It is off by default.

- FD mode: even when turned on, it does not send if no device that would accept it shows up within 3 seconds. If a sensor hub is running, SYNC keeps going out even when you turn it off — turning it off only removes your own request. SYNC is 1 byte (a counter).
- Classic mode: SYNC has length 0. While it is on, you cannot send ID `0x080` with `XM_CanSendFrame`.
- SYNC can pause for a moment or be missed now and then. Let the device use it only for aligning its sampling, and keep sending at its own period when SYNC is absent.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `on` | `bool` | `true`: on. `false`: off |

**Return value**: none

**⚠️ Call context**: Call it in `Control_Setup()` or `Control_Loop()`.

**See also**: [`XM_CanSendFrame`](#xm_cansendframe), `sync_skip_*` ([`XmCanBusStats_t`](#xmcanbusstats_t))

---

#### `XM_CanEnableHeartbeat`

```c
XmCanResult_t XM_CanEnableHeartbeat(uint8_t node_id, uint16_t period_ms);
```

Sends the XM10 heartbeat to your device — ID `0x700` + `node_id`, one data byte `0x05`. It is off by default.

It is sent right after `Control_Loop()` finishes, when the period has come. So when `Control_Loop()` does not run, no new heartbeat is sent — use this to let the other side stop by itself when the heartbeat is lost. A message that is already waiting to be sent can still go out after that, and whether the other side really stops depends on its own settings. Check it on the board. (not yet confirmed on the board) <!-- board-check -->

If it cannot be queued, it is tried again on the next tick, not the next period (the period counts from the last success). While the bus is recovering, the interval can get longer.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `node_id` | `uint8_t` | 1 – 127 — the number the other device is waiting for |
| `period_ms` | `uint16_t` | Send period 1 – 65535 [ms]. `0` = off (`node_id` is ignored) |

**Return value**: [`XmCanResult_t`](#xmcanresult_t)

- `XM_CAN_OK`: turned on, turned off, or the period was changed.
- `XM_CAN_REJECTED`: a number out of range, an unavailable mode, `0x02` or `0x0C` · `0x0D` · `0x0F` in FD mode, or that ID is already being received with `XM_CanAttachDevice` · `XM_CanSubscribe` · monitoring (`XM_CanOpenWatchNode`). The reverse order (turning the heartbeat on first and registering the receive later) is rejected too.
- `XM_CAN_NOT_OWNER`: called outside `Control_Setup()` / `Control_Loop()`.

**⚠️ Call context**: Call it in `Control_Setup()` or `Control_Loop()`.

**See also**: `heartbeat_sent_count` · `heartbeat_fail_count` · `heartbeat_last_fail` ([`XmCanBusStats_t`](#xmcanbusstats_t))

---

### Free send and receive

#### `XM_CanSendFrame`

```c
XmCanResult_t XM_CanSendFrame(uint16_t id, const void* data, uint8_t len);
```

Sends one message. It can be accepted even if no device receives it. Accepted is not delivered (the same rule as [`XM_CanSend`](#xm_cansend)).

**Parameters**

| Name | Type | Description |
|------|------|------|
| `id` | `uint16_t` | Message ID (11-bit). See the table below for the IDs you can send |
| `data` | `const void*` | The data to send. It may be `NULL` only when the length is 0 |
| `len` | `uint8_t` | Length [B]. FD mode 0 – 64 (padded with zeros up to the next allowed length), classic mode 0 – 8 |

**IDs you can send**

| Mode | IDs you can send |
|------|------|
| FD | Every ID from `0x000` to `0x7FF` except these three: ① IDs whose lower 7 bits are `0x0C` · `0x0D` · `0x0F` (the sensor hub slots) ② `0x080` (send SYNC with [`XM_CanEnableSync`](#xm_canenablesync)) ③ `0x702` (the XM10 system heartbeat). ID `0x000` is sent only when the length is 2 and the second byte is the target number (`0` = all; sensor hub numbers are rejected) |
| Classic | Every ID from `0x000` to `0x7FF`, except `0x080` while SYNC is on. Only 11-bit IDs work |

**Return value**: [`XmCanResult_t`](#xmcanresult_t) — `XM_CAN_OK`: put in the send queue. See the table for the other values.

**⚠️ Call context**: Call it in `Control_Setup()` or `Control_Loop()`.

**See also**: [`XM_CanIsIdAllowed`](#xm_canisidallowed), [`XM_CanFitLength`](#xm_canfitlength)

---

#### `XM_CanReadFrame`

```c
bool XM_CanReadFrame(XmCanFrame_t* out);
```

Takes out one message in receive order. It returns `false` if there is none.

Messages that no mailbox or device API has taken come into a queue of depth 64. IDs you did not register come in too (except the sensor hub slots in FD mode). When the queue is full, new messages are dropped and `rx_queue_drop_count` counts them. Empty the queue every period in `Control_Loop()`.

The bus order between different kinds of messages (alive · responses · error notices versus everything else) is not guaranteed.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `out` | [`XmCanFrame_t*`](#xmcanframe_t) | Struct that receives the message |

**Return value**: `bool` — `true`: took out one. `false`: the queue is empty.

**⚠️ Call context**: Call it in `Control_Setup()` or `Control_Loop()`.

**Example**

```c
XmCanFrame_t f;
while (XM_CanReadFrame(&f)) {
    /* use f.id, f.len, f.data */
}
```

**See also**: `rx_queue_drop_count` · `rx_queue_peak` ([`XmCanBusStats_t`](#xmcanbusstats_t))

---

#### `XM_CanSubscribe`

```c
bool XM_CanSubscribe(uint16_t id);
```

Receives this ID into a **latest-value mailbox** instead of the queue. It suits 1 kHz periodic data.

The mailbox has 16 slots, shared with the device API streams. These are rejected:

- An ID outside 11 bits
- The sensor hub slots in FD mode (lower 7 bits `0x0C` · `0x0D` · `0x0F`)
- The ID of a registered device
- An ID that is already registered
- An ID you send yourself with `XM_CanEnableHeartbeat`

**Parameters**

| Name | Type | Description |
|------|------|------|
| `id` | `uint16_t` | The message ID to receive into a mailbox |

**Return value**: `bool` — `true`: registered. `false`: rejected.

**⚠️ Call context**: Call it only in `Control_Setup()`.

**See also**: [`XM_CanReadLatestById`](#xm_canreadlatestbyid), [`XM_CanAgeMs`](#xm_canagems)

---

#### `XM_CanReadLatestById`

```c
bool XM_CanReadLatestById(uint16_t id, XmCanFrame_t* out);
```

Reads the latest value of a mailbox ID. The rules are the same as [`XM_CanReadLatest`](#xm_canreadlatest) — the same `seq` also returns `true`, and on failure `out` is unchanged.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `id` | `uint16_t` | An ID registered with `XM_CanSubscribe` |
| `out` | [`XmCanFrame_t*`](#xmcanframe_t) | Struct that receives the value |

**Return value**: `bool` — `true`: `out` has a value. `false`: could not read — `out` is unchanged.

**⚠️ Call context**: Call it in `Control_Setup()` or `Control_Loop()`.

---

#### `XM_CanAgeMs`

```c
uint32_t XM_CanAgeMs(uint16_t id);
```

Returns the time [ms] since a mailbox ID was last received. It returns `XM_CAN_AGE_NONE` if the ID was never received, 2^31 ms or more have passed, or the ID is not registered.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `id` | `uint16_t` | An ID registered with `XM_CanSubscribe` |

**Return value**: `uint32_t` — elapsed time [ms], or `XM_CAN_AGE_NONE` (`0xFFFFFFFF`)

**⚠️ Call context**: Call it in `Control_Setup()` or `Control_Loop()`.

---

### Helper functions (anywhere)

#### `XM_CanIsIdAllowed`

```c
bool XM_CanIsIdAllowed(uint16_t id);
```

Returns whether the ID can be sent under this mode's ID rules. It looks at the ID only. The data check of ID `0x000`, the rejection of `0x080` while SYNC is on, `XM_CAN_BUSY` and `XM_CAN_RECOVERY` are reported separately by [`XM_CanSendFrame`](#xm_cansendframe).

**Parameters**

| Name | Type | Description |
|------|------|------|
| `id` | `uint16_t` | Message ID |

**Return value**: `bool` — `true`: the ID can be sent.

**⚠️ Call context**: Can be called from anywhere.

---

#### `XM_CanIsDeviceIdAllowed`

```c
bool XM_CanIsDeviceIdAllowed(uint8_t id);
```

Returns whether the number can be registered in the device API. It is any of 1 – 127 except the reserved numbers (`0x01` · `0x02` · `0x0C` · `0x0D` · `0x0F` · `0x10` · `0x11`).

**Parameters**

| Name | Type | Description |
|------|------|------|
| `id` | `uint8_t` | Device number |

**Return value**: `bool` — `true`: the number can be registered.

**⚠️ Call context**: Can be called from anywhere.

---

#### `XM_CanFitLength`

```c
uint8_t XM_CanFitLength(uint8_t len);
```

Rounds a length up to one that fits on the bus (0 stays 0). If the length cannot be carried in this mode, it returns `XM_CAN_LENGTH_INVALID`.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `len` | `uint8_t` | Data length [B] |

**Return value**: `uint8_t` — the length that goes on the bus [B]. `XM_CAN_LENGTH_INVALID` (`0xFF`) if it cannot be carried

**⚠️ Call context**: Can be called from anywhere.

---

#### `XM_CanSeqIsNewer`

```c
bool XM_CanSeqIsNewer(uint8_t last, uint8_t now);
```

Returns whether an 8-bit sequence number is a new one. If `now` is 1 – 127 ahead of `last`, it is new. A difference of 0 or 128 – 255 means a duplicate or an old value.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `last` | `uint8_t` | The sequence number received last time |
| `now` | `uint8_t` | The sequence number received this time |

**Return value**: `bool` — `true`: it is a new value.

**⚠️ Call context**: Can be called from anywhere.

**See also**: [`XM_CanSend`](#xm_cansend)

---

## Types / constants

### `XmCanResult_t`

The result of every "send" function. **Always check it with `== XM_CAN_OK`.**

| Value | Name | Meaning |
|----|------|------|
| 0 | `XM_CAN_OK` | Put in the send queue (accepted). It does not mean the other side received it |
| 1 | `XM_CAN_BUSY` | Could not be queued this time (queue full · overlap with another send) — try again next period |
| 2 | `XM_CAN_RECOVERY` | The bus is recovering (after a bus error · clearing blocked sends) — try again shortly |
| 3 | `XM_CAN_NOT_CONNECTED` | (device API only) The device is not connected |
| 4 | `XM_CAN_REJECTED` | An ID, length or argument that cannot be sent, or not supported in this mode |
| 5 | `XM_CAN_NOT_OWNER` | Called outside `Control_Setup()` / `Control_Loop()` |

---

### `XmCanBusMode_t`

The bus mode of the CAN-FD #2 port. It is set once at power-on ([`XM_CanBusMode`](#xm_canbusmode)).

| Value | Name | Meaning |
|----|------|------|
| 0 | `XM_CAN_BUS_FD` | Default — CAN-FD (1 / 5 Mbit/s). Can be used together with the IMU · EMG · FES hubs |
| 1 | `XM_CAN_BUS_CLASSIC` | Classic CAN only (1 Mbit/s by default — [`XM_CanClassicBitTiming`](#xm_canclassicbittiming)). For motor drivers and the like. Do not connect a hub to this port |
| 2 | `XM_CAN_BUS_UNAVAILABLE` | (read-only) Classic was requested but could not be set up. Nothing is sent from this port |

---

### `XmCanBitTiming_t`

The classic CAN bit timing. Speed = 80 MHz ÷ `prescaler` ÷ (1 + `seg1` + `seg2`), and sample position = (1 + `seg1`) ÷ (1 + `seg1` + `seg2`). Use values that match the other device.

```c
typedef struct {
    uint16_t prescaler;
    uint16_t seg1;
    uint8_t  seg2;
    uint8_t  sjw;
} XmCanBitTiming_t;
```

| Field | Type | Description |
|------|------|------|
| `prescaler` | `uint16_t` | Divider 1 – 512 |
| `seg1` | `uint16_t` | Segment before the sample point 1 – 256 |
| `seg2` | `uint8_t` | Segment after the sample point 1 – 128 |
| `sjw` | `uint8_t` | Resynchronization width 1 – `seg2` |

Four presets can be used as the initial value of an `XmCanBitTiming_t`. The sample position is 87.5 % for all of them.

| Preset | Speed | `prescaler` | `seg1` | `seg2` | `sjw` |
|------|------|:---:|:---:|:---:|:---:|
| `XM_CAN_TIMING_1M` | 1 Mbit/s (default) | 5 | 13 | 2 | 2 |
| `XM_CAN_TIMING_500K` | 500 kbit/s | 10 | 13 | 2 | 2 |
| `XM_CAN_TIMING_250K` | 250 kbit/s | 20 | 13 | 2 | 2 |
| `XM_CAN_TIMING_125K` | 125 kbit/s | 40 | 13 | 2 | 2 |

---

### `XmCanFrame_t`

One received message.

```c
typedef struct {
    uint32_t seq;
    uint32_t tick_ms;
    uint16_t id;
    uint8_t  len;
    uint8_t  data[XM_CAN_MAX_DATA];
} XmCanFrame_t;
```

| Field | Type | Description |
|------|------|------|
| `seq` | `uint32_t` | **Queue**: the order number in which the message entered the queue. Messages dropped on overflow also use a number, so a missing number is a dropped message. **Latest value (device API · mailbox)**: goes up by 1 every time that ID is received. If it is the same as last time, it is not a new message |
| `tick_ms` | `uint32_t` | Time of reception (based on `XM_GetTick`, 1 ms) |
| `id` | `uint16_t` | Message ID (11-bit) |
| `len` | `uint8_t` | Length carried on the bus [B]. FD: 0 – 8 · 12 · 16 · 20 · 24 · 32 · 48 · 64 / classic: 0 – 8 |
| `data` | `uint8_t[64]` | Data (`XM_CAN_MAX_DATA`) |

---

### `XmCanDeviceStats_t`

Per-device statistics (registered devices only). Read them with [`XM_CanGetStats()`](#xm_cangetstats). Each value is consistent on its own, but the values are not from the same instant.

| Field | Type | Description |
|------|------|------|
| `rx_count[2]` | `uint32_t` | Number of periodic data 1 and 2 received |
| `rx_busy_drop_count` | `uint32_t` | Number dropped because it could not be written while being read |
| `unread_overwrite_count` | `uint32_t` | Number of times a new value overwrote one that was not yet read |
| `alive_count` | `uint32_t` | Number of alive signals (`0x05`) received |
| `alive_invalid_count` | `uint32_t` | Number of other values or empty messages that arrived in the alive slot |
| `reboot_count` | `uint32_t` | Number of restart notices (`0x00`) received |
| `last_rx_tick` | `uint32_t` | Time the last message from this device was received |
| `last_alive_tick` | `uint32_t` | Time the last alive signal (`0x05`) was received |
| `send_accept_count` | `uint32_t` | Number of times `XM_CanSend` was accepted |
| `send_fail_count` | `uint32_t` | Number of times `XM_CanSend` failed |
| `read_busy_count` | `uint32_t` | Number of times `XM_CanReadLatest` could not read this time because a write was in progress |
| `last_send_fail` | `uint8_t` | Result of the last failed `XM_CanSend` (`XmCanResult_t`). 0 = none yet |

---

### `XmCanBusStats_t`

Statistics for the whole port. Read them with [`XM_CanGetBusStats()`](#xm_cangetbusstats). Each value is consistent on its own, but the values are not from the same instant.

| Field | Type | Description |
|------|------|------|
| `bad_call_count` | `uint32_t` | Unregistered number · invalid argument · registration outside `Control_Setup()` |
| `owner_violation_count` | `uint32_t` | Number of calls from another task or an interrupt |
| `flush_request_count` | `uint32_t` | Number of times canceling waiting messages was "requested" because no device would take them. Not the number completed |
| `busoff_flush_count` | `uint32_t` | Number of waiting messages cleared after a bus error (bus-off) |
| `tx_pending_count` | `uint32_t` | Number of sends still in the hardware right now (at the moment of the call) |
| `sync_skip_count` | `uint32_t` | Number of ticks in which SYNC could not be sent — the sum of the four reasons below |
| `sync_skip_busy` | `uint32_t` | Overlap with another send · send queue full |
| `sync_skip_hub_setup` | `uint32_t` | While a sensor hub was being set up |
| `sync_skip_recovery` | `uint32_t` | While the bus was recovering |
| `sync_skip_clear` | `uint32_t` | (classic) While waiting sends are cleared after a device request ended in a timeout |
| `rx_frame_count` | `uint32_t` | Number of messages that came into this API |
| `rx_queue_drop_count` | `uint32_t` | Number dropped because the receive-order queue was full |
| `rx_queue_last_seq` | `uint32_t` | `seq` of the last message sent to the queue. After you empty the queue, compare it with the drop count |
| `rx_queue_peak` | `uint32_t` | Maximum number that was ever waiting in the queue |
| `rejected_id_count` | `uint32_t` | Number of attempts to send an ID that is not allowed |
| `classic_hub_tx_refused_count` | `uint32_t` | (classic · unavailable) Number of hub sends that XM10 blocked from going out of this port |
| `timing_write_fail_count` | `uint32_t` | Number of times `XM_CanSetClassicBitTiming` could not write the speed |
| `heartbeat_sent_count` | `uint32_t` | Number of heartbeats from `XM_CanEnableHeartbeat` that were accepted |
| `heartbeat_fail_count` | `uint32_t` | Number of heartbeats that could not be queued and were put off to the next period |
| `heartbeat_last_fail` | `uint8_t` | Result of the last failed heartbeat (`XmCanResult_t`). 0 = none yet |
| `bus_mode` | `uint8_t` | The current bus mode (`XmCanBusMode_t`) |
| `tec` | `uint8_t` | Transmit error counter (at the moment of the call) |
| `rec` | `uint8_t` | Receive error counter (at the moment of the call) |
| `bus_status` | `uint8_t` | bit0 = bus-off · bit1 = error warning · bit2 = error passive |

---

### Constants

| Name | Value | Description |
|------|----|------|
| `XM_CAN_MAX_DEVICES` | `4U` | Number of devices you can register. It does not guarantee that this many can run fast at the same time |
| `XM_CAN_MAX_DATA` | `64U` | Maximum data bytes in one message (8 in classic mode) |
| `XM_CAN_ALIVE_TIMEOUT_MS` | `3000U` | If no alive signal arrives for this long, the device counts as "lost" [ms] |
| `XM_CAN_LENGTH_INVALID` | `0xFFU` | Returned by `XM_CanFitLength` — a length that cannot be carried in this mode |
| `XM_CAN_AGE_NONE` | `0xFFFFFFFFUL` | Returned by `XM_CanAgeMs` — never received · too old · not registered |
| `XM_CAN_TIMING_1M` | `{ 5U, 13U, 2U, 2U}` | Classic CAN 1 Mbit/s (default) |
| `XM_CAN_TIMING_500K` | `{10U, 13U, 2U, 2U}` | Classic CAN 500 kbit/s |
| `XM_CAN_TIMING_250K` | `{20U, 13U, 2U, 2U}` | Classic CAN 250 kbit/s |
| `XM_CAN_TIMING_125K` | `{40U, 13U, 2U, 2U}` | Classic CAN 125 kbit/s |

---

## Rev1.1 / Rev2.0 Difference Summary

| Item | Rev1.1 | Rev2.0 |
|------|--------|--------|
| CAN bus API (`xm_api_canbus.h`) | ❌ Absent | 🟢 Exclusive |

---

## Related documents

- [11. CAN Communication (concept)](../11-canbus.en.md) — order of use, wiring, the message agreement on the device side, common mistakes
- [`xm_api_canopen.h` — CANopen Device Helper Functions](xm-api-canopen.en.md) — NMT, heartbeat monitoring and SDO in classic mode
- [device-examples](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/device-examples/) — partner devices for trying the examples with your own device or a PC
