# XM10 Documentation

This is the complete documentation index for XM10. If you are just getting started, read from top to bottom in order.

> **🧭 Looking for something?** — For quick keyword search, check **[find-it.md](find-it.md)**: a single page that maps common keywords to the right documentation page.

---

## Getting Started Flow

```
Environment Setup ──► First Build ──► Learn with Examples ──► Write Your Own Code with the API
                                               │
                                               └──► Stuck? Troubleshooting / find-it.md
```

---

## 1. [Getting Started](getting-started/) — Environment Setup

Start here if this is your first time.

| Step | Document | Description |
| :---: | :--- | :--- |
| 00 | [Get Started with Claude Code](getting-started/00-claude-code-quickstart.md) | AI-guided setup (fastest path) |
| 01 | [Hardware Setup](getting-started/01-hardware-setup.md) | Connect KIT H10, ST-Link, and sensor hubs |
| 02 | [Development Environment Setup](getting-started/02-software-setup.md) | Install STM32CubeIDE, clone the repository |
| 03 | [First Build & Run](getting-started/03-first-build.md) | Import the project, build, and upload firmware |

---

## 2. [Tutorials](tutorials/) — Step-by-Step Learning with 50 Examples

Each example follows a consistent 5-step structure: `Goal → Prerequisites → Key Code → Experiment → Next Steps`. Introductory examples are designed to be completable in under 30 minutes.

| Part | Topic | Examples |
| :---: | :--- | :--- |
| 0 | Board verification | Ex.00 |
| 1 | Basic I/O (LED, button, external IO) | Ex.01–06 |
| 2 | USB communication + memory logging | Ex.07–10c |
| 3 | KIT H10 exoskeleton basic modes | Ex.11–13 |
| 4 | Control algorithm fundamentals + debugging | Ex.14–19 |
| 5 | Advanced control algorithms (Hogan, HZD, CPG, etc.) | Ex.20–25 |
| 6 | Learning & adaptive control (ILC, MRAC, etc.) | Ex.26–30 |
| 7 | Physical AI applications | Ex.31–36 |
| 8 | External module integration + RTOS utilities + Sensor Hub | Ex.37–42 |

Full learning path and recommended order: **[Tutorials README](tutorials/)**

---

## 3. [API Reference](api-reference/) — Function Reference

Keep this open while writing your algorithms. You do not need to read it all at once.

| Document | Description |
| :--- | :--- |
| [Task State Machine](api-reference/01-task-state-machine.md) | State machine — separating behavior by state |
| [KIT H10 Control & Data](api-reference/02-h10-control-n-data.md) | Read exoskeleton sensor data + send torque/position commands |
| [LED & Button](api-reference/03-led-btn-control.md) | Board LED control, button input |
| [External IO](api-reference/04-external-io.md) | GPIO and ADC pin control |
| [USB Serial Communication](api-reference/05-usb-connectivity.md) | Send messages and data to a PC |
| [USB Memory Logging](api-reference/06-usb-data-logging.md) | Automatically save data to USB storage |
| [Memory Regions](api-reference/07-memory-management.md) | Fast memory and non-volatile storage |
| [Real-Time Clock](api-reference/08-rtc-clock.md) | Read and write date/time |

---

## 4. [Hardware](hardware/) — Board External Interfaces & Pinmap

What connectors and peripherals are available on the board and where to plug things in. The external GPIO pinmap is a page you will refer to often — it is split by board revision (Rev 1.1 / Rev 2.0).

---

## 5. [KIT H10 Firmware](kit-h10-firmware/) — H10 Firmware & Contents

The XM firmware version and the KIT H10 firmware version must match for correct operation. Come here when an update is needed.

---

## 6. [Architecture](architecture/) — System Overview

Where and how your code runs — reading this once at the start gives you the big picture.

---

## 7. [Bootloader](bootloader/) — Firmware Upload Methods

Two upload methods: direct ST-Link SWD upload and PhAI Studio USB upload, plus automatic bootloader rollback.

---

## 8. [Advanced Topics](advanced/) — Self-Directed Learning Paths

Recommended example paths by area of interest: control, AI, data analysis, transparent mode, and more.

---

## 9. [Troubleshooting](troubleshooting.md) — When You're Stuck

Common issues covering build errors, USB connection problems, communication failures, and more.

---

## Additional Resources

- [🧭 find-it.md](find-it.md) — Quick keyword-to-page lookup
- [Examples](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/) — Source code for all 50 examples (each folder includes a 5-step README)
- [Python Tools](https://github.com/AGR-EXO/Extension_Module/tree/Develop/PythonDecoder/) — USB serial receiver, USB memory decoder
- [Release Notes](release-notes/) — Per-version attachments and compatibility matrix
- [Changelog](https://github.com/AGR-EXO/Extension_Module/blob/Develop/CHANGELOG.md) — Summary of changes by version
- [XM10 SDK](https://github.com/AGR-EXO/Extension_Module/tree/Develop/XM10_SDK/) — Rev1.1 / Rev2.0 SDK projects
- [5-User Walkthrough Simulations](student-walkthrough-simulations.md) — UX validation walkthroughs for instructors and mentors
