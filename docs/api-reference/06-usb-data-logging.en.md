# API Reference: USB Data Logging

> **Header file:** `xm_api_usb.h`
>
> A complete data logging system that stores sensor data to a USB flash drive (MSC) in binary format at high speed and converts it to CSV via the Python decoder for analysis.
>
> 📌 **After reading this page**: you will be able to use registration-based USB auto-logging, error monitoring, file rolling, and session markers.
> ⏱️ Estimated reading time: 30 minutes
> 🧰 Prerequisites: [Ex.10a~10c](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/10a_MSC_Basic_Log/) MSC series + FAT32 / 32 KB cluster USB flash drive
> 🎯 Key functions: `XM_SetUsbLogSource` / `XM_StartUsbDataLog` / `XM_StopUsbDataLog` / `XM_GetUsbLogStatus` / `XM_InsertUsbLogMarker`

---

## 📌 Overview

The XM10 USB Data Logging API safely stores large volumes of sensor data to a USB flash drive without compromising the performance of the 1 ms real-time control loop.

**Key features:**
- **Non-blocking background writes**: data is written to USB storage in the background without stalling the 1 ms control loop
- **Self-describing file format**: 32-byte file header + 4 KB block CRC + 12-byte footer
- **RTC timestamps**: file creation time is recorded automatically based on the real-time clock
- **Event markers**: mark specific points in time during logging (mode transitions, errors, etc.)
- **Disk monitoring**: real-time free-space monitoring with automatic low-space warning
- **Auto session numbering**: when `NULL` is passed as the session name, sessions are named `S_001`, `S_002`, ... automatically

---

## 🛠 Data Structures

### XmLogStatus_e — Logger state

```c
typedef enum {
    XM_LOG_STATUS_IDLE,               // Idle (initial state)
    XM_LOG_STATUS_LOGGING,            // Logging normally
    XM_LOG_STATUS_WARNING_QUEUE_FULL, // Buffer utilization is high
    XM_LOG_STATUS_WARNING_DISK_LOW,   // Less than 50 MB remaining on disk
    XM_LOG_STATUS_ERROR_STOPPED,      // Forcibly stopped due to an error
} XmLogStatus_e;
```

| Value | Meaning | Action |
|----|------|------|
| `IDLE` | Logging inactive | Normal standby |
| `LOGGING` | Logging normally | Data is being recorded |
| `WARNING_QUEUE_FULL` | Internal buffer ≥ 90 % | USB write is delayed (risk of data loss) |
| `WARNING_DISK_LOW` | Less than 50 MB free | Running low — stop the session soon |
| `ERROR_STOPPED` | Forcibly stopped by error | USB disconnected, write failure, etc. — restart required |

### XmLogStats_t — Real-time session statistics

```c
typedef struct {
    uint32_t total_bytes;          // Total bytes written
    uint32_t total_records;        // Total records written
    uint32_t dropped_records;      // Dropped records (buffer overflow)
    uint32_t write_errors;         // Write failure count
    uint32_t duration_ms;          // Session elapsed time (ms)
    uint8_t  hot_buffer_percent;   // Hot buffer peak utilization (0–100)
    uint8_t  cold_buffer_percent;  // (Legacy — not used in current Hot-only architecture; always 0)
    uint32_t disk_free_mb;         // USB free space (MB)
    uint32_t disk_total_mb;        // USB total capacity (MB)
} XmLogStats_t;
```

- `dropped_records > 0` → data loss has occurred. Reduce the struct size or adjust the logging period.
- `hot_buffer_percent` → peak value. If ≥ 80 %, the margin for GC stalls is insufficient.
- `disk_free_mb` → cached value updated every 10 seconds. Not precise in real time, but sufficient for monitoring.

### XmLogMarkerType_e — Event marker type

```c
typedef enum {
    XM_LOG_MARKER_USER   = 0x01,  // Manual mark (button / command)
    XM_LOG_MARKER_MODE   = 0x02,  // Mode transition
    XM_LOG_MARKER_ERROR  = 0x03,  // Error occurred
    XM_LOG_MARKER_SYNC   = 0x04,  // Time synchronization point
} XmLogMarkerType_e;
```

---

## 📚 Functions

### Setup

#### `XM_SetUsbLogSource()`

Register the data source to log. Call this once from `Control_Setup()`.

```c
void XM_SetUsbLogSource(void* data_ptr, uint32_t size);
```

- **Parameters**
  - `data_ptr` — pointer to the struct to store (`&myData`)
  - `size` — size of the struct (`sizeof(myData)`)

- **Example**
  ```c
  typedef struct {
      float hip_angle_L;
      float hip_angle_R;
  } MyLogData_t;

  MyLogData_t myData;

  void Control_Setup(void) {
      XM_SetUsbLogSource(&myData, sizeof(myData));
  }
  ```

#### `XM_SetUsbLogAutoTimestamp()`

Enable or disable the automatic timestamp (4-byte `tick_ms`) prepended to each record.

```c
void XM_SetUsbLogAutoTimestamp(bool enabled);
```

- **Parameters**
  - `enabled` — `true`: automatically insert tick before every record (default). `false`: disabled.
- **Note** Call this in `Control_Setup()` before `XM_StartUsbDataLog()`.

#### `XM_SetUsbLogRollingSize()`

Set the file rolling (split) size.

```c
void XM_SetUsbLogRollingSize(uint32_t size_mb);
```

- **Parameters**
  - `size_mb` — file split size in MB. Range: 1–100. Default: 10.

---

### Session Control

#### `XM_IsUsbLogReady()`

Check whether a USB storage device is connected and ready for logging.

```c
bool XM_IsUsbLogReady(void);
```

- **Return** `true` if USB is ready; otherwise `false`.
- **Note** Safe to call from the 2 ms real-time loop.

#### `XM_StartUsbDataLog()`

Start a logging session.

```c
bool XM_StartUsbDataLog(const char* sessionName, const char* metadata);
```

- **Parameters**
  - `sessionName` — session folder name (e.g., `"S_001_TestRun"`). Pass **NULL or an empty string** to auto-generate names `S_001`, `S_002`, ...
  - `metadata` — string describing the data layout (e.g., `"hip_L(float), hip_R(float)"`)
- **Return** `true` on success. Returns `false` if USB is not ready, the queue has expired, or the name contains invalid characters.
- **Warning** May block for up to 100 ms — **do not call from the 2 ms loop.**

- **Example**
  ```c
  // Manual session name
  XM_StartUsbDataLog("Gait_001", "hip_L(float), hip_R(float)");

  // Auto session numbering
  XM_StartUsbDataLog(NULL, "hip_L(float), hip_R(float)");
  // → creates /LOGS/S_001/, /LOGS/S_002/, ... in sequence
  ```

#### `XM_StopUsbDataLog()`

Stop the current logging session.

```c
void XM_StopUsbDataLog(void);
```

- **Warning** **Do not call from the 2 ms loop.** Call it once from a state-transition function (`on_exit`).

---

### Status & Monitoring

#### `XM_GetUsbLogStatus()`

Return the current logger state.

```c
XmLogStatus_e XM_GetUsbLogStatus(void);
```

- **Return** An `XmLogStatus_e` enum value.
- **Note** Safe to call from the 2 ms loop. Useful for LED feedback and similar indicators.

- **State transition diagram**
  ```
  IDLE ──(Start)──▶ LOGGING ──(Stop)──▶ IDLE
                      │
                      ├──(Buffer 90%+)──▶ WARNING_QUEUE_FULL ──(recovered)──▶ LOGGING
                      ├──(Disk <50MB)───▶ WARNING_DISK_LOW
                      └──(Error)────────▶ ERROR_STOPPED
  ```

#### `XM_GetUsbLogStats()`

Retrieve real-time statistics for the current logging session.

```c
bool XM_GetUsbLogStats(XmLogStats_t* out_stats);
```

- **Parameters**
  - `out_stats` — pointer to an `XmLogStats_t` struct that receives the statistics
- **Return** `true` on success; `false` if the parameter is NULL.

- **Example**
  ```c
  XmLogStats_t stats;
  if (XM_GetUsbLogStats(&stats)) {
      if (stats.dropped_records > 0) {
          // Data loss warning
      }
      if (stats.disk_free_mb < 100) {
          // Low disk space warning
      }
  }
  ```

#### `XM_GetUsbDiskFreeMB()` / `XM_GetUsbDiskTotalMB()`

Query USB disk capacity (cached; updated every 10 seconds).

```c
uint32_t XM_GetUsbDiskFreeMB(void);
uint32_t XM_GetUsbDiskTotalMB(void);
```

- **Return** Capacity in MB. Returns 0 if no USB drive is connected.

---

### Event Markers

#### `XM_InsertUsbLogMarker()`

Insert an event marker during an active logging session.

```c
bool XM_InsertUsbLogMarker(XmLogMarkerType_e type, uint16_t data);
```

- **Parameters**
  - `type` — marker type (`XmLogMarkerType_e`)
  - `data` — context data (error code, mode ID, etc.; pass 0 if not needed)
- **Return** `true` on success. Returns `false` if logging is inactive or the buffer is full.
- **Note** Safe to call from the 2 ms loop (non-blocking).

- **Usage scenarios**

  | Marker type | Purpose | Example `data` value |
  |----------|------|----------|
  | `USER` | Manual mark (button, debug) | 0 |
  | `MODE` | State transition (Active → Standby) | New mode ID |
  | `ERROR` | Error occurrence | Error code |
  | `SYNC` | Time synchronization with an external system | Sync ID |

- **Example**
  ```c
  // On entering Active mode
  XM_InsertUsbLogMarker(XM_LOG_MARKER_MODE, STATE_ACTIVE);

  // On detecting an error
  XM_InsertUsbLogMarker(XM_LOG_MARKER_ERROR, err_code);
  ```

---

## 🔄 Session Output File Structure

```
/LOGS/<SessionName>/
  ├── metadata.txt            ← data format + system information
  ├── summary.txt             ← session statistics + termination status
  ├── data_000_part_000.bin   ← binary data (file header + CRC blocks + footer)
  ├── data_000_part_001.bin   ← (additional part after file rolling)
  └── ...
```

### metadata.txt

Generated automatically when the session starts. The Python decoder parses this file to determine the binary format.

```
hip_L(float), hip_R(float), gait_phase(uint8_t), _pad(3bytes)

=== System Info ===
auto_timestamp=1
timestamp_bytes=4
user_payload_bytes=12
record_header_bytes=4
record_total_bytes=20
rolling_size_mb=10
buffer_size_kb=128
logger_period_ms=100
rtc_start=2026-03-04 14:30:25
file_format_version=1
```

### summary.txt

Generated automatically when the session ends (normally or due to an error). Records session statistics.

```
=== Session Summary ===
total_bytes_written=15728640
total_packets_logged=786432
dropped_packets=0
write_errors=0
sync_count=150
start_tick=1000
end_tick=1573864
status=OK
error_reason=NORMAL
rtc_start=2026-03-04 14:30:25
rtc_end=2026-03-04 14:56:38
```

- `status=OK` → normal termination. `status=ERROR` → error termination (USB disconnected, write failure, etc.).
- `error_reason` → `NORMAL`, `HOT_OVERFLOW`, `USB_DISCONNECT`, `COLD_OVERFLOW`, `WRITE_FAILURE`, `ROLLING_FAILURE`

### .bin Binary Format

```
+-------------------------------------+
| FileHeader (32 bytes)               |  magic + version + flags + sizes
+-------------------------------------+
| Block 0 (4KB data)                  |  [Record][Record]...[Record]
| CRC32 (4 bytes)                     |
+-------------------------------------+
| Block 1 (4KB data)                  |
| CRC32 (4 bytes)                     |
+-------------------------------------+
| ...                                 |
+-------------------------------------+
| Block M (last block, <=4KB)         |
| CRC32 (4 bytes)                     |
+-------------------------------------+
| FileFooter (12 bytes)               |  record_count + data_bytes + magic
+-------------------------------------+
```

- **FileHeader**: contains magic number `0xA14C4F47`, format version, and record size information. The decoder can parse the file without `metadata.txt`.
- **Block CRC**: STM32 hardware CRC32 appended after every 4 KB of data. If a block is corrupted, it is skipped and the remaining blocks are recovered.
- **FileFooter**: footer magic `0x474F4CA1`. A missing footer indicates abnormal termination (power cut, etc.).

---

## 🐍 PythonDecoder Usage

### Installation

Requires Python 3.6 or later. No external package dependencies (uses the standard library only).

### Running the decoder

```bash
# Basic usage
python data_decoder_xm10.py /LOGS/BasicTest

# Specify struct format manually (when the metadata does not include type information)
python data_decoder_xm10.py /LOGS/BasicTest --fmt "<2fB3x"

# Disable stale-record trimming at the end
python data_decoder_xm10.py /LOGS/BasicTest --no-trim
```

### Output

- `decoded_output.csv` — main data CSV
- `events.csv` — event markers (when markers are present)

### Decoder features

| Feature | Description |
|------|------|
| Automatic file header detection | Supports both legacy (no header) and new (with header) formats |
| Block CRC verification | Detects corrupted blocks and recovers intact blocks |
| Event marker separation | Automatically separates data records from marker records |
| Footer verification | Confirms whether the file ended normally |
| Trailing stale trim | Automatically removes frozen records appended at session end |

---

## ⚠️ Caveats

- **RTC not set**: file timestamps will be recorded as 2025-01-01. Set the time with `XM_RTC_SetDateTime()` before logging.
- **Abnormal termination**: the footer may be absent and the CRC of the last block may be incomplete. The decoder will parse as many records as possible up to the end of the file.
- **FATFS forbidden characters**: the characters `< > : " / \ | ? *` are not allowed in session names. Passing a name with these characters causes the function to return `false`.
- **Struct alignment**: maintaining 4-byte alignment makes `__attribute__((packed))` unnecessary. Document any padding in the metadata string using the `_pad(Nbytes)` notation.
- **Functions prohibited in the 2 ms loop**: `XM_StartUsbDataLog()` and `XM_StopUsbDataLog()` — these may block.
