# External IO Control API

Functions in `xm_api_external_io.h` for controlling the Extension Port. You can read external sensor values or control digital devices in a style similar to Arduino. The default configuration is 8 DIO + 4 ADC channels; if needed, DIO pins can be dynamically switched to ADC mode to expand up to 12 analog input channels.

> **Board pin locations and labels** may vary by board revision. Refer to the [Hardware Pinmap — Rev 1.1](../hardware/external-gpio-rev1.1.md) / [Rev 2.0](../hardware/external-gpio-rev2.0.md) for physical details. This page focuses on **how to use the functions**.

---

## Pin Overview

| API Name | Type | Notes |
| :--- | :--- | :--- |
| `XM_EXT_DIO_1` ~ `XM_EXT_DIO_8` | Digital I/O | All 8 pins can be dynamically switched to ADC |
| `XM_EXT_ADC_1` ~ `XM_EXT_ADC_4` | Analog input (fixed) | 0–3.3 V, 10 kHz sampling |
| `XM_EXT_ADC_5` ~ `XM_EXT_ADC_12` | Analog input (dynamic) | Available after calling `XM_SwitchDioToAdc()` |

> For PCB labels, header locations, and pin assignments, see the [hardware pinmap documentation](../hardware/).

---

## Note on External IMU Usage

When you enable an external IMU (XSENS MTi-630) by calling `XM_AttachXsensMTi630()`, some ADC pins on Rev 1.1 are reassigned for UART communication. (Rev 2.0 uses a dedicated USART2 port, so no ADC pins are occupied.) Which pins are affected depends on the board revision — see the "Note on External IMU Usage" section in the relevant [pinmap documentation](../hardware/).

---

## Data Structures

### Pin Identifiers

#### `XmDioPin_t`

```c
typedef enum {
    XM_EXT_DIO_1 = 0,
    XM_EXT_DIO_2,
    XM_EXT_DIO_3,
    XM_EXT_DIO_4,
    XM_EXT_DIO_5,
    XM_EXT_DIO_6,
    XM_EXT_DIO_7,
    XM_EXT_DIO_8,
    XM_EXT_DIO_COUNT
} XmDioPin_t;
```

#### `XmAdcPin_t`

```c
typedef enum {
    XM_EXT_ADC_1 = 0,
    XM_EXT_ADC_2,
    XM_EXT_ADC_3,
    XM_EXT_ADC_4,
    /* Available after dynamic DIO → ADC switch (requires XM_SwitchDioToAdc call) */
    XM_EXT_ADC_5,   // DIO 1 → ADC
    XM_EXT_ADC_6,   // DIO 2 → ADC
    XM_EXT_ADC_7,   // DIO 3 → ADC
    XM_EXT_ADC_8,   // DIO 4 → ADC
    XM_EXT_ADC_9,   // DIO 5 → ADC
    XM_EXT_ADC_10,  // DIO 6 → ADC
    XM_EXT_ADC_11,  // DIO 7 → ADC
    XM_EXT_ADC_12,  // DIO 8 → ADC
    XM_EXT_ADC_COUNT
} XmAdcPin_t;
```

### Configuration Types

#### `XmPinMode_t`

Determines the operating mode of a pin. Used with the `XM_SetPinMode` function.

```c
typedef enum {
    XM_EXT_DIO_MODE_INPUT,           /**< Digital input (floating) */
    XM_EXT_DIO_MODE_INPUT_PULLUP,    /**< Digital input (internal pull-up resistor) */
    XM_EXT_DIO_MODE_INPUT_PULLDOWN,  /**< Digital input (internal pull-down resistor) */
    XM_EXT_DIO_MODE_OUTPUT           /**< Digital output */
} XmPinMode_t;
```

| Mode | Description |
| :--- | :--- |
| **`XM_EXT_DIO_MODE_INPUT`** | **Digital input (default).** Leaves the pin in a floating state. |
| **`XM_EXT_DIO_MODE_INPUT_PULLUP`** | **Pull-up input.** Connects the pin to 3.3 V through an internal resistor. Useful when connecting switches. |
| **`XM_EXT_DIO_MODE_INPUT_PULLDOWN`** | **Pull-down input.** Connects the pin to 0 V through an internal resistor. |
| **`XM_EXT_DIO_MODE_OUTPUT`** | **Digital output.** Drives 0 V or 3.3 V. (5 V output is possible with hardware modification.) |

#### `XmLogicLevel_t`

Represents the level of a digital signal.

```c
typedef enum {
    XM_LOW  = 0, /**< 0 V (GND) */
    XM_HIGH = 1  /**< 3.3 V (VCC) */
} XmLogicLevel_t;
```

-----

## 3\. Function Reference

### 3.1. Configuration Function

#### `XM_SetPinMode`

Sets the operating mode (input / output / analog) of a pin. Must be called before using the pin.

  * **Syntax**
    ```c
    void XM_SetPinMode(XmDioPin_t pin, XmPinMode_t mode);
    ```
  * **Parameters**
      * `pin`: Pin to configure (`XM_EXT_DIO_1` \~ `8`)
      * `mode`: Operating mode (`XM_EXT_DIO_MODE_INPUT`, `XM_EXT_DIO_MODE_INPUT_PULLUP`, `XM_EXT_DIO_MODE_INPUT_PULLDOWN`, `XM_EXT_DIO_MODE_OUTPUT`)
  * **Note**: This is a non-real-time function. It calls `HAL_GPIO_Init` internally, so do **not** call it inside the 1 ms real-time control loop (1 kHz). Configure pins once in `Control_Setup()`.
  * **Example**
    ```c
    // Configure pin 3 as pull-up input (for a switch)
    XM_SetPinMode(XM_EXT_DIO_3, XM_EXT_DIO_MODE_INPUT_PULLUP);

    // Configure pin 1 as pull-down input
    XM_SetPinMode(XM_EXT_DIO_1, XM_EXT_DIO_MODE_INPUT_PULLDOWN);
    ```

-----

### 3.2. Digital I/O Functions

#### `XM_DigitalWrite`

Drives a digital pin high or low. Only effective when the pin is in `XM_EXT_DIO_MODE_OUTPUT` mode.

  * **Syntax**
    ```c
    void XM_DigitalWrite(XmDioPin_t pin, XmLogicLevel_t level);
    ```
  * **Parameters**
      * `pin`: Target pin
      * `level`: `XM_HIGH` (3.3 V) or `XM_LOW` (0 V)
  * **Example**
    ```c
    // Turn on the LED connected to pin 4
    XM_DigitalWrite(XM_EXT_DIO_4, XM_HIGH);
    ```

#### `XM_DigitalRead`

Reads the current voltage state of a digital pin.

  * **Syntax**
    ```c
    XmLogicLevel_t XM_DigitalRead(XmDioPin_t pin);
    ```
  * **Returns**: `XM_HIGH` (input is near 3.3 V) or `XM_LOW` (input is near 0 V)
  * **Example**
    ```c
    // Check whether the pull-up switch on pin 3 is pressed (LOW when pressed)
    if (XM_DigitalRead(XM_EXT_DIO_3) == XM_LOW) {
        // Handle switch press
    }
    ```

-----

### 3.3. Analog I/O Functions

#### `XM_AnalogRead`

Reads the pin voltage as a 16-bit integer.

  * **Syntax**
    ```c
    uint16_t XM_AnalogRead(XmAdcPin_t pin);
    ```
  * **Returns**: 0 \~ 65535 (maps to 0 V \~ 3.3 V; 16-bit by default)
  * **Note**: Even if the hardware ADC resolution is 12-bit, the API always returns a **16-bit normalized** value. Use `XM_SetAnalogReadResolution()` to change the output resolution.

#### `XM_AnalogReadMillivolts`

Reads the pin voltage in millivolts (mV).

  * **Syntax**
    ```c
    uint16_t XM_AnalogReadMillivolts(XmAdcPin_t pin);
    ```
  * **Returns**: 0 ~ 3300 (mV)

#### `XM_SetAnalogReadResolution`

Sets the output resolution of `XM_AnalogRead`.

  * **Syntax**
    ```c
    void XM_SetAnalogReadResolution(uint8_t bits);
    ```
  * **Parameters**
      * `bits`: Desired resolution in bits (8, 10, 12, or 16)
  * **Example**
    ```c
    XM_SetAnalogReadResolution(12); // Returns values in the 0–4095 range
    ```

#### `XM_GetAnalogResolution`

Returns the **hardware-native** resolution of the specified ADC pin.

  * **Syntax**
    ```c
    uint8_t XM_GetAnalogResolution(XmAdcPin_t pin);
    ```
  * **Parameters**: `pin` — ADC pin to query
  * **Returns**: Native resolution of that ADC channel (in bits)
  * **Example**
    ```c
    uint8_t res1 = XM_GetAnalogResolution(XM_EXT_ADC_1);  // 12 (ADC1, 12-bit)
    uint8_t res2 = XM_GetAnalogResolution(XM_EXT_ADC_2);  // 16 (ADC2, 16-bit)
    ```

#### `XM_GetAnalogReadResolution`

Returns the current **output** resolution of `XM_AnalogRead`. Reflects the value set by `XM_SetAnalogReadResolution()`.

  * **Syntax**
    ```c
    uint8_t XM_GetAnalogReadResolution(void);
    ```
  * **Returns**: Current output resolution in bits (default: 16)

-----

### 3.4. DIO ↔ ADC Dynamic Switch Functions *(new in v2.0.0)*

Switch DIO pins to ADC mode at runtime, enabling up to 12 analog input channels (8 DIO + 4 fixed ADC).

#### `XM_SwitchDioToAdc`

Switches a specific DIO pin to ADC mode.

  * **Syntax**
    ```c
    bool XM_SwitchDioToAdc(XmDioPin_t pin);
    ```
  * **Parameters**
      * `pin`: DIO pin to switch to ADC
  * **Returns**: `true` (switch successful), `false` (pin not supported)
  * **Example**
    ```c
    // Switch DIO_1 to ADC, then read it
    XM_SwitchDioToAdc(XM_EXT_DIO_1);
    uint16_t val = XM_AnalogRead(XM_DIO_TO_ADC_PIN(XM_EXT_DIO_1));
    ```

#### `XM_SwitchAllDioToAdc`

Switches all DIO pins to ADC mode at once.

  * **Syntax**
    ```c
    bool XM_SwitchAllDioToAdc(void);
    ```
  * **Returns**: `true` (switch successful)

#### `XM_IsDioSwitchedToAdc`

Checks whether a specific DIO pin is currently in ADC mode.

  * **Syntax**
    ```c
    bool XM_IsDioSwitchedToAdc(XmDioPin_t dio_pin);
    ```
  * **Returns**: `true` (ADC mode), `false` (DIO mode)

#### `XM_DIO_TO_ADC_PIN` (macro)

Converts a DIO pin identifier to the corresponding ADC pin identifier. Use this with `XM_AnalogRead()` after calling `XM_SwitchDioToAdc()`.

  * **Syntax**
    ```c
    #define XM_DIO_TO_ADC_PIN(dio) // Maps DIO index → ADC index
    ```

-----

### 3.5. External IMU Control

#### `XM_AttachXsensMTi630` / `XM_ConfigureXsensMTi630`

Attaches an external IMU (XSENS MTi-630) to the External UART (USART2). After attaching, access IMU data through `XM.status.ext_imu.*`.

  * **Syntax**
    ```c
    void XM_AttachXsensMTi630(void);     // Attach the IMU to USART2 (call once in Control_Setup)
    void XM_ConfigureXsensMTi630(void);  // Send one-time configuration to a brand-new or factory-reset sensor (blocking ~1 s; call Attach first)
    ```
  * **Note**: `XM_AttachXsensMTi630()` is safe to call even when no sensor is connected — it activates automatically once the cable is plugged in. On Rev 1.1, attaching the IMU reassigns `XM_EXT_ADC_1` and `XM_EXT_ADC_3` to UART, making them unavailable for ADC use. (Rev 2.0 uses a dedicated USART2 port, so no ADC pins are affected.)

---

### 3.6. Extension Port Power (3.3V / 5V) *(Rev 2.0)*

Switches the **sensor supply voltage** of the extension port between 3.3V and 5V (board signal `EXT_PWR_SEL_5V`, PE3). To use an external sensor that runs on 5V (e.g. some EMG modules), switch to 5V in `Control_Setup()`. **The default is 3.3V.**

> ⚠️ This voltage **powers** the sensor. The **ADC signal input range is always 0~3.3V**, independent of the supply. If the **output (signal)** of a 5V-powered sensor exceeds 3.3V it can damage the ADC pin — keep the signal line within 0~3.3V and add a voltage divider if needed.

#### `XM_SetExtPowerVoltage`

  * **Syntax**
    ```c
    void XM_SetExtPowerVoltage(XmExtPwrVoltage_t voltage);
    ```
  * **Parameters**
      * `voltage`: `XM_EXT_PWR_3V3` (3.3V, default) or `XM_EXT_PWR_5V` (5V)
  * **Example**
    ```c
    void Control_Setup(void) {
        XM_SetExtPowerVoltage(XM_EXT_PWR_5V);   // power a 5V sensor
        XM_SwitchDioToAdc(XM_EXT_DIO_1);        // DIO_1 -> ADC (XM_EXT_ADC_5)
        // then read with XM_AnalogReadMillivolts(XM_EXT_ADC_5) in mV
    }
    ```
  * **Note**: Extension-port voltage switching is confirmed on Rev 2.0.

---

## Related Examples

| Example | Difficulty | External I/O Usage |
|------|--------|-------------|
| [04_Ext_IO_Basic](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/04_Ext_IO_Basic/) | Beginner | DIO input and output |
| [05_Ext_IO_analog](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/05_Ext_IO_analog/) | Beginner | Reading fixed ADC voltage |
| [05a_Ext_IO_DIO_to_ADC](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/05a_Ext_IO_DIO_to_ADC/) | Beginner | Single DIO → ADC switch |
| [05b_Ext_IO_FSR_8ch](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/05b_Ext_IO_FSR_8ch/) | Intermediate | Batch switch all 8 channels + resolution |
| [05c_Ext_IO_Mixed_ADC](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/05c_Ext_IO_Mixed_ADC/) | Intermediate | Mixed fixed and dynamic ADC |
| [05d_Ext_IO_DIO_ADC_Hybrid](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/05d_Ext_IO_DIO_ADC_Hybrid/) | Advanced | Mixed GPIO and ADC mode |
| [06_Ext_IO_Safety_Switch](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/06_Ext_IO_Safety_Switch/) | Intermediate | Safety switch interlock |

---

## ⚠️ Common Mistakes

| Symptom | Cause | Fix |
|------|------|------|
| `XM_EXT_ADC_1` or `_3` always returns 0 mV | `XM_AttachXsensMTi630()` is active and has claimed the pin for UART (Rev 1.1) | On Rev 1.1: use `XM_EXT_ADC_2` / `XM_EXT_ADC_4` (PA0_C / PA1_C). On Rev 2.0: unaffected (dedicated USART2 port) |
| GPIO not restored after `SwitchDioToAdc` | ADC mode switch is permanent until reboot (by design) | Reset the board or power-cycle |
| `DigitalWrite(ADC-switched DIO, ...)` has no effect | Guard mechanism — intentional behavior | ADC pins must only be used through the ADC API |
| Raw value exceeds 255 with 8-bit resolution | `SetAnalogReadResolution` not called, or mV API used (resolution does not apply to mV reads) | Call `SetAnalogReadResolution(8)` in setup |
| mV always reads 0 or 3300 with FSR voltage divider | Missing 10 kΩ pull-down resistor, or FSR short circuit | Wire as: FSR (3.3 V) — DIO pin — 10 kΩ — GND |
| `XM_DIO_TO_ADC_PIN` does not compile | Pin outside DIO 1–8 range used (e.g., DIO_9) | Macro is only valid for DIO_1 through DIO_8 |
| High ADC noise | Long wires or ground potential difference | Use short jumper wires, share a common GND, and add an external RC filter if needed |
