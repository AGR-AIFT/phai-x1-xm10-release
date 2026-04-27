# Changelog

모든 주요 변경 사항은 이 문서에 기록됩니다. [Semantic Versioning](https://semver.org/)을 따릅니다.

---

## [v2.1.0] — 2026-04-02

### Highlights

* **AGR_BOOT V2 부트로더 최초 도입** — USB CDC FTP를 통한 펌웨어 업데이트, 자동 백업/롤백, CRC-32 검증
* **듀얼 HW 리비전 SDK 동시 배포** — `XM10_SDK/Rev1.1/` + `XM10_SDK/Rev2.0/` 폴더 구조
* **예제 42개** — 입문부터 Physical AI 고급 제어까지 완전한 학습 경로
* **libXM_Lib.a Release 빌드** — `-O2` 최적화로 전환 (이전 Debug `-Og`)

### Added

* **부트로더 지원**
  * `boot_fw_info.c` SDK 직접 컴파일 — `.fw_header` 섹션에 `AGRBOOT` 시그니처 배치 (링커 GC 회피)
  * Post-Build 4단계 자동화: `size_report.py` → `version_generator.py` → `patch_fw_info.py` → `fw_packager.py`
  * 최종 출력: `XM10_X_X_X_X.bin` (PhAI Studio FTP 업로드용 패키징 바이너리)
  * 부트로더 매뉴얼: [docs/bootloader/README.md](docs/bootloader/README.md)
* **Rev2.0 SDK 신규**
  * Ethernet (LwIP + UDP), PSRAM (8MB QSPI), RTC (MCP79510), LED Driver (PCA9957)
  * 신규 XM API: `xm_api_memory.h` (PSRAM/Workspace), `xm_api_rtc.h` (RTC 시간 관리)
  * 신규 디바이스 드라이버: am_drv (Application Module), mcp79510, pca9957, rtl8201f
  * FDCAN ISR-Direct V5.0, DOP Transport/UDP, ETH UDP Socket
* **예제 대규모 확장 (20개 → 42개)**
  * Physical AI 토크 제어 시리즈 (Ex.20~33): Impedance, Gravity Comp, CPG, ILC, MRAC, Admittance, Bilateral, DOB, Kinesthetic Teaching 등
  * Gait Analysis 로깅 (Ex.34): H10 보행 데이터 자동 수집 + Python 디코더
* **Data Map Code-Gen**: `xm_total_data.yaml` → `xm_total_data_packet.h` 자동 생성
  * Total Data Packet v2.5 (365B): FDCAN Ch1/Ch2 독립 진단, `xm_loop_count` 도입
* **PhAI Studio 연동 강화**
  * Total Data Packet (Module ID 0x20) 시스템 자동 전송 (1kHz)
  * User Custom 채널 (0xF0~0xFE): `XM_SetUsbCustomMeta()` + `XM_SendUsbDataWithId()`
  * Auto-Stream 모드 (레거시 "AGRB MON START" 불필요)

### Changed

* **SDK 폴더 구조**: `XM10_SDK/Extension_Module/` → `XM10_SDK/Rev1.1/` + `XM10_SDK/Rev2.0/`
* **libXM_Lib.a 빌드 최적화**: Debug (`-Og -g3`) → **Release (`-O2 -g0`)**
  * Rev1.1: 598KB (59 obj) | Rev2.0: 525KB (66 obj)
* **AGR_MW 서브모듈 최신화**
  * OD Discovery (이름/단위 조회), SDO non-expedited Upload (4B 초과 데이터)
  * `PDO_MAP_MAX_ENTRIES`를 `agr_dop_config.h`로 이동 (재정의 경고 해결)
* **IOIF 서브모듈 최신화**
  * TIM PWM/OC 인터럽트 API, `IOIF_TIM_SetCallback` 런타임 콜백 주입
  * ISR-safe `SetOCMode` / `GenerateUpdate` / `FindByHandle` API
* **CubeIDE .cproject**: `-lXM_Lib` + `-L XM_FW/` 링커 설정 (이전: `--whole-archive`)
* **CMakeLists.txt**: XM_FW 소스 컴파일 → `libXM_Lib.a` 링크 방식으로 전환
* **`ExitRun0Mode()` 추가**: CubeMX 6.13+ startup assembly 호환 (LDO 전원 설정)
* **Include 경로 정리**: `BuffMngr/Inc` 삭제, `Transport/Serial` + `Transport/UDP` 추가, `Xsens` → `XSENS` 대소문자 수정

### Fixed

* **`.fw_header` 섹션 누락 수정**: `boot_fw_info.c`를 `libXM_Lib.a`에서 분리 → SDK 직접 컴파일 (링커 GC가 .a 내부 미참조 섹션 제거하는 문제 해결)
* **예제 A→B→C 3경로 완전 동기화**: 42개 예제 `.c` 파일 내용 일치 확인
* **Rev2.0 XM_Lib 구조 정리**: 소스 복사본 354파일 삭제 (327K줄), `../Extension_Module/` 직접 참조로 전환

### Removed

* `BuffMngr` 모듈 (AGR_MW에서 삭제됨)
* `user_app.c` 루트 복사본 (`XM_Apps/User_Algorithm/`에서만 관리)
* SDK 불필요 스크립트: `cproject_to_cmake.py`, `patch_cubemx_overrides.py`
* Examples A 경로의 README.md 3개 (B 경로에서만 관리 — rule-26)
* Rev2.0 XM_Lib 내 Drivers/FATFS/Middlewares/Core/Compatible/XM_FW 복사본 전부

### Compatibility

| 컴포넌트 | 최소 버전 | 권장 버전 |
|----------|----------|----------|
| AGR_BOOT (부트로더) | v1.1.0 | v1.1.0 |
| KIT H10 CM | v2.3.0 | v2.3.0+ |
| KIT H10 ESP32 | v2.3.0 | v2.3.0+ |
| KIT H10 SAM10/MD | v2.3.0 | v2.3.0+ |
| STM32CubeIDE | v1.13.2 | v1.14.1+ |
| Python | 3.8+ | 3.12+ |
| PhAI Studio | — | 최신 ([studio.onephai.com](https://studio.onephai.com)) |

---

## [v2.0.1] — 2026-03-09

### Fixed

* **libXM_Lib.a 재빌드**: XM_FW 고유 코드(53개)만 포함하도록 수정
  * AS-IS: Core/Drivers/Middlewares/FATFS/Compatible 등 SDK가 소스로 컴파일하는 코드까지 .a에 포함 → 심볼 중복 + 헤더 ABI 불일치로 런타임 크래시 (vPortFree heap corruption)
  * TO-BE: XM_FW 레이어만 포함, SDK 측 소스와 충돌 없음
* **AGR_DOP 리팩토링 구조 반영**: `agr_dop.c` → `Core/` + `Transport/` 분리 구조로 업데이트
* **SDK 링커 설정 수정**: `--whole-archive` 적용으로 `__weak` 심볼 정상 오버라이드
  * Libraries(-l) → Other flags 이동 (CubeIDE makefile 명령줄 순서 문제 해결)
* **SDK XM_FW 헤더 동기화**: ARC_ExtensionBoard 원본과 완전 동기화
  * AGR_DOP Core/Transport 헤더 추가, 폐기된 `agr_dop.h` 제거
* **CMake CLI 빌드 도구 추가**: `cproject_to_cmake.py`, `stm32_gcc_toolchain.cmake`

### Note

* libXM_Lib.a는 Debug 빌드(-Og -g3)로 제공됩니다. Release 최적화(-O2) 빌드는 향후 지원 예정입니다.
* v2.0.0의 libXM_Lib.a는 동작하지 않습니다. **반드시 v2.0.1을 사용하세요.**

---

## [v2.0.0] — 2026-02-24 ⚠️ Deprecated — v2.0.1 사용 권장

### Breaking Changes

* **링크(Links) 프로토콜 → AGR DOP V2 + AGR PnP V2 전면 교체**
  * 기존 Links 기반 통신 코드는 v2.0.0과 호환되지 않습니다.
  * 마이그레이션 필요: `Links_*` API → `XM_*` API로 전환
* **IOIF Submodule V3.0 도입**
  * 기존 직접 HAL 호출 코드는 IOIF 래퍼로 전환 필요
* **XM_FW 정적 라이브러리(libXM_Lib.a)로 제공**
  * 사용자는 `XM_Apps/User_Algorithm/user_app.c`만 수정
  * XM_FW 소스 코드 직접 수정 불가 (헤더만 제공)

### Added

* **AGR DOP V2 (Data Object Protocol):** CANopen 기반 데이터 객체 관리 프로토콜
* **AGR PnP V2 (Plug & Play):** Master/Slave 디바이스 자동 검색 및 구성
* **IMU Hub Module 디바이스 드라이버:** IMU 센서 허브 연동 지원
* **USB CDC 개선:** PhAI V2.1 프로토콜, ISR-to-Task 지연 처리 패턴
* **USB MSC 개선:** 자동 타임스탬프, 롤링 파일, 구조체 등록 기반 로깅
* **XM API 모듈화 (파사드 패턴):**
  * `xm_api.h` — 메인 API (TSM, H10 제어)
  * `xm_api_data.h` — 데이터 인터페이스
  * `xm_api_tsm.h` — Task State Machine
  * `xm_api_led_btn.h` — LED & 버튼
  * `xm_api_external_io.h` — GPIO/ADC 제어
  * `xm_api_usb.h` — USB CDC/MSC
* **External I/O 확장:** DIO↔ADC 동적 전환, 밀리볼트 단위 읽기, 해상도 설정
* **신규 예제 7개:**
  * Ex.05a ~ 05d: ADC 튜토리얼 시리즈
  * Ex.10a ~ 10c: MSC 로깅 단계별 시리즈

### Changed

* 권장 STM32CubeIDE 버전: v1.14.1 → **v2.0.0 이상**
* SDK 빌드 방식: 소스 직접 빌드 → 정적 라이브러리(libXM_Lib.a) 링크
* 예제 구조: 난이도별 시리즈화 (ADC 5단계, MSC 3단계)

### Fixed

* CMake `--specs=nano.specs` 중복 적용 오류 수정
* IOIF 매크로 재정의 경고 제거 (`ioif_conf.h` 단일 소스 관리)

---

## [v1.0.1] — 2025-12-02

### Changed

* 예제 코드 업데이트 (Button/LED, External I/O)
* README.md 개선

---

## [v1.0.0] — 2025-10-13

### Added

* 초기 릴리즈
* XM10 SDK (소스 코드 형태)
* 기본 예제 13개 (Button/LED, External I/O, CDC, MSC, Robot Control)
* Quick Start Guide
* API Reference 문서 5종
* PythonDecoder 도구 (CDC/MSC)
