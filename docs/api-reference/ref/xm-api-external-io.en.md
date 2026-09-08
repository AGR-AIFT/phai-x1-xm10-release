# `xm_api_external_io.h` — External Extension Port GPIO/ADC Control

> **Header**: `XM_FW/XM_API/xm_api_external_io.h` (shared file across Rev1.1 / Rev2.0 — some pin mappings and the power-select API are Rev2.0-only, see the 🟢 badges below)
> **Related concept doc**: [04. External IO Control API](../04-external-io.en.md)
> **Related examples**: [04_Ext_IO_Basic](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/04_Ext_IO_Basic/) · [05_Ext_IO_analog](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/05_Ext_IO_analog/) · [05a_Ext_IO_DIO_to_ADC](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/05a_Ext_IO_DIO_to_ADC/) · [05b_Ext_IO_FSR_8ch](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/05b_Ext_IO_FSR_8ch/) · [05c_Ext_IO_Mixed_ADC](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/05c_Ext_IO_Mixed_ADC/) · [05d_Ext_IO_DIO_ADC_Hybrid](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/05d_Ext_IO_DIO_ADC_Hybrid/) · [06_Ext_IO_Safety_Switch](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/06_Ext_IO_Safety_Switch/) · [40_EMG_Proportional_Assist](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/40_EMG_Proportional_Assist/) (power voltage switch)

This is the API for the board's side-mounted Extension Port: 8 digital I/O (DIO) pins and 4 analog input (ADC) pins. If needed, DIO pins can be dynamically switched to ADC mode to get up to 12 analog input channels. On Rev2.0, this header also includes switching the extension port's supply voltage (3.3V/5V).

---

## When to Use

Use this API when you want to read values from external sensors (FSR, switches, potentiometers, etc.) connected to the board, or control external digital devices such as LEDs or relays. It's designed to feel similar to Arduino's `pinMode`/`digitalWrite`/`analogRead`. It's recommended to read [04. External IO Control API](../04-external-io.en.md) first for the operating principle, electrical cautions, and a list of common mistakes — this page only covers the exact signature/parameters of each function.

---

## Function List

| Function / Macro | One-line description |
|------|-----------|
| [`XM_SetPinMode`](#xm_setpinmode) | Set a DIO pin's input/output/pull-up/pull-down mode |
| [`XM_DigitalWrite`](#xm_digitalwrite) | Drive a DIO pin HIGH/LOW |
| [`XM_DigitalRead`](#xm_digitalread) | Read a DIO pin's current logic level |
| [`XM_AnalogRead`](#xm_analogread) | Read an ADC pin's voltage as a normalized integer (unified across ADC1/2/3) |
| [`XM_AnalogReadMillivolts`](#xm_analogreadmillivolts) | Read an ADC pin's voltage in millivolts (mV) |
| [`XM_SetAnalogReadResolution`](#xm_setanalogreadresolution) | Set the output resolution of `XM_AnalogRead`'s return value |
| [`XM_GetAnalogResolution`](#xm_getanalogresolution) | Query an ADC pin's native hardware resolution |
| [`XM_GetAnalogReadResolution`](#xm_getanalogreadresolution) | Query the currently configured output resolution |
| [`XM_SwitchDioToAdc`](#xm_switchdiotoadc) | Switch a single DIO pin to a high-speed ADC3 input |
| [`XM_SwitchAllDioToAdc`](#xm_switchalldiotoadc) | Switch all 8 DIO pins to ADC3 inputs at once |
| [`XM_IsDioSwitchedToAdc`](#xm_isdioswitchedtoadc) | Check whether a DIO pin has been switched to ADC mode |
| [`XM_DIO_TO_ADC_PIN`](#xm_dio_to_adc_pin-macro) (macro) | Convert a DIO pin number to its corresponding ADC pin number |
| [`XM_SetExtPowerVoltage`](#xm_setextpowervoltage) 🟢 Rev 2.0 Only | Switch the extension port's supply voltage to 3.3V/5V |
| [`XM_AttachExternalUart`](#xm_attachexternaluart) 🟢 Rev 2.0 only | Start External UART reception and register a callback |
| [`XM_SetExternalUartBaudrate`](#xm_setexternaluartbaudrate) 🟢 Rev 2.0 only | Set the External UART baud rate |
| [`XM_SendExternalUartData`](#xm_sendexternaluartdata) 🟢 Rev 2.0 only | Non-blocking send (max 128 B per call) |
| [`XM_SendExternalUartDataBlocking`](#xm_sendexternaluartdatablocking) 🟢 Rev 2.0 only | Blocking send (`Control_Setup()` only) |
| [`XM_EnsureExternalUartRxArmed`](#xm_ensureexternaluartrxarmed) 🟢 Rev 2.0 only | Revive stalled reception (call every ~100 ms) |
| [`XM_AttachXsensMTi630`](#xm_attachxsensmti630) ⚠️ not in the default build | Attach a Xsens MTi-630 IMU to the External UART |
| [`XM_ConfigureXsensMTi630`](#xm_configurexsensmti630) | Send the Xsens MTi-630 Output Configuration once |

---

## Function Details

### 1. Digital I/O (DIO)

### `XM_SetPinMode`

```c
void XM_SetPinMode(XmDioPin_t pin, XmPinMode_t mode);
```

Sets a DIO pin's operating mode (input/output/pull-up/pull-down). Must be called before using any other DIO function on that pin.

**Parameters**

| Name | Type | Description |
|------|------|--------------|
| `pin` | `XmDioPin_t` | Pin to configure (`XM_EXT_DIO_1` ~ `XM_EXT_DIO_8`) |
| `mode` | `XmPinMode_t` | Target mode (`XM_EXT_DIO_MODE_INPUT`/`_PULLUP`/`_PULLDOWN`/`_OUTPUT`) |

**Returns**: None (`void`)

**⚠️ Calling context**: Non-real-time function. It calls `HAL_GPIO_Init()` internally, so do not call it inside the 1ms(1kHz) `Control_Loop()` — configure it once in `Control_Setup()`.
> Header wording note: the Rev2.0 source comment says "do not call inside a 2ms real-time loop," while the Rev1.1 source comment says "1ms real-time loop" — the wording differs between the two revision headers. Since the SDK-wide `Control_Loop()` period is 1ms(1kHz) (see [09. Auxiliary Tasks + Data Sharing](../09-task-creation.en.md), [Architecture Overview](../../architecture/README.en.md)), this page standardizes on 1ms.

**Example**

```c
void Control_Setup(void)
{
    XM_SetPinMode(XM_EXT_DIO_3, XM_EXT_DIO_MODE_INPUT_PULLUP);   // for a switch
    XM_SetPinMode(XM_EXT_DIO_4, XM_EXT_DIO_MODE_OUTPUT);         // for an LED
}
```

**See also**: [`XmPinMode_t`](#xmpinmode_t) — full list of modes

---

### `XM_DigitalWrite`

```c
void XM_DigitalWrite(XmDioPin_t pin, XmLogicLevel_t level);
```

Drives a DIO pin to a voltage level (HIGH/LOW). Only takes effect on a pin configured as `XM_EXT_DIO_MODE_OUTPUT`.

**Parameters**

| Name | Type | Description |
|------|------|--------------|
| `pin` | `XmDioPin_t` | Target pin |
| `level` | `XmLogicLevel_t` | Output level (`XM_HIGH` = 3.3V, `XM_LOW` = 0V) |

**Returns**: None (`void`)

**⚠️ Calling context**: Assume `Control_Setup()` / `Control_Loop()` context.

> If the pin has already been switched to ADC mode via `XM_SwitchDioToAdc()`, an `XM_DigitalWrite` call is silently ignored by a guard mechanism (see [04. External IO — Common Mistakes](../04-external-io.en.md)).

**Example**

```c
XM_DigitalWrite(XM_EXT_DIO_4, XM_HIGH);  // Turn on an LED wired to pin 4
```

**See also**: [`XM_IsDioSwitchedToAdc`](#xm_isdioswitchedtoadc) — check whether a pin has been switched to ADC mode

---

### `XM_DigitalRead`

```c
XmLogicLevel_t XM_DigitalRead(XmDioPin_t pin);
```

Reads a DIO pin's current voltage state.

**Parameters**

| Name | Type | Description |
|------|------|--------------|
| `pin` | `XmDioPin_t` | Pin to check |

**Returns**: `XmLogicLevel_t` — `XM_HIGH` (near 3.3V) or `XM_LOW` (near 0V)

**⚠️ Calling context**: Assume `Control_Setup()` / `Control_Loop()` context.

**Example**

```c
if (XM_DigitalRead(XM_EXT_DIO_2) == XM_HIGH) {
    // Switch input detected
}
```

---

### 2. Analog Input (ADC) — Unified Read API

### `XM_AnalogRead`

```c
uint16_t XM_AnalogRead(XmAdcPin_t pin);
```

Reads an ADC pin's voltage as a normalized integer value. The function automatically determines which of ADC1/2/3 the pin belongs to, so the caller only needs to specify the pin number. Every pin's return value is normalized to the same output resolution (16-bit by default), so you don't need to worry about differing hardware resolutions across pins.

**Parameters**

| Name | Type | Description |
|------|------|--------------|
| `pin` | `XmAdcPin_t` | Pin to read (`XM_EXT_ADC_1` ~ `XM_EXT_ADC_12`) |

**Returns**: Normalized ADC value (0~65535 by default, range depends on the configured output resolution). `0` on an invalid pin.

> `XM_EXT_ADC_5` ~ `_12` (the ADC3 group) only return valid values after being switched via [`XM_SwitchDioToAdc()`](#xm_switchdiotoadc). Reading before the switch returns `0` (indistinguishable from a genuine 0V reading); on Rev2.0 this condition can be checked via the [`g_xm_adc_read_before_switch`](#diagnostic-global-variable-🟢-rev-20-only) diagnostic variable.

**⚠️ Calling context**: Assume `Control_Setup()` / `Control_Loop()` context. This is a read-only query, so it is safe to call repeatedly inside the 1ms loop.

**Example**

```c
void Control_Setup(void) {
    XM_SetAnalogReadResolution(12);   // 12-bit mode (0~4095)
    XM_SwitchDioToAdc(XM_EXT_DIO_1);
    XM_SwitchDioToAdc(XM_EXT_DIO_2);
}

void Control_Loop(void) {
    uint16_t fsr1 = XM_AnalogRead(XM_EXT_ADC_5);  // 0~4095
    uint16_t fsr2 = XM_AnalogRead(XM_EXT_ADC_6);  // 0~4095
}
```

**See also**: [`XM_AnalogReadMillivolts`](#xm_analogreadmillivolts), [`XM_SetAnalogReadResolution`](#xm_setanalogreadresolution)

---

### `XM_AnalogReadMillivolts`

```c
uint16_t XM_AnalogReadMillivolts(XmAdcPin_t pin);
```

Reads an ADC pin's voltage in millivolts (mV). This converts directly from the native raw value, so it always keeps the same accuracy regardless of the `XM_SetAnalogReadResolution()` setting.

**Parameters**

| Name | Type | Description |
|------|------|--------------|
| `pin` | `XmAdcPin_t` | Pin to read (`XM_EXT_ADC_1` ~ `XM_EXT_ADC_12`) |

**Returns**: Voltage in millivolts (0 ~ 3300, based on VREF = 3.3V). `0` on an invalid pin.

**⚠️ Calling context**: Assume `Control_Setup()` / `Control_Loop()` context.

**Example**

```c
uint16_t mv = XM_AnalogReadMillivolts(XM_EXT_ADC_1);
float voltage = mv / 1000.0f;  // mv == 1650 → 1.65V
```

---

### `XM_SetAnalogReadResolution`

```c
void XM_SetAnalogReadResolution(uint8_t bits);
```

Sets the output resolution of `XM_AnalogRead()`'s return value (compatible with Arduino's `analogReadResolution()`). This does not change the actual hardware ADC resolution — it normalizes the value in software via left/right bit shifting.

**Parameters**

| Name | Type | Description |
|------|------|--------------|
| `bits` | `uint8_t` | Output resolution (8, 10, 12, 14, 16) |

**Returns**: None (`void`). Default is 16.

**⚠️ Calling context**: The header does not state any real-time restriction. The setting applies globally to every subsequent `XM_AnalogRead()` call, so it's generally recommended to call this once in `Control_Setup()`.

**Example**

```c
XM_SetAnalogReadResolution(12);  // All subsequent XM_AnalogRead() calls → 0~4095
```

---

### `XM_GetAnalogResolution`

```c
uint8_t XM_GetAnalogResolution(XmAdcPin_t pin);
```

Queries the specified ADC pin's **hardware-native** resolution. Use this to check what bit depth the actual ADC operates at.

**Parameters**

| Name | Type | Description |
|------|------|--------------|
| `pin` | `XmAdcPin_t` | ADC pin to query |

**Returns**: Native resolution (bit count). `0` on an invalid pin.

Per-pin native resolution differs by board revision — see the [`XmAdcPin_t`](#xmadcpin_t) table below.

> ⚠️ **Header wording inconsistency (flagged)**: The `@code` example comment inside the Rev2.0 `xm_api_external_io.h` states `XM_EXT_ADC_1`=12-bit and `XM_EXT_ADC_2`=16-bit (this appears to be copied verbatim from the Rev1.1 example). However, the same file's `XmAdcPin_t` enum comment (`/* ADC1 fixed pins (Rev2.0: all ADC1 16-bit, always available) */`) and the [Rev 2.0 hardware pinmap](../../hardware/external-gpio-rev2.0.en.md) document both state that `XM_EXT_ADC_1~4` are 16-bit. This page follows the latter (the enum comment + pinmap doc). To be certain, call `XM_GetAnalogResolution()` on the actual board.

**Example**

```c
uint8_t res = XM_GetAnalogResolution(XM_EXT_ADC_1);
```

---

### `XM_GetAnalogReadResolution`

```c
uint8_t XM_GetAnalogReadResolution(void);
```

Queries the **output** resolution currently configured by `XM_SetAnalogReadResolution()`.

**Parameters**: None

**Returns**: Current output resolution (bit count, default 16)

**⚠️ Calling context**: Assume `Control_Setup()` / `Control_Loop()` context.

**Example**

```c
XM_SetAnalogReadResolution(12);
uint8_t res = XM_GetAnalogReadResolution();  // 12
```

---

### 3. DIO → ADC Dynamic Switching (10kHz High-Speed Analog Input)

### `XM_SwitchDioToAdc`

```c
bool XM_SwitchDioToAdc(XmDioPin_t pin);
```

Switches an external DIO pin to a high-speed ADC3 input. By default, DIO pins (D1~D8) are digital I/O; calling this function switches the given pin to a 16-bit ADC3 analog input. ADC3 supports up to 8 simultaneous scan channels at 10kHz sampling.

**Parameters**

| Name | Type | Description |
|------|------|--------------|
| `pin` | `XmDioPin_t` | DIO pin to switch (`XM_EXT_DIO_1` ~ `XM_EXT_DIO_8`) |

**Returns**: `bool` — `true` (success), `false` (failure)

**⚠️ Calling context**: **Non-real-time** function. Call it only during initialization (`Control_Setup()`).
> After switching, the pin can no longer be restored to digital GPIO (a reboot is required) — this is intentional, safety-oriented behavior.

**Example**

```c
void Control_Setup(void) {
    XM_SetAnalogReadResolution(12);
    for (int i = 0; i < 8; i++) {
        XM_SwitchDioToAdc(XM_EXT_DIO_1 + i);  // DIO 1~8 → ADC3
    }
}

void Control_Loop(void) {
    uint16_t fsr[8];
    for (int i = 0; i < 8; i++) {
        fsr[i] = XM_AnalogRead(XM_EXT_ADC_5 + i);  // all 0~4095
    }
}
```

**See also**: [`XM_SwitchAllDioToAdc`](#xm_switchalldiotoadc) — batch-switch version for all 8 pins, [`XM_DIO_TO_ADC_PIN`](#xm_dio_to_adc_pin-macro) — conversion macro

---

### `XM_SwitchAllDioToAdc`

```c
bool XM_SwitchAllDioToAdc(void);
```

A convenience function that switches all DIO pins (D1~D8) to ADC3 inputs at once. Use this to switch all 8 channels without a loop.

**Parameters**: None

**Returns**: `bool` — `true` (all succeeded), `false` (one or more failed)

**⚠️ Calling context**: **Non-real-time** function. Like `XM_SwitchDioToAdc()`, call it only in `Control_Setup()`.

**Example**

```c
void Control_Setup(void) {
    XM_SetAnalogReadResolution(12);
    XM_SwitchAllDioToAdc();  // Batch-switch DIO 1~8 → ADC3
}
```

---

### `XM_IsDioSwitchedToAdc`

```c
bool XM_IsDioSwitchedToAdc(XmDioPin_t pin);
```

Checks whether a DIO pin is currently switched to ADC mode.

**Parameters**

| Name | Type | Description |
|------|------|--------------|
| `pin` | `XmDioPin_t` | DIO pin to check (`XM_EXT_DIO_1` ~ `XM_EXT_DIO_8`) |

**Returns**: `bool` — `true` (ADC mode), `false` (GPIO mode or an invalid pin)

**⚠️ Calling context**: Assume `Control_Setup()` / `Control_Loop()` context.

**Example**

```c
XM_SwitchDioToAdc(XM_EXT_DIO_1);
if (XM_IsDioSwitchedToAdc(XM_EXT_DIO_1)) {
    // ADC mode — XM_AnalogRead() is now usable
}
```

---

### `XM_DIO_TO_ADC_PIN` (macro)

```c
#define XM_DIO_TO_ADC_PIN(dio)  ((XmAdcPin_t)((dio) + XM_EXT_ADC_5))
```

Converts a DIO pin number to its corresponding ADC pin number. Use this after `XM_SwitchDioToAdc()` to intuitively obtain the ADC pin value to pass into `XM_AnalogRead()`.

**Parameters**

| Name | Type | Description |
|------|------|--------------|
| `dio` | `XmDioPin_t` | DIO pin number (`XM_EXT_DIO_1` ~ `XM_EXT_DIO_8`) |

**Returns**: The corresponding `XmAdcPin_t` value (`XM_EXT_ADC_5` ~ `XM_EXT_ADC_12`)

**⚠️ Calling context**: A preprocessor macro — no restriction (compile-time substitution).

**Example**

```c
XM_SwitchDioToAdc(XM_EXT_DIO_1);
uint16_t val = XM_AnalogRead(XM_DIO_TO_ADC_PIN(XM_EXT_DIO_1));  // reads XM_EXT_ADC_5
```

---

### 4. Extension Port Power Voltage 🟢 Rev 2.0 Only

### `XM_SetExtPowerVoltage`

```c
void XM_SetExtPowerVoltage(XmExtPwrVoltage_t voltage);
```

> 🟢 **Rev 2.0 Only** — This function and the `XmExtPwrVoltage_t` type do not exist in the Rev1.1 header.

Switches the extension port's **sensor supply voltage** to 3.3V or 5V (controls the power MUX via the `EXT_PWR_SEL_5V` board signal, PE3 GPIO). The default is 3.3V; switch to 5V when using an external sensor that operates at 5V.

**Parameters**

| Name | Type | Description |
|------|------|--------------|
| `voltage` | `XmExtPwrVoltage_t` | `XM_EXT_PWR_3V3` (3.3V, default) or `XM_EXT_PWR_5V` (5V) |

**Returns**: None (`void`)

**⚠️ Calling context**: Non-real-time function (per the header). Call it in `Control_Setup()`.

> ⚠️ This voltage powers (drives) the sensor. **The ADC signal input range is always 0~3.3V, regardless of the supply voltage.** If a sensor driven at 5V outputs a signal above 3.3V, the ADC pin can be damaged — verify the signal line stays within 0~3.3V, and add a voltage divider if needed.

**Example**

```c
void Control_Setup(void) {
    XM_SetExtPowerVoltage(XM_EXT_PWR_5V);   // Drive a 5V sensor
    XM_SwitchDioToAdc(XM_EXT_DIO_1);        // DIO_1 → ADC (XM_EXT_ADC_5)
    // Then read the sensor voltage (mV) with XM_AnalogReadMillivolts(XM_EXT_ADC_5)
}
```

**See also**: [`XmExtPwrVoltage_t`](#xmextpwrvoltage_t-🟢-rev-20-only), [40_EMG_Proportional_Assist example](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/40_EMG_Proportional_Assist/)

---

### 5. External UART IMU Attachment (Xsens MTi-630)

> ⚠️ **As of v2.7.0 these two functions are not in the default build.**
> The single External UART port cannot be shared between the general-purpose Serial API and
> the Xsens driver (there is only one receive-callback slot per port, and whoever registers
> last silently overwrites the other). `XM_EXTERNAL_UART_XSENS_ENABLE` in
> `XM_FW/System/Config/module.h` therefore selects **one or the other at build time**, and it
> **defaults to `0` (general-purpose Serial)**.
>
> - Default build (`0`) → use the [general-purpose External Serial API](#6-general-purpose-external-serial-api--rev-20-only).
>   Calling the two functions below gives an **"undeclared function" compile error**.
> - To use the Xsens driver, set `XM_EXTERNAL_UART_XSENS_ENABLE` to `1` in `module.h` and
>   rebuild. The general-purpose Serial API then disappears instead.
>
> This is deliberate: the build stops rather than misbehaving silently.

### `XM_AttachXsensMTi630`

```c
void XM_AttachXsensMTi630(void);
```

Attaches an external Xsens MTi-630 IMU sensor to the External UART port. Call it once in `Control_Setup()`; afterward you can access sensor data through the `XM.status.ext_imu.*` fields. It is safe to call even when no sensor is physically connected — plugging in the cable automatically transitions it to the OPERATIONAL state.

> ⚠️ **Revision-specific behavior** — the function signature and user code are identical across both revisions, but the underlying hardware port they attach to differs.
> - **Rev 2.0**: Attaches to a dedicated USART2 port (PD5=TX, PD6=RX, 921600bps). No resource conflict with any ADC/DIO pin currently in use.
> - **Rev 1.1**: Attaches by dynamically switching PA0/PA1 to UART4 (internally calls `ExternalIO_SwitchToUartMode`). After this call, `XM_EXT_ADC_1`/`XM_EXT_ADC_3` (PA0/PA1) can no longer be used as ADC — use `XM_EXT_ADC_2`/`XM_EXT_ADC_4` (PA0_C/PA1_C) instead. If not called, PA0/PA1 remain available as ADC.

**Parameters**: None

**Returns**: None (`void`)

**⚠️ Calling context**: Call once in `Control_Setup()` (per the header). Not intended for repeated calls inside `Control_Loop()`.

**Example**

```c
void Control_Setup(void)
{
    XM_AttachXsensMTi630();
    // If this is a brand-new/factory-reset sensor:
    // XM_ConfigureXsensMTi630();
}
```

**See also**: [`XM_ConfigureXsensMTi630`](#xm_configurexsensmti630), [04. External IO — Note on External IMU Usage](../04-external-io.en.md#note-on-external-imu-usage)

---

### `XM_ConfigureXsensMTi630`

```c
void XM_ConfigureXsensMTi630(void);
```

Sends the Xsens MTi-630's Output Configuration (1kHz Quaternion + Acc + Gyro output) once. This is only needed for a brand-new or factory-reset sensor — if the sensor's EEPROM already retains its configuration (a sensor being reused), this call is unnecessary.

**Parameters**: None

**Returns**: None (`void`)

**⚠️ Calling context**: Blocks for about 1 second, so do not call it inside the 1ms `Control_Loop()`. Call it only in `Control_Setup()` or an equivalent non-real-time point, and only after calling [`XM_AttachXsensMTi630()`](#xm_attachxsensmti630) first.

**Example**

```c
void Control_Setup(void)
{
    XM_AttachXsensMTi630();
    XM_ConfigureXsensMTi630();  // One-time setup for a new sensor (blocks ~1s)
}
```

**See also**: [`XM_AttachXsensMTi630`](#xm_attachxsensmti630)

---

### 6. General-purpose External Serial API 🟢 Rev 2.0 only

**New in v2.7.0.** Use the External UART port with whatever message format you decide.
The peer can be another XM10, an Arduino, a PC, a Raspberry Pi — anything. Framing,
checksums and parsing are yours to define (raw bytes are transferred).

**The hardware settings are fixed — configure your peer to match.**

| Item | Value | Changeable |
|---|---|---|
| Peripheral / pins | **USART2**, TX=**PD5** (`EXT_UART_TX`) / RX=**PD6** (`EXT_UART_RX`) | ✗ |
| Logic level | **3.3 V** — never wire 5 V directly | ✗ |
| Default baud | **921600 bps** | ✅ `XM_SetExternalUartBaudrate()` |
| Data / parity / stop | **8 / none / 1** | ✗ |
| Flow control | **None** (RTS/CTS unused) | ✗ |
| Max bytes per send | **128** (`XM_EXT_UART_TX_MAX_BYTES`) | ✗ |

> 🛑 **Do not use this wiring on a Rev 1.1 board.** Rev 1.1 does not have this port, and
> **PD6 there is `USB_PWR_ON` — an output that switches USB power.** Connecting a peer's TX
> to it puts two outputs against each other. This API is also absent from the Rev 1.1 SDK.

Related example: [43_External_UART_PingPong](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/43_External_UART_PingPong/)

---

### `XM_AttachExternalUart`

```c
typedef void (*XmExternalUartRxFunc_t)(const uint8_t* data, uint32_t len);

bool XM_AttachExternalUart(XmExternalUartRxFunc_t rx_callback);
```

Starts External UART reception and registers a callback. Call it once in `Control_Setup()`.
Passing `NULL` detaches reception.

**Returns**: `true` = registered / `false` = port not ready

> ⚠️ **Copy, and nothing else, inside the callback.** It runs on the XM10's **shared receive
> task**, and that same task also handles the **1 kHz foot-sensor (GRF) stream**. Parsing,
> computing or waiting here delays GRF data by exactly that much.

> ⚠️ **The `data` pointer is invalid once the callback returns.** It points at an internal
> scratch buffer — copy the contents, do not store the pointer.

> ⚠️ **One callback is not one message.** Reception arrives in chunks of up to 128 bytes and
> may split or concatenate messages. You must find the message boundaries yourself.

**Example**

```c
static volatile uint8_t  s_rx[256];
static volatile uint16_t s_rx_len;

static void OnSerialRx(const uint8_t* data, uint32_t len)
{
    for (uint32_t i = 0; i < len && s_rx_len < sizeof(s_rx); i++) {
        s_rx[s_rx_len++] = data[i];      /* copy only! */
    }
}

void Control_Setup(void)
{
    XM_SetExternalUartBaudrate(XM_UART_BAUD_115200);
    XM_AttachExternalUart(OnSerialRx);
}
```

---

### `XM_SetExternalUartBaudrate`

```c
bool XM_SetExternalUartBaudrate(XmUartBaudrate_t baud);
```

Changes the baud rate. It **must match the peer device**. Call it **before**
`XM_AttachExternalUart()`.

**Parameters**: `XM_UART_BAUD_9600` / `_19200` / `_38400` / `_57600` / `_115200` / `_230400` /
`_460800` / `_921600` (boot default)

**Returns**: `true` = success / `false` = port not ready, or value out of range

---

### `XM_SendExternalUartData`

```c
bool XM_SendExternalUartData(const void* data, uint32_t len);
```

Sends data. **It does not wait.**

**Parameters**: `data` payload / `len` byte count (1 – `XM_EXT_UART_TX_MAX_BYTES` = 128)

**Returns**: `true` = transmission started / `false` = previous transmission still in flight,
or invalid argument

> **`false` is not an error — it means "busy right now".** Calling this every tick from a
> 1 kHz `Control_Loop()` will not stall the loop; on `false`, just retry next tick.
> More than 128 bytes is rejected with `false`, so split longer payloads.

---

### `XM_SendExternalUartDataBlocking`

```c
bool XM_SendExternalUartDataBlocking(const void* data, uint32_t len);
```

Waits until transmission completes.

**⚠️ Call context**: **`Control_Setup()` only.** It can block for up to 5 seconds, so calling
it from `Control_Loop()` breaks the control period, and calling it from the receive callback
stalls GRF reception too. Use it only to send a one-off setup command.

---

### `XM_EnsureExternalUartRxArmed`

```c
void XM_EnsureExternalUartRxArmed(void);
```

Revives reception if it has stalled. Unplugging and replugging a cable, or a framing error
caused by noise, can leave hardware reception stopped; calling this periodically recovers it
automatically. **When everything is fine it does nothing** — it is a cheap call.

**Call period**: **~100 ms** recommended (use a counter inside `Control_Loop()`).

---

## Types / Macros

### `XmDioPin_t`

All 8 DIO pins map to the same physical pins on both revisions.

| Value | Physical Pin | Description |
|----|--------|------|
| `XM_EXT_DIO_1` = 0 | PF3 | |
| `XM_EXT_DIO_2` | PF4 | |
| `XM_EXT_DIO_3` | PF5 | |
| `XM_EXT_DIO_4` | PF6 | |
| `XM_EXT_DIO_5` | PF7 | |
| `XM_EXT_DIO_6` | PF8 | |
| `XM_EXT_DIO_7` | PF9 | |
| `XM_EXT_DIO_8` | PF10 | |
| `XM_EXT_DIO_COUNT` | — | (internal) pin count |

### `XmAdcPin_t`

The physical pin and native resolution of the 4 fixed ADC pins (`XM_EXT_ADC_1~4`) **differ by revision**. The dynamic ADC3 group (`XM_EXT_ADC_5~12`) is identical on both revisions.

| Value | Rev 1.1 | Rev 2.0 | Notes |
|----|---------|---------|------|
| `XM_EXT_ADC_1` = 0 | PA0, **ADC1 12-bit** native [Shared: UART4_TX] | PB0 (ADC1_INP9), **ADC1 16-bit** native | Rev1.1: occupied by UART4 once the IMU is attached |
| `XM_EXT_ADC_2` | PA0_C, **ADC2 16-bit** native | PB1 (ADC1_INP5), **ADC1 16-bit** native | Rev1.1: use this instead of ADC_1 after IMU attach |
| `XM_EXT_ADC_3` | PA1, **ADC1 12-bit** native [Shared: UART4_RX] | PF11 (ADC1_INP2), **ADC1 16-bit** native | Rev1.1: occupied by UART4 once the IMU is attached |
| `XM_EXT_ADC_4` | PA1_C, **ADC2 16-bit** native | PF12 (ADC1_INP6), **ADC1 16-bit** native | Rev1.1: use this instead of ADC_3 after IMU attach |
| `XM_EXT_ADC_5` | PF3 (DIO 1 → ADC3, 16-bit) | PF3 (DIO 1 → ADC3, 16-bit) | Requires `XM_SwitchDioToAdc()` |
| `XM_EXT_ADC_6` | PF4 (DIO 2 → ADC3, 16-bit) | PF4 (DIO 2 → ADC3, 16-bit) | ditto |
| `XM_EXT_ADC_7` | PF5 (DIO 3 → ADC3, 16-bit) | PF5 (DIO 3 → ADC3, 16-bit) | ditto |
| `XM_EXT_ADC_8` | PF6 (DIO 4 → ADC3, 16-bit) | PF6 (DIO 4 → ADC3, 16-bit) | ditto |
| `XM_EXT_ADC_9` | PF7 (DIO 5 → ADC3, 16-bit) | PF7 (DIO 5 → ADC3, 16-bit) | ditto |
| `XM_EXT_ADC_10` | PF8 (DIO 6 → ADC3, 16-bit) | PF8 (DIO 6 → ADC3, 16-bit) | ditto |
| `XM_EXT_ADC_11` | PF9 (DIO 7 → ADC3, 16-bit) | PF9 (DIO 7 → ADC3, 16-bit) | ditto |
| `XM_EXT_ADC_12` | PF10 (DIO 8 → ADC3, 16-bit) | PF10 (DIO 8 → ADC3, 16-bit) | ditto |
| `XM_EXT_ADC_COUNT` | — | — | (internal) pin count |

> The Rev2.0 `XM_EXT_ADC_1~4` resolutions in this table are based on the enum comment and the [Rev 2.0 hardware pinmap](../../hardware/external-gpio-rev2.0.en.md) document — see the header-wording inconsistency note under [`XM_GetAnalogResolution`](#xm_getanalogresolution) as well.

Common input range: 0~3.3V, sampling at 10kHz (fixed pins) / 10kHz (ADC3 group). Output can be selected among 8/10/12/14/16-bit via [`XM_SetAnalogReadResolution()`](#xm_setanalogreadresolution).

### `XmPinMode_t`

| Value | Description |
|----|------|
| `XM_EXT_DIO_MODE_INPUT` | Digital input (floating) |
| `XM_EXT_DIO_MODE_INPUT_PULLUP` | Digital input (internal pull-up resistor) |
| `XM_EXT_DIO_MODE_INPUT_PULLDOWN` | Digital input (internal pull-down resistor) |
| `XM_EXT_DIO_MODE_OUTPUT` | Digital output |

### `XmLogicLevel_t`

| Value | Description |
|----|------|
| `XM_LOW` = 0 | 0V (GND) |
| `XM_HIGH` = 1 | 3.3V (VCC) |

### `XmExtPwrVoltage_t` 🟢 Rev 2.0 Only

> Not defined in the Rev1.1 header.

| Value | Description |
|----|------|
| `XM_EXT_PWR_3V3` = 0 | 3.3V output (default, Low) |
| `XM_EXT_PWR_5V` = 1 | 5V output (High) |

---

## Diagnostic Global Variable 🟢 Rev 2.0 Only

### `g_xm_adc_read_before_switch`

```c
extern volatile uint16_t g_xm_adc_read_before_switch;
```

> 🟢 **Rev 2.0 Only** — This variable does not exist in the Rev1.1 header.

A diagnostic bitmask that records, one bit per pin, whether an ADC3-group pin (`XM_EXT_ADC_5~12`) was read via [`XM_AnalogRead()`](#xm_analogread)/[`XM_AnalogReadMillivolts()`](#xm_analogreadmillivolts) without first being switched via [`XM_SwitchDioToAdc()`](#xm_switchdiotoadc). `bit0` = `XM_EXT_ADC_5` (DIO_1) … `bit7` = `XM_EXT_ADC_12` (DIO_8). Reading an ADC3 pin before switching causes the lower layer to return `0` (indistinguishable from a genuine 0V reading); that situation is recorded per-channel in this bitmask. A value of `0` means every ADC read so far has been valid.

**Type**: `volatile uint16_t` (extern) — the header provides no write API, so treat this as read-only.

**⚠️ Calling context**: Can be read from `Control_Loop()`, STM32CubeIDE Live Expressions, or anywhere in user code (observable even in a USB-less, SWD-only setup — it does not depend on any particular output channel). Being `volatile`, every read fetches the actual memory value.

**Example**

```c
// When ADC reads keep coming back as 0 and you want to find out why:
if (g_xm_adc_read_before_switch != 0U) {
    // One of bit0=ADC_5(DIO1) ... bit7=ADC_12(DIO8) is missing its XM_SwitchDioToAdc() call
}
```

**See also**: [`XM_SwitchDioToAdc`](#xm_switchdiotoadc), [`XM_IsDioSwitchedToAdc`](#xm_isdioswitchedtoadc)

---

## Internal Only (Do Not Call)

No functions, types, or variables defined in this header are marked `[Internal]` or System-only — all 14 functions, 1 macro, 5 types, and 1 diagnostic variable covered above are public API.

---

## Rev1.1 / Rev2.0 Differences Summary

| Item | Rev1.1 | Rev2.0 |
|------|--------|--------|
| Basic DIO control (`XM_SetPinMode`/`DigitalWrite`/`DigitalRead`) for all 8 pins | ✅ | ✅ |
| Unified ADC read API (`XM_AnalogRead`, etc.) | ✅ | ✅ |
| `XM_EXT_ADC_1~4` physical pins / native resolution | PA0/PA0_C/PA1/PA1_C — mix of ADC1 12-bit + ADC2 16-bit | PB0/PB1/PF11/PF12 — all ADC1 16-bit |
| DIO → ADC3 dynamic switching (`XM_SwitchDioToAdc`, etc.) | ✅ | ✅ |
| `XM_AttachXsensMTi630` internal attach method | PA0/PA1 dynamically switched to UART4 (occupies `XM_EXT_ADC_1`/`_3`) | Dedicated USART2 (PD5/PD6), no ADC resource occupied |
| `XM_SetExtPowerVoltage` / `XmExtPwrVoltage_t` (3.3V/5V power switch) | ❌ Not available | 🟢 Only |
| `g_xm_adc_read_before_switch` diagnostic variable | ❌ Not available | 🟢 Only |

---

## Related Documents

- [04. External IO Control API (concept)](../04-external-io.en.md) — operating principle, common mistakes, example mapping
- [Hardware Pinmap — Rev 1.1](../../hardware/external-gpio-rev1.1.en.md) / [Rev 2.0](../../hardware/external-gpio-rev2.0.en.md)
- [Board Revision Comparison](../../hardware/README.en.md#board-revision-comparison)
