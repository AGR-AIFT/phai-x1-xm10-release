# eXtension Module SDK v2.0.0

## 🚀 Major Release (v2.0.0)

v1.0.1 이후 **아키텍처 전면 재설계**, 새로운 통신 프로토콜 도입, IOIF 서브모듈 V3.0 전환 등 대규모 변경이 포함된 메이저 릴리즈입니다.

> ⚠️ **Breaking Changes**: v1.x와 호환되지 않는 API/구조 변경이 다수 포함되어 있습니다. 기존 프로젝트를 업그레이드하는 경우 아래 마이그레이션 가이드를 반드시 확인하세요.

---

### 🏗️ 아키텍처 변경 (Architecture Overhaul)

| 영역 | v1.0.x | v2.0.0 |
|------|--------|--------|
| 디바이스 통신 | `System/Links/*` (Link 레이어) | **`Services/AGR_DOP`** + **`Services/AGR_PnP`** |
| IOIF | 인라인 소스 코드 (Legacy) | **IOIF V3.0** (Git Submodule, 조건부 컴파일) |
| XM API | 단일 파일 (`xm_api.c/h`) | **6개 도메인별 모듈** 분리 |
| PnP 관리 | `System/Links/PnP_Manager` | **`System/Core/pnp_task.c`** + **`Services/AGR_PnP`** |

**핵심 변경:**
- `System/Links/` 레이어 **완전 제거** — 모든 디바이스 통신이 AGR DOP(Data Object Protocol) V2 + PnP(Plug & Play) V2 기반으로 전환
- Link 파일들(`cm_xm_link`, `grf_xm_link`, `xsens_imu_xm_link`, `pnp_manager`) 삭제 → 디바이스 드라이버와 서비스 레이어로 로직 합병
- `can_bus_monitor` 디버그 모듈 제거 (DOP/PnP 프로토콜이 내장 진단 기능 대체)

---

### 🔌 새로운 통신 프로토콜

#### AGR DOP V2 (Data Object Protocol)
- CANopen 기반 Object Dictionary 관리
- SDO/PDO 자동 처리
- NMT(Network Management) 상태 머신 내장
- 파일: `agr_dop.c/h`, `agr_nmt.c/h`, `agr_dop_types.h`, `agr_dop_config.h`, `agr_dop_node_id.h`

#### AGR PnP V2 (Plug & Play)
- Master/Slave 역할 분리 아키텍처
- Heartbeat 기반 자동 감지 및 복구
- NMT 명령 통합
- 파일: `agr_pnp_master.c/h`, `agr_pnp_slave.c/h`, `agr_pnp_types.h`

---

### 🧩 IOIF V3.0 (I/O Interface Layer)

**인라인 IOIF 코드 전체를 Git Submodule 기반 V3.0으로 교체.**

| 변경 | 내용 |
|------|------|
| **조건부 컴파일** | `AGRB_IOIF_XXX_ENABLE` 매크로로 모듈별 활성화/비활성화 |
| **신규 페리페럴** | DMA Pool Manager, DWT Profiler, I2C, SPI, SAI Audio, PSRAM/QSPI |
| **기존 모듈 재설계** | ADC (DMA Circular), FDCAN (내부 RxTask + 콜백 등록), UART (StreamBuffer + 공유 RxTask), GPIO (EXTI, Analog Mode), Timer (PWM Trigger), USB (CDC/MSC 통합), FileSystem (FatFs 래퍼) |
| **Legacy 코드 제거** | `IOIF/Legacy/` 디렉토리 전체 삭제 |

---

### 🎮 새로운 디바이스 드라이버

#### IMU Hub Module (`imu_hub_drv.c/h`)
- AGR IMU Hub 모듈 CAN-FD 드라이버
- CANopen 기반 Pre-Op State Machine
- PDO/SDO 자동 처리
- DOP V2 프로토콜 연동

---

### 📡 USB 스택 강화

| 기능 | 설명 |
|------|------|
| **PhAI Studio 통합** | `phai_packet_builder.c/h` — PhAI V2 바이너리 프로토콜 지원 |
| **ISR-to-Task 패턴** | USB/CANFD/UART 인터럽트를 RTOS Task로 디퍼드 처리 → 안정성 향상 |
| **MSC 편의 기능** | Auto-Timestamp, Rolling Size, Log Status 조회 |
| **CDC 리팩토링** | 자동 스트리밍 모드, Module ID 설정, 디버그 메시지 분리 |

---

### 🖥️ XM API V2.0 (모듈 분리)

기존 단일 `xm_api.c/h` 파일이 6개 도메인별 모듈로 분리되었습니다:

| 모듈 | 파일 | 역할 |
|------|------|------|
| **Core** | `xm_api.c/h` | 통합 Entry Point, `XM_GetTick()` |
| **Data & Control** | `xm_api_data.c/h` | H10 로봇 데이터, PIF-Vector, 토크 제어 |
| **TSM** | `xm_api_tsm.c/h` | Task State Machine (Entry→Loop→Exit) |
| **LED & Button** | `xm_api_led_btn.c/h` | 내장 LED/Button UI 제어 |
| **External IO** | `xm_api_external_io.c/h` | 외부 확장 GPIO/ADC 제어 |
| **USB** | `xm_api_usb.c/h` | MSC 로깅 + CDC 스트리밍 |

#### 신규 API 함수

**Data & Control:**
- `XM_GetXMNmtState()` — XM NMT 상태 조회
- `XM_SetAssistTorqueRH()` / `XM_SetAssistTorqueLH()` — 좌/우 독립 토크 설정
- `XM_SetDegreeLimitRoutine()`, `XM_SetVelocityLimitRoutine()` — 루틴 기반 제한 설정

**External IO:**
- `XM_SwitchDioToAdc()` / `XM_SwitchAllDioToAdc()` — DIO ↔ ADC 핀 모드 동적 전환
- `XM_IsDioSwitchedToAdc()` — 핀 모드 상태 조회
- `XM_AnalogReadMillivolts()` — 밀리볼트 단위 ADC 읽기
- `XM_SetAnalogReadResolution()` / `XM_GetAnalogReadResolution()` — ADC 해상도 설정
- `XM_EnableExternalImu()` — 외부 IMU 활성화

**USB:**
- `XM_GetUsbLogStatus()` — 로그 상태 조회 (enum)
- `XM_SetUsbLogAutoTimestamp()` — 타임스탬프 자동 삽입
- `XM_SetUsbLogRollingSize()` — 파일 롤링 크기 설정
- `XM_IsUsbStreamingActive()` — 스트리밍 활성 상태 조회
- `XM_SetUsbAutoStream()` — 자동 스트리밍 모드
- `XM_SetUsbStreamModuleId()` — PhAI 모듈 ID 설정
- `XM_USB_ProcessPeriodic()` — 주기적 USB 처리

---

### 📚 예제 추가

**신규 External IO 예제 (4종):**
- `05a_Ext_IO_DIO_to_ADC` — DIO 핀을 ADC로 동적 전환
- `05b_Ext_IO_FSR_8ch` — FSR 센서 8채널 동시 읽기
- `05c_Ext_IO_Mixed_ADC` — 혼합 ADC 구성
- `05d_Ext_IO_DIO_ADC_Hybrid` — DIO/ADC 하이브리드 모드

**신규 USB MSC 예제 (3종):**
- `10a_MSC_Basic_Log` — 등록 기반 자동 로깅 기초
- `10b_MSC_Custom_Struct` — 커스텀 구조체 로깅
- `10c_MSC_Advanced_Log` — 롤링, 타임스탬프, 상태 관리

---

### 🔧 기타 변경사항

- **FDCAN V2.0 콜백 패턴**: 수신 핸들러가 콜백 등록 방식으로 전환
- **UART ISR 최소화**: StreamBuffer 기반으로 ISR 내 처리 최소화
- **파일명 수정**: `button_mananger.c` → `button_manager.c` (오타 수정)
- **Data Object Dictionary 업데이트**: `data_object_dictionaries.h` 확장

---

### ⚠️ 마이그레이션 가이드 (v1.x → v2.0)

1. **STM32CubeIDE 업그레이드**: **v2.0.0** 이상 필요 (기존 v1.14.x 불가)
2. **Link 레이어 코드 제거**: `System/Links/` 관련 `#include` 및 호출 제거
3. **IOIF 매크로 추가**: `ioif_conf.h`에서 `AGRB_IOIF_XXX_ENABLE` 매크로 설정
4. **XM API 헤더 변경**: 기존 `#include "xm_api.h"`는 그대로 동작 (하위 모듈 자동 포함)
5. **libXM_Lib.a 교체**: 새 라이브러리 파일로 교체 필수

---

### 📦 Assets

| 파일 | 설명 |
|------|------|
| `XM10_SDK.zip` | 전체 SDK 프로젝트 (소스코드 + 라이브러리) |
| `libXM_Lib.a` | 정적 라이브러리 (Cortex-M7, FPv5-D16, Hard Float) |

---

### 📋 전체 변경 통계

- **변경 파일**: 99개
- **추가**: +14,368줄
- **삭제**: -12,296줄
- **신규 모듈**: AGR_DOP, AGR_PnP, IMU Hub, IOIF V3.0 (DMA, DWT, I2C, SPI, SAI, PSRAM)
- **제거된 모듈**: System/Links (전체), IOIF Legacy (전체), can_bus_monitor
