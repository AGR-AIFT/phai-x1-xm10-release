# API Reference: CAN Communication — Talk to Your Own Device

> 📌 **After reading this page** you will be able to connect your own CAN-FD or classic CAN device to the CAN-FD #2 port of the XM10, receive its data and send it commands.
> ⏱️ Estimated reading time: 30 minutes
> 🧰 Prerequisites: Sending a graph to the PC in [Ex.41](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/41_IMU_Hub_Dashboard/)
> 🎯 Key functions: `XM_CanAttachDevice` / `XM_CanReadLatest` / `XM_CanSend` / `XM_CanSendFrame` / `XM_CanReadFrame`
>
> 🟢 **Rev 2.0 only** *(new in v2.10.0)*: The Rev 1.1 SDK does not have `xm_api_canbus.h`.

This page is a guide to the **CAN communication API** defined in `xm_api_canbus.h`. For the signature and return value of every function, see the [`xm_api_canbus.h` reference](ref/xm-api-canbus.en.md). This page covers **how to use it**.

> ℹ️ **Board status**: The behavior below is what the API defines. We have not yet confirmed on a board the whole flow of connecting your own device to the XM10 and exchanging data. (not yet confirmed on the board) <!-- board-check -->

---

## 1. Two Ways

| Way | When to use it | Functions |
|------|------|------|
| **Device API** | A device you build yourself. Decide a device number, and XM10 receives that device's periodic data and alive signal for you | `XM_CanAttachDevice` · `XM_CanReadLatest` · `XM_CanSend` · `XM_CanIsConnected` |
| **Free send and receive** | A device that already uses a fixed protocol (such as a motor driver). You send and receive with just an ID and data | `XM_CanSendFrame` · `XM_CanReadFrame` · `XM_CanSubscribe` · `XM_CanReadLatestById` |

You may use both together. The protocol, the device setup and the start-up order are decided in your own code. XM10 blocks only the IDs that have a reason to be blocked.

---

## 2. Port and Bus Mode

Connect your device to the **CAN-FD #2 port**. It is the same port that the sensor hubs (IMU Hub · EMG Hub) use. The bus mode of the port is set once, at power-on.

| Mode | Speed | Can be used together with | How to set it |
|------|------|------|------|
| **CAN-FD** (default) | Arbitration 1 Mbit/s · data 5 Mbit/s, sample point 75 % | Sensor hubs + your CAN-FD device | If you do nothing, it is FD |
| **Classic CAN** | 1 Mbit/s by default (presets 500 · 250 · 125 kbit/s), sample point 87.5 % | Classic CAN devices only — **the sensor hubs do not work** | Write `XM_CanBusMode()` in your code and return `XM_CAN_BUS_CLASSIC` |

```c
XmCanBusMode_t XM_CanBusMode(void) { return XM_CAN_BUS_CLASSIC; }   // called once at power-on — only return a value

void XM_CanClassicBitTiming(XmCanBitTiming_t* timing)               // (optional) classic speed
{
    const XmCanBitTiming_t t = XM_CAN_TIMING_500K;
    *timing = t;
}
```

- You can change the classic speed while running with `XM_CanSetClassicBitTiming()`. Use it when you set a device with a different factory speed to the new speed and then move the bus along with it.
- Read the current mode with `XM_CanGetBusMode()`. If classic could not be set up, it is `XM_CAN_BUS_UNAVAILABLE`, and nothing is sent from this port.
- **Wiring** — Connect CAN_H · CAN_L · GND of the device transceiver to the pins of the same name on the port. The bus needs a 120 Ω termination resistor at both ends. The XM10 board already has one, so put one at the device end. [Hardware — CAN-FD Ports](../hardware/README.en.md#can-fd-ports)

---

## 3. Numbers at a Glance

XM10 tells devices apart by a number n (1 – 127). Each message ID is the start value of its kind plus n — periodic data 1 `0x180 + n` · periodic data 2 `0x280 + n` · command `0x200 + n` · alive `0x700 + n`. Seven numbers are used or reserved by XM10 and the sensor hubs, so you cannot give them to your device.

This ID layout has the same shape as the default ID layout of CANopen (CiA 301). The device API only shares the layout and is not CANopen — handle CANopen devices in classic mode with the helper functions in [Working with CANopen Devices](12-canopen.en.md).

| Number n | Used by | Device API registration | FD mode free send and receive | Classic mode free send and receive | Related |
|------|------|:---:|------|------|------|
| `0x01` | KIT H10 control module (CM, CAN-FD #1) — fixed | ✗ | ✓ | ✓ | [KIT H10 Control](02-h10-control-n-data.en.md) |
| `0x02` | XM10 — fixed | ✗ | ✓ — only `0x702` (XM10 heartbeat) is ✗ | ✓ | [6. Sync Signal and the XM10 Heartbeat](#6-sync-signal-and-the-xm10-heartbeat) |
| `0x0C` | Reserved | ✗ | ✗ — IDs whose lower 7 bits are this number can be neither sent nor received (the same rule as the hub numbers) | ✓ | — |
| `0x0D` | IMU Hub — fixed | ✗ | ✗ — same as above (e.g. `0x18D` · `0x70D`) | ✓ — the hub does not work | [Ex.41](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/41_IMU_Hub_Dashboard/) · [Ex.48](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/48_IMU_Hub_Calibration/) · [IMU Hub Sensor Calibration](13-imu-calibration.en.md) |
| `0x0F` | EMG Hub — fixed | ✗ | ✗ — same as above | ✓ — the hub does not work | [Ex.42](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/42_EMG_Hub_Biofeedback/) |
| `0x10` · `0x11` | Reserved | ✗ | ✓ | ✓ | — |
| The other 120 — `0x03` – `0x0B` · `0x0E` · `0x12` – `0x7F` | **Your devices** | ✓ (up to 4) | ✓ | ✓ | [Ex.45](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/45_CANFD_My_Sensor/) · [device examples](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/device-examples/) · classic: [Ex.46](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/46_CANopen_Drive/) · [Ex.47](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/47_CANopen_Drive_PDO/) |

- **Fixed numbers cannot be changed.** Pick your device numbers from the 120 "your devices" numbers and check them with `XM_CanIsDeviceIdAllowed(n)` (0 means "all devices", so it is not used). Give each device on the same bus a different number — XM10 cannot detect a duplicate.
- In classic mode the sensor hubs do not work, so every ID can be sent and received with free send and receive, and the CANopen helper functions accept every node number from 1 to 127. Only the numbers you can register in the device API stay the same in both modes (the 120).
- Two IDs are fixed regardless of number. `0x080` (sync signal) — in FD mode it goes out only through `XM_CanEnableSync()`; in classic mode you can send it yourself, except while the sync signal is on. `0x000` — in FD mode it is sent only when the length is 2 and the second byte is the target number (not target 0 and not a hub slot number). In classic mode there is no restriction.

---

## 4. Device API

### Device number

Pick one number n for each device and register it with `XM_CanAttachDevice(n)` in `Control_Setup()`. You can register up to 4 devices. The numbers you can use are the 120 "your devices" numbers in [3. Numbers at a Glance](#3-numbers-at-a-glance); check them with `XM_CanIsDeviceIdAllowed()`.

If two devices on the same bus use the same number, XM10 cannot detect it. Give each device a different number.

### Messages

| Message | Direction | ID | Content |
|------|------|:---:|------|
| Periodic data 1 | device → XM10 | `0x180 + n` | Data you define (up to 64 B in FD · 8 B in classic) |
| Periodic data 2 | device → XM10 | `0x280 + n` | A second set of data |
| Alive | device → XM10 | `0x700 + n` | One byte, `0x05`. Once at power-on, `0x00` (restart notice, optional) |
| Command | XM10 → device | `0x200 + n` | Sent with `XM_CanSend()` |

### Receiving — one latest value

```c
XmCanFrame_t f;
if (XM_CanReadLatest(n, 1, &f) && (f.seq != s_last_seq)) {   // latest value of periodic data 1 — only when it is new
    s_last_seq = f.seq;
    /* f.data[0 .. f.len-1] */
}
bool connected = XM_CanIsConnected(n);                        // did 0x05 arrive within 3 seconds?
```

- XM10 keeps only **one latest value** per device. Reading the same value again also returns `true`, so use `seq` to tell whether it is new.
- An old value is still read as `true` after the connection is lost. Check the connection separately with `XM_CanIsConnected()`.
- When a restart notice (`0x00`) arrives, the values kept so far are discarded, and the device counts as "disconnected" until the next `0x05`.

### Sending — commands

```c
const uint8_t cmd[2] = { s_seq, led };
if (XM_CanSend(n, cmd, sizeof(cmd)) == XM_CAN_OK) {   // XM_CAN_OK is 0 — always compare with ==
    s_seq++;                                          // raise the sequence number only when accepted
}
```

- `XM_CAN_OK` means **put in the send queue**. It does not mean the device received it.
- It is accepted only while XM10 sees the device as "connected". A command that stayed in the hardware while the device was disconnected can go out when it reconnects — **use the first byte of the command as a sequence number** so that the device can filter out duplicates and reordering. On the device side, judge with the same rule as `XM_CanSeqIsNewer()` (1 – 127 ahead means a new value).

---

## 5. Free Send and Receive

### Sending

```c
uint8_t d[8] = { /* … */ };
if (XM_CanIsIdAllowed(0x321)) {
    (void)XM_CanSendFrame(0x321, d, sizeof(d));
}
```

| Mode | IDs you can send |
|------|------|
| CAN-FD | Every ID from `0x000` to `0x7FF` except IDs whose lower 7 bits are `0x0C` · `0x0D` · `0x0F` (the sensor hub slots), `0x080` and `0x702`. `0x000` only when the length is 2 and the second byte is the target number (not 0 and not a hub slot number) |
| Classic | Every ID from `0x000` to `0x7FF` (except `0x080` while the sync signal is on) |

For the same rules by number, see [3. Numbers at a Glance](#3-numbers-at-a-glance).

The length is 0 – 64 in FD (padded with zeros up to the next allowed length) and 0 – 8 in classic. You can see the length that will go on the bus beforehand with `XM_CanFitLength()`. Only 11-bit IDs work, and remote frames are not supported.

### Receiving — receive-order queue and latest-value mailbox

| Way | Function | Good for |
|------|------|------|
| Receive-order queue (depth 64) | `XM_CanReadFrame()` — empty it with a `while` every period | Responses · messages that come now and then. When it is full, new messages are dropped and counted in `rx_queue_drop_count` |
| Latest-value mailbox (16 slots, shared with the device API) | `XM_CanSubscribe(id)` in `Control_Setup()` → `XM_CanReadLatestById()` | 1 kHz periodic data. The time since the last reception is `XM_CanAgeMs()` |

```c
XmCanFrame_t f;
while (XM_CanReadFrame(&f)) {
    /* f.id · f.len · f.data — a missing number in f.seq = messages dropped because the queue was full */
}
```

---

## 6. Sync Signal and the XM10 Heartbeat

| Function | What it does |
|------|------|
| `XM_CanEnableSync(true)` | Sends the 1 kHz sync signal (ID `0x080`) to your device too. In FD it is 1 B (a counter); in classic the length is 0. In FD mode it is not sent if no device that would accept it shows up within 3 seconds. It can pause for a moment or be missed, so let the device use it only for aligning its sampling, and keep sending at its own period when it is absent |
| `XM_CanEnableHeartbeat(node, period_ms)` | Sends the XM10 heartbeat (ID `0x700 + node`, `0x05`). It is sent when `Control_Loop()` finishes, so when the loop stops, the heartbeat stops too — use it to make the other device stop by itself when the heartbeat is lost (depends on the other device's settings · check on the board). `0` turns it off |

---

## 7. Rules for One Port

- Do not connect classic devices and CAN-FD devices together.
- All devices must use the same speed.
- Two devices must not send the same ID — do not let the numbers overlap.
- There is a limit to how much you can send — add up the message times and calculate whether they fit within the bus time.
- The data rate is 5 Mbit/s at most. CAN XL is not supported.
- If you send the same ID while the earlier message is still waiting, the order can change — send after the earlier message has gone out.

---

## 8. Result Values and Where to Call

| Result (`XmCanResult_t`) | Meaning | What to do |
|------|------|------|
| `XM_CAN_OK` (0) | Put in the send queue | — |
| `XM_CAN_BUSY` | Could not be queued this time (queue full · overlap with another send) | Try again next period |
| `XM_CAN_RECOVERY` | The bus is recovering | Try again shortly |
| `XM_CAN_NOT_CONNECTED` | (device API) The device is not connected | Check the connection |
| `XM_CAN_REJECTED` | An ID, length or argument that cannot be sent, or not supported in this mode | Check the rules |
| `XM_CAN_NOT_OWNER` | Called outside `Control_Setup()` · `Control_Loop()` | Move the call |

- Call every function only inside `Control_Setup()` or `Control_Loop()` (the registration functions only inside `Control_Setup()`). No function waits.
- `XM_CanGetBusMode` · `XM_CanIsIdAllowed` · `XM_CanIsDeviceIdAllowed` · `XM_CanFitLength` · `XM_CanSeqIsNewer` can be called from anywhere.

---

## 9. Finding Causes with Statistics

`XM_CanGetStats(n, &s)` (per device) · `XM_CanGetBusStats(&b)` (whole port). Each value is consistent on its own, but the values are not from the same instant.

| What you see | Meaning |
|------|------|
| `s.unread_overwrite_count` goes up | A new value replaced it before you read it — only the latest value is kept, so this is normal with a fast device |
| `s.last_send_fail` | Result of the last failed `XM_CanSend` (0 = none yet) |
| `b.tx_pending_count` stays large | No device is accepting the messages, or the bus is busy |
| `b.bus_status` | bit0 bus-off · bit1 error warning · bit2 error passive — check the speed · wiring · termination resistor |
| `b.rx_queue_drop_count` | Number dropped because the receive-order queue was full — empty the queue every period |
| `b.bad_call_count` · `b.owner_violation_count` | Invalid arguments · unregistered numbers · calls from the wrong place |

---

## 10. Partner Devices and Examples

| What | Link |
|------|------|
| Talk to your own device | [Ex.45 CAN-FD My Sensor](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/45_CANFD_My_Sensor/) |
| Classic CAN · CANopen drivers | [Ex.46](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/46_CANopen_Drive/) · [Ex.47](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/47_CANopen_Drive_PDO/) · [Working with CANopen Devices](12-canopen.en.md) |
| Reference device firmware (NUCLEO-G474RE + CAN-FD transceiver) | [canfd-sensor-stm32g474](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/device-examples/canfd-sensor-stm32g474/) |
| Try it on a PC without a device | [canfd-sensor-pc-sim](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/device-examples/canfd-sensor-pc-sim/) · [canopen-pc-sim](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/device-examples/canopen-pc-sim/) |
| Every function in detail | [`xm_api_canbus.h` reference](ref/xm-api-canbus.en.md) |
