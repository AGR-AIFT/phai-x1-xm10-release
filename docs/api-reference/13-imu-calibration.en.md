# API Reference: IMU Hub Sensor Calibration

> 📌 **After reading this page** you will be able to calibrate the sensors of an IMU Hub connected over CAN-FD while they stay mounted in a case or clothing.
> ⏱️ Estimated reading time: 20 minutes
> 🧰 Prerequisites: Viewing IMU Hub data in [Ex.41](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/41_IMU_Hub_Dashboard/)
> 🎯 Key functions: `XM_ImuCalibStart` / `XM_ImuCalibGetStatus` / `XM_ImuCalibFinish` / `XM_ImuCalibAbort`
>
> 🟢 **Rev 2.0 only** *(new in v2.10.0)*: The Rev 1.1 SDK does not have `xm_api_imu_calib.h`.
>
> ℹ️ The IMU Hub is a module under internal development. To use it, please contact us at https://huphailab.com/contact.

This page is a guide to the **in-place IMU Hub sensor calibration API** defined in `xm_api_imu_calib.h`. For each function in detail, see the [`xm_api_imu_calib.h` reference](ref/xm-api-imu-calib.en.md).

> ℹ️ **Board status**: The behavior below is what the API defines, and we have not yet confirmed it on a board. (not yet confirmed on the board) <!-- board-check -->

---

## 1. Why Calibrate in Place

A sensor is calibrated correctly only if you calibrate it while it is built into its case or clothing. The magnetometer in particular must be rotated together with the object it is mounted on. With this API you can start and finish the calibration from XM10 user code.

---

## 2. Order of Use

```c
// ① Start — only once, like the moment a button is pressed
if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
    (void)XM_ImuCalibStart(0x01, XM_IMU_CALIB_OP_MAG);       // magnetometer of channel 0
}

// ② Check the state — it does not wait, so do it in Control_Loop
XmImuCalibStatus_t st;
if (XM_ImuCalibGetStatus(0, &st)) {
    if (st.state == XM_IMU_CALIB_WAIT_USER) {
        /* ③ After you rotate the sensor as it is mounted, call XM_ImuCalibFinish(0x01) — it saves the values */
    } else if (st.state == XM_IMU_CALIB_DONE) {
        /* ④ Finished */
    } else if (st.state == XM_IMU_CALIB_FAILED) {
        /* look at st.result for the reason */
    }
}
```

- Call the start **only once**. If you call it every period, the calibration restarts each time it ends.
- The channel is a bit mask (bit0 – bit5 = channel 0 – 5), so you can pass several channels at once. A channel that is already running is left as it is.
- The start request is sent to the IMU Hub **only once** — to stop the same calibration from running twice. If no answer comes, the state stays at `XM_IMU_CALIB_PENDING`, and a further request during that time returns `XM_IMU_CALIB_REQ_BUSY`. It is released when the IMU Hub reports its state again or the connection is lost.
- No function waits. You can call them in `Control_Setup`, `Control_Loop` and a background task, but do not call them from an interrupt.

---

## 3. Calibration Items

| Item | What it does | How it ends |
|------|------|------|
| `XM_IMU_CALIB_OP_GYRO` | Gyro zero — keep the sensor still | Ends by itself |
| `XM_IMU_CALIB_OP_MAG` | Magnetometer — rotate it in several directions while mounted as it is | `XM_ImuCalibFinish` (up to 120 seconds) |
| `XM_IMU_CALIB_OP_ACCEL_SIMPLE` | Simple accelerometer — keep the sensor level and still | Ends by itself |
| `XM_IMU_CALIB_OP_ACCEL_FULL` | Full accelerometer — hold several postures | `XM_ImuCalibFinish` (up to 600 seconds) |

- The MTi-630 supports only the gyro zero and the magnetometer.
- Ask which items each channel supports with `XM_ImuCalibQuerySupported()`, then read them with `XM_ImuCalibGetSupported(ch, &mask)`. Check with `mask & XM_IMU_CALIB_OP_BIT(op)`; if `mask` is 0, there is no sensor on that channel.
- If the moving phase runs past its time limit, the calibration ends with `XM_IMU_CALIB_RESULT_SESSION_TIMEOUT`.

---

## 4. Request Results · States · Results

| Request result (`XmImuCalibRequest_t`) | Meaning |
|------|------|
| `XM_IMU_CALIB_REQ_OK` | Accepted — it is not finished yet. Look at the result with the state |
| `XM_IMU_CALIB_REQ_BUSY` | The previous request is still being handled — try again shortly |
| `XM_IMU_CALIB_REQ_NOT_CONNECTED` | The IMU Hub is not connected |
| `XM_IMU_CALIB_REQ_CONTROL_ACTIVE` | It is control mode — switch to `XM_CTRL_MONITOR`, then start |
| `XM_IMU_CALIB_REQ_INVALID` | The channel or item value is wrong |

| State (`XmImuCalibState_t`) | Meaning |
|------|------|
| `XM_IMU_CALIB_IDLE` | Never calibrated |
| `XM_IMU_CALIB_PENDING` | The request is being sent — the IMU Hub has not confirmed it yet |
| `XM_IMU_CALIB_RUNNING` | In progress |
| `XM_IMU_CALIB_WAIT_USER` | The phase in which you move the sensor — call `XM_ImuCalibFinish` when you are done |
| `XM_IMU_CALIB_DONE` | Finished — it was also confirmed that the sensor data is coming in again |
| `XM_IMU_CALIB_FAILED` | Failed or aborted — look at `result` |

| Result (`XmImuCalibResult_t`) | Meaning · what to do |
|------|------|
| `OK` | Success |
| `NO_SENSOR` · `NOT_SUPPORTED` | There is no sensor on this channel · this sensor does not support this item |
| `SENSOR_TIMEOUT` · `SENSOR_REJECTED` | The sensor did not answer · the sensor refused — `info` has the step · the sensor state value |
| `STREAM_NOT_RESUMED` | The sensor data did not come in again after calibration |
| `HUB_BUSY` | The IMU Hub cannot handle the request right now — wait a moment, check the connection, and try again |
| `ABORTED` | Aborted — the values were not saved |
| `SESSION_TIMEOUT` | The time limit of the moving phase was exceeded |
| `NO_RESPONSE` | The IMU Hub does not answer — if FAILED, the request did not arrive, so start again; if PENDING, it may already be running, so do not use this channel and wait |
| `HUB_NOT_SUPPORTED` | The IMU Hub refused — the firmware may be old, so update it |
| `LINK_LOST` | The connection dropped during calibration, or the IMU Hub restarted |

(The names in the table are the part after `XM_IMU_CALIB_RESULT_`.)

---

## 5. Things to Know

- **You cannot start in control mode.** Switch to `XM_CTRL_MONITOR`, then start. Finishing and aborting work in control mode too.
- **Do not control with a channel that is being calibrated.** While it is being calibrated, that channel's `XM.status.imu_hub.connected_mask` bit is off, and it comes back on when new data arrives after the calibration ends. If the connection drops during calibration and comes back, the bit stays off until you check the IMU Hub state. If you drive an external driver directly with `XM_CanSend`, always check this bit.
- **You can abort only until you finish the magnetometer or the full accelerometer calibration.** The gyro zero and the simple accelerometer calibration end quickly, so they cannot be aborted.
- **If you abort an EBIMU magnetometer or full accelerometer calibration, or its moving time runs past the limit,** the power of all channels goes off for about 2 seconds and then comes back. During that time the other channels drop too, and each channel looks reconnected only after new data arrives for it. If another sensor is saving its values, it is waited for first, so it can take about 17 seconds until the abort result comes.
- **Do not use these functions while you calibrate from the PC program.** The later request erases the earlier one.
- **IMU Hub firmware** — With old firmware that has no calibration feature, the calibration ends with "not supported" or "no response".

---

## 6. Examples

| What | Link |
|------|------|
| Gyro zero · magnetometer calibration · abort with the buttons | [Ex.48 IMU Hub Calibration](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/48_IMU_Hub_Calibration/) |
| See the attitude after calibration | [Ex.41 IMU Hub Dashboard](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/41_IMU_Hub_Dashboard/) |
| Every function in detail | [`xm_api_imu_calib.h` reference](ref/xm-api-imu-calib.en.md) |
