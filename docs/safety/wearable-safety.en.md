# Wearable Safety One-Pager — EMG-based PhAI-X1 Assist Control

> **Audience**: Any team running their own EMG → torque code on PhAI-X1 (H10 + XM10) **while a person is wearing the suit** and applying **real assist force**.
> **Principle**: You are free to design your algorithm, but the guardrails below are **non-negotiable**. Assist force goes directly into a human leg.

---

## 0. One-Line Rule

> **Never run unverified code in wearable mode — bench-test everything first.**

---

## 1. Bench First (Required Before Wearing)

With torque output disabled (`control_ON = 0` or `XM_CTRL_MONITOR`), verify the **signal, direction, and magnitude** first.

1. Connect the EMG sensor and check the **raw / envelope** signal in PhAI Studio (or Live Expressions).
2. Relax the muscle → resting calibration (BTN1); representative contraction → active calibration (BTN2). **Order must be BTN1 → BTN2.**
3. Confirm envelope is ~0 at rest and rises on contraction (increase deadband if noise is high).
4. **Direction check**: verify that the intended muscle contraction maps to the intended joint direction (Flexion / Extension) — sign convention is **positive(+) = Flexion, negative(−) = Extension**.
5. **Left/right check**: `XM_SetAssistTorque(R, L)` takes **right first**. If in doubt, use `XM_SetAssistTorqueRH/LH()` to set one side at a time. Reversed left/right can cause a fall.
6. Gradually increase torque from very small values (e.g., 0.2~0.5 Nm) and observe motor response in the **bench-mounted state**.
7. Re-confirm that torque is nearly 0 when relaxed → only then set `control_ON = 1`.

---

## 2. Torque Limits (Do Not Exceed)

| Category | Value | Nature |
|---|---|---|
| EMG example default output | **1.25 Nm** (`EMG_MAX_TORQUE_NM 2.5 × scale 0.5`) | Conservative starting point — recommended starting value |
| Example soft ceiling | 2.5 Nm | Saturation inside the example code |
| **XM10 internal hard clamp** | **±10 Nm** | Inside the library (`libXM_Lib.a`) — **users cannot change this**; inputs above this value saturate |
| Motor-driver protection | 14A / impedance 10A | Additional saturation on the H10 side |

> Set limits **conservatively** to match the wearer and purpose. Never start with a large value.

---

## 3. Emergency Stop Paths (Memorize These)

- **Suit STANDBY button** — Physical button on the H10 body. Switching to STANDBY immediately zeroes torque and commands a gentle stop before holding position. **This is the fastest stop.**
- **Extension cable disconnect / CM link lost** — When `XM_IsCmConnected()` returns `false`, the firmware **automatically switches OFF** (torque 0).
- **Cut power** — Last resort.

> ⚠️ The XM10 firmware has no hardware watchdog. Therefore, **in an emergency a person must use one of the above paths to stop the suit manually** — this is the primary safety net. A dedicated **safety assistant** who can reach the stop control must always be present alongside the wearer.

---

## 4. Four Code Sections That Must Not Be Removed

Even when modifying the algorithm in the EMG example, **keep the following in place**:

1. **Final torque saturation** — limit clamp immediately before output.
2. **Zero-torque during calibration** — force torque to 0 while calibration is in progress.
3. **Zero-torque on mode exit** — in `Active_Exit`, set torque to 0 and return to `XM_SetControlMode(XM_CTRL_MONITOR)`.
4. **`XM_SetControlMode`** — gate for entering `XM_CTRL_TORQUE` / returning to `MONITOR`.

---

## 5. Pre-Wear Checklist ✅

- [ ] Passed bench steps 1–7 (direction, left/right, and magnitude verified)
- [ ] Starting assist torque is conservative (≈ 1 Nm or less)
- [ ] Emergency stop path (suit STANDBY) located + **safety assistant standing by**
- [ ] Four must-not-remove code sections remain intact
- [ ] Calibration order BTN1 → BTN2 understood
- [ ] First motion after donning: **small torque / slow speed**

---

## 6. Monitoring During Wear

- Use PhAI Studio to monitor **envelope, output torque, and joint angle** in real time throughout the session.
- If torque spikes unexpectedly or fails to return to 0 on relaxation, **immediately press suit STANDBY**.

---

> This document defines safety guardrails only. **Which muscle to read, what intent to infer, and how much assist to apply** is your team's design space.
