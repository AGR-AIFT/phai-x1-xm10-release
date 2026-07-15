# Tutorials — Learning Through 50 Examples

XM10's features are covered through 50 progressively structured examples. Each folder contains source code and a README, and every README follows the same format: Goal → Prerequisites → Key Code → Experiments → Next Steps + Common Mistakes. The introductory examples (Ex.10 and below) are designed to complete in under 30 minutes. Control and advanced examples (Ex.11 and above) may take anywhere from 45 minutes to several weeks, depending on difficulty.

The full example catalog is at [examples/README.md](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/README.md). If you get stuck, check the "Common Mistakes" section at the bottom of each README first. Claude Code users can simply say `"Ex.XX isn't working"` and the `example-helper` skill will respond with the relevant common mistakes and troubleshooting documentation.

---

## Recommended Learning Paths

Choose the path that matches your current level.

| Path | Target Audience | Sequence | Estimated Time |
|------|-----------------|----------|----------------|
| Beginner | New to embedded systems — just get it working | Ex.00 → 01 → 04 → 07 → 10a → 11 | About 3 hours |
| Intermediate | Familiar with embedded basics — communication, logging, real-time control | Ex.02 → 05b → 08 → 10b → 12 → 14 → 18 | About 1 week (1–2 hours/day self-study) |
| Advanced | Control algorithms + hands-on AI design | Ex.03 → 09 → 10c → 15 → 16 → 17 → 19 → 20+ | About one semester (1–2 weeks per example from Ex.20+) |
| Physical AI Applications | Rehabilitation, learning-based control, etc. | Ex.21 → 31 → 32 → 33 → 36 | Self-directed (difficulty: ⭐⭐⭐) |

Estimated times are for typical users. The "one semester" estimate for the Advanced path assumes spending **1–2 weeks per example** for in-depth study. A quick run-through is possible in a few days, but following the variant experiments and cited papers takes significantly longer.

---

## Sample 16-Week Course Schedule

A suggested semester schedule for university courses. Adjust freely based on student background and board delivery timing.

| Week | Topic | Examples | Assignment / Deliverable |
|:---:|------|------|------|
| 1 | Environment setup + first build | Ex.00 | Video demo of board LED turning on |
| 2 | Button + LED + state machine | Ex.01 → 02 → 03 | Add 4 custom LED modes of your own |
| 3 | External GPIO + ADC | Ex.04 → 05 → 05a | Control 4 LEDs using an external switch |
| 4 | Multi-channel ADC + safety switch | Ex.05b → 05c → 06 | Demo simultaneous measurement of 8 FSR channels |
| 5 | USB serial communication | Ex.07 → 08 | Real-time sensor monitoring via PC terminal |
| 6 | Binary streaming + PhAI Studio | Ex.09 | Capture 4-channel graph + analysis report |
| 7 | USB memory logging | Ex.10a → 10b | Collect 10 minutes of data → convert to CSV |
| 8 | **Midterm / Mini Project 1** | (open) | "My board, my data" mini project presentation |
| 9 | KIT H10 exoskeleton basic modes | Ex.11 → 12 → 13 | Comparison video of 3 operating modes |
| 10 | PD real-time control | Ex.14 | Tune your own PD gains + compare step responses |
| 11 | Debugging + memory patterns | Ex.18 → 19 | Health Dashboard result analysis |
| 12 | Gait intent recognition + Tiny AI | Ex.16 → 17 | 7-phase FSM demo + intent signal graph |
| 13 | Impedance + transparent mode | Ex.20 → 21 | Compare virtual spring stiffness variations |
| 14 | Gait phase adaptive torque | Ex.23 → 25 | Measure gait phase estimation accuracy |
| 15 | Learning / adaptive control | Ex.26 or 27 | ILC convergence curve or MRAC adaptive trajectory |
| 16 | **Final Project** | (open application) | Team project presentation + GitHub submission |

Teaching tips:
- The first 2 weeks are where students struggle most with environment setup. Schedule dedicated time for TA or instructor-led check-ins.
- The Week 8 mini project is effective for maintaining motivation. Run it as an open-ended prompt like "build anything using what you've learned from Ex.05–10a."
- For Week 16, teams of 3–4 are recommended. Teams that dive deep into the Physical AI application track (Ex.21, 31, 32, 33) at the end tend to produce the strongest final projects.

Examples not in this schedule (Ex.05d, 15, 22, 24, 28–42) are naturally left for self-directed study or a follow-on course in the next semester.

---

## Part 0 — Board Verification

| Example | Title | Difficulty | What You Learn |
| :---: | :--- | :---: | :--- |
| [00](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/00_Quick_Start/) | Board smoke test | ⭐ | State machine + LED + USB serial — no external hardware required |

---

## Part 1 — XM10 Basic I/O Control

From fundamental I/O to state-based programming (FSM) — the core of embedded control.

### Button & LED (Ex.01 ~ 03)

| Example | Title | Difficulty | What You Learn |
| :---: | :--- | :---: | :--- |
| [01](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/01_Button_LED_Basic/) | Button + LED basics | ⭐ | Poll button state and control LEDs |
| [02](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/02_Button_LED_Event/) | Events + special effects | ⭐⭐ | Detect click events, single-shot LED blink |
| [03](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/03_Button_LED_FSM/) | State machine (FSM) | ⭐⭐ | Long-press mode switching, per-state behavior separation |

### External I/O — GPIO & ADC (Ex.04 ~ 06)

| Example | Title | Difficulty | What You Learn |
| :---: | :--- | :---: | :--- |
| [04](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/04_Ext_IO_Basic/) | External digital control | ⭐⭐ | External switch/LED on expansion port |
| [05](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/05_Ext_IO_analog/) | Analog sensor introduction | ⭐⭐ | Voltage measurement on fixed ADC pins |
| [05a](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/05a_Ext_IO_DIO_to_ADC/) | DIO→ADC dynamic switching | ⭐⭐ | Read FSR by switching a DIO pin to ADC mode |
| [05b](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/05b_Ext_IO_FSR_8ch/) | FSR 8-channel batch read | ⭐⭐ | 8 channels + resolution configuration |
| [05c](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/05c_Ext_IO_Mixed_ADC/) | Mixed ADC 12-channel | ⭐⭐⭐ | Fixed 4 + DIO→ADC 8 |
| [05d](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/05d_Ext_IO_DIO_ADC_Hybrid/) | DIO/ADC hybrid | ⭐⭐⭐ | Digital and analog simultaneously |
| [06](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/06_Ext_IO_Safety_Switch/) | Safety state machine | ⭐⭐ | Limit-switch-based FSM |

> **Recommended ADC series order:** Ex.05 → 05a → 05b → 05c → 05d (progressive difficulty)

---

## Part 2 — USB Communication + Data Logging

Exchange real-time messages with a PC or save data to a USB drive. Essential for debugging and data collection.

> **Note**: The USB serial (CDC) port can only be opened by one program at a time. For Ex.07–09, make sure only a serial terminal **or** PhAI Studio is open — not both. Running them simultaneously will cause a conflict.

### USB Serial Communication — Ex.07 ~ 09

| Example | Title | Difficulty | What You Learn |
| :---: | :--- | :---: | :--- |
| [07](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/07_CDC_Basic_Print/) | USB serial basics | ⭐⭐ | Send text messages to a PC terminal |
| [08](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/08_CDC_Sensor_Print/) | Sensor data monitoring | ⭐⭐ | Real-time `sprintf` output of sensor data |
| [09](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/09_CDC_Stream/) | High-speed binary streaming | ⭐⭐⭐ | PhAI Studio-compatible protocol, 500 Hz transmission |

### USB Memory Logging — Ex.10 ~ 10c

| Example | Title | Difficulty | What You Learn |
| :---: | :--- | :---: | :--- |
| [10](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/10_MSC_Manual_log/) | Manual logging (legacy) | ⭐⭐ | Use 10a/10b/10c instead |
| [10a](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/10a_MSC_Basic_Log/) | Auto-save with one registration | ⭐⭐ | Register a struct once and logging happens automatically |
| [10b](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/10b_MSC_Custom_Struct/) | Custom struct logging | ⭐⭐⭐ | Your own data fields + manual timestamp |
| [10c](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/10c_MSC_Advanced_Log/) | Advanced logging | ⭐⭐⭐ | File rolling, error monitoring, LED feedback |

> Recommended order: Ex.10a → 10b → 10c (Ex.10 is legacy — start with 10a for new projects).

---

## Part 3 — KIT H10 Exoskeleton Basic Modes

The three fundamental operating modes of the KIT H10 exoskeleton robot.

| Example | Title | Difficulty | What You Learn |
| :---: | :--- | :---: | :--- |
| [11](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/11_Passive_Mode/) | Passive Mode | ⭐⭐⭐ | Automatic reciprocal motion using predefined movement commands |
| [12](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/12_Active_Assist_Mode/) | Active Assist Mode | ⭐⭐⭐ | Detect user intent and apply assistive torque |
| [13](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/13_Resistive_Mode/) | Resistive Mode | ⭐⭐ | Built-in H10 mode — resistance sensation like walking through water |

---

## Part 4 — Control Algorithm Fundamentals

Your first hands-on control algorithms.

| Example | Title | Difficulty | What You Learn |
| :---: | :--- | :---: | :--- |
| [14](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/14_PD_Realtime_Control/) | PD real-time torque control | ⭐⭐⭐ | PD formula, discrete differentiation, torque saturation |
| [15](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/15_Inverted_Pendulum_Control/) | Inverted pendulum gait assist | ⭐⭐⭐ | Gravity compensation MgL·sin(θ) + Lyapunov stability |
| [16](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/16_TinyAI_Sensor_Fusion/) | Tiny AI sensor fusion | ⭐⭐⭐ | On-board 3-layer neural network inference |
| [17](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/17_FSM_Gait_Intent/) | Gait intent recognition (FSM) | ⭐⭐⭐ | 7-phase gait state machine + per-phase torque |
| [18](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/18_Debug_Monitor/) | System debug monitor | ⭐⭐ | Loop execution time measurement, status dashboard |
| [19](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/19_Memory_Aware_Design/) | Memory-aware design | ⭐⭐⭐ | Ring buffer, pool allocator, malloc-free implementation |

---

## Part 5 — Advanced Control Algorithms

Interaction dynamics, transparent mode, and gait-phase-based control — the canonical techniques of exoskeleton control.

| Example | Title | Difficulty | What You Learn |
| :---: | :--- | :---: | :--- |
| [20](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/20_Impedance_Control/) | Hogan impedance control | ⭐⭐⭐ | Virtual spring-damper interaction (Hogan 1985) |
| [21](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/21_Gravity_Compensation/) | Gravity compensation (transparent mode) | ⭐⭐⭐ | Mgl·sin(θ) + friction compensation + gradual activation |
| [22](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/22_CPG_Oscillator/) | CPG adaptive oscillator | ⭐⭐⭐ | Automatic gait rhythm synchronization (Ronsse 2011) |
| [23](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/23_Gait_Phase_Adaptive_Torque/) | Gait phase adaptive torque | ⭐⭐⭐ | 4-segment sinusoidal torque profile (Quinlivan 2017) |
| [24](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/24_Virtual_Constraint/) | Virtual constraint (HZD) | ⭐⭐⭐ | 5th-order Bézier trajectory (Westervelt 2003) |
| [25](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/25_Stance_Stiffness_Modulation/) | Stance-phase variable stiffness | ⭐⭐⭐ | Smooth stance/swing transition (Collins 2015) |

---

## Part 6 — Learning + Adaptive Control

Control that learns from each gait cycle or automatically adapts to changes in the user.

| Example | Title | Difficulty | What You Learn |
| :---: | :--- | :---: | :--- |
| [26](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/26_Iterative_Learning_Control/) | Iterative Learning Control (ILC) | ⭐⭐⭐ | Learn torque profile from each cycle (Emken 2007) |
| [27](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/27_MRAC/) | Model Reference Adaptive Control | ⭐⭐⭐ | Online gain adaptation via MIT Rule (Slotine 1991) |
| [28](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/28_Admittance_Control/) | Admittance control | ⭐⭐⭐ | Force input → position output (the dual of impedance, Keemink 2018) |
| [29](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/29_Bilateral_Coordination/) | Left/right coordination control | ⭐⭐⭐ | Anti-phase symmetry + affected-side reinforcement (rehabilitation) |
| [30](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/30_FF_FB_Hybrid_Control/) | FF + FB hybrid control | ⭐⭐⭐ | Model-based feedforward compensation + PD feedback |

---

## Part 7 — Physical AI Applications

The key stages of Physical AI — transparency → intent detection → learning → autonomous replay.

| Example | Title | Difficulty | What You Learn |
| :---: | :--- | :---: | :--- |
| [31](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/31_Friction_Comp_DOB/) | Disturbance Observer (DOB) | ⭐⭐⭐ | Estimate remaining disturbances for true transparent mode |
| [32](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/32_GRF_Gait_Intent/) | Gait intent via foot contact | ⭐⭐⭐ | Gait phase estimation from heel strike events |
| [33](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/33_Kinesthetic_Teaching/) | Kinesthetic teaching + replay | ⭐⭐⭐ | Human demonstrates by hand → board replays the motion |
| [34](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/34_MSC_GaitAnalysis_Log/) | Gait analysis data logging | ⭐⭐ | H10 → USB drive → Python → MATLAB |
| [35](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/35_MultiLayer_Transparent_Control/) | Multi-layer transparent control | ⭐⭐⭐ | Real-time switching between transparent / wall / left-right coupling modes |
| [36](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/36_OnDevice_Kinesthetic_Learning/) | On-device kinesthetic learning 🛑 **Rev 2.0 only** | ⭐⭐⭐ | Train a small neural network on-board → replay with LQR — Internal Flash UserNV API is supported on Rev 2.0 only |
| [37](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/37_FES_Hub_Module_Ctrl/) | FES Hub module control | ⭐⭐⭐ | Connect FES Hub over CAN-FD, control per-channel electrical stimulation parameters |
| [38](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/38_Periodic_Background_Task/) | Periodic background task | ⭐⭐ | Offload low-frequency auxiliary work, minimize control loop jitter |
| [39](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/39_Task_Lifecycle/) | Task lifecycle management | ⭐⭐⭐ | Create, suspend, and terminate tasks; RTOS task state machine pattern |
| [40](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/40_EMG_Proportional_Assist/) | EMG proportional assist 🛑 **Rev 2.0 only** | ⭐⭐⭐⭐ | External 4-channel ADC EMG → envelope → proportional torque, button calibration + PhAI Studio 0xF0 streaming (EMG competition foundation) |
| [41](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/41_IMU_Hub_Dashboard/) | IMU Hub attitude dashboard 🛑 **Rev 2.0 only** | ⭐⭐⭐ | Up to 6 IMUs' quaternion → Euler (roll/pitch/yaw) conversion, auto connection detection + PhAI Studio 0xF0 18-channel (50Hz) streaming (FDCAN2 sensor-hub bus) |
| [42](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/42_EMG_Hub_Biofeedback/) | EMG Hub biofeedback 🛑 **Rev 2.0 only** | ⭐⭐⭐ | Receive hub-processed muscle activation (envelope/MVC%), button calibration + LED/PhAI Studio 0xF0 4-channel (50Hz) real-time feedback (FDCAN2 sensor-hub bus, no motor drive) |

---

## Trying an Example

1. Open the `.c` file for the example you want from the `examples/` folder.
2. Copy its entire contents into `XM_Apps/Control_Task/control_task.c`.
3. Build in STM32CubeIDE and upload to the XM10.
4. Follow the example's README to verify the behavior.

If this is your first build or flash: [First Build & Run](../getting-started/03-first-build.md) · For AI-guided setup: [Getting Started with Claude Code](../getting-started/00-claude-code-quickstart.md)
