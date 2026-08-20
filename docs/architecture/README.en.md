# System Architecture

This page gives you the big picture of **how your code runs inside XM10**. The goal is not deep implementation detail — it is to help first-time users understand "where my code fits, and who handles everything else."

---

## The Big Picture

The `KIT H10` exoskeleton robot is built on a distributed control architecture modeled after the human motor-control system. Each subsystem has its own responsibility, so a slowdown in one part does not freeze the whole system.

<p align="center">
  <img width="712" height="351" alt="Angel Robotics Module Architecture" src="https://github.com/user-attachments/assets/911991bc-8c85-4590-8177-47c9af677cbf" />
</p>

This is the distributed control structure of KIT H10 itself. Each motor driver operates independently while the central control module (CM) coordinates the whole system.

<p align="center">
  <img width="753" height="383" alt="KIT H10 Distributed Control" src="https://github.com/user-attachments/assets/656dbcc5-6e87-495b-8d1c-9741060ee3d6" />
</p>

Adding XM10 creates the integrated system. XM10 is a "brain-expansion" board that issues commands directly to the exoskeleton and receives sensor data so you can prototype new algorithms.

<p align="center">
  <img width="1934" height="987" alt="KIT H10 + XM10 System Architecture" src="https://github.com/user-attachments/assets/29428128-6802-4bbf-ad9f-51a806e92d2b" />
</p>

---

## XM10 Firmware — Where Does My Code Live?

XM10's firmware is organized in layers. Users only ever touch **the top layer**.

<p align="center">
  <img width="1984" height="900" alt="xm10-system-architecture" src="https://github.com/user-attachments/assets/16cea885-2f31-4e1e-a59f-c2eb6dc6d985" />
</p>

| Layer | Who builds it | What you do |
|-------|--------------|-------------|
| **User algorithm** (`Control_Task/control_task.c`) | You | ✅ Write here only |
| **XM API** — thin call interface (`XM_*` functions) | XM library | Call, don't modify |
| **Internal system** — sensor comms, USB, exoskeleton protocol, etc. | XM library | Do not touch |
| **ST platform code** — OS, USB driver, board peripherals | ST + open source | Do not touch |

Think of it like driving a car: you are the driver. You operate the accelerator (`XM_SetAssistTorqueRH`), the brake (`XM_SetControlMode`), and the steering wheel (`XM_GetButtonEvent`). The engine, transmission, and ECU are all managed by the XM library — you never open the hood.

> **Why this split?**
> If every user had to implement CAN communication, USB transmission, and motor command parsing from scratch, bugs would be frequent and no one's code would be reusable by others. By moving shared infrastructure into the library and exposing only the algorithm entry point, the system stays reliable while you stay focused on what matters — the control algorithm itself.

---

## When Does My Code Run — The 1 ms Control Loop

The heart of XM10 is a **control loop that repeats every 1 ms (1 kHz)**. Each millisecond, the following three things happen in order:

```
  ┌────────────────────────────────────────────────────────────┐
  │           Repeats every 1 ms  (1 kHz control loop)        │
  │                                                            │
  │   [INPUT]           [MY CODE]           [OUTPUT]           │
  │     ↓                  ↓                   ↓               │
  │  Sensor data         Control_Loop()     XM functions       │
  │  received via  →    runs your      →   send CAN commands  │
  │  CAN is auto-        algorithm           to KIT H10        │
  │  refreshed                                                  │
  │                                                            │
  │  XM.status.h10.    XM_TSM_Run         XM_SetAssistTorque  │
  │   leftHipAngle      → on_loop()        XM_SendUsbData     │
  │   rightHipAngle                                            │
  │   isLeftFootContact                                        │
  └────────────────────────────────────────────────────────────┘
                              ↓ repeats 1 ms later
```

- **Input (automatic)**: KIT H10 sensor and status data received over CAN-FD is written into the `XM.status.h10.*` global variables automatically every cycle.
- **Processing (you write this)**: Inside `Control_Loop()`, read that data and run your algorithm.
- **Output (automatic)**: Call a function such as `XM_SetAssistTorqueRH(...)` and the library converts it to a CAN message and sends it to KIT H10 automatically.

USB communication works the same way — a single function call is all you need. Your only concern is the algorithm.

---

## From Power-On to Your Code

After you flash firmware with `Debug (F11)`, the board starts up in the following sequence:

```
  Power ON
    ↓
  Boot code + board initialization  (automatic)
    ↓
  XM library initialization
    — peripheral setup, USB ready, CAN link, sensor discovery, etc.
    ↓
  Internal scheduler starts
    ↓
  ┌──────────────────────────────────────────────────────────────┐
  │  Scheduler runs multiple tasks by priority, round-robin:    │
  │                                                              │
  │   • User task       → Control_Setup() once → Control_Loop() every 1 ms  │
  │   • CAN RX          → receives exoskeleton sensor data, updates XM.status │
  │   • CAN TX          → sends motor commands                  │
  │   • USB task        → USB-CDC serial communication          │
  │   • Device discovery → auto-connects sensor hubs            │
  └──────────────────────────────────────────────────────────────┘
```

Your code has two entry points:

- `Control_Setup()` — called once immediately after boot. Put initialization code here (LED state setup, TSM registration, etc.).
- `Control_Loop()` — called every 1 ms. Put your algorithm body here.

Other tasks such as CAN RX run concurrently, but you do not need to manage them. The system handles everything.

---

## Where Is My File?

```
Extension_Module/
├── XM_FW/
│   ├── XM_Apps/
│   │   └── Control_Task/
│   │       └── control_task.c     ← your workspace
│   └── (all other folders are XM library — do not modify)
└── examples/                  ← 45 example control_task.c files
    ├── 00_Quick_Start/quick_start.c
    ├── 14_PD_Realtime_Control/pd_realtime_control.c
    └── ...
```

To try an example, copy the entire contents of that folder's `.c` file into `XM_Apps/Control_Task/control_task.c` and build.

---

## Communication at a Glance

There are two channels — that is all you need to know.

| Channel | Who talks to whom | Functions you use |
|---------|-------------------|------------------|
| **CAN-FD** | XM10 ↔ KIT H10 exoskeleton | Read `XM.status.h10.*`, write `XM_SetAssistTorque*()` |
| **USB Serial (CDC)** | XM10 → PC terminal / PhAI Studio | `XM_SendUsbDebugMessage`, `XM_SendUsbDataWithId` |

The underlying protocol details are handled by the library. You only need to call the functions.

---

## Common Mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Code added to `main()` does nothing | `main()` starts the internal scheduler and exits immediately. User code belongs in `control_task.c` | Only modify `XM_Apps/Control_Task/control_task.c` |
| `while(1)` used inside `Control_Loop` | The loop never returns, starving all other tasks | `Control_Loop` must run once and return — the system handles the repetition |
| `HAL_Delay(100)` used inside `Control_Loop` | Blocks all other tasks for 100 ms | Use the difference of `XM_GetTick()` to build a non-blocking timer |
| `XM.status.h10.leftHipAngle` is always 0 | KIT H10 not connected, or assist mode not yet entered | Check `XM_IsCmConnected()` and confirm `h10Mode == XM_H10_MODE_ASSIST` |
| Called `XM_SetAssistTorque*` but no torque output | Torque control mode was never set | Call `XM_SetControlMode(XM_CTRL_CONTROL)` once when entering Active |
| Modified files in the XM library folders (IOIF, Devices, etc.) | Library code is production firmware — arbitrary changes break the system | Always work inside `Control_Task/` only |
| `Control_Loop` does not finish within 1 ms | Heavy `sprintf` calls, accumulated floating-point operations, etc. | Measure execution time with [Ex.18 Debug Monitor](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/18_Debug_Monitor/) and spread the work across cycles |
| Attempting to build two examples at the same time | Only one `.c` file is allowed in `Control_Task/` | Copy one example at a time and build |

---

## Next Steps

- First build and flash: [Getting Started — 03 First Build](../getting-started/03-first-build.md)
- State machine pattern: [TSM API Reference](../api-reference/01-task-state-machine.md)
- Example learning path: [Tutorials](../tutorials/README.md)
- Verify your code finishes within 1 ms: [Ex.18 Debug Monitor](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/18_Debug_Monitor/)
