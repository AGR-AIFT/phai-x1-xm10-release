# API Reference: Struct Logging

> 📌 **After reading this page** you will be able to send your own structs to the PC together with their field names, units and types.
> ⏱️ Estimated reading time: 20 minutes
> 🧰 Prerequisites: [Ex.09](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/09_CDC_Stream/) CDC streaming
> 🎯 Key macros: `XM_LOG_DECLARE` / `XM_LOG_FIELDS` / `XM_LOG_REGISTER` / `XM_LOG_SEND`
>
> 🟢 **Rev 2.0 only** *(new in v2.9.0)*: The Rev 1.1 SDK does not have `xm_api_logging.h`. On Rev 1.1, use the [Ex.09](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/09_CDC_Stream/) way (`XM_SetUsbCustomMeta` + `XM_SendUsbDataWithId`).
>
> ⚠️ **USB-CDC single-owner rule**: **Connect only one PC program at a time.** If PhAI Studio, the `xm10` tool or a serial terminal (PuTTY, RealTerm, etc.) open the same COM port together, the port conflicts and data is lost.

This page is a guide to the **struct logging API** defined in `xm_api_logging.h`.
Register a C struct of your own once, and its field names, units and types go to the PC together with the values. (not yet confirmed on the board) <!-- board-check -->
Unlike the Ex.09 way, which sends only `float`, you can also use integer, `bool`, `double` and array members.

For the signature and return value of every macro and function, see the [`xm_api_logging.h` reference](ref/xm-api-logging.en.md). This page covers **how to use it**.

> ℹ️ **Board status**: We have not yet confirmed on a board the whole flow of sending a struct to the PC and viewing it in the `xm10` tool. Sentences that describe what you see on the PC are marked '(not yet confirmed on the board)'. <!-- board-check -->

-----

## 1. Operating Principle

Struct logging is **register once, send every loop**.

1. **Register** — In `Control_Setup()`, register the struct and its field descriptions (name, unit, scale) once. You also set the module ID and the send rate here.
2. **Send** — In `Control_Loop()`, fill in the values and call `XM_LOG_SEND()`. You can call it every loop; data goes out only at the registered rate, and before the rate is due nothing is sent and it returns `false`.
3. **Interpret** — The board also sends the field names, units and types to the PC together with the values. The `xm10` tool uses that information to decode the values and shows them as tabs, graphs and CSV. (not yet confirmed on the board) <!-- board-check -->

### How it differs from the existing way

| Item | Existing way ([Ex.09](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/09_CDC_Stream/)) | Struct logging |
|------|------|------|
| What you use | `XM_SetUsbCustomMeta()` + `XM_SendUsbDataWithId()` | `XM_LOG_DECLARE` · `XM_LOG_FIELDS` · `XM_LOG_REGISTER` · `XM_LOG_SEND` |
| Value types | No type is sent, so the PC reads every value as `float` | The C type of each member (`float` · `double` · `bool` · integers · arrays) is used as it is |
| Names and units | You write a JSON string separately and match it to the struct field order by hand | You write the name, unit and scale once in the field description (`XM_FIELD`) |
| When data is sent | Goes out whenever you call it — you build the rate yourself | Goes out only at the registered rate — you can call it every loop |
| Size per send | Up to 1,020 bytes | Struct up to 1,008 bytes (a multiple of 4) |
| Structs you can name | One per connection | Up to 4, one module ID per struct |
| If you break a rule | Columns shift or odd numbers appear | Compile error or registration `false` |
| Board | Rev 1.1 · Rev 2.0 | 🟢 **Rev 2.0 only** |

### Use it when

- You want to see a struct that mixes integers, `bool` and arrays on the PC together with names and units
- You want to split structs and send them at different rates (e.g., fast sensor values at 1 kHz, slow status values at 10 Hz)
- You want to keep a record with names and units attached to the struct fields

### Use something else when

- To check just a few `float` values quickly, use `XM_SendUsbDataWithId()`. → [05. USB Serial](05-usb-connectivity.en.md)
- To view a string in a terminal, use `XM_SendUsbDebugMessage()`. → [05. USB Serial](05-usb-connectivity.en.md)
- To put a few values into the 28-byte slot inside Total Data (`0x20`), use `XM_UserCustom_*`. → [`xm_api_user_custom.h`](ref/xm-api-user-custom.en.md)
- To save to a USB memory stick instead of sending to a PC → [06. USB Memory Logging](06-usb-data-logging.en.md)

-----

## 2. Getting Started — Send One Struct

This is a shortened version of [Ex.44](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/44_CDC_Struct_Logging/). It sends one struct at 100 Hz.

```c
#include "xm_api.h"
#include "xm_api_logging_raw.h"   // Always include this in a .c file that uses the XM_LOG_* macros

typedef struct {
    uint32_t tick_ms;             // Board time [ms]
    int16_t  ramp_x10;            // Integer in units of 0.1 % (0 ~ 999)
    uint16_t adc_mv[2];           // Extension ADC 1 ~ 2 [mV]
    bool     button[3];           // Whether buttons 1 ~ 3 are pressed
    uint8_t  pad[3];              // Empty bytes that make the size a multiple of 4 (not listed as a field)
} SensorLog_t;                                                        // ① 16 B

XM_LOG_DECLARE(SensorLog, SensorLog_t);                               // ② Give the struct a name (token)

XM_LOG_FIELDS(SensorLog) = {                                          // ③ Fields to send — the empty bytes (pad) are not listed
    XM_FIELD(SensorLog, tick_ms, "tick", "ms", 1.0f),
    XM_FIELD(SensorLog, ramp_x10, "ramp", "%", 0.1f),                 //    Physical value = value sent × 0.1 (scale)
    XM_FIELD_ARRAY(SensorLog, adc_mv, "adc", "mV", 1.0f),
    XM_FIELD_ARRAY(SensorLog, button, "button", "", 1.0f),
};

void Control_Setup(void)
{
    if (!XM_LOG_REGISTER(SensorLog, 0xF0, 100)) {                     // ④ Module ID · send rate [Hz]
        XM_SetLedEffect(XM_LED_3, XM_LED_BLINK, 200);                 //    Report a registration failure with an LED
    }
}

void Control_Loop(void)
{
    SensorLog_t s = { 0 };                                            // Zero everything, including the empty bytes (pad)
    s.tick_ms   = XM_GetTick();
    s.ramp_x10  = (int16_t)(s.tick_ms % 1000U);                       // Repeats 0 → 999 every second
    s.adc_mv[0] = XM_AnalogReadMillivolts(XM_EXT_ADC_1);
    s.adc_mv[1] = XM_AnalogReadMillivolts(XM_EXT_ADC_2);
    s.button[0] = (XM_GetButtonState(XM_BTN_1) == XM_PRESSED);
    s.button[1] = (XM_GetButtonState(XM_BTN_2) == XM_PRESSED);
    s.button[2] = (XM_GetButtonState(XM_BTN_3) == XM_PRESSED);

    (void)XM_LOG_SEND(SensorLog, &s);                                 // ⑤ You can call it every loop — data goes out at 100 Hz only
}
```

- ① The struct size must be a **multiple of 4** and at most 1,008 bytes. Fill the spare bytes with `uint8_t pad[]` and do not list them with `XM_FIELD`. Bytes you do not list as a field are still carried in the data.
- ② `XM_LOG_DECLARE(token, struct)` gives the struct a short name (11 characters or fewer). This token becomes the struct name.
- ③ One field is one line of `XM_FIELD(token, member, "field name", "unit", scale)`. Use `XM_FIELD_ARRAY` for an array member. The type, position and array length are decided from the member, so you do not write them.
- ④ Call `XM_LOG_REGISTER(token, module ID, rate)` only inside `Control_Setup()`. The module ID (`0xF0` ~ `0xFE`) is a number you choose for each struct, and the rate is in Hz. Check the result (`true` / `false`). A message you send to the PC from `Control_Setup()` usually does not go out because USB is not ready yet, so report a failure with an LED.
- ⑤ Call `XM_LOG_SEND(token, pointer)` inside `Control_Loop()`. You can call it every loop and it goes out only at the registered rate, so a `false` before the rate is due is normal. The struct is copied inside the call, so a local variable is fine.
- If you have more structs, write one more set of `XM_LOG_DECLARE`, `XM_LOG_FIELDS`, `XM_LOG_REGISTER` and `XM_LOG_SEND` for each token. You can register up to 4, and their module IDs must differ.
- The example file uses `#define` constants instead of raw numbers (`SENSOR_LOG_MODULE_ID`, `SENSOR_LOG_RATE_HZ`, etc.).

### Check it on the PC

1. Build → upload → connect the USB-C cable to the PC.
2. Open the board's COM port with the [`xm10` tool](../getting-started/04-pc-data-tool.en.md) (`python xm10.py recv` → choose the Port → Connect).
3. Check that a tab for module ID `0xF0` appears with the columns `tick`, `ramp`, `adc[0]`, `adc[1]` and `button[0]` ~ `button[2]`. (not yet confirmed on the board) <!-- board-check -->
4. Press the buttons and connect something like a potentiometer to the ADC pins of the extension port, and watch the values follow. (not yet confirmed on the board) <!-- board-check -->

-----

## 3. Field Types

You do not write the field type. `XM_FIELD` and `XM_FIELD_ARRAY` decide it automatically from the **C type of the member**.

| Member C type | Field type | Size | Kind |
|------|------|------|------|
| `float` | `XM_FIELD_F32` | 4 B | Real number |
| `double` | `XM_FIELD_F64` | 8 B | Real number |
| `int8_t` | `XM_FIELD_I8` | 1 B | Integer |
| `uint8_t` | `XM_FIELD_U8` | 1 B | Integer |
| `int16_t` | `XM_FIELD_I16` | 2 B | Integer |
| `uint16_t` | `XM_FIELD_U16` | 2 B | Integer |
| `int32_t` · `int` | `XM_FIELD_I32` | 4 B | Integer |
| `uint32_t` · `unsigned` | `XM_FIELD_U32` | 4 B | Integer |
| `int64_t` | `XM_FIELD_I64` | 8 B | Integer |
| `uint64_t` | `XM_FIELD_U64` | 8 B | Integer |
| `bool` | `XM_FIELD_BOOL` | 1 B | True / false |

- An enumeration (`enum`) member is accepted as the integer type of the same size.
- List an array with `XM_FIELD_ARRAY`. The element type is the same as in the table above.
- Gaps the compiler inserts between members are fine. `XM_FIELD` works out and records the position of each field.
- `char`, pointer and struct members cannot be used. If you list one with `XM_FIELD`, you get a compile error (`unsupported member type`). Use `uint8_t` or `int8_t` instead of `char`.

-----

## 4. Limits

| Item | Limit | If you exceed it |
|------|------|--------|
| Struct size | A multiple of 4, at most 1,008 B (`XM_LOG_MAX_STRUCT_SIZE`) | Compile error at the `XM_LOG_DECLARE` line |
| Struct name (token) | At most 11 characters (`XM_LOG_MAX_TOKEN_LEN`) | Compile error at the `XM_LOG_DECLARE` line |
| Number of fields | 1 ~ 90 per struct (`XM_LOG_MAX_FIELDS`) — an array counts as one field per line | Compile error at the `XM_LOG_REGISTER` line |
| Array elements | 1 ~ 255 per field (`XM_LOG_MAX_ARRAY_LEN`) | Compile error at the `XM_FIELD_ARRAY` line |
| Field name | Up to 15 letters, digits and symbols (ASCII) — Korean letters and characters such as `°` are not allowed | `XM_LOG_REGISTER` returns `false` |
| Unit | Up to 7 letters, digits and symbols (ASCII); `""` if none — characters such as `°` are not allowed | `XM_LOG_REGISTER` returns `false` |
| Scale | A number other than 0 (0, infinity and NaN are not allowed) | `XM_LOG_REGISTER` returns `false` |
| Structs you can register | Up to 4 | The 5th registration returns `false` |
| Module ID | `0xF0` ~ `0xFE`, one per struct (duplicates are not allowed) | `XM_LOG_REGISTER` returns `false` |
| Send rate | 1 ~ 1,000 Hz — data never goes out faster than 1,000 Hz | If 0, `XM_LOG_REGISTER` returns `false` |
| Send volume | Grows with bigger structs, higher rates and more structs | If too much, `XM_LOG_REGISTER` returns `false` |

- The send rate is **rounded up** to a 1 ms step. If it is not a divisor of 1,000, data goes out less often than the value you declared (e.g., 300 Hz → 250 Hz, 600 Hz → 500 Hz).
- If the volume to send exceeds what USB can carry, the registration is rejected. In that case lower the rate or shrink the struct. Structs you registered earlier are counted together.
- How large a struct and how high a rate still register has not yet been confirmed on the board. (not yet confirmed on the board) <!-- board-check -->

-----

## 5. Where to Call Each One

| Function / macro | Where to call it | If you do not |
|------|------|------|
| `XM_LOG_DECLARE` · `XM_LOG_FIELDS` (with `XM_FIELD` · `XM_FIELD_ARRAY` inside) | Near the top of the `.c` file, outside any function | Compile error |
| `XM_USB_SetHostProfile()` (optional) | `Control_Setup()` — before the first `XM_LOG_REGISTER` | The call is ignored after registration |
| `XM_SetUsbCustomMeta()` (optional) | `Control_Setup()` — before the first `XM_LOG_REGISTER` | After registration, `false` outside `Control_Setup()` |
| `XM_Log_SetMinStallMs()` (optional) | `Control_Setup()` — before the first `XM_LOG_REGISTER` | `false` after registration or outside `Control_Setup()` |
| `XM_Log_EnableLinkHealth()` (optional) | `Control_Setup()` | `false` outside `Control_Setup()` |
| `XM_LOG_REGISTER` | Only inside `Control_Setup()` | `false` |
| `XM_LOG_SEND` | Inside `Control_Loop()` | `false` |
| `XM_Log_GetStats()` | `Control_Loop()` | — |

- Use the four macros (`XM_LOG_DECLARE`, `XM_LOG_FIELDS`, `XM_LOG_REGISTER`, `XM_LOG_SEND`) in the same `.c` file. The macros are for C files only.
- Do not call them from an interrupt or from a background task you created yourself (`XM_Task_*`); they return `false`.

-----

## 6. Viewing It on the PC — the `xm10` Tool

> PhAI Studio is still under development, so for now use the `xm10` tool to view and save your own (custom) data structs.

Use the `xm10` tool that comes with the SDK (the `pc-data-tool` folder). With the tool from an earlier release, some values may not appear or may appear wrong.
For how to use it, and for saving and CSV export, see [Get Data on PC](../getting-started/04-pc-data-tool.en.md).

The `xm10` tool shows a struct like this.

- One tab opens per module ID, and the tab title is the module ID plus the struct name (token), such as `0xF0 · SensorLog`. (not yet confirmed on the board) <!-- board-check -->
- The column names of the graph and the CSV are the field names you wrote in `XM_FIELD`. An array gets one column per element, such as `adc[0]`, `adc[1]` …. (not yet confirmed on the board) <!-- board-check -->
- Values are shown as `stored value × scale`. In a CSV made by `xm10 export`, a `bool` is written as `1` / `0`, and an integer with scale 1 stays an integer. The CSV saved live writes every value with six decimals. (not yet confirmed on the board) <!-- board-check -->
- Field information is delivered when a PC program opens the port, and is sent again every few seconds after that. You do not need to re-plug the USB cable, and for the first few seconds after the port opens the field names may not appear yet. (design goal, not yet confirmed on the board) <!-- board-check -->

-----

## 7. What Changes in the Other USB Functions

Once you register even one struct, the changes below apply to the other USB functions. If you register none, these changes do not happen.

| Function | Once you register a struct |
|------|------|
| `XM_SendUsbDataWithId()` | You cannot send with a registered module ID (`false`). You can send with other IDs, but it returns `false` if you send too much in a short time or send something too large for one call. Something too large never goes out however long you wait, so send it with struct logging. |
| `XM_SendUsbDebugMessage()` | It is held to the same send-volume limit (counted together with `XM_SendUsbDataWithId()`). Shorten long messages, or split them and send them with a time gap. |
| `XM_USB_SetHostProfile()` | Call it before the first `XM_LOG_REGISTER`. After you register even one struct, the call is ignored. |
| `XM_SetUsbCustomMeta()` | It works only inside `Control_Setup()`. Calling it later returns `false`. |
| `XM_Log_SetMinStallMs()` | Call it before the first `XM_LOG_REGISTER`. After registration it returns `false`. |

- Do not register a struct when you only want to read text in a plain serial terminal. Even with the `XM_USB_HOST_TERMINAL` profile, the struct data and field information are still sent and may appear mixed into the terminal as garbled characters.
- When you register a struct, Total Data (`0x20`) is sent at most once per 1 ms. If the control loop runs late by more than 1 ms, the `xm_loop_count` value is not continuous in that stretch and skips ahead. The interval at which the board sends and the time at which data arrives on the PC may not be an even 1 ms. (design goal, not yet confirmed on the board) <!-- board-check -->

-----

## 8. When Data Backs Up

- When the transmit buffer is full, the data you tried to send is dropped and `XM_LOG_SEND()` returns `false`. The board does not wait.
- Even if `XM_LOG_SEND()` returns `false` because the buffer was full, the next send goes out after the registered period has passed. Calling it again right away does not send.
- Dropped data shows up as `SEQ↓` (lost frames) in the statistics line of the `xm10` tool. (not yet confirmed on the board) <!-- board-check -->
- The design goal is that the board keeps the data without dropping it even if the PC program pauses briefly (50 ms by default). A struct that sends too much to meet this goal is rejected at registration with `false`. You can change the allowed pause with `XM_Log_SetMinStallMs()`. 50 ms is an initial value that has not yet been measured on the board, so it is a goal, not a guarantee. (design goal) <!-- board-check -->
- When you turn it on with `XM_Log_EnableLinkHealth(true)`, the board sends link status (number of failed sends, buffer usage, etc.) to the PC every second. It is off by default, and it is sent only when at least one struct is registered. The `xm10` tool does not decode and show the link status values separately. (design goal, not yet confirmed on the board) <!-- board-check -->
- To find out why something is not being sent, copy the counters with `XM_Log_GetStats()` and look at them in the debugger. `g_log_stats` in [Ex.44](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/44_CDC_Struct_Logging/) is an example.

---

## Related Examples

| Example | Difficulty | Struct logging usage |
|------|--------|---------|
| [09_CDC_Stream](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/09_CDC_Stream/) | Intermediate | Existing way — `float` array + channel-name JSON |
| [44_CDC_Struct_Logging](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/44_CDC_Struct_Logging/) 🟢 **Rev 2.0 only** | Intermediate | Send two structs at different rates with names, units and types (button and ADC values, optionally H10 values) |

> ℹ️ Ex.44 has not yet been confirmed on a board. (not yet confirmed on the board) <!-- board-check -->

To save H10 gait data to a USB memory stick, see [Ex.34 USB Memory Gait Log](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/34_USB_Memory_Gait_Log/).

---

## ⚠️ Common Mistakes

| Symptom | Cause | Fix |
|------|------|------|
| Build error `implicit declaration of function` (at an `XM_LOG_REGISTER` or `XM_LOG_SEND` line) | `xm_api_logging_raw.h` is not included — `xm_api.h` does not include this header | Add `#include "xm_api_logging_raw.h"` at the top of the `.c` file |
| Compile error `add padding` at the `XM_LOG_DECLARE` line | The struct size is not a multiple of 4 | Add `uint8_t pad[]` at the end to fix the size (do not list it with `XM_FIELD`) |
| Compile error `unsupported member type` | A `char`, pointer or struct member was listed with `XM_FIELD` | Change it to a supported type (see section 3). Use `XM_FIELD_ARRAY` for arrays |
| `XM_LOG_REGISTER` returns `false` (LED 3 blinks fast) | A name or unit is too long or not ASCII (Korean, `°`, etc.), rate is 0, same module ID or same token, 5 or more structs, scale is 0, duplicate field names, too much to send, or called outside `Control_Setup()` | Check them one by one. If the send volume is the problem, lower the rate or shrink the struct |
| `XM_LOG_SEND` often returns `false` | The registered period is not due yet (normal), USB is not ready, or the transmit buffer is briefly full | Do not try to resend the same value; send a new value on the next loop |
| `XM_SendUsbDataWithId()` returns `false` with the same module ID | A registered module ID belongs only to that struct | Use another ID (the rest of `0xF0` ~ `0xFE`) |
| `XM_USB_SetHostProfile()` or `XM_Log_SetMinStallMs()` has no effect | They no longer change after the first registration | Call them before the first `XM_LOG_REGISTER` |
| Field names or values look wrong | An `XM_FIELD` of another token was listed inside `XM_LOG_FIELDS` — the compiler cannot check this rule | Use only `XM_FIELD` and `XM_FIELD_ARRAY` of the same token |
| Garbled characters in a serial terminal | Struct data is sent even with `XM_USB_HOST_TERMINAL` | Do not register a struct when you only want to read text |
| No tab appears, or the values look wrong | Another PC program has the same port open, or the `xm10` tool is from an earlier release | Close every other program and reopen with the `xm10` tool that comes with the SDK |
| The CSV columns are named `ch0`, `ch1` … and from some row on the value cells are empty and the last cell holds hexadecimal | The field information arrived after the first data row — the live CSV fixes its columns on the first row, so once the field information arrives, later rows are written as the received bytes in hexadecimal instead of values | Export again from the `.xmlog`: `python xm10.py export <file>.xmlog --csv out/` |

- How often the field information arrives later than the values right after the port opens has not yet been checked on a real board. (not yet confirmed on the board) <!-- board-check -->

---

## Related Documents

- [`xm_api_logging.h` reference](ref/xm-api-logging.en.md) — all macros, functions, types and limit constants
- [05. USB Serial](05-usb-connectivity.en.md) — the existing way (`XM_SendUsbDataWithId`) and text output
- [Get Data on PC — the `xm10` tool](../getting-started/04-pc-data-tool.en.md)
- [06. USB Memory Logging](06-usb-data-logging.en.md) — save to a USB memory stick instead of the PC
