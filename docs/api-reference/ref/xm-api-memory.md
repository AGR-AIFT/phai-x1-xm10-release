# `xm_api_memory.h` — XM10 메모리 접근 API

> 🟢 **Rev 2.0 전용** — 본 헤더 전체가 XM10 Rev 2.0 SDK에만 포함되어 있습니다. Rev 1.1 SDK (`XM10_SDK/Rev1.1/`) 에는 `xm_api_memory.h` 자체가 존재하지 않으므로, 아래 함수를 Rev 1.1 프로젝트에서 호출하면 **link 단계에서 실패**합니다. 보드 리비전 확인은 [보드 리비전 비교](../../hardware/README.md#보드-리비전-비교) 참고.
>
> - **대상 헤더**: `XM_FW/XM_API/xm_api_memory.h`
> - **관련 개념 문서**: [07. 메모리 영역](../07-memory-management.md)
> - **관련 예제**: [Ex.36 OnDevice Kinesthetic Learning](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/36_OnDevice_Kinesthetic_Learning/) — `XM_UserNV_Read/Write/Erase`로 학습된 신경망 가중치를 Flash에 저장/복원

---

## 언제 사용하나

기본 전역 변수(`.bss`/`.data`)만으로는 부족한 상황 — 대용량 버퍼가 필요하거나, 제어 루프 안에서 캐시 지연 없이 결정론적으로 접근해야 하거나, 전원이 꺼져도 값을 남겨야 하는 상황 — 에서 이 API로 원하는 메모리 영역을 직접 선택합니다. 각 영역의 특성(속도/휘발성/DMA 가능 여부)과 선택 기준은 [07. 메모리 영역](../07-memory-management.md)에 정리되어 있으니, 이 페이지에서는 개별 함수의 시그니처와 파라미터만 다룹니다.

---

## 함수 목록

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_GetUserWorkspace()`](#xm_getuserworkspace) | RAM_D1 사용자 워크스페이스 시작 주소 반환 |
| [`XM_GetUserWorkspaceSize()`](#xm_getuserworkspacesize) | RAM_D1 워크스페이스 크기(바이트) 반환 |
| [`XM_GetUserDTCM()`](#xm_getuserdtcm) | DTCM 사용자 영역 시작 주소 반환 |
| [`XM_GetUserDTCMSize()`](#xm_getuserdtcmsize) | DTCM 사용자 영역 크기(바이트) 반환 |
| [`XM_UserNV_GetSize()`](#xm_usernv_getsize) | Flash User NV 영역 크기(바이트) 반환 |
| [`XM_UserNV_Read()`](#xm_usernv_read) | Flash User NV에서 데이터 읽기 |
| [`XM_UserNV_Write()`](#xm_usernv_write) | Flash User NV에 데이터 쓰기 |
| [`XM_UserNV_Erase()`](#xm_usernv_erase) | Flash User NV 전체 영역 지우기 |
| [`XM_UserNV_IsErased()`](#xm_usernv_iserased) | Flash User NV가 비어있는지(전체 0xFF) 확인 |

> ℹ️ 이전 버전에 있던 `XM_GetUserPSRAM()` / `XM_GetUserPSRAMSize()` 는 공개 API에서 제외되었습니다. 대용량 버퍼가 필요하면 `XM_GetUserWorkspace()`(RAM_D1, 200KB)를 사용하세요.

매크로 2종(`XM_RAMFUNC`, `XM_DTCM_VAR`)은 [타입/매크로](#타입매크로)에서 다룹니다.

---

## 함수 상세

### RAM_D1 사용자 워크스페이스

#### `XM_GetUserWorkspace()`

```c
void* XM_GetUserWorkspace(void);
```

RAM_D1 영역에 마련된 사용자 워크스페이스의 시작 주소를 반환합니다. Cacheable(D-Cache 경유) 영역이라 접근 속도는 빠르지만, 전원이 꺼지면 내용이 사라집니다. 알고리즘 변수, 센서 데이터 배열, 연산 버퍼 등 범용 목적으로 가장 먼저 고려할 영역입니다.

**파라미터**: 없음

**반환값**

| 반환값 | 의미 |
|--------|------|
| `void*` | 연속 메모리 블록의 시작 주소 |

**⚠️ 호출 컨텍스트**: 헤더에 별도 제약이 명시되어 있지 않습니다. `Control_Setup()`/`Control_Loop()` 컨텍스트 기준으로 작성되었으며, 보통 `Control_Setup()`에서 포인터를 한 번 받아 전역/정적 포인터에 저장해두고 이후 `Control_Loop()`에서 재사용하는 방식을 권장합니다.

```c
static float* s_work_buf;

void Control_Setup(void) {
    s_work_buf = (float*)XM_GetUserWorkspace();
}
```

**참고**: [07. 메모리 영역 — RAM_D1 Workspace API](../07-memory-management.md#ram_d1-workspace-api)

#### `XM_GetUserWorkspaceSize()`

```c
uint32_t XM_GetUserWorkspaceSize(void);
```

`XM_GetUserWorkspace()`가 반환한 블록의 전체 크기를 바이트 단위로 반환합니다. 워크스페이스 안에서 직접 오프셋을 나누어 쓸 때 경계를 벗어나지 않는지 확인하는 용도로 사용합니다.

**파라미터**: 없음 · **반환값**: 워크스페이스 크기 (바이트)

---

### DTCM 사용자 영역

#### `XM_GetUserDTCM()`

```c
void* XM_GetUserDTCM(void);
```

DTCMRAM 안의 사용자 변수 영역 시작 주소를 반환합니다. 480MHz에서 Zero-Wait-State로 접근되고 D-Cache를 거치지 않아 접근 지연이 결정론적이지만, **DMA로는 접근할 수 없습니다.** 개별 변수 몇 개만 필요하다면 `XM_DTCM_VAR` 매크로가 더 간편하며, 이 함수는 LUT처럼 큰 연속 블록이 필요할 때 사용합니다.

**파라미터**: 없음

**반환값**

| 반환값 | 의미 |
|--------|------|
| `void*` | DTCM 연속 메모리 블록의 시작 주소 |

**⚠️ 호출 컨텍스트**: 헤더에 별도 제약이 명시되어 있지 않습니다. `Control_Setup()`/`Control_Loop()` 컨텍스트 기준으로 작성되었습니다. 이 블록에 DMA 버퍼를 두지 마세요 — DMA 컨트롤러가 DTCM 주소 공간에 접근할 수 없습니다.

**참고**: [07. 메모리 영역 — DTCM API](../07-memory-management.md#dtcm-api)

#### `XM_GetUserDTCMSize()`

```c
uint32_t XM_GetUserDTCMSize(void);
```

`XM_GetUserDTCM()`이 반환한 블록의 전체 크기를 바이트 단위로 반환합니다.

**파라미터**: 없음 · **반환값**: DTCM 사용자 영역 크기 (바이트)

---

### Flash User NV (비휘발성 저장소)

Flash Bank 2, Sector 7 (128KB)을 사용하는 비휘발성 저장 영역입니다. 전원이 꺼져도 값이 보존되지만, Flash 특유의 제약(Erase 후에만 Write 가능, 유한한 erase 사이클)이 그대로 적용됩니다.

#### `XM_UserNV_GetSize()`

```c
uint32_t XM_UserNV_GetSize(void);
```

User NV 영역 전체 크기(바이트)를 반환합니다.

**파라미터**: 없음 · **반환값**: User NV 영역 크기 (바이트, 128KB 고정)

#### `XM_UserNV_Read()`

```c
int32_t XM_UserNV_Read(uint32_t offset, void *data, uint32_t size);
```

Flash User NV 영역에서 데이터를 읽어 `data` 버퍼에 채웁니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `offset` | `uint32_t` | User NV 시작부터의 바이트 오프셋 (0 ~ `Size`-1) |
| `data` | `void*` | 읽은 데이터를 저장할 버퍼 |
| `size` | `uint32_t` | 읽을 바이트 수 |

**반환값**

| 반환값 | 의미 |
|--------|------|
| `0` | 성공 |
| `-1` | 파라미터 오류 (`offset + size` > NV 크기) |

**⚠️ 호출 컨텍스트**: 헤더에 별도 제약이 명시되어 있지 않습니다. `Control_Setup()`/`Control_Loop()` 컨텍스트 기준으로 작성되었으며, 보통 부팅 시 `Control_Setup()`에서 1회 호출하는 패턴을 권장합니다.

**참고**: [`XM_UserNV_IsErased()`](#xm_usernv_iserased)로 유효한 데이터가 있는지 먼저 확인하는 것이 안전합니다 — Erase 직후에는 전체가 `0xFF`입니다.

#### `XM_UserNV_Write()`

```c
int32_t XM_UserNV_Write(uint32_t offset, const void *data, uint32_t size);
```

Flash User NV 영역에 데이터를 기록합니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `offset` | `uint32_t` | User NV 시작부터의 바이트 오프셋 (32-byte 정렬 권장) |
| `data` | `const void*` | 기록할 데이터 포인터 |
| `size` | `uint32_t` | 기록할 바이트 수 |

**반환값**

| 반환값 | 의미 |
|--------|------|
| `0` | 성공 |
| `-1` | 파라미터 오류 |
| `-2` | Flash 프로그래밍 실패 |

**⚠️ 호출 컨텍스트**: 헤더에 별도 제약이 명시되어 있지 않습니다. `Control_Setup()`/`Control_Loop()` 컨텍스트 기준으로 작성되었습니다. 다만 아래 두 가지는 헤더에 명시된 제약이므로 반드시 지켜야 합니다.

- Flash는 **Erase 후에만 Write 가능**합니다 (1→0만 가능, 0→1 불가). 새 데이터를 쓰기 전 [`XM_UserNV_Erase()`](#xm_usernv_erase)를 먼저 호출하세요.
- STM32H7 Flash는 32바이트(256-bit) 단위로 기록되며, `offset`이 정렬되지 않으면 자동으로 정렬 후 기록합니다.
- 빈번한 Write는 Flash 수명(최소 10,000 erase 사이클)에 영향을 줍니다. 부팅 시 1회 읽기, 설정 변경/종료 시 1회 쓰기 패턴을 권장합니다.

```c
// Ex.36 방식 — 오프셋을 상수로 정의해두고 구조체 필드별로 기록
#define NV_MAGIC          0xCA5E0A01
#define NV_OFFSET_MAGIC   0
#define NV_OFFSET_WEIGHTS 4

XM_UserNV_Erase();
XM_UserNV_Write(NV_OFFSET_MAGIC, &magic, sizeof(magic));
XM_UserNV_Write(NV_OFFSET_WEIGHTS, &s_nn, sizeof(s_nn));
```

**참고**: [Ex.36 OnDevice Kinesthetic Learning](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/36_OnDevice_Kinesthetic_Learning/)에서 학습된 신경망 가중치를 이 방식으로 저장합니다.

#### `XM_UserNV_Erase()`

```c
int32_t XM_UserNV_Erase(void);
```

Flash User NV 전체 영역(128KB)을 지웁니다 (전체 `0xFF`로 초기화).

**파라미터**: 없음

**반환값**

| 반환값 | 의미 |
|--------|------|
| `0` | 성공 |
| `-2` | Flash Erase 실패 |

**⚠️ 호출 컨텍스트**: 128KB 전체 섹터를 지우는 데 약 1~2초가 걸립니다 — 헤더가 명시적으로 **"실시간 제어 루프에서 호출하지 마세요"**라고 경고합니다. 즉 1kHz `Control_Loop()` 안에서는 절대 호출하지 말고, 버튼 이벤트 처리·부팅 시 초기화처럼 1회성으로 실행되는 경로에서만 사용하세요.

#### `XM_UserNV_IsErased()`

```c
bool XM_UserNV_IsErased(void);
```

Flash User NV 영역 전체가 비어있는 상태(전체 `0xFF`, Erase 직후)인지 확인합니다.

**파라미터**: 없음

**반환값**

| 반환값 | 의미 |
|--------|------|
| `true` | 전체 영역이 `0xFF` (Erase 직후 상태) |
| `false` | 데이터가 존재하거나 부분적으로 기록됨 |

**⚠️ 호출 컨텍스트**: 헤더에 별도 제약이 명시되어 있지 않습니다. `Control_Setup()`/`Control_Loop()` 컨텍스트 기준으로 작성되었습니다. 보통 `XM_UserNV_Read()`로 값을 불러오기 전에, 유효한 데이터가 저장되어 있는지 미리 확인하는 가드로 사용합니다 (또는 읽은 뒤 magic number 필드로 검증하는 방식도 가능).

---

## 타입/매크로

### `XM_RAMFUNC`

```c
#define XM_RAMFUNC  __attribute__((section(".itcm_text")))
```

함수를 ITCMRAM에 배치하여 Zero-Wait-State로 실행되게 하는 함수 어트리뷰트 매크로입니다. Flash에서 직접 실행할 때 발생하는 분기 예측 실패 지연이 없어, ISR이나 제어 루프처럼 실행 시간이 결정론적이어야 하는 함수에 적합합니다. Startup 코드가 Flash → ITCM 복사를 자동으로 처리하므로 사용자가 별도로 복사할 필요는 없습니다.

| 필드 | 설명 |
|------|------|
| 적용 대상 | 함수 선언 앞에 붙이는 어트리뷰트 |
| 배치 영역 | ITCMRAM (`.itcm_text` 섹션) |
| 제약 | ITCM 용량 제한(64KB) — 함수 코드만 배치하고, 큰 배열/버퍼는 RAM_D1 등 다른 영역에 두세요 |

```c
XM_RAMFUNC void MyControlLoop(void) {
    // 이 함수는 ITCMRAM에서 실행됨 (1-cycle fetch @ 480MHz)
}
```

### `XM_DTCM_VAR`

```c
#define XM_DTCM_VAR  __attribute__((section(".dtcm_data")))
```

변수를 DTCMRAM에 배치하여 Zero-Wait-State로 접근되게 하는 변수 어트리뷰트 매크로입니다. D-Cache를 경유하지 않으므로 접근 지연이 결정론적이며, 제어 알고리즘의 상태 변수·Lookup Table처럼 개별 변수 단위로 빠르게 접근해야 할 때 적합합니다.

| 필드 | 설명 |
|------|------|
| 적용 대상 | 변수 선언 앞에 붙이는 어트리뷰트 |
| 배치 영역 | DTCMRAM (`.dtcm_data` 섹션) |
| 제약 | DMA 접근 불가 — DMA로 채워야 하는 버퍼에는 사용하지 마세요 |

```c
XM_DTCM_VAR static float s_pid_state[6];
XM_DTCM_VAR static float s_lookup_table[256];
```

**참고**: 변수 몇 개가 아니라 큰 연속 블록이 필요하다면 [`XM_GetUserDTCM()`](#xm_getuserdtcm)을 사용하세요.

---

## 내부 전용 (호출 금지)

`xm_api_memory.h`에는 [Internal]/System-only로 표시된 함수가 없습니다 — 위에 나열된 11개 함수와 2개 매크로 모두 사용자 코드(`XM_Apps`)에서 호출하도록 공개된 API입니다.

---

## 관련 문서

- [07. 메모리 영역](../07-memory-management.md) — 4개 메모리 영역의 특성 비교, 흔한 실수, 관련 예제
- [Ex.36 OnDevice Kinesthetic Learning](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/36_OnDevice_Kinesthetic_Learning/) — Flash User NV API의 실제 사용 예
