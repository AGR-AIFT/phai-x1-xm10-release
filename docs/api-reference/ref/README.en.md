# API Function Reference

> Parent: [API Reference Overview](../README.en.md) · [Documentation Index](../../README.en.md)

A per-header collection of pages documenting the **exact function signatures, parameters, return values, and types** declared by each header under `XM_FW/XM_API/`. For "why to use this API / how it fits your algorithm," check the [concept guides (01–09)](../README.en.md#function-groups) first. These pages are for looking up the **precise calling contract** while you write code.

---

## Umbrella Entry Point

| Header | One-line purpose | Reference | Related concept doc | Representative example |
|--------|-------------------|-----------|----------------------|-------------------------|
| `xm_api.h` | Includes every SDK API header in one shot + `XM_GetTick()` | [xm-api.en.md](xm-api.en.md) | [API Reference Overview](../README.en.md) | [00_Quick_Start](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/00_Quick_Start/) — included by virtually every example |

## Control · Data

| Header | One-line purpose | Reference | Related concept doc | Representative example |
|--------|-------------------|-----------|----------------------|-------------------------|
| `xm_api_data.h` | Read KIT H10 sensor data (`XM.status`) + issue torque/position commands (P/I/F-Vector) — 23 functions · 3 macros · 13 types | [xm-api-data.en.md](xm-api-data.en.md) | [02. KIT H10 Control & Data](../02-h10-control-n-data.en.md) | [12_Active_Assist_Mode](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/12_Active_Assist_Mode/) |

## State Machine

| Header | One-line purpose | Reference | Related concept doc | Representative example |
|--------|-------------------|-----------|----------------------|-------------------------|
| `xm_api_tsm.h` | Task State Machine that separates control logic per state — 4 functions · 6 types | [xm-api-tsm.en.md](xm-api-tsm.en.md) | [01. Task State Machine (TSM)](../01-task-state-machine.en.md) | [03_Button_LED_FSM](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/03_Button_LED_FSM/) |

## IO

| Header | One-line purpose | Reference | Related concept doc | Representative example |
|--------|-------------------|-----------|----------------------|-------------------------|
| `xm_api_led_btn.h` | Onboard LED effect/state control + button event reads — 6 functions · 7 types | [xm-api-led-btn.en.md](xm-api-led-btn.en.md) | [03. LED & Button](../03-led-btn-control.en.md) | [01_Button_LED_Basic](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/01_Button_LED_Basic/) |
| `xm_api_external_io.h` | Expansion port digital GPIO I/O + ADC analog reads — 21 symbols (14 functions and more) | [xm-api-external-io.en.md](xm-api-external-io.en.md) | [04. External IO](../04-external-io.en.md) | [04_Ext_IO_Basic](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/04_Ext_IO_Basic/) |

## USB · Logging

| Header | One-line purpose | Reference | Related concept doc | Representative example |
|--------|-------------------|-----------|----------------------|-------------------------|
| `xm_api_usb.h` | USB serial (CDC) real-time streaming + USB storage (MSC) data logging | [xm-api-usb.en.md](xm-api-usb.en.md) | [05. USB Serial](../05-usb-connectivity.en.md) · [06. USB Storage Logging](../06-usb-data-logging.en.md) | [09_CDC_Stream](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/09_CDC_Stream/) |

## Memory · RTC

| Header | One-line purpose | Reference | Related concept doc | Representative example |
|--------|-------------------|-----------|----------------------|-------------------------|
| `xm_api_memory.h` | Workspace/PSRAM/DTCM fast memory + non-volatile (Flash) storage access — 🟢 **Rev 2.0 only**, 11 functions | [xm-api-memory.en.md](xm-api-memory.en.md) | [07. Memory Regions](../07-memory-management.en.md) | [36_OnDevice_Kinesthetic_Learning](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/36_OnDevice_Kinesthetic_Learning/) |
| `xm_api_rtc.h` | RTC date/time set & read — ⚠️ stub on Rev1.1 (no hardware), 3 functions | [xm-api-rtc.en.md](xm-api-rtc.en.md) | [08. Real-Time Clock](../08-rtc-clock.en.md) | [34_MSC_GaitAnalysis_Log](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/34_MSC_GaitAnalysis_Log/) |

## RTOS

| Header | One-line purpose | Reference | Related concept doc | Representative example |
|--------|-------------------|-----------|----------------------|-------------------------|
| `xm_api_freertos.h` | Auxiliary task creation (one-shot/periodic) + Mutex — student-friendly FreeRTOS wrapper, 17 functions (15 active + 2 deprecated) | [xm-api-freertos.en.md](xm-api-freertos.en.md) | [09. Auxiliary Tasks & Data Sharing](../09-task-creation.en.md) | [38_Periodic_Background_Task](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/38_Periodic_Background_Task/) |

## User Data

| Header | One-line purpose | Reference | Related concept doc | Representative example |
|--------|-------------------|-----------|----------------------|-------------------------|
| `xm_api_user_custom.h` | 28-byte user custom slot inside the Total Data Packet (0x20) — 7 functions · 4 macros · 1 type | [xm-api-user-custom.en.md](xm-api-user-custom.en.md) | [05. USB Serial Communication](../05-usb-connectivity.en.md) | No direct-usage example yet (new in v2.3.0) — see [09_CDC_Stream](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/09_CDC_Stream/) for the same packet |

---

## See Also

- [API Reference Overview](../README.en.md) — index of the 01–09 concept guides
- [Documentation Index](../../README.en.md)
