# `xm_api_imu_calib.h` — Calibrate IMU Hub Sensors in Place

> 🟢 **Rev 2.0 only** — This header is for XM10 Rev 2.0. You cannot use the functions below from a Rev 1.1 project (`XM10_SDK/Rev1.1/`). To check your board revision, see [Board Revision Comparison](../../hardware/README.en.md#board-revision-comparison).
>
> - **Header**: `XM_FW/XM_API/xm_api_imu_calib.h` (you get it together with `xm_api.h`)
> - **Related concept doc**: [13. IMU Hub Sensor Calibration](../13-imu-calibration.en.md)
> - **Related example**: [48_IMU_Hub_Calibration](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/48_IMU_Hub_Calibration/)
> - **Board check**: The behavior below is what the API defines; it has not been confirmed on a board yet. (not yet confirmed on the board) <!-- board-check -->
>
> ℹ️ IMU Hub is a module under internal development. To use it, please contact us at <https://huphailab.com/contact>.

This header defines the API for **calibrating the sensors of an IMU Hub connected over CAN-FD, in place**. You calibrate a sensor that is built into a case or clothing without taking it out. A magnetometer calibration is correct only if you rotate the sensor while it is mounted as it is.

---

## When to use it

Use it when you want to calibrate the gyro zero, the magnetometer and the accelerometer while the sensors stay mounted in a case or clothing. For the order of use, how to hold the sensor and common mistakes, we recommend reading [13. IMU Hub Sensor Calibration](../13-imu-calibration.en.md) first. This page only covers the signatures and parameters of **every function, type and constant** declared in the header.

---

## How to use it

1. Call `XM_ImuCalibStart(channel, item)` **only once**. Call it from a place where something happens only once, such as the moment a button is pressed. If you call it every period, the calibration restarts each time it ends.
2. Look at the state with `XM_ImuCalibGetStatus()`. The function does not wait, so you may call it in `Control_Loop()`.
3. For the magnetometer and the full accelerometer calibration, when the state becomes `XM_IMU_CALIB_WAIT_USER`, move the sensor and then call `XM_ImuCalibFinish()`.
4. If the state is `XM_IMU_CALIB_DONE`, it is finished. If it is `XM_IMU_CALIB_FAILED`, look at `result`.

The start request is sent to the IMU Hub only once, to stop the same calibration from running twice. If no answer comes, the state stays at `XM_IMU_CALIB_PENDING`, and a further request during that time returns `XM_IMU_CALIB_REQ_BUSY`. It is released when the IMU Hub reports its state again or the connection is lost.

---

## Things to know

- **You cannot start in control mode.** Calling it in control mode (`XM_CTRL_CONTROL`) does not start. Switch to `XM_CTRL_MONITOR` and then start.
- **Do not control with a channel that is being calibrated.** While a channel is being calibrated, its `XM.status.imu_hub.connected_mask` bit is off. After it ends, the bit comes back on when new data arrives. If the connection drops during calibration and comes back, the bit stays off until you check the IMU Hub state. If you drive an external driver directly with `XM_CanSend`, always check this bit.
- **You can abort only until you call `XM_ImuCalibFinish()`.** This applies to the magnetometer and the full accelerometer calibration. The gyro zero and the simple accelerometer calibration finish quickly, so they cannot be aborted.
- **With EBIMU, aborting briefly cuts the power.** When you abort an EBIMU magnetometer or full accelerometer calibration, or when its moving phase runs past its time limit, the power of all channels goes off for about 2 seconds and then comes back. During that time the other channels drop too, and each channel looks reconnected only after new data arrives for it. If another sensor is saving its values, it is waited for first, so it can take about 17 seconds until the abort result comes.
- **The moving phase has a time limit.** It is up to 120 seconds for the magnetometer and 600 seconds for the full accelerometer calibration. If it is exceeded, the calibration ends with `XM_IMU_CALIB_RESULT_SESSION_TIMEOUT`.
- **The MTi-630 supports only the gyro zero and the magnetometer.**
- **Do not use it together with the PC program.** While you calibrate from the PC program, do not use these functions at the same time. The later request erases the earlier one.

---

## Calling rules

No function waits.

| Kind | Functions | Where to call |
|------|------|-----------|
| All (6) | `XM_ImuCalibStart` · `XM_ImuCalibFinish` · `XM_ImuCalibAbort` · `XM_ImuCalibGetStatus` · `XM_ImuCalibQuerySupported` · `XM_ImuCalibGetSupported` | `Control_Setup()` · `Control_Loop()` · a background task (`xm_api_freertos.h`). **Do not call them from an interrupt** |

---

## Function list

**Start · finish · abort**

| Function | One-line description |
|------|-----------|
| [`XM_ImuCalibStart`](#xm_imucalibstart) | Starts a calibration (cannot start in control mode) |
| [`XM_ImuCalibFinish`](#xm_imucalibfinish) | Ends the moving phase and saves the values |
| [`XM_ImuCalibAbort`](#xm_imucalibabort) | Aborts a calibration |

**State**

| Function | One-line description |
|------|-----------|
| [`XM_ImuCalibGetStatus`](#xm_imucalibgetstatus) | Reads the calibration state of a channel |

**Supported items**

| Function | One-line description |
|------|-----------|
| [`XM_ImuCalibQuerySupported`](#xm_imucalibquerysupported) | Asks the IMU Hub which items each channel supports |
| [`XM_ImuCalibGetSupported`](#xm_imucalibgetsupported) | Reads the supported items |

**Types · macro**: [`XmImuCalibOp_t`](#xmimucalibop_t) · [`XmImuCalibRequest_t`](#xmimucalibrequest_t) · [`XmImuCalibState_t`](#xmimucalibstate_t) · [`XmImuCalibResult_t`](#xmimucalibresult_t) · [`XmImuCalibStatus_t`](#xmimucalibstatus_t) · [`XM_IMU_CALIB_OP_BIT`](#xm_imu_calib_op_bit)

---

## Function details

### Start · finish · abort

#### `XM_ImuCalibStart`

```c
XmImuCalibRequest_t XM_ImuCalibStart(uint8_t ch_mask, XmImuCalibOp_t op);
```

Starts a calibration. Call it **only once** — call it from a place where something happens only once, such as the moment a button is pressed. If you call it every period, the calibration restarts each time it ends.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `ch_mask` | `uint8_t` | bit0 – bit5 = channel 0 – 5. You can start several channels at once. A channel that is already running is left as it is |
| `op` | [`XmImuCalibOp_t`](#xmimucalibop_t) | The calibration item |

**Return value**: [`XmImuCalibRequest_t`](#xmimucalibrequest_t) — whether the request was accepted. Accepted does not mean the calibration is finished. Look at the result with [`XM_ImuCalibGetStatus`](#xm_imucalibgetstatus).

**⚠️ Call context**: Call it in `Control_Setup()`, `Control_Loop()` or a background task. Do not call it from an interrupt. You cannot start in control mode (`XM_CTRL_CONTROL`) (`XM_IMU_CALIB_REQ_CONTROL_ACTIVE`).

**Example**

```c
#define CALIB_CH0  (0x01U)    /* ch_mask: bit0 = channel 0 */

/* from a place where something happens only once (for example, a button press) */
XmImuCalibRequest_t req = XM_ImuCalibStart(CALIB_CH0, XM_IMU_CALIB_OP_MAG);
if (req != XM_IMU_CALIB_REQ_OK) {
    /* see the XmImuCalibRequest_t table for the reason */
}
```

**See also**: [How to use it](#how-to-use-it), [Things to know](#things-to-know)

---

#### `XM_ImuCalibFinish`

```c
XmImuCalibRequest_t XM_ImuCalibFinish(uint8_t ch_mask);
```

Ends the moving phase. It saves the values. Call it after the state becomes `XM_IMU_CALIB_WAIT_USER` and you have moved the sensor.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `ch_mask` | `uint8_t` | bit0 – bit5 = channel 0 – 5 |

**Return value**: [`XmImuCalibRequest_t`](#xmimucalibrequest_t)

**⚠️ Call context**: Call it in `Control_Setup()`, `Control_Loop()` or a background task. Do not call it from an interrupt. You can call it in control mode too.

**See also**: [`XM_ImuCalibGetStatus`](#xm_imucalibgetstatus)

---

#### `XM_ImuCalibAbort`

```c
XmImuCalibRequest_t XM_ImuCalibAbort(uint8_t ch_mask);
```

Aborts a calibration. You can abort the magnetometer and the full accelerometer calibration only until you call `XM_ImuCalibFinish()`. For details, see [Things to know](#things-to-know). An aborted calibration does not save the values (`XM_IMU_CALIB_RESULT_ABORTED`).

**Parameters**

| Name | Type | Description |
|------|------|------|
| `ch_mask` | `uint8_t` | bit0 – bit5 = channel 0 – 5 |

**Return value**: [`XmImuCalibRequest_t`](#xmimucalibrequest_t)

**⚠️ Call context**: Call it in `Control_Setup()`, `Control_Loop()` or a background task. Do not call it from an interrupt. You can call it in control mode too.

**See also**: With EBIMU, the power of all channels goes off for about 2 seconds when you abort — [Things to know](#things-to-know)

---

### State

#### `XM_ImuCalibGetStatus`

```c
bool XM_ImuCalibGetStatus(uint8_t ch, XmImuCalibStatus_t* out);
```

Reads the calibration state of a channel.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `ch` | `uint8_t` | Channel 0 – 5 |
| `out` | [`XmImuCalibStatus_t*`](#xmimucalibstatus_t) | Struct that receives the state |

**Return value**: `bool` — `true`: `out` is filled. `false`: read it again next period.

**⚠️ Call context**: Call it in `Control_Setup()`, `Control_Loop()` or a background task. Do not call it from an interrupt.

**Example**

```c
static bool s_user_moved_sensor;   /* set to true when you have finished moving the sensor */

void Control_Loop(void) {
    XmImuCalibStatus_t st;
    if (XM_ImuCalibGetStatus(0U, &st)) {
        if ((st.state == XM_IMU_CALIB_WAIT_USER) && s_user_moved_sensor) {
            XM_ImuCalibFinish(CALIB_CH0);            /* only once */
            s_user_moved_sensor = false;
        } else if (st.state == XM_IMU_CALIB_DONE) {
            /* finished */
        } else if (st.state == XM_IMU_CALIB_FAILED) {
            /* look at st.result */
        }
    }
}
```

**See also**: [`XmImuCalibStatus_t`](#xmimucalibstatus_t), [`XmImuCalibState_t`](#xmimucalibstate_t)

---

### Supported items

#### `XM_ImuCalibQuerySupported`

```c
XmImuCalibRequest_t XM_ImuCalibQuerySupported(void);
```

Asks the IMU Hub which items each channel supports. Look at the result with [`XM_ImuCalibGetSupported`](#xm_imucalibgetsupported).

**Parameters**: none

**Return value**: [`XmImuCalibRequest_t`](#xmimucalibrequest_t)

**⚠️ Call context**: Call it in `Control_Setup()`, `Control_Loop()` or a background task. Do not call it from an interrupt.

---

#### `XM_ImuCalibGetSupported`

```c
XmImuCalibResult_t XM_ImuCalibGetSupported(uint8_t ch, uint8_t* out_op_mask);
```

Reads the items a channel supports.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `ch` | `uint8_t` | Channel 0 – 5 |
| `out_op_mask` | `uint8_t*` | (output) When the result is `XM_IMU_CALIB_RESULT_OK`, the bit mask of supported items ([`XM_IMU_CALIB_OP_BIT`](#xm_imu_calib_op_bit)). `0` means there is no sensor on this channel |

**Return value**: [`XmImuCalibResult_t`](#xmimucalibresult_t)

| Value | Meaning |
|------|------|
| `XM_IMU_CALIB_RESULT_NONE` | No answer yet. It is also returned when the channel is wrong or `out_op_mask` is `NULL` |
| `XM_IMU_CALIB_RESULT_OK` | `out_op_mask` has the supported items |
| `XM_IMU_CALIB_RESULT_NO_RESPONSE` · `XM_IMU_CALIB_RESULT_HUB_NOT_SUPPORTED` | The firmware may not have the calibration feature |
| `XM_IMU_CALIB_RESULT_LINK_LOST` | The connection to the IMU Hub was lost |

**⚠️ Call context**: Call it in `Control_Setup()`, `Control_Loop()` or a background task. Do not call it from an interrupt.

**Example**

```c
XM_ImuCalibQuerySupported();                 /* ask once */

/* then, in Control_Loop() */
uint8_t mask = 0U;
XmImuCalibResult_t r = XM_ImuCalibGetSupported(0U, &mask);
if ((r == XM_IMU_CALIB_RESULT_OK) && (mask & XM_IMU_CALIB_OP_BIT(XM_IMU_CALIB_OP_MAG))) {
    /* channel 0 supports magnetometer calibration */
}
```

**See also**: [`XM_ImuCalibQuerySupported`](#xm_imucalibquerysupported)

---

## Types / macro

### `XM_IMU_CALIB_OP_BIT`

```c
#define XM_IMU_CALIB_OP_BIT(op)  (1U << ((unsigned)(op) - 1U))
```

Picks the bit of one item in the `out_op_mask` of [`XM_ImuCalibGetSupported`](#xm_imucalibgetsupported). For example: `mask & XM_IMU_CALIB_OP_BIT(XM_IMU_CALIB_OP_MAG)`.

| Item | Bit |
|------|------|
| `XM_IMU_CALIB_OP_GYRO` | bit0 |
| `XM_IMU_CALIB_OP_MAG` | bit1 |
| `XM_IMU_CALIB_OP_ACCEL_SIMPLE` | bit2 |
| `XM_IMU_CALIB_OP_ACCEL_FULL` | bit3 |

---

### `XmImuCalibOp_t`

The calibration item. Pass it to [`XM_ImuCalibStart()`](#xm_imucalibstart).

| Value | Name | Meaning |
|----|------|------|
| 1 | `XM_IMU_CALIB_OP_GYRO` | Gyro zero — keep the sensor still |
| 2 | `XM_IMU_CALIB_OP_MAG` | Magnetometer — rotate it while mounted as it is, then `XM_ImuCalibFinish()` |
| 3 | `XM_IMU_CALIB_OP_ACCEL_SIMPLE` | Simple accelerometer — keep the sensor level and still |
| 4 | `XM_IMU_CALIB_OP_ACCEL_FULL` | Full accelerometer — hold several postures, then `XM_ImuCalibFinish()` |

The MTi-630 supports only the gyro zero and the magnetometer.

---

### `XmImuCalibRequest_t`

Whether the request was accepted. Accepted does not mean the calibration is finished — look at the result with [`XM_ImuCalibGetStatus()`](#xm_imucalibgetstatus).

| Value | Name | Meaning |
|----|------|------|
| 0 | `XM_IMU_CALIB_REQ_OK` | Accepted |
| 1 | `XM_IMU_CALIB_REQ_BUSY` | The previous request is still being handled. Call again shortly |
| 2 | `XM_IMU_CALIB_REQ_NOT_CONNECTED` | The IMU Hub is not connected |
| 3 | `XM_IMU_CALIB_REQ_CONTROL_ACTIVE` | It is control mode. Switch to `XM_CTRL_MONITOR` and then start |
| 4 | `XM_IMU_CALIB_REQ_INVALID` | The channel or item value is wrong |

---

### `XmImuCalibState_t`

The channel state.

| Value | Name | Meaning |
|----|------|------|
| 0 | `XM_IMU_CALIB_IDLE` | Never calibrated |
| 1 | `XM_IMU_CALIB_RUNNING` | In progress |
| 2 | `XM_IMU_CALIB_WAIT_USER` | The phase in which you move the sensor — call `XM_ImuCalibFinish()` when you are done |
| 3 | `XM_IMU_CALIB_DONE` | Finished. It was also confirmed that the sensor data is coming in again |
| 4 | `XM_IMU_CALIB_FAILED` | Failed or aborted — look at `result` |
| 5 | `XM_IMU_CALIB_PENDING` | The request is being sent — the IMU Hub has not confirmed it yet |

---

### `XmImuCalibResult_t`

The result. 6 is not used.

| Value | Name | Meaning |
|----|------|------|
| 0 | `XM_IMU_CALIB_RESULT_NONE` | None yet |
| 1 | `XM_IMU_CALIB_RESULT_OK` | Success |
| 2 | `XM_IMU_CALIB_RESULT_NO_SENSOR` | There is no sensor on this channel |
| 3 | `XM_IMU_CALIB_RESULT_NOT_SUPPORTED` | This sensor does not support this item |
| 4 | `XM_IMU_CALIB_RESULT_SENSOR_TIMEOUT` | The sensor did not answer. `info` = the step that failed |
| 5 | `XM_IMU_CALIB_RESULT_STREAM_NOT_RESUMED` | The sensor data did not come in again after calibration |
| 7 | `XM_IMU_CALIB_RESULT_HUB_BUSY` | The IMU Hub cannot handle the request right now. Wait a moment, check the connection state, and try again |
| 8 | `XM_IMU_CALIB_RESULT_ABORTED` | Aborted. The values were not saved |
| 9 | `XM_IMU_CALIB_RESULT_SENSOR_REJECTED` | The sensor refused. `info` = the state value the sensor sent |
| 10 | `XM_IMU_CALIB_RESULT_SESSION_TIMEOUT` | The time limit of the moving phase was exceeded |
| 11 | `XM_IMU_CALIB_RESULT_NO_RESPONSE` | The IMU Hub does not answer. The firmware is old or there is a communication problem (see below) |
| 12 | `XM_IMU_CALIB_RESULT_HUB_NOT_SUPPORTED` | The IMU Hub refused the request. The firmware may be old. Try updating it |
| 13 | `XM_IMU_CALIB_RESULT_LINK_LOST` | The connection to the IMU Hub was lost during calibration, or the IMU Hub restarted |

**For `XM_IMU_CALIB_RESULT_NO_RESPONSE`, act according to the state.**

| State | What to do |
|------|------|
| `XM_IMU_CALIB_FAILED` | The request did not arrive. Start again |
| `XM_IMU_CALIB_PENDING` | The calibration may already be running. Do not use this channel, and wait for the answer |
| Any other in-progress state | See whether the answer comes back, and call `XM_ImuCalibFinish()` / `XM_ImuCalibAbort()` again |

---

### `XmImuCalibStatus_t`

The calibration state of one channel. Read it with [`XM_ImuCalibGetStatus()`](#xm_imucalibgetstatus).

```c
typedef struct {
    XmImuCalibState_t  state;
    XmImuCalibOp_t     op;
    XmImuCalibResult_t result;
    uint8_t            info;
} XmImuCalibStatus_t;
```

| Field | Type | Description |
|------|------|------|
| `state` | [`XmImuCalibState_t`](#xmimucalibstate_t) | The channel state |
| `op` | [`XmImuCalibOp_t`](#xmimucalibop_t) | The item of this state. 0 if never calibrated |
| `result` | [`XmImuCalibResult_t`](#xmimucalibresult_t) | The result |
| `info` | `uint8_t` | In progress = the step number. Failed = see the `result` description. Otherwise = 0 |

---

## Rev1.1 / Rev2.0 Difference Summary

| Item | Rev1.1 | Rev2.0 |
|------|--------|--------|
| IMU Hub sensor calibration API (`xm_api_imu_calib.h`) | ❌ Absent | 🟢 Exclusive |

---

## Related documents

- [13. IMU Hub Sensor Calibration (concept)](../13-imu-calibration.en.md) — order of use, how to hold the sensor, common mistakes
- [`xm_api_data.h` — XM10 Unified Data & Control Interface](xm-api-data.en.md) — the control modes (`XM_CTRL_*`) and `XM.status`
- [`xm_api_canbus.h` — Talk to Your Own Device over CAN-FD or Classic CAN](xm-api-canbus.en.md) — `XM_CanSend`
