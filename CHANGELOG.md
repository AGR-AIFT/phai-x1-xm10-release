# Changelog

모든 주요 변경 사항은 이 문서에 기록됩니다. [Semantic Versioning](https://semver.org/)을 따릅니다.

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
