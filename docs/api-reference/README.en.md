# API Reference

> Parent: [Documentation Index](../README.md) · [Examples Guide](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/README.md)

This is a function reference to keep open while writing your algorithm. You don't need to read it cover to cover — just look up the relevant group when you need it.

---

## How to Read This — Two Layers

The API documentation is split into **two layers with different jobs**. Open the one that matches what you're looking for.

| Layer | Location | Covers | Open this when |
|-------|----------|--------|-----------------|
| **① Concept Guides** | This document (01–09, the [Function Groups](#function-groups) table below) | **Why** the API exists and **how** to weave it into your algorithm — background, common pitfalls, example code flow | You don't yet know how to combine an API you're using for the first time |
| **② Function Reference** | [`ref/`](ref/README.en.md) — one page per header | **Exact** function signatures, parameter types/ranges, return values, struct fields | You need to double-check a function name, argument order, or exact return meaning |

The two layers don't replace each other. Each concept-guide entry links to its matching `ref/` page, and every `ref/` page links back to its matching concept guide — if you get stuck, jump to the other side.

→ **Full function reference index**: [`ref/README.en.md`](ref/README.en.md)

---

## Function Groups

| # | Document | Contents | Commonly Used Functions |
|---|----------|----------|------------------------|
| 01 | [Task State Machine (TSM)](01-task-state-machine.md) | Per-state behavior separation + transitions | `XM_TSM_Create`, `XM_TSM_AddState`, `XM_TSM_Run`, `XM_TSM_TransitionTo` |
| 02 | [KIT H10 Control + Data](02-h10-control-n-data.md) | Exoskeleton sensor reads + torque/position commands | `XM.status.h10.*`, `XM_SetAssistTorque`, `XM_SendPVector`, `XM_SendIVector` |
| 03 | [LED + Button](03-led-btn-control.md) | Board LED effects, button events | `XM_SetLedEffect`, `XM_SetLedState`, `XM_GetButtonEvent`, `XM_SetChannelLedRGB` |
| 04 | [External IO](04-external-io.md) | Expansion port GPIO/ADC | `XM_SetPinMode`, `XM_DigitalRead/Write`, `XM_AnalogReadMillivolts` |
| 05 | [USB Serial Communication](05-usb-connectivity.md) | Send text/binary data to a PC | `XM_SendUsbDebugMessage`, `XM_SendUsbDataWithId`, `XM_SetUsbCustomMeta` |
| 07 | [Memory Regions](07-memory-management.md) | Fast memory + non-volatile storage | `XM_GetUserWorkspace`, `XM_UserNV_Read/Write` |
| 08 | [Real-Time Clock](08-rtc-clock.md) | Read and write date/time | `XM_RTC_SetDateTime`, `XM_RTC_GetDateTime`, `XM_RTC_IsRunning` |
| 09 | [Auxiliary Tasks + Data Sharing](09-task-creation.md) | Tasks separate from the 1 kHz control loop + safe data sharing | `XM_Task_CreateOneShot/Periodic`, `XM_Mutex_Lock/Unlock` |

---

## Body Data — Read This Before Using Gait Analysis Data

The KIT H10 central module analyzes the user's gait at 1 kHz and estimates the data listed below. However, this analysis requires the user's body parameters (weight, height, and leg lengths) to be accurate. In any example that uses this data — especially Ex.23 and above — you **must** call `XM_SendUserBodyData()` inside `Control_Setup()`.

### What Data?

| Data | API Path | Description |
|------|----------|-------------|
| `isRightFootContact` | `XM.status.h10.isRightFootContact` | Right foot contact state |
| `isLeftFootContact` | `XM.status.h10.isLeftFootContact` | Left foot contact state |
| `forwardVelocity` | `XM.status.h10.forwardVelocity` | Forward velocity (m/s) — improved accuracy when Body Data is configured |

> **Gait phase (`gaitCycle` / `gaitState`) is not directly exposed in the public struct `XmH10Data_t`.** If you need gait cycle information, compute it yourself from `forwardVelocity` and foot contact (`isLeftFootContact` / `isRightFootContact`) — see Ex.23 for this approach.

### Symptoms When Not Configured

- Gait phase estimation from `forwardVelocity` is inaccurate, causing phase-sync errors
- `footContact` is always 0 — stance and swing phases cannot be distinguished
- `forwardVelocity` values are inaccurate

### How to Configure

```c
void Control_Setup(void)
{
    // ⚠️ Must be set for accurate H10 gait analysis.
    // Use measured values whenever possible; standard body approximations are acceptable otherwise.
    uint32_t body_data[8] = {
        70000,  // [0] Body weight (g)            — e.g. 70 kg
        1750,   // [1] Height (mm)                — e.g. 175 cm
        450,    // [2] Right thigh length (mm)
        450,    // [3] Left thigh length (mm)
        420,    // [4] Right shank length (mm)
        420,    // [5] Left shank length (mm)
        60,     // [6] Right ankle height (mm)
        60,     // [7] Left ankle height (mm)
    };
    XM_SendUserBodyData(body_data);
    // ...
}
```

### Body Data Requirements per Example

| Example | Body Data | Reason |
|---------|:---------:|--------|
| 00–19 | Mostly not required | Basic I/O, USB, and simple control |
| 20 Impedance | ✗ | Uses only joint angle and torque feedback |
| 21 Gravity Compensation | ✗ | Gravity calculation based on joint angle |
| 22 CPG Oscillator | △ | Required in `gaitCycle` mode; can be replaced with angle-feedback mode |
| 23 Gait Phase Adaptation | ✔ Required | `gaitCycle` is the phase source for the torque profile |
| 24 Virtual Constraints (HZD) | ✔ Required | `gaitCycle` is the sole source for the phase variable |
| 25 Stance-Phase Variable Stiffness | ✔ Required | Cannot distinguish stance/swing without `footContact` |
| 26 ILC | ✔ Required | `gaitCycle` is the learning index source |
| 27 MRAC | ✗ | Direct joint-angle feedback |
| 28 Admittance | ✗ | Uses only measured torque feedback |
| 29 Bilateral Coordination | △ | Angle-based motion; accuracy improves with Body Data |
| 30 FF+FB Combined | ✗ | Model parameters are set directly via macros |

> If you need gait phase information without `gaitCycle` or `footContact`, you can use external sensors such as an IMU Hub or GRF shoes for independent measurement.

---

## Quick Reference — Commonly Used Functions

### Exoskeleton Sensor Data

```c
// Hip joint angles
float angle_r = XM.status.h10.rightHipMotorAngle;   // Right (deg)
float angle_l = XM.status.h10.leftHipMotorAngle;    // Left (deg)

// Hip motor current (field name says Torque, but unit is A — multiply by 1.594 ≈ Nm)
float cur_r = XM.status.h10.rightHipTorque;         // Right (A)
float cur_l = XM.status.h10.leftHipTorque;          // Left (A)

// Gait analysis data (accuracy improves when Body Data is configured)
bool    right_gnd = XM.status.h10.isRightFootContact;
bool    left_gnd  = XM.status.h10.isLeftFootContact;
float   fwd_vel   = XM.status.h10.forwardVelocity;  // m/s
// gaitCycle is not a public field — compute it from fwd_vel + footContact (Ex.23)

// IMU (preprocessed orientation angles)
float pitch_r = XM.status.h10.rightHipImuSagittalPitch; // Sagittal-plane pitch (deg)
float roll_r  = XM.status.h10.rightHipImuFrontalRoll;   // Frontal-plane roll (deg)

// H10 operating mode
bool is_assist = (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST);
```

### Torque Control

```c
// Set control mode
XM_SetControlMode(XM_CTRL_MONITOR);  // Monitor only (default)
XM_SetControlMode(XM_CTRL_TORQUE);   // Enable direct torque control

// Torque commands (valid only in CTRL_TORQUE mode)
XM_SetAssistTorqueRH(float torque_nm);          // Right
XM_SetAssistTorqueLH(float torque_nm);          // Left
XM_SetAssistTorque(float rh_nm, float lh_nm);   // Both sides

// Safe exit pattern (always perform in Active_Exit)
XM_SetAssistTorqueRH(0.0f);
XM_SetAssistTorqueLH(0.0f);
XM_SetControlMode(XM_CTRL_MONITOR);
```

### Pre-Defined Motion Commands (P-Vector / I-Vector)

Instead of computing torque directly, these single-line commands move the exoskeleton with instructions like "go to this position within this time."

```c
// Position command — move to a target angle within a specified time
PVector_t pv = { .yd = -250, .L = 1000, .s0 = 4, .sd = 4 };
// yd = target angle × 10 (deg × 10),  L = duration (ms)
XM_SendPVector(SYS_NODE_ID_RH, &pv);

// Impedance command — behaves like a virtual spring-damper
IVector_t iv = { .epsilon = 0, .kp = 80, .kd = 1, .lambda = 0, .duration = 50 };
// kp / kd: 0–100 (%)
XM_SendIVector(SYS_NODE_ID_RH, &iv);
```

---

## Control System at a Glance

```
┌─────────────────────────────────────────────────────┐
│  KIT H10 Central Module                             │
│  Real-time gait analysis (1 kHz)                   │
│  ┌──────────────┐   Body Data configuration needed  │
│  │ gaitCycle    │← XM_SendUserBodyData()            │
│  │ footContact  │                                    │
│  │ forwardVel   │                                    │
│  └──────────────┘                                   │
│  Hip motor angle / estimated torque                 │
└──────────────────┬──────────────────────────────────┘
                   │ CAN-FD (1 ms sensor data period)
┌──────────────────▼──────────────────────────────────┐
│  XM10                                               │
│  ┌───────────────────────────────────────────────┐  │
│  │  Control_Loop() — called every 1 ms               │  │
│  │                                               │  │
│  │  Read XM.status.h10.*                         │  │
│  │      ↓                                        │  │
│  │  Run your algorithm (e.g. τ = K·e + B·ė)      │  │
│  │      ↓                                        │  │
│  │  XM_SetAssistTorque(RH, LH)                   │  │
│  └───────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────┘
```

---

## Torque Control Examples at a Glance

### Exoskeleton Basic Modes (Ex.11–13)

| Example | Control Method | Body Data |
|---------|---------------|:---------:|
| 11 Passive | Pre-defined motion commands (P/I-Vector) | ✗ |
| 12 Active Assist | Intent detection + stepped torque | ✗ |
| 13 Resistive | τ = −B·ω (velocity resistance) | ✗ |

### Torque Control Introduction (Ex.14–15)

| Example | Control Method | Body Data |
|---------|---------------|:---------:|
| 14 PD Control | τ = Kp·e + Kd·ė | ✗ |
| 15 Inverted Pendulum Model | τ = Mgl·sin(θ) + PD | ✗ |

### Control Research Series (Ex.20–30)

| Example | Control Method | Body Data | Reference |
|---------|---------------|:---------:|-----------|
| 20 Impedance | τ = K·(θ_d−θ) + B·θ̇ | ✗ | Hogan 1985 |
| 21 Gravity Compensation | τ = α·(Mgl·cos θ + B_f·θ̇) | ✗ | Just 2018 |
| 22 CPG Oscillator | φ̇ = ω + ε·F·cosφ | △ | Ronsse 2011 |
| 23 Gait Phase Adaptation | τ(φ) = A·sin(π·φ) per segment | ✔ | Quinlivan 2017 |
| 24 Virtual Constraints (HZD) | θ_d(s) = Bézier(s) | ✔ | Westervelt 2003 |
| 25 Stance-Phase Variable Stiffness | K_stance / K_swing switching | ✔ | Collins 2015 |
| 26 ILC | τ_{k+1} = τ_k + L·e_k | ✔ | Emken 2007 |
| 27 MRAC | MIT Rule adaptive gain | ✗ | Slotine 1991 |
| 28 Admittance | τ_ext → virtual dynamics → θ_ref | ✗ | Keemink 2018 |
| 29 Bilateral Coordination | τ_R = −K_c·(θ_R+θ_L) | △ | Duschau-Wicke 2010 |
| 30 FF+FB Combined | τ = τ_ff(model) + τ_fb(PD) | ✗ | Slotine 1991 Ch.6 |

---

## Related Documents

- [Getting Started](../getting-started/) — Environment setup + first build
- [Architecture](../architecture/) — System overview
- [KIT H10 Firmware](../kit-h10-firmware/) — Firmware compatibility
- [Tutorials](../tutorials/) — Example learning path
