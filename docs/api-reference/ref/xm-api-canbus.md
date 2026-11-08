# `xm_api_canbus.h` — CAN-FD · 클래식 CAN 으로 연결한 내 장치와 주고받기

> 🟢 **Rev 2.0 전용** — 본 헤더는 XM10 Rev 2.0 용입니다. Rev 1.1 프로젝트(`XM10_SDK/Rev1.1/`)에서는 아래 함수를 쓸 수 없습니다. 보드 리비전 확인은 [보드 리비전 비교](../../hardware/README.md#보드-리비전-비교) 참고.
>
> - **대상 헤더**: `XM_FW/XM_API/xm_api_canbus.h` (`xm_api.h` 를 include 하면 함께 쓸 수 있습니다)
> - **관련 개념 문서**: [11. CAN 통신](../11-canbus.md)
> - **관련 예제**: [45_CANFD_My_Sensor](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/45_CANFD_My_Sensor/) · [46_CANopen_Drive](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/46_CANopen_Drive/) · [47_CANopen_Drive_PDO](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/47_CANopen_Drive_PDO/)
> - **상대 장치 예제**: [device-examples](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/device-examples/) — CAN-FD 내 장치 참조 펌웨어 [`canfd-sensor-stm32g474/`](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/device-examples/canfd-sensor-stm32g474/) · PC 모의 장치 [`canfd-sensor-pc-sim/`](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/device-examples/canfd-sensor-pc-sim/) · [`canopen-pc-sim/`](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/device-examples/canopen-pc-sim/)
> - **보드 확인**: 아래 동작은 API 가 정한 것이며, 보드에서는 아직 확인하지 못했습니다. (보드 확인 전) <!-- board-check -->

이 헤더는 **CAN-FD 또는 클래식 CAN 으로 연결한 내 장치와 메시지를 주고받는** API 를 정의합니다. 두 가지 방법이 있고, 함께 써도 됩니다.

- **① 장치 API** — 장치 번호를 등록해 두면 장치가 보내는 주기 데이터 1 · 2 와 살아있음을 XM10 이 알아서 나눠 담습니다(최신값). 장치에 명령도 보낼 수 있습니다.
- **② 자유 송수신** — 프로토콜 없이 ID 와 데이터만으로 보내고 받습니다(받은 순서 큐 · ID 별 최신값 우편함).

클래식 CAN 전용 장치(모터 드라이버 등)는 `XM_CanBusMode()` 로 포트를 클래식 모드로 정해서 씁니다. 프로토콜 · 장치 설정 · 기동 순서는 내 코드에서 정합니다. XM10 은 막을 이유가 있는 ID 만 막습니다.

---

## 언제 사용하나

직접 만든 센서나 구동기를 XM10 의 CAN 포트에 연결해 값을 읽고 명령을 보내고 싶을 때 사용합니다. 쓰는 순서 · 배선 · 장치 쪽 메시지 약속 같은 전체 그림은 [11. CAN 통신](../11-canbus.md) 을 먼저 읽어 보는 것을 권장합니다 — 이 페이지는 헤더에 선언된 **모든 함수 · 타입 · 상수의 시그니처와 파라미터 상세**만 다룹니다.

CANopen(CiA 301) 장치의 NMT · 하트비트 감시 · SDO 는 [`xm_api_canopen.h`](xm-api-canopen.md) 를 함께 쓰세요.

> ⚠️ **보내기 결과는 꼭 `== XM_CAN_OK` 로 확인하세요.** `XM_CAN_OK` 가 0 이라 `if (XM_CanSend(...))` 는 실패를 성공으로 읽습니다.

---

## 한 포트에서 지킬 것

- 클래식 장치와 CAN-FD 장치를 함께 연결하지 마세요.
- 모든 장치는 같은 속도여야 합니다.
- 두 장치가 같은 ID 를 보내면 안 됩니다 — 번호를 겹치지 마세요.
- 보낼 수 있는 양에는 한계가 있습니다 — 메시지 시간을 더해 버스 시간 안에 드는지 계산하세요.
- 데이터 속도는 최대 5 Mbit/s 입니다. CAN XL 은 지원하지 않습니다.
- 11비트 ID 만 지원합니다. 리모트 프레임은 지원하지 않습니다.
- 같은 ID 를 보낼 때 앞 메시지가 아직 대기 중이면 순서가 바뀔 수 있습니다 — 앞 메시지가 나간 뒤에 보내세요.

---

## 호출 규칙

| 종류 | 함수 | 부르는 곳 |
|------|------|-----------|
| 사용자가 만드는 함수 (2개) | `XM_CanBusMode` · `XM_CanClassicBitTiming` | 시스템이 전원을 켤 때 한 번 부릅니다. 이때는 다른 XM 함수를 아직 쓸 수 없으니 값만 돌려주거나 채우세요 |
| 등록 (2개) | `XM_CanAttachDevice` · `XM_CanSubscribe` | `Control_Setup()` 에서만 |
| 송수신 · 설정 · 통계 (13개) | `XM_CanSetClassicBitTiming` · `XM_CanGetClassicBitTiming` · `XM_CanIsConnected` · `XM_CanReadLatest` · `XM_CanSend` · `XM_CanGetStats` · `XM_CanGetBusStats` · `XM_CanEnableSync` · `XM_CanEnableHeartbeat` · `XM_CanSendFrame` · `XM_CanReadFrame` · `XM_CanReadLatestById` · `XM_CanAgeMs` | `Control_Setup()` · `Control_Loop()` 에서만 |
| 어디서나 (5개) | `XM_CanGetBusMode` · `XM_CanIsIdAllowed` · `XM_CanIsDeviceIdAllowed` · `XM_CanFitLength` · `XM_CanSeqIsNewer` | 어디서나 |

- `Control_Setup()` · `Control_Loop()` 밖(다른 태스크나 인터럽트)에서 부르면 거부됩니다. `bool` 을 돌려주는 함수는 `false`, 결과 값(`XmCanResult_t`)을 돌려주는 함수는 `XM_CAN_NOT_OWNER` 를 돌려주고, [`owner_violation_count`](#xmcanbusstats_t) 로 셉니다.
- 어느 함수도 기다리지 않습니다.

---

## 함수 목록

**버스 모드**

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_CanBusMode`](#xm_canbusmode) | 포트의 버스 모드를 정하는 함수 — **직접 만듭니다**(선택) |
| [`XM_CanGetBusMode`](#xm_cangetbusmode) | 지금 버스 모드 |
| [`XM_CanClassicBitTiming`](#xm_canclassicbittiming) | 클래식 CAN 속도를 정하는 함수 — **직접 만듭니다**(선택) |
| [`XM_CanSetClassicBitTiming`](#xm_cansetclassicbittiming) | 실행 중에 클래식 CAN 속도 바꾸기 |
| [`XM_CanGetClassicBitTiming`](#xm_cangetclassicbittiming) | 지금 클래식 CAN 속도 읽기 |

**장치 API**

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_CanAttachDevice`](#xm_canattachdevice) | 장치 번호 등록 (`Control_Setup()` 에서만) |
| [`XM_CanIsConnected`](#xm_canisconnected) | 3 초 안에 살아있음을 받았는지 |
| [`XM_CanReadLatest`](#xm_canreadlatest) | 장치의 주기 데이터 최신값 읽기 |
| [`XM_CanSend`](#xm_cansend) | 장치에 명령 보내기 |
| [`XM_CanGetStats`](#xm_cangetstats) | 장치별 통계 |
| [`XM_CanGetBusStats`](#xm_cangetbusstats) | 포트 공통 통계 |
| [`XM_CanEnableSync`](#xm_canenablesync) | 1 kHz 동기 신호(SYNC) 보내기 |
| [`XM_CanEnableHeartbeat`](#xm_canenableheartbeat) | XM10 의 하트비트 보내기 |

**자유 송수신**

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_CanSendFrame`](#xm_cansendframe) | 메시지 하나 보내기 |
| [`XM_CanReadFrame`](#xm_canreadframe) | 받은 순서대로 하나 꺼내기 |
| [`XM_CanSubscribe`](#xm_cansubscribe) | ID 를 최신값 우편함으로 받기 (`Control_Setup()` 에서만) |
| [`XM_CanReadLatestById`](#xm_canreadlatestbyid) | 우편함 ID 의 최신값 읽기 |
| [`XM_CanAgeMs`](#xm_canagems) | 우편함 ID 를 마지막으로 받은 뒤 지난 시간 [ms] |

**도움 함수** (어디서나)

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_CanIsIdAllowed`](#xm_canisidallowed) | 이 모드에서 보낼 수 있는 ID 인지 |
| [`XM_CanIsDeviceIdAllowed`](#xm_canisdeviceidallowed) | 장치 API 에 등록할 수 있는 번호인지 |
| [`XM_CanFitLength`](#xm_canfitlength) | 버스에 실릴 길이로 올리기 |
| [`XM_CanSeqIsNewer`](#xm_canseqisnewer) | 8비트 순번이 새 값인지 |

**타입 · 상수**: [`XmCanResult_t`](#xmcanresult_t) · [`XmCanBusMode_t`](#xmcanbusmode_t) · [`XmCanBitTiming_t`](#xmcanbittiming_t) · [`XmCanFrame_t`](#xmcanframe_t) · [`XmCanDeviceStats_t`](#xmcandevicestats_t) · [`XmCanBusStats_t`](#xmcanbusstats_t) · [상수 9개](#상수)

---

## 장치 API 의 메시지 ID

`XM_CanAttachDevice()` 로 번호를 등록하면 XM10 이 그 번호의 아래 메시지를 다룹니다.

| 메시지 | ID | 방향 | 설명 |
|------|----|:---:|------|
| 주기 데이터 1 | `0x180` + 번호 | 장치 → XM10 | 받아서 최신값으로 보관합니다 |
| 주기 데이터 2 | `0x280` + 번호 | 장치 → XM10 | 받아서 최신값으로 보관합니다 |
| 살아있음 | `0x700` + 번호 | 장치 → XM10 | 데이터 1 바이트 `0x05` 면 살아있음, `0x00` 이면 재시작 알림입니다. 3 초(`XM_CAN_ALIVE_TIMEOUT_MS`) 안에 `0x05` 를 받으면 연결로 봅니다 |
| 명령 | `0x200` + 번호 | XM10 → 장치 | [`XM_CanSend`](#xm_cansend) 로 보냅니다 |

- 같은 버스에서 번호는 장치마다 달라야 합니다. XM10 은 중복을 알아채지 못합니다.
- 등록할 수 있는 번호는 1 ~ 127 중 예약 번호(`0x01` · `0x02` · `0x0C` · `0x0D` · `0x0F` · `0x10` · `0x11`)를 뺀 나머지입니다 ([`XM_CanIsDeviceIdAllowed`](#xm_canisdeviceidallowed)). 번호마다 누가 쓰는지는 [11. CAN 통신 — 번호 한눈에 보기](../11-canbus.md#3-번호-한눈에-보기) 표에 있습니다.
- 등록할 수 있는 장치는 최대 `XM_CAN_MAX_DEVICES` (4) 개입니다.

---

## 함수 상세

### 버스 모드

#### `XM_CanBusMode`

```c
XmCanBusMode_t XM_CanBusMode(void);
```

포트의 버스 모드를 정하는 함수입니다. **사용자가 직접 만듭니다**(선택). 만들지 않으면 CAN-FD 모드(`XM_CAN_BUS_FD`)입니다.

전원을 켤 때 한 번 불립니다. 부팅 중이라 다른 XM 함수는 아직 준비 전이니, 값만 돌려주세요. 클래식으로 정하면 이 포트는 클래식 CAN 으로만 보내고 받으며, 센서 허브는 이 포트에서 동작하지 않습니다.

**파라미터**: 없음

**반환값**: [`XmCanBusMode_t`](#xmcanbusmode_t) — `XM_CAN_BUS_FD` 또는 `XM_CAN_BUS_CLASSIC`

**⚠️ 호출 컨텍스트**: 시스템이 전원을 켤 때 한 번 부릅니다. 직접 부르지 않습니다.

**예제**

```c
XmCanBusMode_t XM_CanBusMode(void) { return XM_CAN_BUS_CLASSIC; }
```

**참고**: [`XM_CanClassicBitTiming`](#xm_canclassicbittiming), [`XM_CanGetBusMode`](#xm_cangetbusmode)

---

#### `XM_CanGetBusMode`

```c
XmCanBusMode_t XM_CanGetBusMode(void);
```

지금 버스 모드를 돌려줍니다. 클래식을 설정하지 못했거나 포트가 고장 났으면 `XM_CAN_BUS_UNAVAILABLE` 입니다.

**파라미터**: 없음

**반환값**: [`XmCanBusMode_t`](#xmcanbusmode_t)

**⚠️ 호출 컨텍스트**: 언제나 부를 수 있습니다.

---

#### `XM_CanClassicBitTiming`

```c
void XM_CanClassicBitTiming(XmCanBitTiming_t* timing);
```

클래식 CAN 속도를 정하는 함수입니다. **사용자가 직접 만듭니다**(선택). 만들지 않으면 1 Mbit/s 입니다.

`XM_CanBusMode()` 가 클래식이면 전원을 켤 때 한 번 불립니다. 부팅 중이라 다른 XM 함수는 아직 준비 전이니, 값만 채우세요. 틀린 속도로 시작하면 XM10 이 오류 메시지로 버스를 깨므로 시작 전에 정합니다. 값이 범위 밖이면 포트를 쓰지 못합니다(`XM_CAN_BUS_UNAVAILABLE`).

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `timing` | [`XmCanBitTiming_t*`](#xmcanbittiming_t) | 속도를 채워 넣을 구조체. 프리셋(`XM_CAN_TIMING_*`)을 쓰면 편합니다 |

**반환값**: 없음

**⚠️ 호출 컨텍스트**: 시스템이 전원을 켤 때 한 번 부릅니다. 직접 부르지 않습니다.

**예제**

```c
void XM_CanClassicBitTiming(XmCanBitTiming_t* timing)
{
    const XmCanBitTiming_t t = XM_CAN_TIMING_500K;
    *timing = t;
}
```

**참고**: [`XmCanBitTiming_t`](#xmcanbittiming_t), [`XM_CanSetClassicBitTiming`](#xm_cansetclassicbittiming)

---

#### `XM_CanSetClassicBitTiming`

```c
XmCanResult_t XM_CanSetClassicBitTiming(const XmCanBitTiming_t* t);
```

실행 중에 클래식 CAN 속도를 바꿉니다. 기다리지 않습니다. 대부분은 [`XM_CanClassicBitTiming`](#xm_canclassicbittiming) 이면 충분합니다.

공장 속도가 다른 장치를 새 속도로 설정한 뒤 함께 옮길 때 씁니다. 바꾸는 동안 받아 두고 아직 읽지 않은 메시지는 사라지고, 오류 카운터(`tec` · `rec`)는 그대로입니다. 같은 값이면 아무것도 멈추지 않고 `XM_CAN_OK` 입니다.

장치에 속도 변경을 보낸 뒤에는 다른 메시지를 보내지 말고 바로 이 함수를 부르세요. 장치가 먼저 옮겨 가면 답을 못 받은 메시지가 남아 몇 초 동안 `XM_CAN_BUSY` 일 수 있습니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `t` | [`const XmCanBitTiming_t*`](#xmcanbittiming_t) | 새 속도 (`XM_CAN_TIMING_*` 또는 직접 쓴 값) |

**반환값**: [`XmCanResult_t`](#xmcanresult_t)

- `XM_CAN_OK`: 바뀌었습니다. 다음 보내기부터 새 속도입니다.
- `XM_CAN_BUSY`: 보낼 메시지가 남아 있습니다(SYNC 를 켰으면 먼저 끄세요). 또는 `XM_CanOpenSdoRead` · `XM_CanOpenSdoWrite` 가 진행 중이거나 잠깐 겹쳤습니다. 다음 주기에 다시 부르세요.
- `XM_CAN_RECOVERY`: 버스 복구 중입니다.
- `XM_CAN_REJECTED`: `NULL` 이거나, 값이 범위 밖이거나, 클래식 모드가 아니거나, 포트가 고장 났습니다. 포트가 고장 났으면 재시작하세요(`XM_CanGetBusMode()` 가 `XM_CAN_BUS_UNAVAILABLE`).
- `XM_CAN_NOT_OWNER`: `Control_Setup()` · `Control_Loop()` 밖에서 불렀습니다.

**⚠️ 호출 컨텍스트**: `Control_Setup()` · `Control_Loop()` 에서 부릅니다.

**참고**: 버스의 모든 장치와 같은 속도여야 합니다. 어긋나면 오류가 쌓이고 버스 복구로 보입니다(`tec` · `bus_status`).

---

#### `XM_CanGetClassicBitTiming`

```c
bool XM_CanGetClassicBitTiming(XmCanBitTiming_t* out);
```

지금 클래식 CAN 속도를 읽습니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `out` | [`XmCanBitTiming_t*`](#xmcanbittiming_t) | 속도를 받을 구조체 |

**반환값**: `bool` — `true`: `out` 에 채웠습니다. `false`: 클래식 모드가 아니거나 포트가 고장 났습니다 — `out` 은 그대로입니다.

**⚠️ 호출 컨텍스트**: `Control_Setup()` · `Control_Loop()` 에서 부릅니다.

---

### 장치 API

#### `XM_CanAttachDevice`

```c
bool XM_CanAttachDevice(uint8_t device_id);
```

장치 번호를 등록합니다. 등록하면 그 장치의 주기 데이터 1(ID `0x180` + 번호) · 주기 데이터 2(`0x280` + 번호) · 살아있음(`0x700` + 번호)을 XM10 이 받아 둡니다. 같은 버스에서 번호는 장치마다 달라야 합니다(XM10 은 중복을 알아채지 못합니다).

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `device_id` | `uint8_t` | 장치 번호 — [`XM_CanIsDeviceIdAllowed`](#xm_canisdeviceidallowed) 가 `true` 인 번호 |

**반환값**: `bool` — `true`: 등록했습니다. `false`: 쓸 수 없는 번호이거나, 이미 등록했거나, 칸이 모자라거나(최대 `XM_CAN_MAX_DEVICES`), `Control_Setup()` 밖에서 불렀습니다. 이때 `bad_call_count` 가 1 늘어납니다.

**⚠️ 호출 컨텍스트**: `Control_Setup()` 에서만 부릅니다.

**예제**

```c
#define MY_SENSOR_ID  (0x20U)

void Control_Setup(void) {
    XM_CanAttachDevice(MY_SENSOR_ID);
}
```

**참고**: [장치 API 의 메시지 ID](#장치-api-의-메시지-id), [`XM_CanReadLatest`](#xm_canreadlatest)

---

#### `XM_CanIsConnected`

```c
bool XM_CanIsConnected(uint8_t device_id);
```

3 초(`XM_CAN_ALIVE_TIMEOUT_MS`) 안에 살아있음(`0x05`)을 받았는지 돌려줍니다. 재시작 알림(`0x00`)을 받은 뒤에는 다음 `0x05` 가 올 때까지 `false` 입니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `device_id` | `uint8_t` | 등록한 장치 번호 |

**반환값**: `bool` — `true`: 연결돼 있습니다. `false`: 연결이 끊겼거나 재시작 알림 뒤 아직 `0x05` 가 오지 않았습니다.

**⚠️ 호출 컨텍스트**: `Control_Setup()` · `Control_Loop()` 에서 부릅니다.

---

#### `XM_CanReadLatest`

```c
bool XM_CanReadLatest(uint8_t device_id, uint8_t stream, XmCanFrame_t* out);
```

그 장치의 주기 데이터 최신값을 읽습니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `device_id` | `uint8_t` | 등록한 장치 번호 |
| `stream` | `uint8_t` | `1` = 주기 데이터 1, `2` = 주기 데이터 2 |
| `out` | [`XmCanFrame_t*`](#xmcanframe_t) | 값을 받을 구조체 |

**반환값**: `bool`

- `true`: `out` 에 값이 있습니다. 같은 `seq` 를 다시 읽어도 `true` 입니다 — 새 값인지는 `seq` 를 비교하세요. 끊긴 뒤에도 옛 값은 `true` 로 읽히니, `tick_ms` 와 [`XM_CanIsConnected`](#xm_canisconnected) 로 판단하세요.
- `false`: 아직 받지 못했거나, 재시작 알림 뒤 새 값이 오기 전이거나, 쓰는 중이라 이번엔 읽지 못했거나, 인자가 잘못됐습니다 — `out` 은 그대로입니다.

**⚠️ 호출 컨텍스트**: `Control_Setup()` · `Control_Loop()` 에서 부릅니다.

**예제**

```c
static uint32_t s_last_seq;

void Control_Loop(void) {
    XmCanFrame_t f;
    if (XM_CanReadLatest(MY_SENSOR_ID, 1U, &f) && (f.seq != s_last_seq)) {
        s_last_seq = f.seq;                    /* 새 메시지 */
        if (XM_CanIsConnected(MY_SENSOR_ID)) {
            /* f.data[0 .. f.len - 1] 사용 */
        }
    }
}
```

**참고**: [`XmCanFrame_t`](#xmcanframe_t)

---

#### `XM_CanSend`

```c
XmCanResult_t XM_CanSend(uint8_t device_id, const void* data, uint8_t len);
```

장치에 명령을 보냅니다(ID `0x200` + 번호).

길이는 1 ~ 64 입니다(클래식 모드는 1 ~ 8). 허용 길이(1 ~ 8 · 12 · 16 · 20 · 24 · 32 · 48 · 64)가 아니면 다음 허용 길이까지 0 으로 채워 나갑니다. XM10 이 최근 살아있음으로 '연결' 이라 판단할 때만 수락합니다(끊긴 직후 최대 3 초는 수락될 수 있습니다).

수락된 명령은 하드웨어가 보낼 때까지 다시 시도하고, XM10 이 소프트웨어로 다시 보내지는 않습니다. 끊긴 동안 하드웨어에 남아 있던 명령은 다시 연결될 때 나갈 수 있습니다. 명령 첫 바이트를 순번으로 써서 장치가 중복 · 순서 뒤바뀜을 거르게 하세요([`XM_CanSeqIsNewer`](#xm_canseqisnewer)). 순번은 유효기간이 아닙니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `device_id` | `uint8_t` | 등록한 장치 번호 |
| `data` | `const void*` | 보낼 데이터 |
| `len` | `uint8_t` | 길이 [B], 1 ~ 64 (클래식 1 ~ 8) |

**반환값**: [`XmCanResult_t`](#xmcanresult_t)

- `XM_CAN_OK`: 보내기 큐에 실렸습니다. 전달이 끝났다는 뜻이 아닙니다.
- `XM_CAN_NOT_CONNECTED` · `XM_CAN_BUSY` · `XM_CAN_RECOVERY` · `XM_CAN_REJECTED` · `XM_CAN_NOT_OWNER`: 뜻은 [`XmCanResult_t`](#xmcanresult_t) 표를 보세요.

**⚠️ 호출 컨텍스트**: `Control_Setup()` · `Control_Loop()` 에서 부릅니다.

**예제**

```c
static uint8_t s_cmd_seq;

void Control_Loop(void) {
    uint8_t cmd[2] = { s_cmd_seq, 1U };       /* 첫 바이트 = 순번 */
    if (XM_CanSend(MY_SENSOR_ID, cmd, sizeof(cmd)) == XM_CAN_OK) {
        s_cmd_seq++;
    }
}
```

**참고**: [`XM_CanIsConnected`](#xm_canisconnected), [`XM_CanSeqIsNewer`](#xm_canseqisnewer)

---

#### `XM_CanGetStats`

```c
bool XM_CanGetStats(uint8_t device_id, XmCanDeviceStats_t* out);
```

장치별 통계를 읽습니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `device_id` | `uint8_t` | 등록한 장치 번호 |
| `out` | [`XmCanDeviceStats_t*`](#xmcandevicestats_t) | 통계를 받을 구조체 |

**반환값**: `bool` — `true`: 채웠습니다. `false`: 등록하지 않은 번호이거나 `out` 이 `NULL` 입니다.

**⚠️ 호출 컨텍스트**: `Control_Setup()` · `Control_Loop()` 에서 부릅니다.

---

#### `XM_CanGetBusStats`

```c
bool XM_CanGetBusStats(XmCanBusStats_t* out);
```

포트 공통 통계를 읽습니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `out` | [`XmCanBusStats_t*`](#xmcanbusstats_t) | 통계를 받을 구조체 |

**반환값**: `bool` — `true`: 채웠습니다. `false`: `out` 이 `NULL` 입니다.

**⚠️ 호출 컨텍스트**: `Control_Setup()` · `Control_Loop()` 에서 부릅니다.

---

#### `XM_CanEnableSync`

```c
void XM_CanEnableSync(bool on);
```

1 kHz 동기 신호(SYNC, ID `0x080`)를 내 장치에게도 보냅니다. 기본은 꺼짐입니다.

- FD 모드: 켜도 받아 줄 장치가 3 초 안에 하나도 보이지 않으면 보내지 않습니다. 센서 허브가 동작 중이면 끄더라도 SYNC 는 그대로 나갑니다 — 끄기는 내 요청만 끕니다. SYNC 는 1 바이트(카운터)입니다.
- 클래식 모드: SYNC 는 길이 0 입니다. 켠 동안에는 `XM_CanSendFrame` 으로 ID `0x080` 을 보낼 수 없습니다.
- SYNC 는 잠깐 멈추거나 한 번씩 빠질 수 있습니다. 장치는 샘플링 정렬에만 쓰고, 없으면 자기 주기로 계속 보내게 하세요.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `on` | `bool` | `true`: 켬. `false`: 끔 |

**반환값**: 없음

**⚠️ 호출 컨텍스트**: `Control_Setup()` · `Control_Loop()` 에서 부릅니다.

**참고**: [`XM_CanSendFrame`](#xm_cansendframe), `sync_skip_*` ([`XmCanBusStats_t`](#xmcanbusstats_t))

---

#### `XM_CanEnableHeartbeat`

```c
XmCanResult_t XM_CanEnableHeartbeat(uint8_t node_id, uint16_t period_ms);
```

XM10 의 하트비트를 내 장치에게 보냅니다 — ID `0x700` + `node_id`, 데이터 1 바이트 `0x05`. 기본은 꺼짐입니다.

`Control_Loop()` 가 끝난 직후에, 주기가 됐으면 보냅니다. 그래서 `Control_Loop()` 가 돌지 않으면 새로 보내지 않습니다 — 상대가 하트비트 끊김으로 스스로 멈추게 할 때 씁니다. 다만 이미 송신 대기에 든 메시지는 그 뒤에도 나갈 수 있고, 상대가 실제로 멈추는지는 상대 설정에 달렸습니다. 보드에서 확인하세요. (보드 확인 전) <!-- board-check -->

못 실으면 다음 주기가 아니라 다음 tick 에 다시 시도합니다(주기는 성공한 때부터). 버스 복구 중에는 간격이 길어질 수 있습니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `node_id` | `uint8_t` | 1 ~ 127 — 상대 장치가 기다리는 번호 |
| `period_ms` | `uint16_t` | 보내는 주기 1 ~ 65535 [ms]. `0` = 끔 (`node_id` 는 보지 않습니다) |

**반환값**: [`XmCanResult_t`](#xmcanresult_t)

- `XM_CAN_OK`: 켰거나, 껐거나, 주기를 바꿨습니다.
- `XM_CAN_REJECTED`: 범위 밖 번호이거나, 사용 불가 모드이거나, FD 모드에서 `0x02` 또는 `0x0C` · `0x0D` · `0x0F` 이거나, 그 ID 를 `XM_CanAttachDevice` · `XM_CanSubscribe` · 감시(`XM_CanOpenWatchNode`)로 이미 받는 중입니다. 반대 순서(하트비트를 먼저 켜고 나중에 받기 등록)도 거부됩니다.
- `XM_CAN_NOT_OWNER`: `Control_Setup()` · `Control_Loop()` 밖에서 불렀습니다.

**⚠️ 호출 컨텍스트**: `Control_Setup()` · `Control_Loop()` 에서 부릅니다.

**참고**: `heartbeat_sent_count` · `heartbeat_fail_count` · `heartbeat_last_fail` ([`XmCanBusStats_t`](#xmcanbusstats_t))

---

### 자유 송수신

#### `XM_CanSendFrame`

```c
XmCanResult_t XM_CanSendFrame(uint16_t id, const void* data, uint8_t len);
```

메시지 하나를 보냅니다. 받는 장치가 없어도 수락될 수 있습니다. 수락은 전달이 아닙니다([`XM_CanSend`](#xm_cansend) 와 같은 규칙).

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `id` | `uint16_t` | 메시지 ID (11비트). 보낼 수 있는 ID 는 아래 표를 보세요 |
| `data` | `const void*` | 보낼 데이터. 길이가 0 일 때만 `NULL` 이어도 됩니다 |
| `len` | `uint8_t` | 길이 [B]. FD 모드 0 ~ 64 (다음 허용 길이까지 0 으로 채움), 클래식 모드 0 ~ 8 |

**보낼 수 있는 ID**

| 모드 | 보낼 수 있는 ID |
|------|------|
| FD | `0x000` ~ `0x7FF` 중 아래 3가지를 뺀 전부입니다. ① 아래 7비트가 `0x0C` · `0x0D` · `0x0F` 인 ID(센서 허브 자리) ② `0x080` (SYNC 는 [`XM_CanEnableSync`](#xm_canenablesync) 로) ③ `0x702` (XM10 시스템 하트비트). ID `0x000` 은 길이 2 이고 둘째 바이트가 대상 번호일 때만 보냅니다(`0` = 전체, 센서 허브 번호는 거부) |
| 클래식 | `0x000` ~ `0x7FF` 전부입니다. 단 SYNC 를 켠 동안의 `0x080` 은 뺍니다. 11비트만 됩니다 |

**반환값**: [`XmCanResult_t`](#xmcanresult_t) — `XM_CAN_OK`: 보내기 큐에 실렸습니다. 나머지 값의 뜻은 표를 보세요.

**⚠️ 호출 컨텍스트**: `Control_Setup()` · `Control_Loop()` 에서 부릅니다.

**참고**: [`XM_CanIsIdAllowed`](#xm_canisidallowed), [`XM_CanFitLength`](#xm_canfitlength)

---

#### `XM_CanReadFrame`

```c
bool XM_CanReadFrame(XmCanFrame_t* out);
```

받은 순서대로 메시지 하나를 꺼냅니다. 없으면 `false` 입니다.

우편함이나 장치 API 가 가져가지 않은 메시지가 깊이 64 의 큐로 들어옵니다. 등록하지 않은 ID 도 모두 옵니다(FD 모드의 센서 허브 자리는 제외). 가득 차면 새 메시지를 버리고 `rx_queue_drop_count` 를 셉니다. `Control_Loop()` 에서 매 주기 비우세요.

서로 다른 종류의 메시지(살아있음 · 응답 · 오류 알림 대 그 밖) 사이의 버스 순서는 보장하지 않습니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `out` | [`XmCanFrame_t*`](#xmcanframe_t) | 메시지를 받을 구조체 |

**반환값**: `bool` — `true`: 하나 꺼냈습니다. `false`: 큐가 비었습니다.

**⚠️ 호출 컨텍스트**: `Control_Setup()` · `Control_Loop()` 에서 부릅니다.

**예제**

```c
XmCanFrame_t f;
while (XM_CanReadFrame(&f)) {
    /* f.id, f.len, f.data 사용 */
}
```

**참고**: `rx_queue_drop_count` · `rx_queue_peak` ([`XmCanBusStats_t`](#xmcanbusstats_t))

---

#### `XM_CanSubscribe`

```c
bool XM_CanSubscribe(uint16_t id);
```

이 ID 는 큐 대신 **최신값 우편함**으로 받습니다. 1 kHz 주기 데이터에 알맞습니다.

우편함은 장치 API 의 스트림과 합쳐 16 칸입니다. 다음은 거부합니다.

- 11비트 밖 ID
- FD 모드의 센서 허브 자리(아래 7비트 `0x0C` · `0x0D` · `0x0F`)
- 등록한 장치의 ID
- 이미 등록한 ID
- `XM_CanEnableHeartbeat` 로 내가 보내는 ID

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `id` | `uint16_t` | 우편함으로 받을 메시지 ID |

**반환값**: `bool` — `true`: 등록했습니다. `false`: 거부했습니다.

**⚠️ 호출 컨텍스트**: `Control_Setup()` 에서만 부릅니다.

**참고**: [`XM_CanReadLatestById`](#xm_canreadlatestbyid), [`XM_CanAgeMs`](#xm_canagems)

---

#### `XM_CanReadLatestById`

```c
bool XM_CanReadLatestById(uint16_t id, XmCanFrame_t* out);
```

우편함 ID 의 최신값을 읽습니다. [`XM_CanReadLatest`](#xm_canreadlatest) 와 같은 규칙입니다 — 같은 `seq` 도 `true` 이고, 실패하면 `out` 은 그대로입니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `id` | `uint16_t` | `XM_CanSubscribe` 로 등록한 ID |
| `out` | [`XmCanFrame_t*`](#xmcanframe_t) | 값을 받을 구조체 |

**반환값**: `bool` — `true`: `out` 에 값이 있습니다. `false`: 읽지 못했습니다 — `out` 은 그대로입니다.

**⚠️ 호출 컨텍스트**: `Control_Setup()` · `Control_Loop()` 에서 부릅니다.

---

#### `XM_CanAgeMs`

```c
uint32_t XM_CanAgeMs(uint16_t id);
```

우편함 ID 를 마지막으로 받은 뒤 지난 시간 [ms] 을 돌려줍니다. 받은 적이 없거나, 2^31 ms 이상 지났거나, 등록하지 않은 ID 이면 `XM_CAN_AGE_NONE` 입니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `id` | `uint16_t` | `XM_CanSubscribe` 로 등록한 ID |

**반환값**: `uint32_t` — 지난 시간 [ms]. 또는 `XM_CAN_AGE_NONE` (`0xFFFFFFFF`)

**⚠️ 호출 컨텍스트**: `Control_Setup()` · `Control_Loop()` 에서 부릅니다.

---

### 도움 함수 (어디서나)

#### `XM_CanIsIdAllowed`

```c
bool XM_CanIsIdAllowed(uint16_t id);
```

이 모드의 ID 규칙으로 보낼 수 있는 ID 인지 돌려줍니다. ID 만 봅니다. ID `0x000` 의 데이터 검사 · SYNC 를 켠 동안의 `0x080` 거부 · `XM_CAN_BUSY` · `XM_CAN_RECOVERY` 는 [`XM_CanSendFrame`](#xm_cansendframe) 이 따로 알려 줍니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `id` | `uint16_t` | 메시지 ID |

**반환값**: `bool` — `true`: 보낼 수 있는 ID 입니다.

**⚠️ 호출 컨텍스트**: 어디서나 부를 수 있습니다.

---

#### `XM_CanIsDeviceIdAllowed`

```c
bool XM_CanIsDeviceIdAllowed(uint8_t id);
```

장치 API 에 등록할 수 있는 번호인지 돌려줍니다. 1 ~ 127 중 예약 번호(`0x01` · `0x02` · `0x0C` · `0x0D` · `0x0F` · `0x10` · `0x11`)를 뺀 나머지입니다. 번호별 표: [11. CAN 통신 — 번호 한눈에 보기](../11-canbus.md#3-번호-한눈에-보기).

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `id` | `uint8_t` | 장치 번호 |

**반환값**: `bool` — `true`: 등록할 수 있는 번호입니다.

**⚠️ 호출 컨텍스트**: 어디서나 부를 수 있습니다.

---

#### `XM_CanFitLength`

```c
uint8_t XM_CanFitLength(uint8_t len);
```

버스에 실릴 길이로 올립니다(0 은 0). 이 모드에서 못 실으면 `XM_CAN_LENGTH_INVALID` 를 돌려줍니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `len` | `uint8_t` | 데이터 길이 [B] |

**반환값**: `uint8_t` — 버스에 실릴 길이 [B]. 못 실으면 `XM_CAN_LENGTH_INVALID` (`0xFF`)

**⚠️ 호출 컨텍스트**: 어디서나 부를 수 있습니다.

---

#### `XM_CanSeqIsNewer`

```c
bool XM_CanSeqIsNewer(uint8_t last, uint8_t now);
```

8비트 순번이 새 값인지 돌려줍니다. `now` 가 `last` 보다 1 ~ 127 앞서면 새 값입니다. 차이가 0 이거나 128 ~ 255 이면 중복이거나 옛 값입니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `last` | `uint8_t` | 지난번에 받은 순번 |
| `now` | `uint8_t` | 이번에 받은 순번 |

**반환값**: `bool` — `true`: 새 값입니다.

**⚠️ 호출 컨텍스트**: 어디서나 부를 수 있습니다.

**참고**: [`XM_CanSend`](#xm_cansend)

---

## 타입 / 상수

### `XmCanResult_t`

모든 '보내기' 함수의 결과입니다. **꼭 `== XM_CAN_OK` 로 확인하세요.**

| 값 | 이름 | 뜻 |
|----|------|------|
| 0 | `XM_CAN_OK` | 보내기 큐에 실렸습니다(수락). 상대가 받았다는 뜻은 아닙니다 |
| 1 | `XM_CAN_BUSY` | 이번엔 못 실었습니다(큐 가득 · 다른 송신과 겹침) — 다음 주기에 다시 |
| 2 | `XM_CAN_RECOVERY` | 버스 복구 중입니다(버스 오류 뒤 · 막힌 송신을 비우는 중) — 잠시 뒤 다시 |
| 3 | `XM_CAN_NOT_CONNECTED` | (장치 API 만) 장치가 연결돼 있지 않습니다 |
| 4 | `XM_CAN_REJECTED` | 보낼 수 없는 ID · 길이 · 인자이거나, 이 모드에서 지원하지 않습니다 |
| 5 | `XM_CAN_NOT_OWNER` | `Control_Setup()` · `Control_Loop()` 밖에서 불렀습니다 |

---

### `XmCanBusMode_t`

CAN-FD #2 포트의 버스 모드입니다. 전원을 켤 때 한 번 정합니다([`XM_CanBusMode`](#xm_canbusmode)).

| 값 | 이름 | 뜻 |
|----|------|------|
| 0 | `XM_CAN_BUS_FD` | 기본 — CAN-FD(1 / 5 Mbit/s). IMU · EMG 허브와 함께 쓸 수 있습니다 |
| 1 | `XM_CAN_BUS_CLASSIC` | 클래식 CAN 전용(기본 1 Mbit/s — [`XM_CanClassicBitTiming`](#xm_canclassicbittiming)). 모터 드라이버 등에 씁니다. 이 포트에 허브를 연결하지 마세요 |
| 2 | `XM_CAN_BUS_UNAVAILABLE` | (읽기 전용) 클래식을 요청했지만 설정하지 못했습니다. 이 포트로 아무것도 보내지 않습니다 |

---

### `XmCanBitTiming_t`

클래식 CAN 비트 타이밍입니다. 속도 = 80 MHz ÷ `prescaler` ÷ (1 + `seg1` + `seg2`) 이고, 표본 위치 = (1 + `seg1`) ÷ (1 + `seg1` + `seg2`) 입니다. 상대 장치와 맞는 값을 쓰세요.

```c
typedef struct {
    uint16_t prescaler;
    uint16_t seg1;
    uint8_t  seg2;
    uint8_t  sjw;
} XmCanBitTiming_t;
```

| 필드 | 타입 | 설명 |
|------|------|------|
| `prescaler` | `uint16_t` | 분주 1 ~ 512 |
| `seg1` | `uint16_t` | 표본 위치 앞 구간 1 ~ 256 |
| `seg2` | `uint8_t` | 표본 위치 뒤 구간 1 ~ 128 |
| `sjw` | `uint8_t` | 재동기 폭 1 ~ `seg2` |

프리셋 4개를 `XmCanBitTiming_t` 의 초기값으로 쓸 수 있습니다. 표본 위치는 모두 87.5 % 입니다.

| 프리셋 | 속도 | `prescaler` | `seg1` | `seg2` | `sjw` |
|------|------|:---:|:---:|:---:|:---:|
| `XM_CAN_TIMING_1M` | 1 Mbit/s (기본) | 5 | 13 | 2 | 2 |
| `XM_CAN_TIMING_500K` | 500 kbit/s | 10 | 13 | 2 | 2 |
| `XM_CAN_TIMING_250K` | 250 kbit/s | 20 | 13 | 2 | 2 |
| `XM_CAN_TIMING_125K` | 125 kbit/s | 40 | 13 | 2 | 2 |

---

### `XmCanFrame_t`

받은 메시지 하나입니다.

```c
typedef struct {
    uint32_t seq;
    uint32_t tick_ms;
    uint16_t id;
    uint8_t  len;
    uint8_t  data[XM_CAN_MAX_DATA];
} XmCanFrame_t;
```

| 필드 | 타입 | 설명 |
|------|------|------|
| `seq` | `uint32_t` | **큐**: 큐로 들어온 순서 번호입니다. 넘쳐서 버린 것도 번호를 쓰므로 빠진 번호가 버린 수입니다. **최신값(장치 API · 우편함)**: 그 ID 를 받을 때마다 1 씩 늘어납니다. 지난번과 같으면 새 메시지가 아닙니다 |
| `tick_ms` | `uint32_t` | 받은 시각 (`XM_GetTick` 기준, 1 ms) |
| `id` | `uint16_t` | 메시지 ID (11비트) |
| `len` | `uint8_t` | 버스에 실린 길이 [B]. FD: 0 ~ 8 · 12 · 16 · 20 · 24 · 32 · 48 · 64 / 클래식: 0 ~ 8 |
| `data` | `uint8_t[64]` | 데이터 (`XM_CAN_MAX_DATA`) |

---

### `XmCanDeviceStats_t`

장치별 통계입니다(등록한 장치만). [`XM_CanGetStats()`](#xm_cangetstats) 로 읽습니다. 각 값은 온전하지만 서로 같은 순간의 값은 아닙니다.

| 필드 | 타입 | 설명 |
|------|------|------|
| `rx_count[2]` | `uint32_t` | 주기 데이터 1 · 2 를 받은 수 |
| `rx_busy_drop_count` | `uint32_t` | 읽는 중이라 못 쓰고 버린 수 |
| `unread_overwrite_count` | `uint32_t` | 아직 읽지 않은 값을 새 값이 덮은 수 |
| `alive_count` | `uint32_t` | 살아있음(`0x05`)을 받은 수 |
| `alive_invalid_count` | `uint32_t` | 살아있음 자리로 온 다른 값 · 빈 메시지 수 |
| `reboot_count` | `uint32_t` | 재시작 알림(`0x00`)을 받은 수 |
| `last_rx_tick` | `uint32_t` | 이 장치의 메시지를 마지막으로 받은 시각 |
| `last_alive_tick` | `uint32_t` | 살아있음(`0x05`)을 마지막으로 받은 시각 |
| `send_accept_count` | `uint32_t` | `XM_CanSend` 가 수락된 수 |
| `send_fail_count` | `uint32_t` | `XM_CanSend` 가 실패한 수 |
| `read_busy_count` | `uint32_t` | 쓰는 중이라 `XM_CanReadLatest` 가 이번엔 못 읽은 수 |
| `last_send_fail` | `uint8_t` | 마지막으로 실패한 `XM_CanSend` 결과(`XmCanResult_t`). 0 = 아직 없음 |

---

### `XmCanBusStats_t`

포트 공통 통계입니다. [`XM_CanGetBusStats()`](#xm_cangetbusstats) 로 읽습니다. 각 값은 온전하지만 서로 같은 순간의 값은 아닙니다.

| 필드 | 타입 | 설명 |
|------|------|------|
| `bad_call_count` | `uint32_t` | 미등록 번호 · 잘못된 인자 · `Control_Setup()` 밖 등록 |
| `owner_violation_count` | `uint32_t` | 다른 태스크 · 인터럽트에서 부른 수 |
| `flush_request_count` | `uint32_t` | 받아 줄 장치가 없어 대기 중 메시지 취소를 '요청' 한 수. 완료 수가 아닙니다 |
| `busoff_flush_count` | `uint32_t` | 버스 오류(bus-off) 뒤 대기 중 메시지를 비운 수 |
| `tx_pending_count` | `uint32_t` | 지금 하드웨어에 남아 있는 송신 수(부른 순간) |
| `sync_skip_count` | `uint32_t` | SYNC 를 보내지 못한 tick 수 — 아래 네 사유의 합 |
| `sync_skip_busy` | `uint32_t` | 다른 송신과 겹침 · 송신 큐 가득 |
| `sync_skip_hub_setup` | `uint32_t` | 센서 허브를 설정하는 동안 |
| `sync_skip_recovery` | `uint32_t` | 버스 복구 중 |
| `sync_skip_clear` | `uint32_t` | (클래식) 장치 요청이 시간 초과로 끝난 뒤 송신 대기를 비우는 동안 |
| `rx_frame_count` | `uint32_t` | 이 API 로 들어온 메시지 수 |
| `rx_queue_drop_count` | `uint32_t` | 받은 순서 큐가 가득해 버린 수 |
| `rx_queue_last_seq` | `uint32_t` | 큐로 보낸 마지막 메시지의 `seq`. 큐를 다 비운 뒤 drop 수와 대조합니다 |
| `rx_queue_peak` | `uint32_t` | 큐에 쌓였던 최대 개수 |
| `rejected_id_count` | `uint32_t` | 허용 밖 ID 로 보내려 한 수 |
| `classic_hub_tx_refused_count` | `uint32_t` | (클래식 · 사용 불가) XM10 이 이 포트로 내보내지 않고 막은 허브용 송신 수 |
| `timing_write_fail_count` | `uint32_t` | `XM_CanSetClassicBitTiming` 이 속도를 쓰지 못한 수 |
| `heartbeat_sent_count` | `uint32_t` | `XM_CanEnableHeartbeat` 의 하트비트가 수락된 수 |
| `heartbeat_fail_count` | `uint32_t` | 하트비트를 못 실어 다음 주기로 미룬 수 |
| `heartbeat_last_fail` | `uint8_t` | 마지막으로 실패한 하트비트 결과(`XmCanResult_t`). 0 = 아직 없음 |
| `bus_mode` | `uint8_t` | 지금 버스 모드(`XmCanBusMode_t`) |
| `tec` | `uint8_t` | 송신 오류 카운터(부른 순간) |
| `rec` | `uint8_t` | 수신 오류 카운터(부른 순간) |
| `bus_status` | `uint8_t` | bit0 = bus-off · bit1 = 오류 경고 · bit2 = 오류 수동 |

---

### 상수

| 이름 | 값 | 설명 |
|------|----|------|
| `XM_CAN_MAX_DEVICES` | `4U` | 등록할 수 있는 장치 수. 동시에 빠르게 돌릴 수 있는 대수를 보장하는 값은 아닙니다 |
| `XM_CAN_MAX_DATA` | `64U` | 한 번에 싣는 데이터 최대 바이트 (클래식 모드는 8) |
| `XM_CAN_ALIVE_TIMEOUT_MS` | `3000U` | 살아있음이 이만큼 없으면 '끊김' [ms] |
| `XM_CAN_LENGTH_INVALID` | `0xFFU` | `XM_CanFitLength` 가 돌려주는 값 — 이 모드에서 못 싣는 길이 |
| `XM_CAN_AGE_NONE` | `0xFFFFFFFFUL` | `XM_CanAgeMs` 가 돌려주는 값 — 받은 적 없음 · 너무 오래됨 · 등록 안 함 |
| `XM_CAN_TIMING_1M` | `{ 5U, 13U, 2U, 2U}` | 클래식 CAN 1 Mbit/s (기본) |
| `XM_CAN_TIMING_500K` | `{10U, 13U, 2U, 2U}` | 클래식 CAN 500 kbit/s |
| `XM_CAN_TIMING_250K` | `{20U, 13U, 2U, 2U}` | 클래식 CAN 250 kbit/s |
| `XM_CAN_TIMING_125K` | `{40U, 13U, 2U, 2U}` | 클래식 CAN 125 kbit/s |

---

## Rev1.1 / Rev2.0 차이 요약

| 항목 | Rev1.1 | Rev2.0 |
|------|--------|--------|
| CAN 버스 API (`xm_api_canbus.h`) | ❌ 없음 | 🟢 전용 |

---

## 관련 문서

- [11. CAN 통신 (개념)](../11-canbus.md) — 쓰는 순서, 배선, 장치 쪽 메시지 약속, 흔한 실수
- [`xm_api_canopen.h` — CANopen 장치 도움 함수](xm-api-canopen.md) — 클래식 모드에서 NMT · 하트비트 감시 · SDO
- [device-examples](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/device-examples/) — 직접 만든 장치나 PC 로 예제를 돌려 볼 수 있는 상대 장치
