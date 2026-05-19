# Changelog

모든 주요 변경 사항은 이 문서에 기록됩니다. [Semantic Versioning](https://semver.org/)을 따릅니다.

---

## [v2.2.1] — 2026-05-19

> Rev 2.0 SDK 다운로드 직후 빌드 실패 (undefined reference 22 건) 수정. 사용자 코드 / API 변경 없음.

### Fixed

* **Rev 2.0 SDK link error 22 건** — 라이브러리 빌드 단계에서 CDC DOP 라우터 / AGR Serial 트랜스포트 / COBS 인코더 모듈의 소스가 누락되어 있던 문제. 새 `libXM_Lib.a` 로 교체, clean build 169/169 link 통과 확인.
* **Rev 1.1 SDK 안정성 보강** — link 실패는 없었으나 같은 정합성 차원에서 `usbh_diskio.c` 의 USB MSC 진단 함수 정의 복원 + `FreeRTOSConfig.h` 의 `INCLUDE_xTaskGetHandle = 1` 매크로 추가.

### Documentation

* **보드 리비전 비교** — `docs/hardware/README.md` 의 비어 있던 비교 표를 채움 (RJ45 / 채널 LED / PSRAM / 내장 버튼 MCU 핀 / ZIP 매핑). 본인 보드와 다른 Rev 의 ZIP 으로 빌드하면 버튼/LED 핀이 한 칸 어긋난다는 점 명시.
* **Troubleshooting 신규 섹션** — `docs/troubleshooting.md` 에 "보드 리비전 / SDK ZIP 불일치" 추가 (증상, 핀 표, 진단 코드, 해결 단계).
* **흔한 실수 보강** — `examples/README.md`, `docs/api-reference/03-led-btn-control.md` 에 Rev mismatch 안내 추가.
* **깨진 링크 정리** — `README.md` / `CLAUDE.md` / `docs/find-it.md` 의 `docs/architecture/` 비교표 참조를 새 `docs/hardware/README.md#보드-리비전-비교` 로 통일.

### Compatibility

* **사용자 코드** — v2.2.0 코드 그대로 빌드 가능. `Control_Setup` / `Control_Loop` / `XM_BTN_*` / `XM_Task_*` 등 모든 API 변경 없음.
* **KIT H10 펌웨어** — v2.3.0 그대로. 동반 펌웨어 업데이트 불필요.

---

## [v2.2.0] — 2026-05-15

> 사용자 함수 이름 정리 + Rev 1.1 / Rev 2.0 Task API 평준화 + 학습 예제 2 개 추가.

### Added

* **사용자 함수 이름 통일** — `User_Setup` / `User_Loop` → `Control_Setup` / `Control_Loop` (옛 이름 호환 유지)
* **Rev 1.1 에도 보조 task API 추가** — `XM_Task_CreateOneShot/Periodic`, `XM_Task_IsComplete/Delete`, `XM_Mutex_*` (Rev 2.0 와 동일)
* **`Examples/38_Periodic_Background_Task/`** — `Control_Loop`(1 kHz) + 보조 task(100 Hz) 데이터 공유 패턴 데모
* **`Examples/39_Task_Lifecycle/`** — OneShot task Create → Complete → Delete 사이클 데모
* **`docs/dev/12_Task_Topology.md`** — 시스템 task 인벤토리 + prio_hint 가이드 + 데이터 흐름 다이어그램
* **`docs/release-notes/v2.2.0.md`** — 본 릴리즈 노트

### Changed

* **폴더 이름** — `XM_Apps/User_Algorithm/` → `XM_Apps/Control_Task/` (회사 모듈 컨벤션 정합)
* **`libXM_Lib.a` 양 Rev 재빌드** — 신규 task 관리 코드 + 옛/새 함수 이름 자동 매핑 (`user_compat`) + 진단 유틸 (`diag_perf`, `hardfault_dump`) 통합
* **Ex.36 (OnDevice Kinesthetic Learning)** — 옛 `XM_BgTask_Create` → 새 `XM_Task_CreateOneShot` 마이그레이션

### Compatibility

* **기존 v2.1.1 코드** — 새 SDK 로 그대로 컴파일 가능 (옛 함수 이름·옛 API 자동 인식)
* **API surface** — Rev 1.1 / Rev 2.0 동일 (이전까지는 Rev 2.0 에만 일부 task API 존재)

---

## [unreleased — 2026-05] — Docs UX Overhaul

> 코드 변경 없음. 학생 친화 문서 전면 개편 + Claude Code 온보딩 인프라.

### Added

* **Claude Code 온보딩 인프라** — `CLAUDE.md`, `AGENTS.md`, `.claude/skills/student-onboard/` (6 단계 phased 안내), `.claude/skills/example-helper/` (예제별 트러블 응답)
* **`docs/getting-started/00-claude-code-quickstart.md`** — AI 자동 안내 진입 페이지
* **`docs/hardware/`** — 보드 외부 인터페이스 통합 안내 + Rev 1.1 / Rev 2.0 별 외부 GPIO 핀맵
* **`docs/advanced/ai-data-pipeline.md`** — 보드 데이터 → PyTorch / sklearn 학습 흐름 한 페이지 정리 (3 가지 길 + PhAI Studio 연동)
* **`docs/tutorials/README.md` 16 주 수업 진도표** — 강사용 한 학기 운영 참고 진도표
* **`docs/student-walkthrough-simulations.md`** — 가상 학생 5 명 UX 시뮬레이션 (강사·멘토용 체크리스트)
* **`docs/find-it.md`** — 키워드 → 페이지 빠른 찾기 인덱스 (자주 묻는 질문 통합)
* **`docs/release-notes/`** — 버전별 첨부 파일 + 호환성 매트릭스 (루트 `RELEASE_v*.md` 이전 위치)
* **`assets/img/README.md`** — 이미지 자료 우선순위 가이드 (Tier 1~6, 35 개 placeholder 목록)

### Changed

* **41 개 예제 README 통일** — 5 단계 lab manual 포맷 (목표 / 사전 지식 / 핵심 코드 / 실험 / 다음 단계 + 흔한 실수)
* **`docs/` 4-tier 재구성** — getting-started · tutorials · api-reference · architecture · advanced · bootloader · kit-h10-firmware · troubleshooting 학생 친화 톤
* **루트 `README.md` 재설계** — Claude Code 우선 + 수동 3 단계 간단 명령
* **학생 친화 용어 교체** — 내부 약어 (AGR DOP V2, IOIF V3.0, Facade Layer, RTOS Task, PI-Vector, TSM, PDO, CDC, MSC, Cortex-M7 등) → 학생이 이해할 수 있는 자연스러운 표현
* **AI 틱한 표현 제거** — 📌/⏱️/🧰 메타 박스 + WHY/WHAT/HOW 영문 헤더 + 🧒 비유 박스 정리, 한국어 자연스러운 톤
* **`examples/README.md`** — Rev 1.1 / Rev 2.0 호환성 통합 안내 (41 개 예제 모두 빌드 호환, 외부 GPIO 핀맵만 리비전별 확인)

### Fixed

* **`docs/api-reference/04-external-io.md`** — 잘못된 ADC 핀 정보 (PA0/PA1) → 실제 (PB0/PB1/PF11/PF12) 로 정정, Rev 2.0 누락 보강
* **`.vscode/settings.json`** — 개발자 절대 경로 박힌 파일을 추적에서 제외 (`.template` 만 추적)

### Moved

* **`RELEASE_v2.1.1.md`** → **`docs/release-notes/v2.1.1.md`** (루트 정리, 향후 릴리즈도 동일 폴더로 일관성)
* 루트 README 에 **폴더 구조 시각 가이드 + 길 찾기 박스** 추가 — 학생 "어디 가야 하지" 마찰 감소

---

## [v2.1.1] — 2026-04-04

> v2.1.0 + 링커 수정 + 디버그 심볼 복원 + xm_api_freertos + Ex.35~36 통합

### Fixed (from v2.1.0)

* **`--whole-archive` 링커 설정 복원**: v2.0.1에서 해결한 HAL `__weak` 오버라이드 문제가 재발 → vPortFree heap corruption 수정
* **libXM_Lib.a 디버그 심볼 복원**: `-O2 -g0` → `-Og -g3` (Live Expression + 브레이크포인트)
* **Rev2.0 pre-build**: `patch_cubemx_overrides.py` 복원 (LwIP LWIP_RAND 가드)
* **Rev2.0 post-build**: `cproject_to_cmake.py` 제거 (SDK CMakeLists.txt 파손 방지)
* **Rev1.1 xm_api.h**: Rev2.0 전용 include (`xm_api_memory.h`, `xm_api_rtc.h`) 제거

### Added (from v2.1.0)

* **xm_api_freertos.h/c** — 백그라운드 태스크 API (FreeRTOS 래퍼)
* **Ex.35 MultiLayer Transparent Control** — 다층 투명 제어
* **Ex.36 OnDevice Kinesthetic Learning** — 온디바이스 동작 학습
* Ex.11/12 homing 튜닝: accel 4→2 deg/s², IVectorKpKd (6,1)→(6,6)

---

## [v2.1.0] — 2026-04-02 ⚠️ Pre-Release — v2.1.1 사용 권장

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
