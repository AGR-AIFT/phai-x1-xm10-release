# `xm_api_data.h` — XM10 Unified Data & Control Interface

> **Header**: `XM_FW/XM_API/xm_api_data.h` (shared by Rev1.1 / Rev2.0 — the GRF extension fields are 🟢 Rev 2.0 only)
> **Related concept doc**: [02. KIT H10 Control & Data](../02-h10-control-n-data.en.md) (IPO cycle, Body Data, torque sign convention)
> **Related examples**: [11 Passive Mode](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/11_Passive_Mode/) · [12 Active Assist](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/12_Active_Assist_Mode/) · [13 Resistive](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/13_Resistive_Mode/) · [14 PD Realtime](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/14_PD_Realtime_Control/) · [16 TinyAI Sensor Fusion](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/16_TinyAI_Sensor_Fusion/) · [32 GRF Gait Intent](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/32_GRF_Gait_Intent/) · [37 FES Hub Ctrl](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/37_FES_Hub_Module_Ctrl/) 🛑 Rev 2.0 only · [41 IMU Hub Dashboard](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/41_IMU_Hub_Dashboard/) 🛑 Rev 2.0 only · [42 EMG Hub Biofeedback](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/42_EMG_Hub_Biofeedback/) 🛑 Rev 2.0 only

---

## When to use this

`xm_api_data.h` is the facade you will open **most often** while working on XM10 firmware. Every function that reads the latest state of `KIT H10` and the various sensor hubs (GRF, External IMU, IMU Hub, EMG Hub, FES Hub) inside `Control_Loop()` (`XM.status`), and every function that issues control commands (`XM_Set*` / `XM_Send*`), is declared here.

This page does not re-explain how the global `XM` object gets filled and flushed (the **IPO — Input-Process-Output — cycle**), the torque sign convention, or the Body Data prerequisite. If this is your first time, read [02. KIT H10 Control & Data](../02-h10-control-n-data.en.md) first; use this page to quickly look up function signatures, parameters, and struct fields.

> ℹ️ **Note on the common `nodeId` parameter**: most joint-control functions in this header take a `SystemNodeID_t nodeId`. In practice only two values are used — `SYS_NODE_ID_RH` (right hip) and `SYS_NODE_ID_LH` (left hip) — defined in `data_object_dictionaries.h` (a different header). Since this page only covers `xm_api_data.h`, it does not list the full `SystemNodeID_t` enum.

---

## Function List

### Control Mode · Real-time Torque

| Function | One-line description |
|---|---|
| [`XM_SetControlMode()`](#xm_setcontrolmode) | Switches whether torque commands are actually sent (monitor / torque control) |
| [`XM_SetAssistTorque()`](#xm_setassisttorque) | Sets both hip assist torques at once |
| [`XM_SetAssistTorqueRH()` / `XM_SetAssistTorqueLH()`](#xm_setassisttorquerh--xm_setassisttorquelh) | Sets a single hip's assist torque |

### System · Connection Status

| Function | One-line description |
|---|---|
| [`XM_IsCmConnected()`](#xm_iscmconnected) | Checks whether communication with the CM is Operational |
| [`XM_GetXMNmtState()`](#xm_getxmnmtstate) | Returns the detailed PnP (NMT) state with the CM |

### Data Transmission (Body Data · PIF-Vector)

| Function | One-line description |
|---|---|
| [`XM_SendUserBodyData()`](#xm_senduserbodydata) | Sends user body info (weight/height/segment lengths) to the CM |
| [`XM_SendPVector()`](#xm_sendpvector) | Sends a position-based trajectory (P-Vector) |
| [`XM_SendPVectorReset()`](#xm_sendpvectorreset) | Immediately cancels an in-progress P-Vector trajectory |
| [`XM_ClearPVectorDoneFlag()`](#xm_clearpvectordoneflag) | Manually clears the P-Vector completion flag |
| [`XM_SendIVector()`](#xm_sendivector) | Sends impedance control parameters (I-Vector) |
| [`XM_SendFVector()`](#xm_sendfvector) | Sends a force-based trajectory (F-Vector) |
| [`XM_SendIVectorKpKdMax()`](#xm_sendivectorkpkdmax) | Sets the Kp/Kd ceiling (the 100% reference) used by I-Vector |

### Routines · Parameters

| Function | One-line description |
|---|---|
| [`XM_SetDegreeLimitRoutine()` / `XM_SetDegreeLimit()`](#xm_setdegreelimitroutine--xm_setdegreelimit) | Toggle the angle-limit routine + set upper/lower bounds |
| [`XM_SetVelocityLimitRoutine()` / `XM_SetVelocityLimit()`](#xm_setvelocitylimitroutine--xm_setvelocitylimit) | Toggle the angular-velocity-limit routine + set upper/lower bounds |
| [`XM_SetDOBRoutine()`](#xm_setdobroutine) | Toggles the disturbance observer (DOB) routine |
| [`XM_SetNormalCompGain()` / `XM_SetResistiveCompGain()`](#xm_setnormalcompgain--xm_setresistivecompgain) | Sets the gravity-compensation / resistive-compensation gain |
| [`XM_SetH10AssistExistingMode()`](#xm_seth10assistexistingmode) | Enables/disables H10's built-in (legacy) assist algorithm |

### Utility

| Function | One-line description |
|---|---|
| [`XM_CaptureLoopCountBase()` / `XM_GetRelativeLoopCount()`](#xm_captureloopcountbase--xm_getrelativeloopcount) | Captures a baseline for the Assist Loop Count and returns the relative count since then |

**23 functions** total, plus 3 macros and 13 structs/enums (see [Types · Macros](#types--macros) below).

---

## Function Details

### `XM_SetControlMode()`

```c
void XM_SetControlMode(XmControlMode_t mode);
```

This is the safety switch that decides whether the robot's **output (actuation) is ON or OFF**. In both `XM_CTRL_MONITOR` and `XM_CTRL_CONTROL`, your `Control_Loop()` algorithm itself still runs every tick — the only difference is whether the computed torque command is actually sent to the CM. In other words, it is not a switch that turns the algorithm off, only the output.

| Parameter | Type | Description |
|---|---|---|
| `mode` | `XmControlMode_t` | `XM_CTRL_MONITOR` (default, no output) or `XM_CTRL_CONTROL` (actual actuation) |

**Return value**: none

**Safety logic**: the instant the mode changes (especially MONITOR → TORQUE), **all torque commands are internally reset to 0** to prevent a jerk at the start of actuation.

⚠️ **Call context**: assumed to be called from `Control_Setup()`/`Control_Loop()` (the header does not state any ISR-safety guarantee). The common pattern is `XM_CTRL_CONTROL` on algorithm Entry and `XM_CTRL_MONITOR` on Exit.

**Example**
```c
void Active_Entry(void) {
    XM_SetControlMode(XM_CTRL_CONTROL);   // start real actuation
}

void Active_Exit(void) {
    XM_SetControlMode(XM_CTRL_MONITOR);  // safely fall back to monitoring
}
```

**See also**: [`XmControlMode_t`](#xmcontrolmode_t)

---

### `XM_SetAssistTorque()`

```c
void XM_SetAssistTorque(float rh, float lh);
```

Sets both hip assist torques in one call. The actual CAN transmission happens automatically in the Output stage of the IPO cycle, not inside this function.

| Parameter | Type | Description |
|---|---|---|
| `rh` | `float` | Right (RH) hip assist torque, unit **Nm** — first argument |
| `lh` | `float` | Left (LH) hip assist torque, unit **Nm** — second argument |

**Return value**: none

> ⚠️ Argument order is **(right rh, left lh)**. If that's confusing, use [`XM_SetAssistTorqueRH()` / `XM_SetAssistTorqueLH()`](#xm_setassisttorquerh--xm_setassisttorquelh) to set one side at a time.
> For the sign convention, unit, and the hard clamp (±10 Nm), see [Doc 02 §Real-time Control](../02-h10-control-n-data.en.md#xm_setassisttorque) — this header itself does not define the scaling/clamp values (they live in another internal module).

⚠️ **Call context**: assumed to be `Control_Loop()`. Requires `XM_SetControlMode(XM_CTRL_CONTROL)` to actually be transmitted.

**Example**
```c
void Active_Loop(void) {
    float cmd_R = XM.status.h10.rightHipAngle * 0.5f;   // simple P control
    float cmd_L = XM.status.h10.leftHipAngle  * 0.5f;
    XM_SetAssistTorque(cmd_R, cmd_L);
}
```

---

### `XM_SetAssistTorqueRH()` / `XM_SetAssistTorqueLH()`

```c
void XM_SetAssistTorqueRH(float rh);
void XM_SetAssistTorqueLH(float lh);
```

Sets the assist torque for one leg only. The other leg's torque keeps whatever value was previously set.

| Parameter | Type | Description |
|---|---|---|
| `rh` (RH version) | `float` | Right hip assist torque, unit Nm |
| `lh` (LH version) | `float` | Left hip assist torque, unit Nm |

**Return value**: none

⚠️ **Call context**: assumed to be `Control_Loop()`. Works the same way as `XM_SetAssistTorque()` by setting an internal Dirty Flag (`XmOutput_t._dirty_flags`).

**Example**
```c
XM_SetAssistTorqueRH(5.0f);   // right side only, to 5.0 Nm; left keeps its previous value
```

---

### `XM_IsCmConnected()`

```c
bool XM_IsCmConnected(void);
```

Checks the communication status with the Control Module (CM).

**Return value**

| Value | Meaning |
|---|---|
| `true` | Connection is Operational |
| `false` | Disconnected, or not yet ready |

⚠️ **Call context**: assumed to be `Control_Setup()`/`Control_Loop()`.

**Example**
```c
if (XM_IsCmConnected()) {
    // use as an algorithm start condition
}
```

**See also**: [`XM_GetXMNmtState()`](#xm_getxmnmtstate) — for finer-grained state branching

---

### `XM_GetXMNmtState()`

```c
CM_NmtState_t XM_GetXMNmtState(void);
```

Returns the detailed DOP V3 PnP (NMT) state with the CM. `XM_IsCmConnected()` is internally equivalent to checking `XM_GetXMNmtState() == CM_NMT_OPERATIONAL`.

**Return value**: `CM_NmtState_t` (defined in `cm_drv.h` — a different header this one includes)

| State | Value | Description |
|---|---|---|
| `CM_NMT_INITIALISING` | 0 | Booting (boot-up message not yet received) |
| `CM_NMT_PRE_OPERATIONAL` | 1 | SDO communication available, PDO inactive |
| `CM_NMT_OPERATIONAL` | 2 | All communication active (normal state) |
| `CM_NMT_STOPPED` | 3 | Communication stopped |

⚠️ **Call context**: assumed to be `Control_Setup()`/`Control_Loop()`. No SDK example currently calls this function directly (`XM_IsCmConnected()` covers most cases).

---

### `XM_SendUserBodyData()`

```c
void XM_SendUserBodyData(const uint32_t bodyData[8]);
```

Sends the user's body information (weight, height, segment lengths, etc.) to the CM. The CM uses this to improve the accuracy of its real-time gait analysis.

| Parameter | Type | Description |
|---|---|---|
| `bodyData` | `const uint32_t[8]` | An array holding the 8 values below |

**Meaning of each `bodyData` index** (units per header comment):

| Index | Description | Unit |
|---|---|---|
| 0 | Wearer's weight | g |
| 1 | Wearer's height | mm |
| 2 | Right thigh segment length | mm |
| 3 | Left thigh segment length | mm |
| 4 | Right shank segment length | mm |
| 5 | Left shank segment length | mm |
| 6 | Right ankle segment length | mm |
| 7 | Left ankle segment length | mm |

**Return value**: none

⚠️ **Call context**: `Control_Setup()` recommended (call once). Body info typically does not change during a session, so there is no need to call this every tick — if omitted, estimated data such as `forwardVelocity`/`leftKneeAngle` become inaccurate (see [README §Body Data](../README.en.md)).

**Example**
```c
void Control_Setup(void) {
    uint32_t body_data[8] = {
        70000, 1750, 450, 450, 420, 420, 60, 60,
    };
    XM_SendUserBodyData(body_data);
}
```

---

### `XM_SendPVector()`

```c
void XM_SendPVector(SystemNodeID_t nodeId, const PVector_t* pVector);
```

Sends a **position-based trajectory (P-Vector)** to the specified joint. The motor driver generates a 5th-order polynomial trajectory that smoothly moves to the target position (`yd`) within the specified time (`L`).

| Parameter | Type | Description |
|---|---|---|
| `nodeId` | `SystemNodeID_t` | The joint to command (`SYS_NODE_ID_RH` / `SYS_NODE_ID_LH`) |
| `pVector` | `const PVector_t*` | Pointer to the P-Vector data struct to send |

**Return value**: none

⚠️ Impedance parameters must already be configured via [`XM_SendIVector()`](#xm_sendivector) beforehand.

⚠️ **Call context**: assumed to be `Control_Loop()`.

**Example**
```c
PVector_t pv = { .yd = 250, .L = 1000, .s0 = 4, .sd = 4 };  // target 25.0deg, moves over 1s
XM_SendPVector(SYS_NODE_ID_RH, &pv);
```

**See also**: [`PVector_t`](#pvector_t)

---

### `XM_SendPVectorReset()`

```c
void XM_SendPVectorReset(SystemNodeID_t nodeId);
```

Immediately resets (cancels) the currently running or pending P-Vector command.

| Parameter | Type | Description |
|---|---|---|
| `nodeId` | `SystemNodeID_t` | The joint whose P-Vector should be reset |

**Return value**: none

⚠️ **Call context**: assumed to be `Control_Loop()`. Used for mode transitions / emergency stops.

---

### `XM_ClearPVectorDoneFlag()`

```c
void XM_ClearPVectorDoneFlag(SystemNodeID_t nodeId);
```

After confirming that `XM.status.h10.isPVectorRHDone` / `isPVectorLHDone` has become `true` and finishing the associated logic, call this manually to signal that you've "consumed" the event.

| Parameter | Type | Description |
|---|---|---|
| `nodeId` | `SystemNodeID_t` | The joint whose completion flag should be cleared |

**Return value**: none

⚠️ **Warning**: if you don't call this, the flag stays `true` and the same completion event may be processed repeatedly.

⚠️ **Call context**: assumed to be `Control_Loop()`.

---

### `XM_SendIVector()`

```c
void XM_SendIVector(SystemNodeID_t nodeId, const IVector_t* iVector);
```

Sends **impedance control parameters (I-Vector)** to the specified joint, making it behave like a virtual spring-damper.

| Parameter | Type | Description |
|---|---|---|
| `nodeId` | `SystemNodeID_t` | The joint to command |
| `iVector` | `const IVector_t*` | Pointer to the impedance parameter struct |

**Return value**: none

⚠️ **Call context**: assumed to be `Control_Loop()`. `kp`/`kd` are interpreted as a percentage of the ceiling set by [`XM_SendIVectorKpKdMax()`](#xm_sendivectorkpkdmax).

**Example**
```c
IVector_t iv = { .epsilon = 0, .kp = 80, .kd = 1, .lambda = 0, .duration = 50 };
XM_SendIVector(SYS_NODE_ID_RH, &iv);
```

**See also**: [`IVector_t`](#ivector_t), [`XM_SendIVectorKpKdMax()`](#xm_sendivectorkpkdmax)

---

### `XM_SendFVector()`

```c
void XM_SendFVector(SystemNodeID_t nodeId, const FVector_t* fVector);
```

Sends a **force-based trajectory (F-Vector)** to the specified joint, commanding it to generate a pre-defined torque profile.

| Parameter | Type | Description |
|---|---|---|
| `nodeId` | `SystemNodeID_t` | The joint to command |
| `fVector` | `const FVector_t*` | Pointer to the struct holding target torque / mode info |

**Return value**: none

⚠️ **Call context**: assumed to be `Control_Loop()`. No current SDK example uses this function — the snippet below is an illustrative initialization built purely from the struct's field definitions.

**Example**
```c
FVector_t fv = { .modeIdx = 1, .tauMax = 500, .delay = 0, .zero = 0 };  // tauMax=5.00A (scaled by 100)
XM_SendFVector(SYS_NODE_ID_RH, &fv);
```

**See also**: [`FVector_t`](#fvector_t)

---

### `XM_SendIVectorKpKdMax()`

```c
void XM_SendIVectorKpKdMax(SystemNodeID_t nodeId, const float kpMax, const float kdMax);
```

Sets the **ceiling values** applied when `XM_SendIVector()`'s `kp`/`kd` (%) for the specified joint reach 100%.

| Parameter | Type | Description |
|---|---|---|
| `nodeId` | `SystemNodeID_t` | The joint to configure |
| `kpMax` | `const float` | Maximum virtual spring stiffness applied when `kp` = 100% |
| `kdMax` | `const float` | Maximum virtual damper stiffness applied when `kd` = 100% |

**Return value**: none

⚠️ **Call context**: recommended at `Control_Setup()` or on state Entry — it must be configured before `XM_SendIVector()` for the percentage interpretation to make sense.

**Example**
```c
XM_SendIVectorKpKdMax(SYS_NODE_ID_RH, 6.0f, 1.0f);
XM_SendIVectorKpKdMax(SYS_NODE_ID_LH, 6.0f, 1.0f);
```

---

### `XM_SetDegreeLimitRoutine()` / `XM_SetDegreeLimit()`

```c
void XM_SetDegreeLimitRoutine(SystemNodeID_t nodeId, bool isSet);
void XM_SetDegreeLimit(SystemNodeID_t nodeId, float upperLimit, float lowerLimit);
```

Toggles the **angle-limit routine** for the specified joint (`Routine`) and sets its upper/lower bounds (`Limit`).

| Parameter | Type | Description |
|---|---|---|
| `nodeId` | `SystemNodeID_t` | The joint to configure |
| `isSet` (Routine) | `bool` | `true`: enable the routine / `false`: disable |
| `upperLimit` (Limit) | `float` | Upper bound of range of motion, unit degree |
| `lowerLimit` (Limit) | `float` | Lower bound of range of motion, unit degree |

**Return value**: none

⚠️ **Call context**: assumed to be `Control_Setup()`/`Control_Loop()`. No current SDK example uses this directly.

**Example**
```c
XM_SetDegreeLimitRoutine(SYS_NODE_ID_RH, true);
XM_SetDegreeLimit(SYS_NODE_ID_RH, 30.0f, -30.0f);
```

---

### `XM_SetVelocityLimitRoutine()` / `XM_SetVelocityLimit()`

```c
void XM_SetVelocityLimitRoutine(SystemNodeID_t nodeId, bool isSet);
void XM_SetVelocityLimit(SystemNodeID_t nodeId, float upperLimit, float lowerLimit);
```

Toggles the **angular-velocity-limit routine** for the specified joint and sets its upper/lower bounds.

| Parameter | Type | Description |
|---|---|---|
| `nodeId` | `SystemNodeID_t` | The joint to configure |
| `isSet` (Routine) | `bool` | `true`: enable the routine / `false`: disable |
| `upperLimit` (Limit) | `float` | Upper bound of velocity range, unit deg/s |
| `lowerLimit` (Limit) | `float` | Lower bound of velocity range, unit deg/s |

**Return value**: none

⚠️ **Call context**: assumed to be `Control_Setup()`/`Control_Loop()`. No current SDK example uses this directly.

**Example**
```c
XM_SetVelocityLimitRoutine(SYS_NODE_ID_LH, true);
XM_SetVelocityLimit(SYS_NODE_ID_LH, 100.0f, -100.0f);
```

---

### `XM_SetDOBRoutine()`

```c
void XM_SetDOBRoutine(SystemNodeID_t nodeId, bool isSet);
```

Enables/disables the **disturbance observer (DOB)** routine for the specified joint.

| Parameter | Type | Description |
|---|---|---|
| `nodeId` | `SystemNodeID_t` | The joint to configure |
| `isSet` | `bool` | `true`: enable / `false`: disable |

**Return value**: none

⚠️ **Call context**: assumed to be `Control_Setup()`/`Control_Loop()`. No current SDK example uses this directly. Per the header comment, using DOB requires the motor driver side to have completed System Identification.

---

### `XM_SetNormalCompGain()` / `XM_SetResistiveCompGain()`

```c
void XM_SetNormalCompGain(SystemNodeID_t nodeId, uint8_t gain);
void XM_SetResistiveCompGain(SystemNodeID_t nodeId, float gain);
```

Adjusts the strength of **normal compensation (e.g. gravity compensation)** and **resistive compensation (resistance-training mode)**, respectively.

| Parameter | Type | Description |
|---|---|---|
| `nodeId` | `SystemNodeID_t` | The joint to configure |
| `gain` (`Normal`) | `uint8_t` | Normal compensation gain |
| `gain` (`Resistive`) | `float` | Resistive compensation gain |

**Return value**: none

⚠️ **Note**: the two functions' `gain` parameters have different types (`uint8_t` vs `float`). The header does not separately define a range/scale for either, so pay attention to the integer/float distinction as-is in the signature.

⚠️ **Call context**: assumed to be `Control_Loop()`. `XM_SetNormalCompGain()` currently has no direct SDK example usage.

**Example**
```c
float strongResistance = 0.8f;
XM_SetResistiveCompGain(SYS_NODE_ID_RH, strongResistance);
```

---

### `XM_SetH10AssistExistingMode()`

```c
void XM_SetH10AssistExistingMode(bool isSet);
```

Sets whether H10's **built-in (legacy) assist algorithm** is active. When XM10 and H10 are connected, the default is **disabled** (XM10 controls directly).

| Parameter | Type | Description |
|---|---|---|
| `isSet` | `bool` | `true`: enable H10's built-in assist algorithm / `false`: disable |

**Return value**: none

⚠️ **Call context**: assumed to be `Control_Setup()`.

**Example**
```c
void Off_Entry(void) {
    XM_SetH10AssistExistingMode(true);  // switch to H10's built-in assist
}
```

---

### `XM_CaptureLoopCountBase()` / `XM_GetRelativeLoopCount()`

```c
void XM_CaptureLoopCountBase(void);
uint32_t XM_GetRelativeLoopCount(void);
```

Stores the `h10AssistModeLoopCnt` value at the moment of the call as a baseline (0) (`Capture`), and afterwards returns the count relative to that baseline (`GetRelative`). Calling this at the start of a data-logging session makes the logged count always start from 0.

**Return value** (`XM_GetRelativeLoopCount`): `h10AssistModeLoopCnt - baseline`. If `XM_CaptureLoopCountBase()` was never called, the absolute value is returned as-is.

⚠️ **Call context**: `Capture` at `Control_Setup()`/state Entry, `GetRelative` queried inside `Control_Loop()`. No current SDK example uses this directly.

**Example**
```c
void Active_Entry(void) {
    XM_CaptureLoopCountBase();
}

void Active_Loop(void) {
    uint32_t elapsed = XM_GetRelativeLoopCount();  // relative count starting from 0
    myLogData.loopCnt = elapsed;
}
```

---

## Types · Macros

### Macros

| Macro | Value | Description |
|---|---|---|
| `XM_GRF_CHANNEL_SIZE` | `14` | Channel count of the legacy GRF (FSR) shoe sensor |
| `XM_IMU_HUB_SENSOR_COUNT` | `6` | Number of sensors (ports) on the IMU Hub Module |
| `XM_FES_HUB_CH_COUNT` | `2` | Channel count of the FES Hub Module |
| `XM_GRF_FSR_CH_TOTAL` 🟢 Rev 2.0 only | `24` | FSR channel count of the SM-GRF fixed-frame module (ADC1 15ch + ADC3 9ch). Defined in `module.h`, which `xm_api_data.h` includes — Rev1.1 does not have that include at all |

---

### `XmControlMode_t`

XM10's control-authority (output ON/OFF) mode.

```c
typedef enum {
    XM_CTRL_MONITOR = 0,  // monitoring mode (default) — algorithm runs, torque command not sent
    XM_CTRL_CONTROL  = 1   // control mode — computed torque and vector commands are sent periodically (real actuation)
} XmControlMode_t;
```

| Value | Name | Description |
|---|---|---|
| 0 | `XM_CTRL_MONITOR` | Default. The algorithm still runs every tick, but output (actuation) is blocked |
| 1 | `XM_CTRL_CONTROL` | The computed torque command is actually sent to the CM |

**See also**: [`XM_SetControlMode()`](#xm_setcontrolmode)

---

### `XmH10Mode_t`

H10 robot's current operating mode (read-only via `XM.status.h10.h10Mode`).

```c
typedef enum {
    XM_H10_MODE_STANDBY = 0,  // standby mode (no output)
    XM_H10_MODE_ASSIST  = 1,  // assist mode (torque output)
    XM_H10_MODE_UNKNOWN
} XmH10Mode_t;
```

---

### `PVector_t`

A position-based trajectory command vector. Parameter of [`XM_SendPVector()`](#xm_sendpvector).

```c
typedef struct {
    int16_t  yd; // target position, deg×10 scale
    uint16_t L;  // move duration (ms)
    uint8_t  s0; // starting acceleration profile (deg/s^2)
    uint8_t  sd; // ending deceleration profile (deg/s^2)
} PVector_t;
```

| Field | Type | Unit | Description |
|---|---|---|---|
| `yd` | `int16_t` | deg × 10 | Target position. The header comment states a field-verified example: **25.0deg → 250** |
| `L` | `uint16_t` | ms | Trajectory move duration |
| `s0` | `uint8_t` | deg/s² | Starting acceleration profile |
| `sd` | `uint8_t` | deg/s² | Ending deceleration profile |

> ⚠️ **Scaling discrepancy to be aware of**: some example text in [02. KIT H10 Control & Data](../02-h10-control-n-data.en.md) still shows `yd` as "scaled by 100." The header (`xm_api_data.h`, the ground truth for this page) explicitly comments **"scaled by 10 — field-verified value"**. Follow this page (the header) when writing code.

---

### `IVector_t`

An impedance-control parameter vector. Parameter of [`XM_SendIVector()`](#xm_sendivector).

```c
typedef struct {
    uint8_t  epsilon; // half width of the corridor (deg×10)
    uint8_t  kp;      // virtual spring magnitude (%)
    uint8_t  kd;      // virtual damper magnitude (%)
    uint8_t  lambda;  // impedance ratio (×100 scale)
    uint16_t duration;// transition duration (ms)
} IVector_t;
```

| Field | Type | Unit | Description |
|---|---|---|---|
| `epsilon` | `uint8_t` | deg × 10 | Half width of the corridor |
| `kp` | `uint8_t` | % | Virtual spring magnitude — percentage of `kpMax` set via `XM_SendIVectorKpKdMax()` |
| `kd` | `uint8_t` | % | Virtual damper magnitude — percentage of `kdMax` |
| `lambda` | `uint8_t` | ratio × 100 | Impedance ratio |
| `duration` | `uint16_t` | ms | Parameter transition time |

---

### `FVector_t`

A force-based trajectory command vector. Parameter of [`XM_SendFVector()`](#xm_sendfvector).

```c
typedef struct {
    uint16_t modeIdx; // torque profile index, 0.1~10
    int16_t  tauMax;  // max torque (A × 100 scale)
    uint16_t delay;   // initial delay (ms)
    uint16_t zero;    // dummy data for reset
} FVector_t;
```

| Field | Type | Unit | Description |
|---|---|---|---|
| `modeIdx` | `uint16_t` | Index (0.1 ~ 10) | Torque profile index (Tp) |
| `tauMax` | `int16_t` | A × 100 | Max torque |
| `delay` | `uint16_t` | ms | Initial delay |
| `zero` | `uint16_t` | - | Dummy data for reset (header comment: "Dummy data for reset") |

---

### `XmH10Data_t`

**KIT H10 body data** (DOP V1), accessed via `XM.status.h10`. Includes encoders, joint angles, gait state, and more.

| Field | Type | Unit | Description |
|---|---|---|---|
| `is_connected` | `bool` | - | Communication connection status with H10 |
| `h10AssistModeLoopCnt` | `uint32_t` | - | H10 assist-mode loop count (counts from the start of Assist Mode) |
| `h10PostProcessingCnt` | `uint32_t` | - | GaitAnalysis post-processing sample count (0~30000, for 5m analysis) |
| `h10Mode` | `XmH10Mode_t` | - | H10 operating mode (Assist ↔ Standby) |
| `h10AssistLevel` | `uint8_t` | 0~10 | H10 assist level |
| `h10FSMcurrentState` | `uint8_t` | - | H10's current FSM state |
| `isPVectorRHDone` | `bool` | - | RH P-Vector completion flag |
| `isPVectorLHDone` | `bool` | - | LH P-Vector completion flag |
| `h10IsNeutralPosSet` | `bool` | - | H10 neutral-angle setup completion status |
| `leftHipAngle` / `rightHipAngle` | `float` | deg | Left/right hip angle |
| `leftThighAngle` / `rightThighAngle` | `float` | deg | Left/right absolute thigh angle |
| `leftKneeAngle` / `rightKneeAngle` | `float` | deg | Left/right knee angle (estimated) |
| `pelvicAngle` | `float` | deg | Pelvic tilt angle |
| `isLeftFootContact` / `isRightFootContact` | `bool` | - | Left/right foot ground contact |
| `forwardVelocity` | `float` | m/s | Forward gait velocity |
| `leftHipTorque` / `rightHipTorque` | `float` | **Nm** | **Estimated** joint torque. There is no torque sensor; the value is converted internally from motor current (`Kt 0.085 × gear 18.75 ≈ 1.594`). It is already converted — do not multiply by Kt again |
| `leftHipMotorAngle` / `rightHipMotorAngle` | `float` | deg | Left/right motor encoder angle (may differ from joint angle due to gear ratio) |
| `left/rightHipImuFrontalRoll` | `float` | deg | Hip IMU Frontal Roll |
| `left/rightHipImuSagittalPitch` | `float` | deg | Hip IMU Sagittal Pitch |
| `left/rightHipImuTransverseYaw` | `float` | deg | Hip IMU Transverse Yaw |
| `left/rightHipImuGlobalAccX/Y/Z` | `float` | m/s² | Hip IMU global acceleration |
| `left/rightHipImuGlobalGyrX/Y/Z` | `float` | deg/s | Hip IMU global gyroscope |

---

### `XM_GRF_SPACE_e`

Left/right identifier for the legacy GRF (FSR) module.

```c
typedef enum {
    XM_SPACE_LEFT = 1,
    XM_SPACE_RIGHT,
    XM_SPACE_UNKNOWN,
} XM_GRF_SPACE_e;
```

---

### `XmGrfData_t`

**Ground-reaction-force (foot pressure) sensor data**, accessed via `XM.status.grf`. The legacy 14-channel MarvelDex FSR shoe fields are always present; the 🟢 **Rev 2.0 only** 24-channel fixed-frame (SM-GRF) fields are only included in the struct when the `XM_GRF_FIXED_FRAME_MODULE` macro is on (Rev 2.0).

| Field | Type | Unit | Description |
|---|---|---|---|
| `is_left_grf_connected` / `is_right_grf_connected` | `bool` | - | Left/right GRF module connection status |
| `leftLastUpdateTick` / `rightLastUpdateTick` | `uint32_t` | ms | Left/right data receive timestamp |
| `leftSensorSpace` / `rightSensorSpace` | `XM_GRF_SPACE_e` | - | 1=left foot, 2=right foot |
| `leftRollingIndex` / `rightRollingIndex` | `uint8_t` | - | Packet sequence (0~199) |
| `leftSensorData[14]` / `rightSensorData[14]` | `uint8_t[XM_GRF_CHANNEL_SIZE]` | 0~255 raw | 14-channel FSR values |
| `leftBatteryLevel` / `rightBatteryLevel` | `uint8_t` | 0~100 | Battery level |
| `leftStatusFlags` / `rightStatusFlags` | `uint8_t` | - | Status flags |
| `leftFsr[24]` / `rightFsr[24]` 🟢 Rev 2.0 only | `uint16_t[XM_GRF_FSR_CH_TOTAL]` | ADC LSB | SM-GRF 24ch FSR raw values (ADC1 15ch + ADC3 9ch) |
| `leftImu[7]` / `rightImu[7]` 🟢 Rev 2.0 only | `int16_t[7]` | raw | 6-axis IMU raw values in `acc[3], gyr[3], temp` order |
| `leftGrfTick` / `rightGrfTick` 🟢 Rev 2.0 only | `uint32_t` | ms | SM-GRF module control tick — used to judge gap/freshness across consecutive receives |

> ℹ️ **L/R is based on the physical port**, not on packet content (header comment: right-foot GRF → XM's right-side port = UART8). This is separate from the legacy fields' "1=left,2=right" discrimination — the Rev 2.0 extension fields are hard-wired to a fixed left/right port.

---

### `XmExtImuData_t`

**External UART precision IMU** data (currently Xsens MTi-630), accessed via `XM.status.ext_imu`. The "ext_imu" prefix distinguishes it from the CAN-FD-based IMU Hub, and the name is kept even if the device is swapped for a different UART IMU.

| Field | Type | Unit | Description |
|---|---|---|---|
| `is_connected` | `bool` | - | External IMU connection status |
| `lastUpdateTick` | `uint32_t` | ms | Data receive timestamp |
| `q_w, q_x, q_y, q_z` | `float` | - | Orientation (quaternion) |
| `acc_x, acc_y, acc_z` | `float` | m/s² | Calibrated acceleration |
| `gyr_x, gyr_y, gyr_z` | `float` | deg/s or rad/s | Calibrated gyroscope (header comment lists both units — check per device) |

---

### `XmImuHubSensor_t` / `XmImuHubData_t`

**IMU Hub Module** data (EBIMU-9DOFV6 × 6, DOP V3), accessed via `XM.status.imu_hub`.

```c
typedef struct {
    float q_w, q_x, q_y, q_z;           // orientation (quaternion)
    float roll, pitch, yaw;             // orientation (Euler, deg)
    float acc_x, acc_y, acc_z;          // calibrated acceleration (g)
    float gyr_x, gyr_y, gyr_z;          // calibrated gyroscope (deg/s)
    float mag_x, mag_y, mag_z;          // calibrated magnetometer (uT)
} XmImuHubSensor_t;
```

| Field | Type | Unit | Description |
|---|---|---|---|
| `q_w, q_x, q_y, q_z` | `float` | - | Orientation (quaternion) |
| `roll, pitch, yaw` | `float` | deg | Orientation (Euler angles) |
| `acc_x, acc_y, acc_z` | `float` | g | Calibrated acceleration |
| `gyr_x, gyr_y, gyr_z` | `float` | deg/s | Calibrated gyroscope |
| `mag_x, mag_y, mag_z` | `float` | uT | Calibrated magnetometer |

`XmImuHubData_t` (the top-level struct wrapping all 6 sensors in an array):

| Field | Type | Description |
|---|---|---|
| `is_connected` | `bool` | IMU Hub Module connection status |
| `lastUpdateTick` | `uint32_t` | Data receive timestamp (ms) |
| `sensor[6]` | `XmImuHubSensor_t[XM_IMU_HUB_SENSOR_COUNT]` | IMU sensor data for ports 0~5 |
| `connected_mask` | `uint8_t` | Per-sensor connection bitmask (bit0~5) |

**Example** (pattern from Ex.41 IMU Hub Dashboard)
```c
const XmImuHubData_t* hub = &XM.status.imu_hub;
if (hub->is_connected) {
    float roll0 = hub->sensor[0].roll;   // port 0 IMU's Roll (deg)
    bool port2_ok = (hub->connected_mask & (1u << 2)) != 0;
}
```

---

### `XmEmgHubData_t`

**EMG Hub Module** data (sEMG sensor hub, DOP V3), accessed via `XM.status.emg_hub`. Sourced from the EMG Hub's TPDO1 (CAN ID `0x18F`), 1kHz sampling, including the results of the HPF 20Hz → rectification → RMS 200ms → envelope 8Hz → MVC → activation pipeline.

| Field | Type | Unit | Description |
|---|---|---|---|
| `is_connected` | `bool` | - | EMG Hub Module connection status |
| `lastUpdateTick` | `uint32_t` | ms | Slave control tick (OD `0x6050 ctrl_tick_ms`, 32-bit, wraps at ~49.7 days). If the delta between consecutive receives isn't 1, it's a gap. Falls back to the 24-bit Metadata timestamp on the older 14B TPDO format |
| `raw_adc` | `uint16_t` | 12-bit (HW OVS 16×) | Raw ADC value |
| `voltage_uv` | `float` | µV | EMG voltage (AFE-converted) |
| `rms_uv` | `float` | µV | RMS value (200ms sliding window) |
| `envelope_uv` | `float` | µV | Envelope value (EMA fc≈8Hz) |
| `mvc_percent` | `uint8_t` | 0~100% | MVC normalization |
| `is_active` | `bool` | - | Muscle contraction detection (Schmitt trigger) |
| `status_flags` | `uint8_t` | bitfield | See the table below |

**`status_flags` bit meanings:**

| Bit | Name | Meaning |
|---|---|---|
| bit0 | `ADC_OK` | ADC normal |
| bit1 | `IS_ACTIVE` | Muscle-contraction detection state |
| bit2 | `SATURATED` | Signal saturated |
| bit3 | `CALIB_VALID` | MVC calibration valid |

**Example** (pattern from Ex.42 EMG Hub Biofeedback)
```c
const XmEmgHubData_t* emg = &XM.status.emg_hub;
if (emg->is_connected) {
    bool calib_valid = (emg->status_flags & (1u << 3)) != 0U;
    uint8_t mvc = emg->mvc_percent;
}
```

---

### `XmFesHubData_t`

**FES Hub Module** feedback data (functional electrical stimulation, DOP V3 ES-vector, Node `0x0C`), accessed via `XM.status.fes_hub`. Sourced from the FES Hub's TPDO1 (CAN ID `0x18C`, 37B, 10ms period) — composed of a legacy 16B block (channel state/current/HV, etc.) plus a 21B KHJ extension (FSM/ISI/target amplitude, etc.). Commands are sent via SDO (`0x6300` ES-vector, `0x6310` Master Command), not through this struct.

| Field | Type | Unit | Description |
|---|---|---|---|
| `is_connected` | `bool` | - | FES Hub Module connection status |
| `lastUpdateTick` | `uint32_t` | ms | Data receive timestamp (FES slave 24-bit timestamp, LE) |
| `ch_state[2]` | `uint8_t[XM_FES_HUB_CH_COUNT]` | 0~3 | Channel state: 0=IDLE, 1=READY, 2=STIMULATING, 3=FAULT |
| `ch_current_mA[2]` | `float[2]` | mA | Current feedback (PID output) |
| `ch_fault_code[2]` | `uint8_t[2]` | - | Per-channel fault code |
| `hv_voltage_V` | `float` | V | HV boost converter output voltage |
| `digipot_pos` | `uint8_t` | 0~127 | Digipot position (amplitude control) |
| `es_state_packed` | `uint8_t` | bitfield | `[3:0]`=CH0 ESState, `[7:4]`=CH1 ESState |
| `error_register` | `uint8_t` | - | Error register |
| `fsm_state` / `fsm_state_prev` | `uint8_t` | - | Current/previous FSM state (KHJ Control Task) |
| `isi_packed` | `uint8_t` | bitmap | `bit[N]`=ISI[N]. EXT7(bit7)/EXT8(bit8)=trace of a Master Command action |
| `ch_es_error_lo[2]` | `uint8_t[2]` | - | ES-vector error code low byte (per channel, separate from the CiA 301 Abort code) |
| `ch_target_amplitude_mA[2]` | `float[2]` | mA | Target amplitude (setpoint) commanded by the Master |
| `ch_impedance[2]` | `float[2]` | ohm | Filtered impedance — an indicator for judging electrode contact |
| `ch_pulse_cnt[2]` | `uint16_t[2]` | - | Cumulative stimulation pulse counter |
| `ch_voltage_diff_V[2]` | `float[2]` | V | Actual stimulation differential voltage |

**See also**: the channel index (0/1) and array sizes are defined by [`XM_FES_HUB_CH_COUNT`](#macros).

---

### `XmInput_t`

The **integrated robot state struct**, accessed via `XM.status`. All read-only sensor data is collected here.

```c
typedef struct {
    XmH10Data_t     h10;      // KIT H10 body data
    XmGrfData_t     grf;      // GRF foot-pressure sensor data
    XmExtImuData_t  ext_imu;  // External UART IMU (Xsens MTi-630)
    XmImuHubData_t  imu_hub;  // IMU Hub sensor data (DOP V3)
    XmEmgHubData_t  emg_hub;  // EMG Hub sensor data (DOP V3)
    XmFesHubData_t  fes_hub;  // FES Hub stimulation feedback (DOP V3)
} XmInput_t;
```

---

### `XmOutput_t`

The **robot control command struct**, accessed via `XM.command`.

```c
typedef struct {
    XmControlMode_t control_mode;
    float assist_torque_rh;
    float assist_torque_lh;
    struct {
        uint8_t torque_rh_updated : 1;
        uint8_t torque_lh_updated : 1;
    } _dirty_flags;
} XmOutput_t;
```

| Field | Type | Description |
|---|---|---|
| `control_mode` | `XmControlMode_t` | Current control mode |
| `assist_torque_rh` / `assist_torque_lh` | `float` | Right/left assist torque (Nm) |
| `_dirty_flags` | anonymous bitfield struct | ⚠️ **Internal only** — header comment: "the user doesn't need to know this, the helper functions manage it." Don't write to it directly; always use `XM_SetAssistTorque()`-family functions |

> ⚠️ **Do not write directly**: `XM.command` is a staging area. Assigning to these fields directly does not set the Dirty Flag, so nothing actually gets transmitted. Always use setter functions such as [`XM_SetAssistTorque()`](#xm_setassisttorque).

---

### `XmRobot_t` (global instance `XM`)

```c
typedef struct {
    XmInput_t  status;  // [Read] sensor values (Input)
    XmOutput_t command; // [Write] command values (Output)
} XmRobot_t;

extern XmRobot_t XM;
```

| Field | Type | Description |
|---|---|---|
| `status` | `XmInput_t` | Read-only sensor data (H10/GRF/ExtIMU/IMU Hub/EMG Hub/FES Hub) |
| `command` | `XmOutput_t` | Write-only control-command staging area — don't write directly, use `XM_Set*` functions |

**Example**
```c
float angle = XM.status.h10.leftHipAngle;  // read the left hip angle
```

---

## Related Documents

- [02. KIT H10 Control & Data](../02-h10-control-n-data.en.md) — IPO cycle, torque sign convention, Body Data details, common mistakes
- [README — API Reference Index](../README.en.md) — full function-group table of contents, per-example Body Data requirements
