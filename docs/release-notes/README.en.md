# Release Notes

This page lists release attachments, compatibility matrices, and known issues for each version. Refer here when you need more detailed download and operational information than the CHANGELOG provides.

| Version | Date | Notes |
|---------|------|-------|
| [v2.8.1](v2.8.1.en.md) | 2026-10-07 | PC tool tabs per module · CAN-FD receive fix · PC tool folder renamed `pc-data-tool` · EMG Hub calibration API · Rev 1.1: 40 examples · bootloader v1.1.0 |
| [v2.8.0](v2.8.0.en.md) | 2026-09-10 | New PC data tool `xm10` — graphs, lossless saving (`.xmlog`), CSV export + user channel names shown |
| [v2.7.0](v2.7.0.en.md) | 2026-09-08 | General-purpose Serial API — External UART (PD5/PD6) opened through XM_API + Ex.43 (two XM10s talking) + improved sensor hub connection stability |
| [v2.6.0](v2.6.0.en.md) | 2026-08-20 | Safety release — safe CONTROL→MONITOR transition + watchdog (8 s) & no-reboot-loop + 15 hardened examples + `forwardVelocity` 60× fix + Ext_Sync |
| [v2.5.1](v2.5.1.en.md) | 2026-07-29 | Rev 2.0 boot-hang hotfix (board stalled during boot — no LED response, no COM port) |
| [v2.5.0](v2.5.0.en.md) | 2026-07-24 | Removed USB-MSC file logging — data capture consolidated on USB-CDC live streaming |
| [v2.4.1](v2.4.1.en.md) | 2026-07-24 | Rev 2.0 float `printf`(`%f`) / `malloc` fix + thread-safe C standard library + USB log decoder as an installable executable |
| [v2.4.0](v2.4.0.en.md) | 2026-07-21 | USB serial usability (terminal text examples appear automatically) + simplified USB mode API (Rev 2.0) |
| [v2.3.1](v2.3.1.en.md) | 2026-07-18 | GRF-module boot stability + comm/storage hardening + ZIP build-out-of-the-box fixes + hipTorque unit (Nm) change |
| [v2.3.0](v2.3.0.en.md) | 2026-07-15 | 2 sensor-hub examples (IMU/EMG Hub) + SDK firmware/example alignment + version.h 2.3.0 fix |
| [v2.2.2](v2.2.2.en.md) | 2026-05-20 | 9 example code cleanups + Ex.36 marked Rev 2.0-only everywhere |
| [v2.2.1](v2.2.1.en.md) | 2026-05-19 | Fixed 22 Rev 2.0 SDK build failures + expanded board revision comparison docs |
| [v2.2.0](v2.2.0.en.md) | 2026-05-15 | User function name cleanup + Rev 1.1 / 2.0 Task API normalization + 2 new learning examples |
| [v2.1.1](v2.1.1.en.md) | 2026-04-04 | Introduced the new bootloader (firmware upload over USB with PhAI Studio) + dual SDK for Rev1.1/Rev2.0 + 44 examples |

For a summarized change history, see the [CHANGELOG](https://github.com/AGR-AIFT/phai-x1-xm10-release/blob/Develop/CHANGELOG.md). For step-by-step downloads, visit the GitHub [Releases](https://github.com/AGR-AIFT/phai-x1-xm10-release/releases) page.
