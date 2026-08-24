# XM10 Total Data Packet — phai-studio Integration Guide
# XM10 Total Data Packet — phai-studio 연동 가이드

> **YAML Version**: v2.4 (2026-04-01)
> **Packet Size**: 365 bytes (fixed)
> **Module ID**: 0x20
> **Transmission Rate**: 1 kHz (every 1 ms)
> **Protocol**: PhAI V2.2 (COBS + CRC16-CCITT)

---

## 1. Wire Format / 와이어 포맷

```
USB CDC Serial → [COBS_ENCODED(Internal Packet)] [0x00 delimiter]

Internal Packet:
[SOF:0xAA] [LEN:1] [SEQ_ID:2LE] [MODULE_ID:1] [STATUS:1] [PAYLOAD:365B] [CRC16:2LE]

STATUS byte:
  - Bits 0-6 (0x7F): Tx drop count (0~127 saturating)
  - Bit  7   (0x80): Reserved
```

**Studio decoding steps / Studio 디코딩 순서:**
1. Accumulate bytes until `0x00` delimiter
2. COBS-decode the frame
3. Verify SOF == `0xAA`
4. Verify CRC16-CCITT (SOF through payload, excluding CRC itself)
5. Check MODULE_ID == `0x20` for Total Data
6. Parse 365-byte payload using `xm_total_data_map.ts`

---

## 2. Scaling Formulas / 스케일링 공식

Three formulas are used, specified per-channel in `scaleFormula`:

| scaleFormula | Formula | Used by | 사용처 |
|---|---|---|---|
| `none` | `physical = raw` | Header, State, GRF, Ext_IO, FDCAN_Diag | 헤더, 상태, GRF, 외부IO, FDCAN 진단 |
| `multiply_divide` | `physical = raw × scale / 32768` | H10 Joint/Segment/Gait/IMU | H10 관절/세그먼트/보행/IMU |
| `divide` | `physical = raw / scale` | IMU Hub, EMG Hub | IMU Hub, EMG Hub |

**Example / 예시:**
```typescript
// leftHipAngle: int16, scale=720, scaleFormula='multiply_divide'
const raw = dataView.getInt16(7, true);  // little-endian
const physical = raw * 720 / 32768;      // → degrees (-360 ~ +360)

// imu_hub_sensor[0].q[0]: int16, scale=10000, scaleFormula='divide'
const raw_q = dataView.getInt16(162, true);
const physical_q = raw_q / 10000;         // → normalized quaternion
```

---

## 3. Packet Groups / 패킷 그룹 구조

### 3.1 Header (7B, offset 0~6)

| Field | Offset | Type | Description (EN) | 설명 (KR) |
|---|---|---|---|---|
| `timestamp_ms` | 0 | uint32 | System uptime in ms | 시스템 가동 시간 (ms) |
| `device_online_mask` | 4 | uint16 | Connected device bitflags | 연결된 디바이스 비트플래그 |
| `phai_x1_status` | 6 | uint8 | Active capability flags | 활성 기능 플래그 |

**`device_online_mask` bits:**
| Bit | Device | 디바이스 |
|---|---|---|
| 0 | H10 (Control Module) | H10 (제어 모듈) |
| 1 | GRF Left (foot sensor) | GRF 왼쪽 (족저압 센서) |
| 2 | GRF Right (foot sensor) | GRF 오른쪽 (족저압 센서) |
| 3 | External IMU (Xsens MTi) | 외부 IMU (Xsens MTi) |
| 4 | IMU Hub (6-sensor module) | IMU Hub (6축 센서 모듈) |
| 5 | EMG Hub (sEMG module) | EMG Hub (표면근전도 모듈) |

**`phai_x1_status` bits:**
| Bit | Meaning | 의미 |
|---|---|---|
| 0 | H10 connected | H10 연결됨 |
| 1 | Torque control active | 토크 제어 활성 |
| 2-7 | Reserved | 예약 |

### 3.2 H10_Joint (16B, offset 7~22)
Hip/knee angles, torque current, motor encoder from the H10 control module.
H10 제어 모듈의 고관절/무릎 각도, 토크 전류, 모터 인코더.

### 3.3 H10_Segment (6B, offset 23~28)
Body segment orientation estimates (thigh angles, pelvic tilt).
신체 세그먼트 자세 추정 (대퇴각, 골반 기울기).

### 3.4 H10_Gait (4B, offset 29~32)
Foot contact detection and forward walking velocity.
발 접촉 감지 및 전진 보행 속도.

### 3.5 H10_IMU (36B, offset 33~68)
Hip IMU orientation (placeholder, currently 0) + global acceleration/gyroscope.
고관절 IMU 자세 (placeholder, 현재 0) + 글로벌 가속도/자이로스코프.

### 3.6 H10_Count (8B, offset 69~76)

| Field | Offset | Type | Description (EN) | 설명 (KR) |
|---|---|---|---|---|
| `h10AssistModeLoopCnt` | 69 | uint32 | H10 control loop counter | H10 제어 루프 카운터 |
| `postProcessingCnt` | 73 | uint32 | Gait analysis sample counter | 보행 분석 샘플 카운터 |

**`h10AssistModeLoopCnt` is critical for data quality analysis:**
- **DUP detection**: consecutive diff == 0 → duplicate (same H10 data read twice)
- **GAP detection**: consecutive diff >= 2 → missed samples
- **Normal**: consecutive diff == 1

**`h10AssistModeLoopCnt`는 데이터 품질 분석의 핵심:**
- **DUP 감지**: 연속 diff == 0 → 중복 (동일 H10 데이터를 두 번 읽음)
- **GAP 감지**: 연속 diff >= 2 → 누락된 샘플
- **정상**: 연속 diff == 1

### 3.7 H10_State (6B, offset 77~82)
FSM state, operation mode, assist level, calibration flags.
FSM 상태, 동작 모드, 보조 레벨, 캘리브레이션 플래그.

### 3.8 GRF_Left / GRF_Right (17B each, offset 83~116)
Foot pressure sensor: 14-channel raw FSR data, battery level, status, rolling index.
족저압 센서: 14채널 FSR raw 데이터, 배터리 레벨, 상태, 롤링 인덱스.

### 3.9 Ext_IMU (40B, offset 117~156)
Xsens MTi-630 external IMU. All values are `float32` (no scaling needed).
Xsens MTi-630 외부 IMU. 모든 값이 `float32` (스케일링 불필요).

### 3.10 IMU_Hub (125B, offset 157~281)
6 IMU sensors, each 20 bytes (quaternion + accel + gyro as int16).
6개 IMU 센서, 각 20바이트 (쿼터니언 + 가속도 + 자이로, int16).

Scale: `q / 10000`, `a / 100` (g), `g / 10` (deg/s)

### 3.11 Ext_IO (27B, offset 282~308)
8 digital IO pins (bit-packed) + 12 ADC channels (16-bit raw).
8개 디지털 IO 핀 (비트팩) + 12개 ADC 채널 (16비트 raw).

### 3.12 EMG_Hub (16B, offset 309~324)
Surface EMG: raw ADC, voltage, RMS, envelope, MVC%, muscle activation.
표면 근전도: raw ADC, 전압, RMS, 포락선, MVC%, 근활성도.

### 3.13 FDCAN1_Diag (6B, offset 325~330) — Ch1: XM↔CM

| Field | Offset | Type | Description (EN) | 설명 (KR) |
|---|---|---|---|---|
| `fdcan1_tec` | 325 | uint8 | Transmit Error Counter (0~255) | 전송 에러 카운터 |
| `fdcan1_rec` | 326 | uint8 | Receive Error Counter (0~127) | 수신 에러 카운터 |
| `fdcan1_lec` | 327 | uint8 | Last Error Code (0~7) | 마지막 에러 코드 |
| `fdcan1_bus_status` | 328 | uint8 | Bus status flags | 버스 상태 플래그 |
| `fdcan1_rx_fifo0_fill` | 329 | uint8 | Rx FIFO0 fill level | Rx FIFO0 채움 수준 |
| `fdcan1_tx_fifo_free` | 330 | uint8 | Tx FIFO free level | Tx FIFO 여유 수준 |

### 3.14 FDCAN2_Diag (6B, offset 331~336) — Ch2: XM↔Sensor Module

| Field | Offset | Type | Description (EN) | 설명 (KR) |
|---|---|---|---|---|
| `fdcan2_tec` | 331 | uint8 | Transmit Error Counter (0~255) | 전송 에러 카운터 |
| `fdcan2_rec` | 332 | uint8 | Receive Error Counter (0~127) | 수신 에러 카운터 |
| `fdcan2_lec` | 333 | uint8 | Last Error Code (0~7) | 마지막 에러 코드 |
| `fdcan2_bus_status` | 334 | uint8 | Bus status flags | 버스 상태 플래그 |
| `fdcan2_rx_fifo0_fill` | 335 | uint8 | Rx FIFO0 fill level | Rx FIFO0 채움 수준 |
| `fdcan2_tx_fifo_free` | 336 | uint8 | Tx FIFO free level | Tx FIFO 여유 수준 |

### 3.15 Reserved_HMMG (28B, offset 337~364)
Reserved for future HMMG module. All zeros. / 미래 HMMG 모듈용 예약. 전부 0.

---

## 4. FDCAN Diagnostics Interpretation / FDCAN 진단 해석

### LEC (Last Error Code)

| Value | Name | Meaning | 의미 |
|---|---|---|---|
| 0 | No Error | No error since last read | 마지막 읽기 이후 에러 없음 |
| 1 | Stuff Error | Bit stuffing violation | 비트 스터핑 위반 |
| 2 | Form Error | Fixed-form bit field error | 고정 형식 비트 필드 에러 |
| 3 | Ack Error | No acknowledgement received | ACK 미수신 |
| 4 | Bit1 Error | Recessive bit sent, dominant received | 열성 비트 전송 중 우성 수신 |
| 5 | Bit0 Error | Dominant bit sent, recessive received | 우성 비트 전송 중 열성 수신 |
| 6 | CRC Error | CRC mismatch | CRC 불일치 |
| 7 | No Change | No new error since last read | 마지막 읽기 이후 새 에러 없음 |

### bus_status Flags

| Bit | Name | Condition | 조건 |
|---|---|---|---|
| 0 | BusOff | TEC >= 256, node disconnected from bus | TEC >= 256, 버스 분리 |
| 1 | Warning | TEC/REC >= 96 | TEC/REC >= 96 |
| 2 | ErrorPassive | TEC/REC >= 128, cannot send active error flags | TEC/REC >= 128, 능동 에러 플래그 전송 불가 |

### CAN Bus State Machine / CAN 버스 상태 머신

```
  Error Active (정상)     ──(TEC/REC >= 96)──→   Warning (경고)
  bus_status = 0x00                              bus_status = 0x02

                          ──(TEC/REC >= 128)──→  Error Passive (수동)
                                                 bus_status = 0x04

                          ──(TEC >= 256)──→      Bus Off (단절)
                                                 bus_status = 0x01
```

### FIFO Monitoring / FIFO 모니터링

| Field | Max | Normal | Warning | Critical |
|---|---|---|---|---|
| `rx_fifo0_fill` | 37 | 0 | 1~5 | 6+ (overflow risk) |
| `tx_fifo_free` | 32 | 32 | 1~10 | 0 (FIFO full, Tx dropped) |

> **IOC Config**: RxFifo0ElmtsNbr=37, TxFifoQueueElmtsNbr=32 (both Ch1 and Ch2)

| 필드 | 최대 | 정상 | 경고 | 위험 |
|---|---|---|---|---|
| `rx_fifo0_fill` | 37 | 0 | 1~5 | 6+ (오버플로 위험) |
| `tx_fifo_free` | 32 | 32 | 1~10 | 0 (FIFO 가득, Tx 유실) |

- **rx_fifo0_fill > 0**: ISR/RxTask is not draining fast enough
- **tx_fifo_free == 0**: Tx messages are being dropped

- **rx_fifo0_fill > 0**: ISR/RxTask가 제때 처리하지 못하는 상태
- **tx_fifo_free == 0**: Tx 메시지가 유실되는 상태

---

## 5. Recommended Studio Display / Studio 화면 표시 권장 사항

### 5.1 Real-time Chart / 실시간 차트
- **Primary**: H10_Joint (hip/knee angles, torques) — time-series line chart
- **Secondary**: H10_IMU (acceleration, gyroscope) — time-series line chart
- **Gait**: foot contact as event markers on the time axis
- **주요**: H10_Joint (고관절/무릎 각도, 토크) — 시계열 라인 차트
- **보조**: H10_IMU (가속도, 자이로) — 시계열 라인 차트
- **보행**: foot contact를 시간축 이벤트 마커로 표시

### 5.2 Data Quality Panel / 데이터 품질 패널
Using `h10AssistModeLoopCnt` diff analysis:

| Metric | Calculation | Display | 표시 |
|---|---|---|---|
| Sync Rate | `count(diff==1) / total × 100` | Percentage, green > 95% | 퍼센트, 95% 초과 시 녹색 |
| DUP Count | `count(diff==0)` | Counter, yellow if > 0 | 카운터, 0 초과 시 노란색 |
| GAP Count | `count(diff>=2)` | Counter, red if > 1% | 카운터, 1% 초과 시 빨간색 |

### 5.3 CAN Bus Health Indicator / CAN 버스 상태 표시기

| State | Color | Condition | 조건 |
|---|---|---|---|
| Healthy | Green | TEC=0, REC=0, LEC=7, bus_status=0x00 | 정상 |
| Warning | Yellow | TEC/REC > 0 or LEC != 7 and LEC != 0 | TEC/REC > 0 또는 LEC 에러 |
| Error | Red | bus_status != 0x00 (Warning/Passive/BusOff) | 버스 이상 |

Display for both Ch1 (XM↔CM) and Ch2 (XM↔Sensor) independently.
Ch1 (XM↔CM)과 Ch2 (XM↔Sensor) 각각 독립적으로 표시.

### 5.4 Device Connection Status / 디바이스 연결 상태
Use `device_online_mask` to show connected/disconnected icons for each device.
`device_online_mask`를 사용하여 각 디바이스의 연결/해제 아이콘 표시.

---

## 6. Additional Module IDs / 추가 Module ID

| Module ID | Description | 설명 |
|---|---|---|
| `0x20` | Total Data (this document) | Total Data (이 문서) |
| `0xEF` | User Meta (JSON, sent once on connect) | 사용자 메타 (JSON, 연결 시 1회 전송) |
| `0xF0`~`0xFE` | User Custom (float arrays, optional) | 사용자 커스텀 (float 배열, 선택) |

---

## 7. TypeScript Data Map / TypeScript 데이터 맵

The auto-generated `xm_total_data_map.ts` contains the complete channel definitions.
Import and use it directly:

자동 생성된 `xm_total_data_map.ts`에 전체 채널 정의가 포함되어 있습니다.
직접 import하여 사용:

```typescript
import { TOTAL_DATA_MAP, TOTAL_PACKET_SIZE, ChannelDef, TYPE_SIZE } from './xm_total_data_map';

function decodePacket(payload: DataView): Record<string, number> {
  const result: Record<string, number> = {};
  
  for (const ch of TOTAL_DATA_MAP) {
    if (ch.unit === 'reserved') continue;
    
    const count = ch.count ?? 1;
    const elemSize = TYPE_SIZE[ch.type];
    
    for (let i = 0; i < count; i++) {
      const off = ch.offset + i * elemSize;
      let raw: number;
      
      switch (ch.type) {
        case 'uint8':   raw = payload.getUint8(off); break;
        case 'int8':    raw = payload.getInt8(off); break;
        case 'uint16':  raw = payload.getUint16(off, true); break;
        case 'int16':   raw = payload.getInt16(off, true); break;
        case 'uint32':  raw = payload.getUint32(off, true); break;
        case 'int32':   raw = payload.getInt32(off, true); break;
        case 'float32': raw = payload.getFloat32(off, true); break;
        default: continue;
      }
      
      let physical: number;
      switch (ch.scaleFormula) {
        case 'multiply_divide': physical = raw * ch.scale / 32768; break;
        case 'divide':          physical = raw / ch.scale; break;
        default:                physical = raw; break;
      }
      
      const key = count > 1 ? `${ch.name}[${i}]` : ch.name;
      result[key] = physical;
    }
  }
  return result;
}
```

---

## 8. Version History / 버전 히스토리

| Version | Date | Changes | 변경 사항 |
|---|---|---|---|
| v2.4 | 2026-04-01 | FDCAN_Diag split to Ch1/Ch2 (12B total) | FDCAN 진단 Ch1/Ch2 분리 (총 12B) |
| v2.3 | 2026-03-31 | Add rx_fifo0_fill, tx_fifo_free | Rx FIFO0/Tx FIFO 레벨 추가 |
| v2.2 | 2026-03-26 | Add FDCAN_Diag (4B) | FDCAN 진단 추가 (4B) |
| v2.1 | 2026-03-23 | Regroup by semantic (H10_Joint, etc.) | 의미 기반 그룹 재구조화 |
| v2.0 | 2026-03-23 | IMU orient x6, EMG real fields, adc_active_mask | IMU orient 복원, EMG 실제 필드 |
