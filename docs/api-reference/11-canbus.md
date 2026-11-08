# API Reference: CAN 통신 — 내 장치와 주고받기

> 📌 **이 페이지를 읽고 나면**: 직접 만든 CAN-FD 장치나 클래식 CAN 장치를 XM10 의 CAN-FD #2 포트에 연결해 데이터를 받고 명령을 보낼 수 있습니다.
> ⏱️ 예상 학습 시간: 30분
> 🧰 사전 지식: [Ex.41](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/41_IMU_Hub_Dashboard/) 의 PC 그래프 보내기
> 🎯 핵심 함수: `XM_CanAttachDevice` / `XM_CanReadLatest` / `XM_CanSend` / `XM_CanSendFrame` / `XM_CanReadFrame`
>
> 🟢 **Rev 2.0 전용** *(v2.10.0 신규)*: Rev 1.1 SDK 에는 `xm_api_canbus.h` 가 없습니다.

`xm_api_canbus.h` 에 정의된 **CAN 통신 API** 안내입니다. 함수 하나하나의 시그니처와 반환값은 [`xm_api_canbus.h` 레퍼런스](ref/xm-api-canbus.md) 에 있습니다. 이 페이지는 **어떻게 쓰는지**를 설명합니다.

> ℹ️ **보드 확인 안내**: 아래 동작은 API 가 정한 것입니다. 내 장치와 XM10 을 연결해 주고받는 전체 흐름은 아직 보드에서 확인하지 못했습니다. (보드 확인 전) <!-- board-check -->

---

## 1. 두 가지 방법

| 방법 | 언제 쓰나 | 함수 |
|------|------|------|
| **장치 API** | 직접 만든 장치. 장치 번호를 정해 두면 XM10 이 그 장치의 주기 데이터와 살아있음 신호를 알아서 받아 둡니다 | `XM_CanAttachDevice` · `XM_CanReadLatest` · `XM_CanSend` · `XM_CanIsConnected` |
| **자유 송수신** | 이미 정해진 프로토콜을 쓰는 장치(모터 드라이버 등). ID 와 데이터만으로 주고받습니다 | `XM_CanSendFrame` · `XM_CanReadFrame` · `XM_CanSubscribe` · `XM_CanReadLatestById` |

둘을 함께 써도 됩니다. 프로토콜 · 장치 설정 · 기동 순서는 내 코드에서 정합니다. XM10 은 막을 이유가 있는 ID 만 막습니다.

---

## 2. 포트와 버스 모드

내 장치는 **CAN-FD #2 포트**에 연결합니다. 센서 허브(IMU · EMG · FES)를 연결하는 포트와 같습니다. 포트의 버스 모드는 전원을 켤 때 한 번 정합니다.

| 모드 | 속도 | 함께 쓸 수 있는 것 | 정하는 법 |
|------|------|------|------|
| **CAN-FD** (기본) | 중재 1 Mbit/s · 데이터 5 Mbit/s, 샘플 포인트 75 % | 센서 허브 + 내 CAN-FD 장치 | 아무것도 하지 않으면 FD 입니다 |
| **클래식 CAN** | 기본 1 Mbit/s (500 · 250 · 125 kbit/s 프리셋), 샘플 포인트 87.5 % | 클래식 CAN 장치만 — **센서 허브는 동작하지 않습니다** | `XM_CanBusMode()` 를 내 코드에 만들어 `XM_CAN_BUS_CLASSIC` 을 돌려줍니다 |

```c
XmCanBusMode_t XM_CanBusMode(void) { return XM_CAN_BUS_CLASSIC; }   // 전원을 켤 때 한 번 불립니다 — 값만 돌려주세요

void XM_CanClassicBitTiming(XmCanBitTiming_t* timing)               // (선택) 클래식 속도
{
    const XmCanBitTiming_t t = XM_CAN_TIMING_500K;
    *timing = t;
}
```

- 클래식 속도는 실행 중에도 `XM_CanSetClassicBitTiming()` 으로 바꿀 수 있습니다. 공장 속도가 다른 장치를 새 속도로 설정한 뒤 함께 옮길 때 씁니다.
- 지금 모드는 `XM_CanGetBusMode()` 로 봅니다. 클래식을 설정하지 못했으면 `XM_CAN_BUS_UNAVAILABLE` 이고, 그때는 이 포트로 아무것도 보내지 않습니다.
- **배선** — 장치 트랜시버의 CAN_H · CAN_L · GND 를 포트의 같은 이름에 연결합니다. 버스 양 끝에는 120 Ω 종단 저항이 있어야 합니다. XM10 보드에는 이미 있으므로 장치 쪽 끝에 하나를 둡니다. [하드웨어 — CAN-FD 포트](../hardware/README.md#can-fd-포트)

---

## 3. 장치 API

### 장치 번호

장치마다 번호 n 을 하나 정하고 `Control_Setup()` 에서 `XM_CanAttachDevice(n)` 로 등록합니다. 4 대까지 등록합니다.

| 번호 | 쓸 수 있나 |
|------|------|
| `0x01` · `0x02` · `0x10` · `0x11` | 예약 번호 — 쓸 수 없습니다 |
| `0x0C` · `0x0D` · `0x0F` | 센서 허브 번호 — 쓸 수 없습니다 |
| 1 ~ 127 의 나머지 | 쓸 수 있습니다 — `XM_CanIsDeviceIdAllowed()` 로 확인 |

같은 버스에서 번호가 겹치면 XM10 은 알아채지 못합니다. 장치마다 다른 번호를 쓰세요.

### 메시지

| 메시지 | 방향 | ID | 내용 |
|------|------|:---:|------|
| 주기 데이터 1 | 장치 → XM10 | `0x180 + n` | 내가 정한 데이터(FD 64 B · 클래식 8 B 까지) |
| 주기 데이터 2 | 장치 → XM10 | `0x280 + n` | 두 번째 데이터 |
| 살아있음 | 장치 → XM10 | `0x700 + n` | 1 바이트 `0x05`. 켜질 때 `0x00` 한 번(재시작 알림, 선택) |
| 명령 | XM10 → 장치 | `0x200 + n` | `XM_CanSend()` 로 보냅니다 |

### 받기 — 최신값 하나

```c
XmCanFrame_t f;
if (XM_CanReadLatest(n, 1, &f) && (f.seq != s_last_seq)) {   // 주기 데이터 1 의 최신값 — 새 값일 때만
    s_last_seq = f.seq;
    /* f.data[0 .. f.len-1] */
}
bool connected = XM_CanIsConnected(n);                        // 3 초 안에 0x05 를 받았나
```

- XM10 은 장치별로 **최신값 하나**만 보관합니다. 같은 값을 다시 읽어도 `true` 이므로 새 값인지는 `seq` 로 가립니다.
- 끊긴 뒤에도 옛 값이 `true` 로 읽힙니다. 연결 여부는 `XM_CanIsConnected()` 로 따로 봅니다.
- 재시작 알림(`0x00`)을 받으면 그동안 받아 둔 값을 버리고, 다음 `0x05` 까지 '끊김' 입니다.

### 보내기 — 명령

```c
const uint8_t cmd[2] = { s_seq, led };
if (XM_CanSend(n, cmd, sizeof(cmd)) == XM_CAN_OK) {   // XM_CAN_OK 는 0 — 꼭 == 로 비교
    s_seq++;                                          // 수락됐을 때만 순번을 올립니다
}
```

- `XM_CAN_OK` 는 **보내기 큐에 실림**입니다. 장치가 받았다는 뜻이 아닙니다.
- XM10 이 장치를 '연결' 로 볼 때만 수락합니다. 끊긴 동안 하드웨어에 남아 있던 명령은 다시 연결될 때 나갈 수 있습니다 — **명령 첫 바이트를 순번으로** 써서 장치가 중복 · 순서 뒤바뀜을 거르게 하세요. 장치 쪽 판단은 `XM_CanSeqIsNewer()` 와 같은 규칙(1 ~ 127 앞서면 새 값)으로 합니다.

---

## 4. 자유 송수신

### 보내기

```c
uint8_t d[8] = { /* … */ };
if (XM_CanIsIdAllowed(0x321)) {
    (void)XM_CanSendFrame(0x321, d, sizeof(d));
}
```

| 모드 | 보낼 수 있는 ID |
|------|------|
| CAN-FD | `0x000` ~ `0x7FF` 중 아래 7비트가 `0x0C` · `0x0D` · `0x0F` 인 ID(센서 허브 자리) · `0x080` · `0x702` 를 뺀 전부. `0x000` 은 길이 2 이고 둘째 바이트가 대상 번호일 때만 |
| 클래식 | `0x000` ~ `0x7FF` 전부(동기 신호를 켠 동안의 `0x080` 제외) |

길이는 FD 0 ~ 64(다음 허용 길이까지 0 으로 채움) · 클래식 0 ~ 8 입니다. `XM_CanFitLength()` 로 실릴 길이를 미리 볼 수 있습니다. 11 비트 ID 만 되고 리모트 프레임은 지원하지 않습니다.

### 받기 — 순서 큐와 최신값 우편함

| 방법 | 함수 | 알맞은 것 |
|------|------|------|
| 받은 순서 큐(깊이 64) | `XM_CanReadFrame()` — 매 주기 `while` 로 비웁니다 | 응답 · 가끔 오는 메시지. 가득 차면 새 메시지를 버리고 `rx_queue_drop_count` 를 셉니다 |
| 최신값 우편함(장치 API 와 합쳐 16 칸) | `Control_Setup()` 에서 `XM_CanSubscribe(id)` → `XM_CanReadLatestById()` | 1 kHz 주기 데이터. 받은 뒤 지난 시간은 `XM_CanAgeMs()` |

```c
XmCanFrame_t f;
while (XM_CanReadFrame(&f)) {
    /* f.id · f.len · f.data — f.seq 의 빈 번호 = 큐가 가득해 버린 수 */
}
```

---

## 5. 동기 신호와 XM10 하트비트

| 함수 | 하는 일 |
|------|------|
| `XM_CanEnableSync(true)` | 1 kHz 동기 신호(ID `0x080`)를 내 장치에게도 보냅니다. FD 는 1 B(카운터) · 클래식은 길이 0. FD 모드에서는 받아 줄 장치가 3 초 안에 보이지 않으면 보내지 않습니다. 잠깐 멈추거나 빠질 수 있으니 장치는 샘플링 정렬에만 쓰고, 없으면 자기 주기로 계속 보내게 하세요 |
| `XM_CanEnableHeartbeat(node, period_ms)` | XM10 의 하트비트(ID `0x700 + node`, `0x05`)를 보냅니다. `Control_Loop()` 가 끝날 때 보내므로 루프가 멈추면 하트비트도 멈춥니다 — 상대 장치가 하트비트 끊김으로 스스로 멈추게 할 때 씁니다(상대 설정 · 보드에서 확인). `0` 이면 끕니다 |

---

## 6. 한 포트에서 지킬 것

- 클래식 장치와 CAN-FD 장치를 **함께 연결하지 마세요.**
- 모든 장치는 **같은 속도**여야 합니다.
- 두 장치가 **같은 ID 를 보내면 안 됩니다** — 번호를 겹치지 마세요.
- 보낼 수 있는 양에는 한계가 있습니다 — 메시지 시간을 더해 버스 시간 안에 드는지 계산하세요.
- 데이터 속도는 최대 5 Mbit/s 입니다. CAN XL 은 지원하지 않습니다.
- 같은 ID 를 보낼 때 앞 메시지가 아직 대기 중이면 순서가 바뀔 수 있습니다 — 앞 메시지가 나간 뒤에 보내세요.

---

## 7. 결과 값과 호출 자리

| 결과 (`XmCanResult_t`) | 뜻 | 할 일 |
|------|------|------|
| `XM_CAN_OK` (0) | 보내기 큐에 실렸습니다 | — |
| `XM_CAN_BUSY` | 이번엔 못 실었습니다(큐 가득 · 다른 송신과 겹침) | 다음 주기에 다시 |
| `XM_CAN_RECOVERY` | 버스 복구 중입니다 | 잠시 뒤 다시 |
| `XM_CAN_NOT_CONNECTED` | (장치 API) 장치가 연결돼 있지 않습니다 | 연결을 확인 |
| `XM_CAN_REJECTED` | 보낼 수 없는 ID · 길이 · 인자, 또는 이 모드에서 지원하지 않음 | 규칙 확인 |
| `XM_CAN_NOT_OWNER` | `Control_Setup()` · `Control_Loop()` 밖에서 불렀습니다 | 호출 자리를 옮김 |

- 모든 함수는 `Control_Setup()` · `Control_Loop()` 안에서만 부르세요(등록 함수는 `Control_Setup()` 안에서만). 어느 함수도 기다리지 않습니다.
- `XM_CanGetBusMode` · `XM_CanIsIdAllowed` · `XM_CanIsDeviceIdAllowed` · `XM_CanFitLength` · `XM_CanSeqIsNewer` 는 어디서나 부를 수 있습니다.

---

## 8. 통계로 원인 찾기

`XM_CanGetStats(n, &s)` (장치별) · `XM_CanGetBusStats(&b)` (포트 공통). 각 값은 온전하지만 서로 같은 순간의 값은 아닙니다.

| 보이는 것 | 뜻 |
|------|------|
| `s.unread_overwrite_count` 가 늘어남 | 읽기 전에 새 값으로 바뀌었습니다 — 최신값 하나만 보관하므로 빠른 장치에서는 정상 |
| `s.last_send_fail` | 마지막 `XM_CanSend` 실패 결과(0 = 아직 없음) |
| `b.tx_pending_count` 가 계속 큼 | 받아 주는 장치가 없거나 버스가 바쁩니다 |
| `b.bus_status` | bit0 bus-off · bit1 오류 경고 · bit2 오류 수동 — 속도 · 배선 · 종단 저항 확인 |
| `b.rx_queue_drop_count` | 받은 순서 큐가 가득해 버린 수 — 매 주기 큐를 비우세요 |
| `b.bad_call_count` · `b.owner_violation_count` | 잘못된 인자 · 미등록 번호 · 호출 자리 문제 |

---

## 9. 상대 장치와 예제

| 무엇 | 링크 |
|------|------|
| 내 장치와 주고받기 | [Ex.45 CAN-FD My Sensor](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/45_CANFD_My_Sensor/) |
| 클래식 CAN · CANopen 드라이버 | [Ex.46](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/46_CANopen_Drive/) · [Ex.47](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/47_CANopen_Drive_PDO/) · [CANopen 장치 다루기](12-canopen.md) |
| 장치 펌웨어 참조 구현 (NUCLEO-G474RE + CAN-FD 트랜시버) | [canfd-sensor-stm32g474](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/device-examples/canfd-sensor-stm32g474/) |
| 장치 없이 PC 로 시험 | [canfd-sensor-pc-sim](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/device-examples/canfd-sensor-pc-sim/) · [canopen-pc-sim](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/device-examples/canopen-pc-sim/) |
| 함수 하나하나 | [`xm_api_canbus.h` 레퍼런스](ref/xm-api-canbus.md) |
