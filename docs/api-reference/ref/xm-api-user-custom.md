# `xm_api_user_custom.h` — Total Data Packet 안 28바이트 사용자 슬롯 API

> **대상 헤더**: `XM_FW/XM_API/xm_api_user_custom.h` (Rev1.1 · Rev2.0 헤더 동일 — 기능 차이 없음, 파일 상단 `@date` 주석만 하루 차이)
> 📚 **관련 개념 문서**: [05. USB 시리얼 통신](../05-usb-connectivity.md) (Total Data Packet 0x20 / User Custom Channel 0xF0~0xFE 구분), [02. KIT H10 제어 + 데이터](../02-h10-control-n-data.md) (`Control_Setup`/`Control_Loop` IPO 주기)
> 🧪 **관련 예제**: 이 API를 직접 호출하는 예제는 아직 없습니다 (v2.3.0 신규 API, SDK 전수 검색 결과 사용 사례 0건). 같은 Total Data Packet(0x20) 을 다루는 [Ex.09 CDC Stream](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/09_CDC_Stream/) 을 먼저 읽으면 이 슬롯이 어떤 스트림에 실리는지 이해하기 쉽습니다.

---

## 언제 사용하나

XM10 은 `Control_Loop()` 가 끝날 때마다 365바이트짜리 **Total Data Packet**(Module ID `0x20`)을 1kHz 로 USB 에 자동 전송합니다. 이 패킷의 마지막 28바이트는 `User_Custom` 영역으로, 필터링한 EMG 신호나 FSM 상태 같은 사용자 값을 그대로 실을 수 있게 비워 둔 슬롯입니다. 본 헤더의 함수로 이 28바이트를 채워 두기만 하면, 별도의 전송 코드 없이 PhAI Studio 실시간 그래프와 OPFS(브라우저 로컬 파일 시스템) 녹화 파일에 자동으로 함께 기록됩니다.

이름이 비슷한 [User Custom **Channel** (`0xF0~0xFE`, `XM_SetUsbCustomMeta`/`XM_SendUsbDataWithId`)](../05-usb-connectivity.md) 과는 다른 기능이니 주의하세요. 그쪽은 패킷 크기와 개수를 자유롭게 늘릴 수 있는 별도 채널이고, 본 API 는 이미 항상 전송되는 `0x20` 패킷 안의 **고정된 28바이트**에 값만 채워 넣는 훨씬 가벼운 방법입니다. 슬롯 4개(float) + 4개(int16) + 16비트(flags) + 2개(uint8) 만으로 충분하다면 이 API 를, 더 크거나 가변적인 데이터를 보내야 한다면 05번 문서의 커스텀 채널을 사용하세요.

### 기본 사용 흐름

```c
void Control_Setup(void) {
    /* (선택) PhAI Studio 채널 라벨 등록 — 미호출 시 기본 라벨(f0, i16_0, ...) 사용 */
    XM_SetUsbCustomMeta(0xE0,
        "[{\"slot\":\"f0\",\"name\":\"EMG_envelope\",\"unit\":\"uV\"},"
         " {\"slot\":\"i16_0\",\"name\":\"PF3_raw\",\"unit\":\"LSB\"}]");
}

void Control_Loop(void) {
    XM_UserCustom_SetFloat(0, envelope_uV);   // user_f[0]
    XM_UserCustom_SetI16  (0, pf3_raw);       // user_i16[0]
    XM_UserCustom_SetFlag (0, is_fsm_active); // user_flags 의 bit0
}
```

`XM_SetUsbCustomMeta` 는 이 헤더가 아니라 `xm_api_usb.h` 소속 함수입니다 — 자세한 시그니처는 [05번 문서](../05-usb-connectivity.md)를 참고하세요.

---

## 함수 목록

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_UserCustom_SetFloat`](#xm_usercustom_setfloat) | Float 슬롯(4개) 중 하나에 값을 기록합니다 |
| [`XM_UserCustom_SetI16`](#xm_usercustom_seti16) | Int16 슬롯(4개) 중 하나에 값을 기록합니다 |
| [`XM_UserCustom_SetU8`](#xm_usercustom_setu8) | uint8 슬롯(2개) 중 하나에 값을 기록합니다 |
| [`XM_UserCustom_SetFlags`](#xm_usercustom_setflags) | 16비트 flags 워드 전체를 한 번에 덮어씁니다 |
| [`XM_UserCustom_SetFlag`](#xm_usercustom_setflag) | flags 중 비트 하나만 set/clear 합니다 |
| [`XM_UserCustom_Reset`](#xm_usercustom_reset) | User_Custom 슬롯 전체를 0으로 초기화합니다 |
| [`XM_UserCustom_GetBlock`](#xm_usercustom_getblock) | 현재 User_Custom 블록 값을 한 번에 스냅샷으로 읽어옵니다 |

내부 전용(`[Internal]`) 함수는 없습니다 — 7개 함수 모두 공개 API 입니다. 다만 `XM_UserCustom_GetBlock` 은 System Layer 가 매 tick 자동으로 호출하므로 일반 사용자가 직접 호출할 일은 거의 없습니다 (아래 상세 참고).

---

## 함수 상세

### `XM_UserCustom_SetFloat`

```c
void XM_UserCustom_SetFloat(uint8_t idx, float value);
```

`user_f[]` 슬롯 중 하나에 float 값을 기록합니다. 필터링/가공된 신호(예: EMG envelope)를 실어 보낼 때 사용합니다.

| 이름 | 타입 | 설명 |
|------|------|------|
| `idx` | `uint8_t` | 슬롯 인덱스 (0 ~ `XM_USER_CUSTOM_FLOAT_COUNT`-1 = 0~3). 범위를 벗어나면 silent ignore |
| `value` | `float` | 기록할 값 |

**반환값**: 없음 (`void`)

⚠️ **호출 컨텍스트**: 헤더 주석에 non-blocking 이며 `Control_Loop()` 뿐 아니라 다른 곳에서도 호출 가능하다고 명시되어 있습니다. 다만 ISR 안전성을 명시적으로 보장하는 문구는 없으므로, `Control_Setup()`/`Control_Loop()` 컨텍스트에서 호출하는 것을 권장합니다.

```c
void Control_Loop(void) {
    float envelope_uV = ComputeEmgEnvelope();
    XM_UserCustom_SetFloat(0, envelope_uV);  // user_f[0] 에 기록
}
```

**참고**: [`XM_UserCustom_GetBlock`](#xm_usercustom_getblock) 으로 되읽기 가능 · [슬롯 레이아웃 표](#xm_usercustomblock_t)

---

### `XM_UserCustom_SetI16`

```c
void XM_UserCustom_SetI16(uint8_t idx, int16_t value);
```

`user_i16[]` 슬롯 중 하나에 int16 값을 기록합니다. raw ADC 값이나 카운터처럼 정수로 충분한 값에 사용합니다.

| 이름 | 타입 | 설명 |
|------|------|------|
| `idx` | `uint8_t` | 슬롯 인덱스 (0 ~ `XM_USER_CUSTOM_I16_COUNT`-1 = 0~3). 범위를 벗어나면 silent ignore |
| `value` | `int16_t` | 기록할 값 |

**반환값**: 없음 (`void`)

⚠️ **호출 컨텍스트**: `XM_UserCustom_SetFloat` 과 동일 — non-blocking, `Control_Setup()`/`Control_Loop()` 컨텍스트 기준.

```c
XM_UserCustom_SetI16(0, (int16_t)pf3_raw_adc);  // user_i16[0] 에 기록
```

**참고**: [`XM_UserCustom_SetU8`](#xm_usercustom_setu8) (더 작은 범위면 uint8 슬롯 고려)

---

### `XM_UserCustom_SetU8`

```c
void XM_UserCustom_SetU8(uint8_t idx, uint8_t value);
```

`user_u8[]` 슬롯 중 하나에 uint8 값을 기록합니다. FSM state 처럼 작은 enum 값에 적합합니다.

| 이름 | 타입 | 설명 |
|------|------|------|
| `idx` | `uint8_t` | 슬롯 인덱스 (0 ~ `XM_USER_CUSTOM_U8_COUNT`-1 = 0~1). 범위를 벗어나면 silent ignore |
| `value` | `uint8_t` | 기록할 값 |

**반환값**: 없음 (`void`)

⚠️ **호출 컨텍스트**: `XM_UserCustom_SetFloat` 과 동일 — non-blocking, `Control_Setup()`/`Control_Loop()` 컨텍스트 기준.

```c
XM_UserCustom_SetU8(0, (uint8_t)fsm_state);  // user_u8[0] 에 기록
```

**참고**: [`XM_UserCustom_SetFlag`](#xm_usercustom_setflag) (bool 하나만 필요하면 flags 비트가 더 저렴)

---

### `XM_UserCustom_SetFlags`

```c
void XM_UserCustom_SetFlags(uint16_t flags);
```

`user_flags` 16비트 워드 전체를 한 번에 덮어씁니다. 여러 비트를 동시에 바꿀 때 `SetFlag` 를 여러 번 호출하는 것보다 효율적입니다.

| 이름 | 타입 | 설명 |
|------|------|------|
| `flags` | `uint16_t` | 새로 기록할 16비트 워드 (기존 값 전부 덮어씀) |

**반환값**: 없음 (`void`)

⚠️ **호출 컨텍스트**: `XM_UserCustom_SetFloat` 과 동일 — non-blocking, `Control_Setup()`/`Control_Loop()` 컨텍스트 기준.

```c
XM_UserCustom_SetFlags(0x0005);  // bit0, bit2 = 1, 나머지 비트 = 0
```

**참고**: [`XM_UserCustom_SetFlag`](#xm_usercustom_setflag) (비트 하나만 바꿀 때)

---

### `XM_UserCustom_SetFlag`

```c
void XM_UserCustom_SetFlag(uint8_t bit, bool value);
```

`user_flags` 중 비트 하나만 set/clear 합니다. 다른 비트는 그대로 유지됩니다.

| 이름 | 타입 | 설명 |
|------|------|------|
| `bit` | `uint8_t` | 비트 인덱스 (0 ~ `XM_USER_CUSTOM_FLAG_COUNT`-1 = 0~15). 범위를 벗어나면 silent ignore |
| `value` | `bool` | `true` = 1 로 set, `false` = 0 으로 clear |

**반환값**: 없음 (`void`)

⚠️ **호출 컨텍스트**: `XM_UserCustom_SetFloat` 과 동일 — non-blocking, `Control_Setup()`/`Control_Loop()` 컨텍스트 기준.

```c
XM_UserCustom_SetFlag(3, is_fsm_active);  // bit3 만 set/clear, 나머지 비트 유지
```

**참고**: [`XM_UserCustom_SetFlags`](#xm_usercustom_setflags) (여러 비트를 한 번에 바꿀 때)

---

### `XM_UserCustom_Reset`

```c
void XM_UserCustom_Reset(void);
```

User_Custom 슬롯 전체(float 4개, int16 4개, flags, uint8 2개)를 0으로 초기화합니다.

파라미터 없음.

**반환값**: 없음 (`void`)

⚠️ **호출 컨텍스트**: 부팅 시 시스템이 자동으로 1회 호출하므로 일반 사용자는 보통 호출할 필요가 없습니다. 값을 명시적으로 리셋하고 싶을 때만 `Control_Setup()`/`Control_Loop()` 컨텍스트 기준으로 호출하세요.

```c
XM_UserCustom_Reset();  // 모든 슬롯을 0으로 되돌림 (평상시엔 불필요)
```

**참고**: [타입/매크로](#xm_usercustomblock_t) (전체 슬롯 구성)

---

### `XM_UserCustom_GetBlock`

```c
void XM_UserCustom_GetBlock(XM_UserCustomBlock_t* out);
```

현재 User_Custom 블록(28바이트)의 값을 `out` 이 가리키는 버퍼로 한 번에 복사합니다.

| 이름 | 타입 | 설명 |
|------|------|------|
| `out` | `XM_UserCustomBlock_t*` | 28바이트 결과를 받을 출력 버퍼. **NULL 전달 금지** |

**반환값**: 없음 (`void`) — 결과는 `out` 포인터를 통해 반환됩니다.

⚠️ **호출 컨텍스트**: System Layer(`XM_TotalData_Snapshot`)가 매 tick 자동으로 호출해 Total Data Packet 을 채우는 함수이며, 일반 사용자 코드에서 직접 호출할 일은 거의 없습니다. 값을 직접 확인하고 싶을 때만 `Control_Setup()`/`Control_Loop()` 컨텍스트 기준으로 호출하세요.
헤더는 `out` 에 NULL 을 전달하지 말라고만 명시할 뿐, NULL 전달 시 내부적으로 가드가 있는지는 문서화되어 있지 않습니다 — 항상 유효한 버퍼를 넘기세요.

```c
XM_UserCustomBlock_t snapshot;
XM_UserCustom_GetBlock(&snapshot);
XM_SendUsbDebugMessage("f0=%.2f, flags=0x%04X\r\n", snapshot.f[0], snapshot.flags);
```

**참고**: [`XM_UserCustomBlock_t`](#xm_usercustomblock_t) (필드 레이아웃)

---

## 타입 / 매크로

### `XM_UserCustomBlock_t`

Total Data Packet(365B, Module ID `0x20`) 끝의 28바이트 `User_Custom` 영역을 그대로 거울처럼 옮겨 담는 구조체입니다. `XM_UserCustom_GetBlock()` 의 출력 타입으로 쓰입니다.

```c
typedef struct {
    float    f[XM_USER_CUSTOM_FLOAT_COUNT];   /**< 16B */
    int16_t  i16[XM_USER_CUSTOM_I16_COUNT];   /**<  8B */
    uint16_t flags;                           /**<  2B */
    uint8_t  u8[XM_USER_CUSTOM_U8_COUNT];     /**<  2B */
} XM_UserCustomBlock_t;
```

| 필드 | 타입 | 크기 | Total Data Packet 오프셋 | 대응 함수 |
|------|------|------|--------------------------|-----------|
| `f[4]` | `float` | 16B | 337 ~ 352 | `XM_UserCustom_SetFloat` |
| `i16[4]` | `int16_t` | 8B | 353 ~ 360 | `XM_UserCustom_SetI16` |
| `flags` | `uint16_t` | 2B | 361 ~ 362 | `XM_UserCustom_SetFlags` / `XM_UserCustom_SetFlag` |
| `u8[2]` | `uint8_t` | 2B | 363 ~ 364 | `XM_UserCustom_SetU8` |

> 오프셋은 `XM_FW/System/Comm/USB/xm_total_data_packet.h` (자동 생성 파일)의 `user_f`/`user_i16`/`user_flags`/`user_u8` 필드 주석 기준입니다. 총 28바이트로, 365바이트 Total Data Packet 의 마지막 구간(offset 337~364)에 해당합니다.
> `XM_UserCustomBlock_t` 자체에는 `packed` 속성이 없지만, 필드 순서(float→int16→uint16→uint8) 상 컴파일러의 자연 정렬만으로도 패딩 없이 정확히 28바이트가 됩니다.

### 매크로

| 매크로 | 값 | 의미 |
|--------|----|------|
| `XM_USER_CUSTOM_FLOAT_COUNT` | `4U` | `user_f[]` 슬롯 수 |
| `XM_USER_CUSTOM_I16_COUNT` | `4U` | `user_i16[]` 슬롯 수 |
| `XM_USER_CUSTOM_U8_COUNT` | `2U` | `user_u8[]` 슬롯 수 |
| `XM_USER_CUSTOM_FLAG_COUNT` | `16U` | `user_flags` 비트 수 |

인덱스/비트 하드코딩 대신 이 매크로로 반복문 상한을 잡으면, 이후 SDK 버전에서 슬롯 수가 바뀌어도 코드를 그대로 재사용할 수 있습니다.
