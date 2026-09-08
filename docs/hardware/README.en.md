# Hardware Specifications

This page is your single reference for all external interfaces on the XM10 board — what connectors it has, how many, and where to plug things in. It is the first page to check when getting started.

> Board photos and per-revision GPIO header images are maintained under [`assets/img/`](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/assets/img/).

---

## At a Glance

| Category | Count / Type | Notes |
|----------|-------------|-------|
| **Board LEDs** | 3 | LED 1 (left) · LED 2 (center) · LED 3 (right) |
| **Channel LEDs** | 7 × RGB | EMG / FES / IMU / HMMG / GRF_L / GRF_R / USB (module status indicators) |
| **Buttons** | 3 | BTN 1 (left) · BTN 2 (center) · BTN 3 (right) |
| **External GPIO** | DIO 8 + ADC 4 (+ dynamic ADC 8) | [See per-revision pinmap](#external-gpio-pinmap) |
| **CAN-FD ports** | 2 | KIT H10 connection + sensor hub expansion |
| **USB-C** | 1 | Serial communication (CDC) — PC connection |
| **External UART** | 1 (**Rev 2.0 only**) | Serial link to external devices — general-purpose Serial API. Rev 1.1 does not have this port |
| **SWD debug** | 4-pin | ST-Link firmware upload and debugging |
| **Main connector** | 1 | KIT H10 body connection (24 V power + CAN-FD) |

> For the exact physical location of each interface, refer to the board photo below.

---

## Board Overview

![XM10 board — interface locations](https://raw.githubusercontent.com/AGR-AIFT/phai-x1-xm10-release/Develop/assets/img/board-photo.png)

This photo shows the physical positions of the 3 LEDs, 3 buttons, main connector, SWD header, USB-C port, CAN-FD ports, external GPIO header, and external UART port at a glance. For the external GPIO header pinmap by revision, see the [Rev 1.1](external-gpio-rev1.1.md) and [Rev 2.0](external-gpio-rev2.0.md) pages.

---

## External Interfaces

### Main Connector (KIT H10 connection)

Delivers both power and communication to the XM10 through a single cable. Uses a Molex 1053081206 6-pin connector.

| Pin | Function |
|-----|----------|
| 1 | NC |
| 2 | 24 V (power) |
| 3 | GND |
| 4 | GND |
| 5 | CAN HIGH |
| 6 | CAN LOW |

> Connection procedure: [Getting Started — 01 Hardware Setup](../getting-started/01-hardware-setup.md)

### CAN-FD Ports

Two ports total. One communicates with KIT H10 through the main connector; the other is for expanding sensor hubs (EMG / GRF / FSR, etc.).

| Port | Purpose |
|------|---------|
| CAN-FD #1 | KIT H10 exoskeleton ↔ XM10 (routed through the main connector) |
| CAN-FD #2 | Sensor hub module expansion |

### USB-C Port

Used for data exchange with a PC via USB serial communication (CDC) for real-time data streaming and debug messages.

| Mode | Purpose | Functions used |
|------|---------|----------------|
| Serial (CDC) | Debug and data transfer via PC terminal / PhAI Studio | `XM_SendUsbDebugMessage`, `XM_SendUsbDataWithId` |

### External UART Port 🟢 **Rev 2.0 only**

A serial (UART) link to external equipment. The peer can be another XM10, an Arduino, a PC,
another MCU — anything.

| Item | Value |
|---|---|
| Peripheral / pins | **USART2**, TX = **PD5** (`EXT_UART_TX`) / RX = **PD6** (`EXT_UART_RX`) |
| Logic level | **3.3 V** — never wire 5 V directly (use a level shifter) |
| Default settings | **921600 8N1, no flow control** (only the baud rate is changeable; 8N1 is fixed) |
| Max bytes per send | 128 |

When wiring, **cross TX and RX** and **always connect GND**. These are the two most common mistakes.

**Software**: as of v2.7.0 the port is used through five functions — `XM_AttachExternalUart()`,
`XM_SetExternalUartBaudrate()`, `XM_SendExternalUartData()`, `XM_SendExternalUartDataBlocking()`,
`XM_EnsureExternalUartRxArmed()`. See the
[API reference](../api-reference/ref/xm-api-external-io.en.md#6-general-purpose-external-serial-api--rev-20-only)
and [Ex.43 External UART Ping-Pong](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/43_External_UART_PingPong/).

A dedicated driver for attaching a specific IMU (XSENS MTi) to this port also exists, but it
**cannot coexist** with the general-purpose Serial API — a build switch selects one or the other
(general-purpose Serial is the default). See section 5 of the API reference above.

> 🛑 **Rev 1.1 does not have this port.** Its only serial ports are UART7/UART8 for the foot
> sensors (GRF), and **PD6 on Rev 1.1 is `USB_PWR_ON` — an output that switches USB power.**
> Applying the Rev 2.0 wiring to a Rev 1.1 board puts that output against the peer's TX output.
> Check your board revision first.

### SWD Debug Port

A 4-pin header for connecting an ST-Link V2/V3 debugger. Use this when flashing firmware for the first time and for debugging sessions.

> Pin layout: [Getting Started — 01 Hardware Setup](../getting-started/01-hardware-setup.md#2-st-link-디버거-pc--xm10)

---

## Board Inputs and Outputs

### Buttons

| Button | Location | API | MCU Pin (Rev 1.1) | MCU Pin (Rev 2.0) |
|--------|----------|-----|-------------------|-------------------|
| BTN 1 | Left | `XM_GetButtonEvent(XM_BTN_1)` | PC10 | PC11 |
| BTN 2 | Center | `XM_GetButtonEvent(XM_BTN_2)` | PC11 | PC12 |
| BTN 3 | Right | `XM_GetButtonEvent(XM_BTN_3)` | PC12 | PC13 |

Supported events: press (`XM_BTN_PRESSED`), release (`XM_BTN_RELEASED`), click (`XM_BTN_CLICK`), and long press — held for 1 second or more (`XM_BTN_LONG_PRESS`).

> ⚠️ Going from Rev 1.1 to Rev 2.0, all three onboard buttons shifted by one MCU pin (PC10 was reassigned to external SRAM (PSRAM) on Rev 2.0). The API names (`XM_BTN_1/2/3`) retain their left/center/right meaning on both revisions, so as long as you **use the SDK ZIP that matches your board's revision**, you do not need to worry about this. Loading the wrong revision's ZIP will offset every button mapping by one position.

### LEDs

| LED | Location | API |
|-----|----------|-----|
| LED 1 | Left | `XM_SetLedState(XM_LED_1, XM_ON)`, etc. |
| LED 2 | Center | `XM_SetLedState(XM_LED_2, ...)` |
| LED 3 | Right | `XM_SetLedState(XM_LED_3, ...)` |

Supported effects: on/off (`XM_LED_SOLID` / `OFF`), blink (`BLINK`), heartbeat (`HEARTBEAT`), and single-shot (`ONESHOT`).

### Channel LEDs (7 × RGB)

RGB LEDs that indicate the connection status of each sensor module by color. The system controls them automatically, but you can also set colors directly with `XM_SetChannelLedRGB()` when needed.

| Channel LED | Indicated module |
|-------------|-----------------|
| `XM_CH_LED_EMG` | EMG module |
| `XM_CH_LED_FES` | FES module |
| `XM_CH_LED_IMU` | IMU module |
| `XM_CH_LED_HMMG` | HMMG module |
| `XM_CH_LED_GRF_L` / `_GRF_R` | Left / right GRF shoes |
| `XM_CH_LED_USB` | USB connection status |

> Full LED and button API reference: [LED & Button Control](../api-reference/03-led-btn-control.md)

---

## External GPIO Pinmap

The external GPIO header is the section you will consult most frequently. Connector position and labeling differ by board revision, so make sure to open the page for your board.

| Board revision | Pinmap page |
|----------------|-------------|
| **Rev 1.1** | [external-gpio-rev1.1.md](external-gpio-rev1.1.md) |
| **Rev 2.0** | [external-gpio-rev2.0.md](external-gpio-rev2.0.md) |

> For how to use the pins themselves (calling input/output functions), see the [External IO API](../api-reference/04-external-io.md).

---

## Board Revision Comparison

First, confirm which revision your board is. The board label reads either `Rev 1.1` or `Rev 2.0`. The easiest visual distinction is the **presence of an RJ45 Ethernet port** — only Rev 2.0 has one.

| Feature | Rev 1.1 | Rev 2.0 |
|---------|---------|---------|
| RJ45 Ethernet port | Not present | **Present** (the quickest visual identifier) |
| Channel LEDs (7 × RGB) | Not present (3 onboard LEDs only) | **Present** (PCA9957, `XM_SetChannelLedRGB` API) |
| External SRAM (PSRAM) | Not present | Present (uses MCU pin PC10) |
| **External UART port** | **Not present** (PD6 is the `USB_PWR_ON` output) | **Present** — USART2, PD5/PD6, general-purpose Serial API |
| Onboard button MCU pins | PC10 / PC11 / PC12 | PC11 / PC12 / PC13 (shifted by one pin) |
| External GPIO connector location / label | [Rev 1.1 pinmap](external-gpio-rev1.1.md) | [Rev 2.0 pinmap](external-gpio-rev2.0.md) |
| Distribution ZIP | `Rev1.1.zip` | `Rev2.0.zip` |

> Building with a ZIP from a different revision than your board — the build itself will succeed, but the onboard button and LED MCU pin mappings will be off by one position and the board will not behave correctly. Always check the board label first.

---

## Related Documents

- [Getting Started — 01 Hardware Setup](../getting-started/01-hardware-setup.md) — Board cable connection procedure
- [External IO API](../api-reference/04-external-io.md) — GPIO/ADC control functions
- [LED & Button API](../api-reference/03-led-btn-control.md) — LED and button functions
- [USB Serial Communication API](../api-reference/05-usb-connectivity.md) — USB-C usage
- [KIT H10 Firmware](../kit-h10-firmware/) — H10 body firmware update
- [Bootloader](../bootloader/) — XM10 firmware upload procedure
