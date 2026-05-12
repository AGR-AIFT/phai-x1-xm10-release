# API 07: Memory Management

> 📌 **이 페이지를 읽고 나면**: XM10 의 4개 메모리 영역 (RAM_D1 / PSRAM / DTCM / Flash NV) 을 용도별로 골라 쓸 수 있습니다.
> ⏱️ 예상 학습 시간: 15분
> 🧰 사전 지식: [Ex.19 Memory Aware Design](../../examples/19_Memory_Aware_Design/) 의 정적 자료구조 패턴
> 🎯 핵심: `XM_RAMFUNC` / `XM_DTCM_VAR` 매크로 + `XM_UserNV_Read/Write/Erase` Flash API

> **헤더 파일**: `xm_api_memory.h`

XM10의 다양한 메모리 영역에 접근하기 위한 API입니다.

---

## 메모리 영역 개요

| 영역 | 특성 | 속도 | 용도 |
|------|------|------|------|
| **RAM_D1 Workspace** | Cacheable, 휘발성 | 고속 | 알고리즘 변수, 센서 버퍼 |
| **PSRAM** | Write-Through, 휘발성 | 중속 (~30MB/s) | AI/ML 가중치, 대용량 LUT |
| **DTCM** | Zero-Wait-State, DMA 불가 | 최고속 | 실시간 제어 변수 |
| **Flash NV** | 비휘발성, 쓰기 느림 | 저속 | 설정값, 캘리브레이션 |

---

## Section Placement 매크로

### `XM_RAMFUNC`

함수를 ITCMRAM에 배치하여 Zero-Wait-State로 실행합니다.

```c
XM_RAMFUNC void MyControlLoop(void) {
    // 이 함수는 ITCMRAM에서 실행됨 (1-cycle fetch @ 480MHz)
}
```

> ISR, 제어 루프 등 분기 예측 실패 시 지연 없이 실행해야 하는 함수에 적합합니다.

### `XM_DTCM_VAR`

변수를 DTCMRAM에 배치하여 Zero-Wait-State로 접근합니다.

```c
XM_DTCM_VAR static float s_pid_state[6];
XM_DTCM_VAR static float s_lookup_table[256];
```

> D-Cache를 경유하지 않아 결정론적 접근이 보장됩니다. DMA 접근은 불가합니다.

---

## RAM_D1 Workspace API

### `XM_GetUserWorkspace()`

```c
void* XM_GetUserWorkspace(void);
```

RAM_D1 사용자 워크스페이스의 시작 주소를 반환합니다.

- Cacheable (D-Cache 경유)
- 전원 차단 시 소멸
- 알고리즘 변수, 센서 데이터 배열, 연산 버퍼 등 범용 목적

### `XM_GetUserWorkspaceSize()`

```c
uint32_t XM_GetUserWorkspaceSize(void);
```

사용자 워크스페이스 크기(바이트)를 반환합니다.

---

## PSRAM API

### `XM_GetUserPSRAM()`

```c
void* XM_GetUserPSRAM(void);
```

PSRAM 사용자 영역의 시작 주소(0x90700000)를 반환합니다.

- Write-Through Cacheable
- QSPI Memory-Mapped 초기화 후 사용 가능
- AI/ML 모델 가중치, 대용량 Lookup Table에 적합

> **주의**: `User_Setup()` 이후부터 안전하게 접근 가능합니다. 이전 접근 시 HardFault 발생.

### `XM_GetUserPSRAMSize()`

```c
uint32_t XM_GetUserPSRAMSize(void);
```

PSRAM 사용자 영역 크기(바이트)를 반환합니다.

---

## DTCM API

### `XM_GetUserDTCM()`

```c
void* XM_GetUserDTCM(void);
```

DTCM 사용자 변수 영역의 시작 주소를 반환합니다.

- Zero-Wait-State @ 480MHz, D-Cache 미경유
- DMA 접근 불가
- 큰 연속 블록이 필요한 경우 사용 (개별 변수는 `XM_DTCM_VAR` 매크로가 더 편리)

### `XM_GetUserDTCMSize()`

```c
uint32_t XM_GetUserDTCMSize(void);
```

DTCM 사용자 영역 크기(바이트)를 반환합니다.

---

## Flash User NV (Non-Volatile Storage) API

비휘발성 저장소로, 전원 차단 후에도 데이터가 보존됩니다.

> **Flash Bank 2, Sector 7 (128KB)** 사용. 최소 10,000 erase 사이클 보장.

### `XM_UserNV_GetSize()`

```c
uint32_t XM_UserNV_GetSize(void);
```

User NV 영역 크기(바이트)를 반환합니다.

### `XM_UserNV_Read()`

```c
int32_t XM_UserNV_Read(uint32_t offset, void *data, uint32_t size);
```

Flash User NV에서 데이터를 읽습니다.

| 파라미터 | 설명 |
|----------|------|
| `offset` | User NV 시작부터의 바이트 오프셋 (0 ~ Size-1) |
| `data` | 읽은 데이터를 저장할 버퍼 |
| `size` | 읽을 바이트 수 |

| 반환값 | 의미 |
|--------|------|
| `0` | 성공 |
| `-1` | 파라미터 오류 (offset + size > NV 크기) |

### `XM_UserNV_Write()`

```c
int32_t XM_UserNV_Write(uint32_t offset, const void *data, uint32_t size);
```

Flash User NV에 데이터를 기록합니다.

| 파라미터 | 설명 |
|----------|------|
| `offset` | User NV 시작부터의 바이트 오프셋 (32-byte 정렬 권장) |
| `data` | 기록할 데이터 포인터 |
| `size` | 기록할 바이트 수 |

| 반환값 | 의미 |
|--------|------|
| `0` | 성공 |
| `-1` | 파라미터 오류 |
| `-2` | Flash 프로그래밍 실패 |

> **주의**: Flash는 Erase 후에만 Write 가능 (1->0만 가능). 새 데이터를 쓰려면 먼저 `XM_UserNV_Erase()`를 호출하세요.
> STM32H7 Flash는 32바이트(256-bit) 단위로 기록됩니다.
> 빈번한 Write는 Flash 수명에 영향을 줍니다. 부팅 시 1회 읽기, 종료/설정변경 시 1회 쓰기를 권장합니다.

### `XM_UserNV_Erase()`

```c
int32_t XM_UserNV_Erase(void);
```

Flash User NV 전체 영역을 지웁니다 (0xFF로 초기화).

| 반환값 | 의미 |
|--------|------|
| `0` | 성공 |
| `-2` | Flash Erase 실패 |

> 128KB 전체 섹터가 지워집니다. 약 1~2초 소요. **실시간 제어 루프에서 호출하지 마세요.**

### `XM_UserNV_IsErased()`

```c
bool XM_UserNV_IsErased(void);
```

Flash User NV 영역이 비어있는지(Erased, 전체 0xFF) 확인합니다.

---

## 사용 예시

```c
// 설정 구조체 정의
typedef struct {
    float kp_gain;
    float kd_gain;
    uint32_t magic;  // 유효성 검사용
} UserSettings_t;

#define SETTINGS_MAGIC 0xCAFEBEEF

void LoadSettings(UserSettings_t* s) {
    XM_UserNV_Read(0, s, sizeof(UserSettings_t));
    if (s->magic != SETTINGS_MAGIC) {
        // 기본값 사용
        s->kp_gain = 4.8f;
        s->kd_gain = 0.6f;
        s->magic = SETTINGS_MAGIC;
    }
}

void SaveSettings(const UserSettings_t* s) {
    XM_UserNV_Erase();  // 전체 섹터 지우기 (필수)
    XM_UserNV_Write(0, s, sizeof(UserSettings_t));
}
```

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| `XM_GetUserPSRAM()` 결과 접근 시 HardFault | `User_Setup()` 이전 (QSPI Memory-Mapped 초기화 전) 접근 | PSRAM 사용 코드는 `User_Setup` 이후로만 |
| `XM_UserNV_Write` 가 `-2` 반환 | Erase 안 한 상태에서 Write (Flash 는 1→0 만 가능) | 먼저 `XM_UserNV_Erase()` 호출 |
| Flash 가 빠르게 마모됨 | 매 cycle 또는 매 초마다 Write | 부팅 시 1회 Read + 종료/설정 변경 시 1회 Write 패턴 |
| DTCM 변수에 DMA 가 동작 안 함 | DTCM 은 CPU 전용, DMA 접근 불가 | RAM_D1 또는 SRAM 으로 변경 |
| `XM_DTCM_VAR` 변수가 0 이 아닌 garbage | DTCM `.bss` 가 zero-init 되지 않는 케이스 | 명시적 `= 0` 초기화 또는 `User_Setup` 에서 `memset` |
| `XM_RAMFUNC` 함수에서 큰 배열 선언 | ITCM 은 크기 제한 (64 KB) | 큰 데이터는 RAM_D1, 함수만 ITCM |
| PSRAM 에 AI 가중치 쓰고 결과가 이상 | Write-Through cache 미일치 (PSRAM 은 캐시됨) | DMA 사용 시 D-Cache Clean/Invalidate 필요 |
| `XM_UserNV_Read` 가 모두 0xFF | Erase 만 하고 Write 안 함 (Erased = 0xFF) | `XM_UserNV_IsErased()` 로 사전 확인 + 기본값 로드 |
| Settings 구조체에 magic 필드 없이 저장 | 초기 부팅 시 garbage 값을 valid 로 오인 | `magic = 0xCAFEBEEF` 같은 sentinel 필드로 valid 확인 |

---

## 관련 예제

| 예제 | 난이도 | 메모리 활용 |
|------|--------|------------|
| [16_TinyAI_Sensor_Fusion](../../examples/16_TinyAI_Sensor_Fusion/) | 고급 | NN 가중치 (작은 모델은 .rodata, 큰 모델은 PSRAM) |
| [19_Memory_Aware_Design](../../examples/19_Memory_Aware_Design/) | 고급 | Ring Buffer + Pool Allocator (.bss 정적) |
| [10c_MSC_Advanced_Log](../../examples/10c_MSC_Advanced_Log/) | 고급 | 사용자 설정 영속화 (Flash NV) 패턴 |
