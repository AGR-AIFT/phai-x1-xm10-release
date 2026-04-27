# IOIF Common Library

Angel Robotics IOIF (I/O Interface) Layer - Git Submodule

## Overview

STM32 MCU의 하드웨어 페리페럴을 추상화하는 공통 라이브러리입니다.
Git Submodule로 사용하여 여러 프로젝트(XM, IMU Hub, CM 등)에서 동일한 IOIF를 공유합니다.

## Architecture

```
Product Project (XM, IMU Hub, ...)
├── System/Config/
│   └── ioif_conf.h           ← 프로젝트별 모듈 Enable/Disable, DMA 풀 설정
└── IOIF/ (Git Submodule)
    ├── Inc/
    │   ├── ioif_agrb_defs.h    ← 핵심: MCU/RTOS 자동 감지, Status Codes
    │   ├── ioif_agrb_dma.h     ← DMA Pool Manager
    │   ├── ioif_agrb_fdcan.h   ← FDCAN (V2.0: 내부 RxTask + 콜백 등록)
    │   ├── ioif_agrb_uart.h    ← UART (V2.0: StreamBuffer + 공유 RxTask)
    │   ├── ioif_agrb_spi.h     ← SPI (Resource Pool + Multi-device)
    │   ├── ioif_agrb_i2c.h     ← I2C (DMA + Semaphore)
    │   ├── ioif_agrb_gpio.h    ← GPIO (EXTI, Analog Mode)
    │   ├── ioif_agrb_tim.h     ← Timer (Base, PWM Trigger)
    │   ├── ioif_agrb_adc.h     ← ADC (DMA Circular, Manual Init)
    │   ├── ioif_agrb_dwt.h     ← DWT (CPU Cycle Profiling)
    │   ├── ioif_agrb_usb.h     ← USB (CDC/MSC)
    │   ├── ioif_agrb_fs.h      ← File System (FatFs)
    │   ├── ioif_agrb_sai.h     ← SAI Audio (H7 only, Stub)
    │   └── ioif_agrb_psram.h   ← PSRAM/QSPI (H7 only, Stub)
    ├── Src/
    │   └── (corresponding .c files)
    └── examples/
        ├── xm_module_example.h
        └── imu_hub_module_example.h
```

## Supported Platforms

| MCU Series | Chip | Usage |
|------------|------|-------|
| STM32H7 | STM32H743xx, STM32H750xx | XM10, CM, MD |
| STM32G4 | STM32G474xx, STM32G431xx | IMU Hub, Sensor Modules |

| Environment | Detection | Features |
|-------------|-----------|----------|
| FreeRTOS | Auto (`__has_include("FreeRTOSConfig.h")`) | Semaphore, Mutex, StreamBuffer, Task |
| BareMetal | Auto (Fallback) | ISR 직접 콜백, Polling |

### Conditional Compilation Macros

| Macro | Source | Description |
|-------|--------|-------------|
| `IOIF_MCU_SERIES_H7` | `ioif_agrb_defs.h` (자동) | STM32H7 계열 자동 감지 |
| `IOIF_MCU_SERIES_G4` | `ioif_agrb_defs.h` (자동) | STM32G4 계열 자동 감지 |
| `USE_FREERTOS` | `ioif_agrb_defs.h` (자동) | FreeRTOS 환경 감지 |
| `AGRB_IOIF_XXX_ENABLE` | `ioif_conf.h` (프로젝트별) | 개별 모듈 활성화 |

## Design Principles

1. **Standalone Functions** - 직접 호출로 최적화 가능 (`IOIF_SPI_Write()`)
2. **ENABLE Pattern** - 기본 비활성, 필요한 모듈만 `ioif_conf.h`에서 활성화
3. **Auto-detect** - MCU Series, RTOS 자동 감지 (`ioif_agrb_defs.h`)
4. **DMA Pool Manager** - 중앙 집중형 DMA 버퍼 관리 (Non-Cacheable RAM)
5. **ISR Minimization** - ISR에서 raw data 전달만, 파싱은 Task 컨텍스트에서
6. **Semaphore Give/Take 동일 소스 원칙** - 세마포어 Give와 Take가 하나의 소스 파일 안에 위치
7. **User Callback Chain** - IOIF 소유 + System Layer 콜백 등록

## V2.0 Changes (Feb 2026)

### UART: ISR Minimization + StreamBuffer

**변경 전 (V1.0)**: ISR에서 파싱 후 Queue로 전달
```
ISR → Packet Parse → xQueueSendFromISR → RxTask → DataLake
```

**변경 후 (V2.0)**: ISR에서 raw bytes를 StreamBuffer로 전달, 파싱은 Task에서 수행
```
ISR → xStreamBufferSendFromISR → 공유 RxTask → 콜백(파싱) → DataLake
```

핵심 변경:
- ISR 실행 시간 최소화 (memcpy + TaskNotify만 수행)
- 단일 공유 `_IOIF_UART_SharedRxTask`가 모든 UART 인스턴스 처리
- Task Notification 비트마스크로 인스턴스 식별
- BareMetal: 기존 ISR 직접 콜백 방식 유지 (변경 없음)

### FDCAN: 내부 RxTask + 콜백 등록

**변경 전 (V1.0)**: System Layer에서 Semaphore 받아서 외부 RxTask 생성
```
ISR → SemaphoreGive ─┐
                      ├─(다른 파일)─ System FDCANRxHandler_Task → SemaphoreTake → Batch Read
IOIF_FDCAN_GetRxSemaphore() ──┘
```

**변경 후 (V2.0)**: IOIF 내부 RxTask, System Layer는 콜백만 등록
```
ISR → SemaphoreGive → (동일 파일) _IOIF_FDCAN_RxTask → SemaphoreTake → Batch Read → 콜백
                                                                         ↓
                                        System: IOIF_FDCAN_RegisterRxCallback(_OnFdcanRxMessage)
```

핵심 변경:
- 세마포어 Give/Take 동일 소스 원칙 적용
- `IOIF_FDCAN_GetRxSemaphore()` 제거 (deprecated)
- System Layer는 `IOIF_FDCAN_RegisterRxCallback()`으로 콜백만 등록
- IOIF RxTask가 HW FIFO Batch Read 후 등록된 콜백 호출
- BareMetal: 기존 ISR 직접 콜백 방식 유지 (변경 없음)

### SPI: Bug Fixes

- **Bug 1**: `ioif_spi_write_isr()`에서 `HAL_SPI_TransmitReceive_DMA()` 호출 시 DMA Pool 버퍼(`tx_dma`) 대신 사용자 버퍼(`tx_buffer`) 전달 → `tx_dma` 사용으로 수정
- **Bug 2**: `HAL_SPI_TxRxCpltCallback` FreeRTOS ISR 경로에서 `SCB_InvalidateDCache_by_Addr()` 주석 처리됨 → 주석 해제

## ioif_conf.h Configuration Guide

각 Product Project의 `System/Config/ioif_conf.h`에서 IOIF 모듈을 선택적으로 활성화합니다.

### XM10 (RTOS + STM32H743) 예시

```c
/* ioif_conf.h - XM10 */

/* 활성화 모듈 */
#define AGRB_IOIF_FDCAN_ENABLE
#define AGRB_IOIF_UART_ENABLE
#define AGRB_IOIF_GPIO_ENABLE
#define AGRB_IOIF_TIM_ENABLE
#define AGRB_IOIF_ADC_ENABLE
#define AGRB_IOIF_DWT_ENABLE
#define AGRB_IOIF_DMA_ENABLE
#define AGRB_IOIF_USB_ENABLE
#define AGRB_IOIF_FILESYSTEM_ENABLE

/* DMA Pool (H7: AXI SRAM) */
#define IOIF_DMA_POOL_SIZE      (56 * 1024)
#define IOIF_BDMA_POOL_SIZE     (1 * 1024)
#define IOIF_MDMA_POOL_SIZE     (4 * 1024)

/* Memory Section (H7 링커 스크립트) */
#define IOIF_DMA_SECTION        ".RAM_D3_data"
#define IOIF_BDMA_SECTION       ".RAM_D3_data"
#define IOIF_USB_CDC_SECTION    ".RAM_D3_data"
```

### IMU Hub (BareMetal + STM32G474) 예시

```c
/* ioif_conf.h - IMU Hub */

/* 활성화 모듈 */
#define AGRB_IOIF_FDCAN_ENABLE
#define AGRB_IOIF_UART_ENABLE
#define AGRB_IOIF_GPIO_ENABLE
#define AGRB_IOIF_TIM_ENABLE
#define AGRB_IOIF_DWT_ENABLE

/* G4: DMA Pool 불필요 (BareMetal, 단일 SRAM) */
/* IOIF_DMA_SECTION 자동으로 빈 문자열 */
```

### Module Enable 매크로 전체 목록

| Macro | Module | Note |
|-------|--------|------|
| `AGRB_IOIF_FDCAN_ENABLE` | FDCAN | 거의 모든 프로젝트에서 사용 |
| `AGRB_IOIF_UART_ENABLE` | UART | 센서 통신 (FSR, IMU) |
| `AGRB_IOIF_GPIO_ENABLE` | GPIO | LED, RS485, 전원 제어 |
| `AGRB_IOIF_TIM_ENABLE` | Timer | RunLoop 트리거, HAL Timebase |
| `AGRB_IOIF_ADC_ENABLE` | ADC | 배터리 전압, External IO |
| `AGRB_IOIF_DWT_ENABLE` | DWT | ISR/Task 성능 프로파일링 |
| `AGRB_IOIF_DMA_ENABLE` | DMA Manager | SPI/I2C 사용 시 필수 (RTOS) |
| `AGRB_IOIF_SPI_ENABLE` | SPI | 외부 센서 직접 연결 |
| `AGRB_IOIF_I2C_ENABLE` | I2C | 외부 센서 직접 연결 |
| `AGRB_IOIF_USB_ENABLE` | USB | CDC 디버그 콘솔, MSC 로그 |
| `AGRB_IOIF_FILESYSTEM_ENABLE` | FileSystem | USB MSC 파일 시스템 |
| `AGRB_IOIF_SAI_ENABLE` | SAI Audio | H7 only (Stub) |
| `AGRB_IOIF_PSRAM_ENABLE` | PSRAM/QSPI | H7 only (Stub) |

## Module Status

| Module | Status | Source | V2.0 Changes |
|--------|--------|--------|--------------|
| FDCAN | Production | XM (HyundoKim) | V2.0: 내부 RxTask, 콜백 등록 패턴 |
| UART | Production | XM (HyundoKim) | V2.0: StreamBuffer + 공유 RxTask |
| GPIO | Production | XM (HyundoKim) | - |
| Timer | Production | XM (HyundoKim) | - |
| ADC | Production | XM (HyundoKim) | - |
| DWT | Production | XM (HyundoKim) | - |
| DMA Manager | Production | aeat_9955 (KimJinwoo) -> Multi-MCU | - |
| SPI | Production | aeat_9955 (KimJinwoo) -> Standalone + Multi-MCU | Bug 1, 2 수정 |
| I2C | Production | aeat_9955 (KimJinwoo) -> Standalone + Multi-MCU | - |
| USB | Production | XM (HyundoKim) | - |
| FileSystem | Production | XM (HyundoKim) | - |
| SAI | Stub (H7 only) | Needs aeat_9955 API adaptation | - |
| PSRAM | Stub (H7 only) | Needs aeat_9955 API adaptation | - |

## Versioning

- v2.0: UART ISR Minimization, FDCAN RxTask Internalization, SPI Bugfix (Feb 2026)
- v1.0: Initial common library (Feb 2026)
- Based on XM IOIF (HyundoKim) + aeat_9955 IOIF (KimJinwoo)
