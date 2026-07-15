# Where Is It? — Quick Reference Index

Common questions and frequently visited pages, all in one place. Use Ctrl+F to search by keyword instead of digging through nine menu levels.

---

## 🚀 Getting Started

| Situation | Where to go |
|-----------|-------------|
| Just getting started — what do I do first? | [docs/getting-started/00-claude-code-quickstart.md](getting-started/00-claude-code-quickstart.md) (AI-guided) or [01 Hardware Setup](getting-started/01-hardware-setup.md) |
| What is Claude Code? | [00 Claude Code Quickstart](getting-started/00-claude-code-quickstart.md) |
| I only have a URL — how do I proceed? | [Page 00 — "Download ZIP" checklist](getting-started/00-claude-code-quickstart.md) |
| I'm stuck on environment setup | [02 Software Setup](getting-started/02-software-setup.md) → [troubleshooting.md](troubleshooting.md) |
| My first build fails | [03 First Build](getting-started/03-first-build.md) + [troubleshooting.md](troubleshooting.md) |
| The LED won't turn on | [Ex.00 Quick Start README](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/00_Quick_Start/) |

## 🛠️ Writing Code

| Situation | Where to go |
|-----------|-------------|
| Where do I write my code? | `XM_Apps/Control_Task/control_task.c` (relative to the unzipped folder) |
| When does my code get called? | [docs/architecture/README.md](architecture/README.md) |
| Function signatures / API reference | [docs/api-reference/](api-reference/) |
| Button / LED control | [api-reference/03-led-btn-control.md](api-reference/03-led-btn-control.md) + Ex.01–03 |
| External GPIO / ADC | [api-reference/04-external-io.md](api-reference/04-external-io.md) + Ex.04–06 |
| Sending data to PC over USB serial | [api-reference/05-usb-connectivity.md](api-reference/05-usb-connectivity.md) + Ex.07–09 |
| Logging to USB storage | [api-reference/06-usb-data-logging.md](api-reference/06-usb-data-logging.md) + Ex.10–10c |
| Memory regions (PSRAM, Workspace) | [api-reference/07-memory-management.md](api-reference/07-memory-management.md) + Ex.19 |
| Date / time (RTC) | [api-reference/08-rtc-clock.md](api-reference/08-rtc-clock.md) |
| KIT H10 exoskeleton control | [api-reference/02-h10-control-n-data.md](api-reference/02-h10-control-n-data.md) + Ex.11–13 |
| State machine (TSM) | [api-reference/01-task-state-machine.md](api-reference/01-task-state-machine.md) + Ex.03, Ex.10c |

## 📚 Examples

| Situation | Where to go |
|-----------|-------------|
| Full index of all 50 examples | [examples/README.md](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/README.md) |
| Learning paths by difficulty and track | [docs/tutorials/README.md](tutorials/README.md) |
| 16-week semester schedule | [tutorials/README.md — Semester Schedule section](tutorials/README.md#한-학기-16-주-수업-진도표-예시) |
| An example (Ex.XX) isn't working | See the "⚠️ Common Mistakes" section in `examples/XX_*/README.md` |
| First 30 minutes for a first-time user | [examples/README.md — Getting Started in 30 Minutes](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/README.md#처음-오신-분--첫-30-분-동선) |
| Algorithm examples: PD control / impedance / CPG / etc. | examples/14, 15, 20–30 (see [examples/README.md](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/README.md) for full mapping) |

## 🔌 Hardware

| Situation | Where to go |
|-----------|-------------|
| What's on the board? (LEDs / buttons / CAN / USB / UART counts) | [docs/hardware/README.md](hardware/README.md) |
| External GPIO pinmap (Rev 1.1) | [hardware/external-gpio-rev1.1.md](hardware/external-gpio-rev1.1.md) |
| External GPIO pinmap (Rev 2.0) | [hardware/external-gpio-rev2.0.md](hardware/external-gpio-rev2.0.md) |
| Is my board Rev 1.1 or Rev 2.0? | [hardware/README.md — Board Revision Comparison](hardware/README.md#보드-리비전-비교) |
| Which board revision do the 50 examples support? | [examples/README.md — Board Revision Compatibility](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/README.md) (47 work on both; Ex.40/41/42 are Rev 2.0-only) |

## 📥 Firmware Upload

| Situation | Where to go |
|-----------|-------------|
| Direct upload via ST-Link SWD | [docs/getting-started/03-first-build.md](getting-started/03-first-build.md) |
| PhAI Studio FTP upload (for distribution) | [docs/bootloader/README.md](bootloader/README.md) |
| What is the bootloader and why do I need it? | [bootloader/README.md — WHY section](bootloader/README.md) |
| Updating the KIT H10 exoskeleton firmware | [docs/kit-h10-firmware/README.md](kit-h10-firmware/README.md) |
| XM10 ↔ KIT H10 version compatibility | [kit-h10-firmware/README.md](kit-h10-firmware/README.md) |

## 🤖 AI / Data / Advanced Topics

| Situation | Where to go |
|-----------|-------------|
| Training an AI model with exoskeleton data | [docs/advanced/ai-data-pipeline.md](advanced/ai-data-pipeline.md) |
| Running a Tiny NN inference on-board | Ex.16, Ex.36 + [advanced/README.md](advanced/README.md) |
| How to use PythonDecoder | [PythonDecoder/](https://github.com/AGR-EXO/Extension_Module/tree/Develop/PythonDecoder/) |
| How to use PhAI Studio | Step 4 of each example's README + [studio.onephai.com](https://studio.onephai.com) |
| Finishing my algorithm within 1 ms | Ex.18 Debug Monitor + Ex.19 Memory Aware Design |
| Self-directed learning paths by interest | [advanced/README.md — Recommended Self-Study Paths](advanced/README.md#자기주도-학습-권장-경로) |

## ❓ Troubleshooting / Debugging

| Situation | Where to go |
|-----------|-------------|
| Build / clone / Korean-path errors | [docs/troubleshooting.md](troubleshooting.md) |
| Serial communication / USB not connecting | [troubleshooting.md](troubleshooting.md) + the relevant example README |
| PhAI Studio won't detect the board (CDC) | ⚠️ Common Mistakes in Ex.07–09 + the PhAI simultaneous-access warning in the `.c` file header |
| Board stuck in an infinite reboot loop | [troubleshooting.md](troubleshooting.md) |
| What is Body Data? (gaitCycle, etc.) | [api-reference/README.md — Body Data Prerequisites](api-reference/README.md) |
| Anticipated student-blocking scenarios for instructors / mentors | [docs/student-walkthrough-simulations.md](student-walkthrough-simulations.md) |
| Known issues by version | [docs/release-notes/](release-notes/) |

## 📖 Meta

| Situation | Where to go |
|-----------|-------------|
| Version / changelog | [CHANGELOG.md](https://github.com/AGR-EXO/Extension_Module/blob/Develop/CHANGELOG.md) |
| Release attachments / compatibility matrix | [docs/release-notes/](release-notes/) |
| License | [LICENSE](https://github.com/AGR-EXO/Extension_Module/blob/Develop/LICENSE) |
| System architecture overview | [docs/architecture/README.md](architecture/README.md) |
| Full documentation index for this repo | [docs/README.md](README.md) |

---

## Claude Code Users

You don't need to memorize the tables above. Just ask Claude Code naturally:

```
"How do I log data to USB storage?"
"I'm getting a build error in Ex.14"
"How do I check my board revision?"
"How do I collect data for AI training?"
```

The `example-helper` / `student-onboard` skills will automatically locate and cite the relevant page.
