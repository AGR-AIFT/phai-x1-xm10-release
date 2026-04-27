# IOIF Common Library 통합 보고서

**작성자**: HyundoKim  
**날짜**: 2026-02-11  
**버전**: 1.0  
**적용 프로젝트**: XM10 (STM32H7), IMU Hub (STM32G4)  
**저장소**: `https://github.com/AGR-EXO/IOIF.git` (Branch: `Develop`)

---

## 목차

1. [통합 개요 및 배경](#1-통합-개요-및-배경)
2. [통합 과정에서 변경된 사항](#2-통합-과정에서-변경된-사항)
3. [페리페럴 아키텍처 철학](#3-페리페럴-아키텍처-철학)
4. [모듈별 데이터 처리 패턴](#4-모듈별-데이터-처리-패턴)
5. [Cursorrules 반영 현황](#5-cursorrules-반영-현황)

---

## 1. 통합 개요 및 배경

### 1.1 통합 이전 상태

IOIF (I/O Interface) Layer는 세 곳에서 **독립적으로** 발전하고 있었습니다.

| 출처 | MCU | 특징 | 한계 |
|------|-----|------|------|
| **XM 프로젝트** | STM32H7 (FreeRTOS) | FDCAN, UART, ADC, GPIO, TIM, DWT, USB, FileSystem 등 풀세트 구현. 실제 로봇 제품에서 운용 중. | H7 전용 코드가 하드코딩. G4에서 빌드 불가. |
| **IMU Hub 프로젝트** | STM32G4 (BareMetal) | FDCAN, UART, GPIO, TIM, DWT 구현. XM IOIF를 복사하여 G4용으로 수정. | XM과 코드 분기 발생. 변경사항 수동 동기화 필요. |
| **aeat_9955 프로젝트** | STM32H7/G4 | SPI, I2C, DMA Pool Manager 등 새로운 페리페럴 모듈의 이상적인 패턴을 연구/설계. | 실제 제품에 미적용. 설계 문서 수준. |

### 1.2 통합의 목적

1. **단일 소스 원칙 (Single Source of Truth)**: 하나의 IOIF 코드베이스가 XM, IMU Hub, 향후 모든 모듈에서 동작
2. **Git Submodule로 관리**: IOIF 변경 시 모든 프로젝트에 일관되게 반영
3. **H7/G4 + RTOS/BareMetal 통합**: 조건부 컴파일로 단일 파일에서 모든 환경 지원
4. **확장성**: SPI, I2C 등 새 페리페럴을 추가해도 기존 프로젝트에 영향 없음

### 1.3 통합 원칙

> **"기존에 동작하던 XM과 IMU Hub의 기능을 절대로 깨뜨리지 않는다."**

이 원칙에 따라 다음 전략을 채택했습니다:

- XM에서 실제 운용 중인 FDCAN, UART, ADC, GPIO, TIM, DWT, USB, FileSystem 코드를 **기준(Baseline)**으로 삼음
- IMU Hub에서 검증된 G4 적응 사항을 **조건부 컴파일(`#if defined(IOIF_MCU_SERIES_H7/G4)`)로 병합**
- aeat_9955의 설계 철학(DMA Pool Manager, SPI Resource Pool, I2C 3-Semaphore 등)을 **신규 모듈(Stub)**로 추가
- 신규 모듈은 `AGRB_IOIF_XXX_ENABLE` guard로 보호하여 **기존 프로젝트 빌드에 영향 없음**

---

## 2. 통합 과정에서 변경된 사항

### 2.1 ioif_conf.h 패턴 도입 (핵심 설계 변경)

#### 문제

IOIF를 Git Submodule로 분리하면, 프로젝트별로 사용하는 모듈이 다른데 `#define AGRB_IOIF_XXX_ENABLE`을 어디서 정의할 것인가?

- Submodule 내부의 `ioif_agrb_defs.h`에 직접 정의 → **금지** (Submodule이 프로젝트 특화됨)
- `CMakeLists.txt`의 `target_compile_definitions` → STM32CubeIDE 네이티브 빌드에서 **미적용**

#### 해결: `ioif_conf.h` 패턴 (STM32 HAL의 `stm32xx_hal_conf.h`와 동일)

```
[IOIF Submodule]                         [각 Product Project]
ioif_agrb_defs.h                         System/Config/ioif_conf.h
  └─ #include "ioif_conf.h" ──────────→  #define AGRB_IOIF_FDCAN_ENABLE
                                          #define AGRB_IOIF_UART_ENABLE
                                          #define AGRB_IOIF_USB_ENABLE  ← XM만
                                          ...
```

- `ioif_agrb_defs.h` (Submodule)에서 무조건 `#include "ioif_conf.h"` 호출
- 각 프로젝트가 자신의 include 경로에 `ioif_conf.h`를 배치
- **빌드 시스템(CMake/STM32CubeIDE)에 무관하게 동작**
- 새 모듈이 추가되어도 `ioif_conf.h`에 해당 ENABLE을 정의하지 않으면 **빈 번역 단위**로 컴파일

#### 프로젝트별 설정

| 모듈 | XM (H7, RTOS) | IMU Hub (G4, BareMetal) |
|------|:---:|:---:|
| FDCAN | ✅ | ✅ |
| UART | ✅ | ✅ |
| GPIO | ✅ | ✅ |
| TIM | ✅ | ✅ |
| DWT | ✅ | ✅ |
| USB | ✅ | - |
| ADC | ✅ | - |
| FileSystem | ✅ | - |
| SPI | - (향후) | - (향후) |
| I2C | - (향후) | - (향후) |

### 2.2 USB/FileSystem 모듈 복원

#### 문제

IOIF Common Library 생성 시 `ioif_agrb_usb.h/c`와 `ioif_agrb_fs.h/c`가 **Stub(뼈대)**로 교체되었습니다.
XM의 `data_logger.c`와 `usb_mode_handler.c`는 원본 IOIF의 완전한 API를 사용하고 있어서 빌드 오류 발생:

- `AGRBFileSystem` 구조체 미정의 (함수 포인터 기반 OOP 패턴)
- `IOIF_FILEx_t`, `IOIF_FileSystem_OK` 등 타입/enum 미정의
- `FRESULT`, `FR_OK` (FatFs 타입) 미정의
- `HOST_USER_CLASS_ACTIVE` 등 USB Host 상수 미정의 (`usbh_core.h` include 누락)
- `ioif_usb_device_init` 함수 시그니처 불일치 (1인자 → 2인자)

#### 해결

XM 프로젝트의 **최종 작동 커밋(32c15f1)**에서 4개 파일을 `git checkout`으로 정확히 복원:

| 파일 | 복원 내용 |
|------|----------|
| `ioif_agrb_fs.h` | `AGRBFileSystem` 구조체, `IOIF_FILEx_t`, `AGRBFileSystemStatusDef` enum, FatFs `#include "ff.h"` |
| `ioif_agrb_fs.c` | Thread-Safe FATFS 래핑 (Mutex 보호), USB Host 마운트 처리, 파일 I/O 전체 구현 |
| `ioif_agrb_usb.h` | `#include "usbh_core.h"`, `ioif_usb_device_init(tx_callback, rx_callback)` 2인자 시그니처 |
| `ioif_agrb_usb.c` | USB Host/Device 모드 전환, CDC Tx/Rx 콜백 관리 전체 구현 |

**핵심**: 이 4개 파일은 XM에서 동작하던 코드와 **100% 동일**합니다. 모두 `#if defined(AGRB_IOIF_USB_ENABLE)` / `#if defined(AGRB_IOIF_FILESYSTEM_ENABLE)` guard 내에 있으므로 IMU Hub에서는 빈 파일로 컴파일됩니다.

### 2.3 H7/G4 DMA 호환성 수정

#### 문제

IOIF UART 모듈의 `ManualInitConfig` 구조체와 DMA 초기화 코드가 **H7 전용**이었습니다.

| H7 | G4 |
|----|----|
| `DMA_Stream_TypeDef*` | `DMA_Channel_TypeDef*` |
| `DMA2_Stream0`, `DMA2_Stream0_IRQn` | `DMA1_Channel1`, `DMA1_Channel1_IRQn` |
| DMA FIFO Mode 지원 | DMA FIFO 없음 |

#### 해결

`ioif_agrb_uart.h` 구조체에 MCU 조건부 컴파일 적용:

```c
/* DMA RX 설정 */
bool enable_dma_rx;
#if defined(IOIF_MCU_SERIES_H7)
    DMA_Stream_TypeDef* dma_rx_stream;   /* H7: DMA Stream */
#elif defined(IOIF_MCU_SERIES_G4)
    DMA_Channel_TypeDef* dma_rx_channel; /* G4: DMA Channel */
#endif
```

`ioif_agrb_uart.c`의 DMA 초기화 및 NVIC 설정에도 동일한 분기 적용:
- H7: `DMA_Stream_TypeDef` 기반 초기화 + FIFO 설정
- G4: `DMA_Channel_TypeDef` 기반 초기화, CubeMX가 NVIC 자동 설정

### 2.4 전처리기 버그 수정

`ioif_agrb_tim.c`에서 `#if USE_BAREMETAL`이 `#if defined(USE_BAREMETAL)`로 수정되었습니다.

- `USE_BAREMETAL`이 정의되지 않은 환경에서 `#if USE_BAREMETAL`은 빈 표현식 오류 발생
- `#if defined(USE_BAREMETAL)`은 매크로 존재 여부만 확인하므로 안전

### 2.5 F4 MCU 지원 제거

Angel Robotics에서 STM32F4를 사용하지 않으므로, 11개 파일에서 모든 F4 참조를 제거:

- `ioif_agrb_defs.h`: `STM32F407xx` 감지 블록 제거
- 모든 페리페럴 헤더: `#elif defined(IOIF_MCU_SERIES_F4)` include 블록 제거
- `ioif_agrb_uart.h/c`, `ioif_agrb_dma.c`: `IOIF_MCU_SERIES_F4` 조건부 코드 제거

결과: IOIF는 **H7과 G4만 지원**. 미지원 MCU 빌드 시 `#error "Unsupported MCU"` 발생.

### 2.6 레거시 Debug/Diagnostics 코드 제거

| 제거 대상 | 위치 | 이유 |
|----------|------|------|
| `can_bus_monitor.h/c` | XM `System/Debug/`, IMU Hub `System/Debug/` | IOIF Submodule에 프로젝트 전용 코드 참조 금지. 향후 전문적 디버깅 시스템으로 재설계 예정. |
| `system_diagnostics.h/c` | IMU Hub `System/Diagnostics/` | 동일 |
| `system_dop_benchmark.h/c` | IMU Hub `System/Diagnostics/` | 동일 |

`ioif_agrb_fdcan.c` 내부의 `CanBusMon_RecordBusOff()` 등 레거시 호출도 TODO 주석으로 대체:
```c
/* TODO: Bus Off 이벤트 로깅 (향후 Debug 시스템에서 처리) */
```

### 2.7 USE_FREERTOS 매크로 통일

기존에 `USE_FREERTOS_DMA`라는 레거시 매크로가 일부 코드에서 사용되었습니다.
`USE_FREERTOS`로 통일했습니다.

- `ioif_agrb_defs.h`가 FreeRTOS 환경을 자동 감지하여 `USE_FREERTOS` 또는 `USE_BAREMETAL` 정의
- 모든 IOIF 모듈은 `#if defined(USE_FREERTOS)` / `#if defined(USE_BAREMETAL)` 사용

### 2.8 신규 Stub 모듈 추가

aeat_9955 설계를 기반으로 IOIF Submodule에 신규 모듈 파일이 추가되었습니다.
모두 `AGRB_IOIF_XXX_ENABLE` guard로 보호되며, 현재 XM/IMU Hub에서는 비활성:

| 모듈 | 파일 | 상태 | 설계 패턴 |
|------|------|------|----------|
| SPI | `ioif_agrb_spi.h/c` | Stub (API 정의 완료) | Resource Pool + Instance 2계층 |
| I2C | `ioif_agrb_i2c.h/c` | Stub (API 정의 완료) | 3-Semaphore (Device/Tx/Rx) |
| DMA Pool | `ioif_agrb_dma.h/c` | Stub | 중앙 DMA 버퍼 관리 |
| SAI | `ioif_agrb_sai.h/c` | Stub | H7 전용 오디오 |
| PSRAM | `ioif_agrb_psram.h/c` | Stub | H7 전용 외부 메모리 |

### 2.9 Git Submodule 브랜치 전략

| 항목 | 설정 |
|------|------|
| 기본 브랜치 | `Develop` |
| `main` 브랜치 | 삭제 (Develop만 사용) |
| XM `.gitmodules` | `branch = Develop` |
| IMU Hub `.gitmodules` | `branch = Develop` |

#### Submodule 워크플로우

```
[IOIF 수정 시 - 2-Step Commit]
1. IOIF Submodule 내에서 commit & push → AGR-EXO/IOIF Develop에 반영
2. 상위 프로젝트(XM/IMU Hub)에서 Submodule 참조 commit

[다른 프로젝트에서 최신 IOIF 가져오기]
git submodule update --remote <IOIF_PATH>
git add <IOIF_PATH> && git commit -m "chore: update IOIF submodule"
```

---

## 3. 페리페럴 아키텍처 철학

### 3.1 핵심 원칙

> **"하드웨어의 능력을 최대한 활용하되, CPU 개입을 최소화한다."**

로봇 시스템은 1ms 제어 루프 내에서 다수의 센서 데이터를 수집하고 통신해야 합니다.
각 페리페럴의 **데이터 특성**에 맞는 최적의 하드웨어 메커니즘을 선택합니다.

### 3.2 페리페럴 분류 체계

모든 페리페럴을 **4가지 Category**로 분류하고, Category에 따라 데이터 처리 패턴을 결정합니다.

| Category | 특성 | 대표 모듈 | 핵심 패턴 |
|----------|------|----------|----------|
| **A: Async Receiver** | 외부에서 예측 불가 시점에 데이터 도착 | FDCAN Rx, UART Rx, USB-CDC Rx | ISR → Queue/Semaphore → Task |
| **B: Sync Master** | 내가 원할 때 데이터를 요청/전송 | SPI, I2C | Task → Mutex + DMA + Semaphore(완료 대기) |
| **C: Continuous Stream** | 연속적으로 데이터를 생산/소비 | ADC, USB-MSC Write | DMA Circular + Atomic Read / Ring Buffer |
| **D: Passive Utility** | 이벤트/설정 위주 | GPIO, TIM, DWT | 직접 레지스터 접근 / HAL 호출 |

### 3.3 왜 이 분류인가?

**Category A (FDCAN, UART, USB-CDC Rx)** — 비동기 수신:
- 데이터가 **언제 올지 모름** → ISR에서 최소한의 처리 후 Task로 위임
- ISR 체류 시간 최소화가 핵심 (< 1µs 목표)
- RTOS: Semaphore/Queue, BareMetal: volatile flag + NVIC Priority

**Category B (SPI, I2C)** — 동기 마스터:
- **내가 시작**하므로 Task에서 DMA 전송을 시작하고 완료를 기다림
- 같은 버스에 여러 디바이스 → Mutex로 버스 독점
- DMA 완료 → BinarySemaphore로 Task 깨움 (Polling 대비 CPU 해방)

**Category C (ADC)** — 연속 스트림:
- Timer Trigger → ADC → DMA Circular → 메모리에 자동 기록
- **CPU 개입 제로**: 제어 루프에서 메모리를 읽기만 하면 됨
- 16-bit 읽기는 32-bit MCU에서 Atomic → Mutex도 불필요

**Category D (GPIO, TIM, DWT)** — 패시브 유틸리티:
- 동기화 이슈 거의 없음
- 단순 HAL 래핑 또는 레지스터 직접 접근

### 3.4 DMA & Cache 전략

> **"DMA 버퍼의 Cache Coherency 문제를 MPU 설정으로 원천 차단한다."**

| MCU | DMA 메모리 전략 | D-Cache 연산 |
|-----|----------------|-------------|
| **H7** | MPU Non-cacheable RAM에 `__attribute__((section("...")))` 배치 | **불필요** (Non-cacheable이므로) |
| **G4** | 일반 전역 변수 | **불필요** (D-Cache 없음) |

**왜 `SCB_InvalidateDCache`를 쓰지 않는가?**

H7은 MPU 설정으로 DMA 버퍼 영역을 Non-cacheable로 지정합니다. CPU가 해당 영역에 접근할 때 D-Cache를 거치지 않으므로, DMA가 직접 쓴 데이터를 CPU가 즉시 읽을 수 있습니다. `SCB_InvalidateDCache_by_Addr()`은 불필요하고, 호출하면 오히려 **성능 저하**만 발생합니다.

### 3.5 RTOS/BareMetal 양립 전략

> **"하나의 코드, 두 개의 환경."**

```c
/* 환경 자동 감지 (ioif_agrb_defs.h) */
#if __has_include("FreeRTOSConfig.h")
    #define USE_FREERTOS    /* RTOS 환경 */
#else
    #define USE_BAREMETAL   /* BareMetal 환경 */
#endif

/* 동기화 분기 예시 */
#if defined(USE_FREERTOS)
    xSemaphoreTake(inst->tx_mutex, pdMS_TO_TICKS(100));
#else
    __disable_irq();
#endif
```

| 메커니즘 | RTOS | BareMetal |
|---------|------|-----------|
| ISR → Task 전달 | Semaphore / Queue | volatile flag / NVIC Priority |
| Mutex (공유 자원) | xSemaphoreTake | __disable_irq / NVIC Priority |
| DMA 완료 대기 | BinarySemaphore | Polling + Timeout |
| 주기적 실행 | Task + vTaskDelayUntil | Timer ISR |

### 3.6 프로젝트 독립성

> **"IOIF는 어떤 프로젝트의 이름도 모른다."**

| 허용 | 금지 |
|------|------|
| `stm32h7xx_hal.h` (STM32 HAL) | `module.h` (프로젝트 설정) |
| `cmsis_os2.h` (RTOS 표준) | `cm_drv.h` (Device Layer) |
| `ioif_conf.h` (프로젝트가 제공) | `system_startup.h` (System Layer) |
| `string.h`, `stdint.h` (C 표준) | `xm_api.h` (Application Layer) |

---

## 4. 모듈별 데이터 처리 패턴

### 4.1 FDCAN (Category A) — 확정, XM/IMU 운용 중

```
[수신]
FDCAN HW FIFO → ISR(SemaphoreGive, <0.5µs) → Rx Task(Batch Read) → Device Layer(Mutex+Snapshot)
                                                                      └→ Control Loop(Mutex+Read)

[송신]
Control Task → IOIF_FDCAN_Transmit() → HAL_FDCAN_AddMessageToTxFifoQ()
```

**선택 근거**:
- ISR에서 `HAL_FDCAN_GetRxMessage()` 호출 시 ~3µs 소요 → **Rx Task로 위임**하여 ISR 최소화
- RTOS: Semaphore + Rx Task (Batch Read로 FIFO Level 0까지 처리)
- BareMetal: FDCAN ISR에서 직접 처리 (NVIC Priority로 Timer ISR보다 낮게 설정)

### 4.2 UART (Category A) — 확정, XM/IMU 운용 중

```
[수신]
UART DMA Circular (하드웨어가 자동 수신)
  → IDLE Line Detection ISR (데이터 끝 감지)
  → 수신 구간 계산 (Old Pos ~ New Pos)
  → Parser Task 또는 직접 파싱
  → DataLake(Mutex+Snapshot)
```

**선택 근거**:
- `Circular DMA + IDLE Line Detection`은 **가변 길이 패킷**에 최적
- DMA가 하드웨어적으로 수신 → CPU는 IDLE 이벤트에서만 깨어남
- 수신 빈도: IMU 센서 1kHz × 6채널 → CPU 부하 최소화 필수
- BareMetal: NVIC Priority(Timer > UART), RTOS: Queue/Semaphore

### 4.3 ADC (Category C) — 확정, XM 운용 중

```
[수신]
PWM Timer(1kHz TRGO) → ADC Conversion → DMA Circular → Non-cacheable RAM
                                                         └→ Control Loop: val = s_adc_buffer[ch]
                                                            (16-bit Atomic Read, Mutex 불필요)
```

**선택 근거**:
- Timer Trigger + DMA Circular = **CPU 개입 완전 제로**
- 32-bit MCU에서 16-bit 읽기는 Atomic → 동기화 오버헤드 없음
- H7: Non-cacheable RAM에 DMA 버퍼 배치 → D-Cache 문제 원천 차단

### 4.4 SPI (Category B) — 설계 확정, Stub 구현

```
[Normal Mode - Task 주도]
Sensor_Task → IOIF_SPI_Read(id, tx, rx)
  → Mutex_Acquire(SPI_Bus)        ← 같은 버스의 다른 디바이스 차단
  → CS_Low → DMA_Start
  → [ISR] DMA_Complete → Sem_Give
  → Sem_Take(completion) → CS_High
  → Mutex_Release(SPI_Bus)

[ISR Mode - Timer 주도, 엔코더 등]
Timer_ISR(1kHz) → IOIF_SPI_WriteIsr(id, tx)
  → CS_Low → DMA_Start
  → [ISR] DMA_Complete → CS_High → Queue Put
Consumer_Task → IOIF_SPI_ReadIsr(id, rx) → Queue Get
```

**선택 근거**:
- SPI는 **내가 시작하는 통신** → Task에서 DMA 시작 + Semaphore 완료 대기
- Resource Pool: 같은 SPI 버스에 여러 디바이스 → 버스 단위 Mutex
- ISR Mode: 엔코더처럼 Timer에서 주기적으로 읽어야 할 때 → DMA + Queue

### 4.5 I2C (Category B) — 설계 확정, Stub 구현

```
[읽기]
LowPriority_Task(100ms) → IOIF_I2C_MemRead(id, addr, reg, data, len)
  → Mutex_Acquire(I2C_Instance)
  → HAL_I2C_Mem_Read_DMA
  → [ISR] MemRxCplt → Sem_Give(rx_complete)
  → Sem_Take(rx_complete)
  → Mutex_Release
  → DataLake 업데이트
Control_Task(500Hz) → Mutex + Snapshot(DataLake)
```

**선택 근거**:
- I2C는 **100µs~ms 단위 지연** 발생 가능 → **고우선순위 Task에서 직접 호출 금지**
- 저우선순위 Task에서 주기적으로 읽고, DataLake에 업데이트
- 제어 루프는 DataLake Snapshot만 읽음 → 블로킹 없음
- 3-Semaphore: Device(인스턴스 보호), Tx(송신 완료), Rx(수신 완료)

### 4.6 USB-CDC (Category A+C 혼합) — 확정, XM 운용 중

```
[Rx - Category A]
USB_Stack_ISR → CDC_Receive → IOIF_USB_CDC_ISR_Receive
  → [RTOS] Queue Put → CDC_Rx_Task → Command Parse → DataLake

[Tx - Category C]
Any_Task → IOIF_USB_CDC_Transmit(data, len)
  → Mutex_Acquire(CDC_Tx)
  → CDC_Transmit_FS + Sem_Take(tx_complete)
  → Mutex_Release
```

### 4.7 USB-MSC / DataLogger — 확정, XM 운용 중

```
Control Loop(500Hz, High Priority):
  → DataLogger_Append(&data, size) → atomic_store(head) [Non-Blocking, ~1µs]

DataLogger Task(Low Priority, 100ms):
  → Ring Buffer Batch Read → f_write(USB) [Blocking 허용]
```

**선택 근거**:
- USB f_write의 Garbage Collection으로 **최대 0.5초 멈춤** 가능
- Ring Buffer(280KB, D2 RAM): 150KB/s × 1.86초 버퍼링 → 안전 마진 충분
- 실시간 Task는 atomic head만 업데이트 → **제어 루프에 영향 제로**

---

## 5. Cursorrules 반영 현황

### 5.1 관련 Cursorrule 파일

| 파일 | 내용 | 상태 |
|------|------|------|
| `00-core-architecture.mdc` | 레이어 구조, 의존성 규칙, 프로젝트 감지 | ✅ 반영 완료 |
| `10-fdcan-architecture.mdc` | FDCAN ISR/Task 패턴, RTOS/BareMetal | ✅ 반영 완료 |
| `12-ioif-mcu-guide.mdc` | H7/G4 DMA, Cache, ADC, UART 차이 | ✅ 반영 완료 (F4 참조는 제거 필요) |
| `13-comm-core-patterns.mdc` | 페리페럴 분류 체계, 동기화 전략 | ✅ 반영 완료 |
| `15-comm-protocols.mdc` | UART/SPI/ADC/I2C/USB 프로토콜별 패턴 | ✅ 반영 완료 |
| `20-ioif-submodule.mdc` | Git Submodule, ioif_conf.h, 디렉토리 구조 | ✅ 반영 완료 |

### 5.2 Cursorrule 업데이트 필요 사항

| 항목 | 현재 | 업데이트 필요 |
|------|------|--------------|
| `12-ioif-mcu-guide.mdc` 제목 | "H7/G4/F4 통합 가이드" | **"H7/G4 통합 가이드"** (F4 제거 반영) |
| `12-ioif-mcu-guide.mdc` 내용 | `IOIF_MCU_SERIES_F4` 참조 잔존 | F4 관련 코드 예시 제거 |
| `20-ioif-submodule.mdc` FS 상태 | `ioif_agrb_fs.h (Stub)` | **`ioif_agrb_fs.h (Production)`** (원본 복원됨) |
| `20-ioif-submodule.mdc` USB 상태 | `ioif_agrb_usb.h (Production - CDC)` | 원본 복원 반영 확인 |

---

## 부록: IOIF Submodule 커밋 히스토리 (이번 통합)

| 커밋 | 내용 |
|------|------|
| `09e73cd` | feat: IOIF Common Library v4.0 (초기 통합) |
| `4ec9d9e` | fix: restore USB/FS original impl + H7/G4 DMA compat |
| `4b763d1` | fix: restore USB/FS to latest XM working version (32c15f1) |
| `dcd8cc4` | refactor: remove STM32F4 support, H7/G4 only |
