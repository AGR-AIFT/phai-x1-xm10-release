# XM10 Extension Module SDK v2.1.1

> **릴리즈 날짜**: 2026-04-04 | **대상 HW**: Rev1.1 / Rev2.0 | **KIT H10 FW**: v2.3.0+

---

## Highlights

- **AGR_BOOT V2 부트로더** 최초 도입 — USB FTP를 통한 무선 펌웨어 업데이트 지원
- **듀얼 HW 리비전 SDK 동시 배포** — Rev1.1과 Rev2.0 SDK를 단일 릴리즈에 포함
- **예제 44개** — 입문부터 고급 제어(Impedance, CPG, ILC, MRAC, DOB 등)까지 완전한 학습 경로
- **PhAI Studio 연동** 강화 — Total Data Packet 자동 전송 + User Custom 채널
- **xm_api_freertos** 신규 — 백그라운드 태스크 API (FreeRTOS 래퍼)
- **libXM_Lib.a 디버깅 완전 지원** — `-Og -g3` + `--whole-archive` 링크

---

## 다운로드

### SDK (레포에 포함)
| 항목 | 경로 | 설명 |
|------|------|------|
| **SDK Rev1.1** | `XM10_SDK/Rev1.1/Extension_Module/` | PCB Rev1.1 전용 프로젝트 |
| **SDK Rev2.0** | `XM10_SDK/Rev2.0/Extension_Module/` | PCB Rev2.0 전용 프로젝트 (ETH, PSRAM, RTC 추가) |

### Release 첨부 파일
| 파일 | 설명 | 비고 |
|------|------|------|
| `AGR_Bootloader.bin` | XM10 부트로더 바이너리 (v1.1.0) | 0x08000000에 프로그래밍 |
| `SUIT_CM_APP_X_X_X.bin` | KIT H10 CM 펌웨어 | [H10 FW 업데이트 가이드](docs/kit-h10-firmware/README.md) 참조 |
| `SUIT_SAM10_APP_X_X_X.bin` | KIT H10 MD(SAM10) 펌웨어 | 동일 가이드 참조 |
| `SUIT_ESP32_FW_X_X_X.bin` | KIT H10 ESP32 펌웨어 | 동일 가이드 참조 |
| `ContentsFiles.zip` | SD카드 Contents 파일 (FSM/MotionMap/RobotSetting) | 별도 업데이트 |

### 도구
| 도구 | 링크 |
|------|------|
| **PhAI Studio** (FW Upload, 센서 모니터링) | [studio.onephai.com](https://studio.onephai.com) |
| **STM32CubeProgrammer** (부트로더 최초 설치) | [st.com](https://www.st.com/en/development-tools/stm32cubeprog.html) |

---

## v2.0.1 대비 전체 변경 사항

### 신규 기능

#### AGR_BOOT V2 부트로더 (신규)
- USB CDC FTP 프로토콜을 통한 무선 펌웨어 업데이트
- 자동 백업 + 롤백 (3회 부팅 실패 시 이전 버전으로 자동 복원)
- CRC-32 펌웨어 무결성 검증
- Rev1.1 / Rev2.0 **단일 부트로더 바이너리** (HW GPIO 자동 감지)
- 상세 가이드: [docs/bootloader/README.md](docs/bootloader/README.md)

#### 듀얼 HW 리비전 SDK
- `XM10_SDK/Rev1.1/` — PCB Rev1.1 전용 (CAN-FD only)
- `XM10_SDK/Rev2.0/` — PCB Rev2.0 전용 (CAN-FD + Ethernet + PSRAM + RTC)
- 각 SDK는 **자체 완결형** STM32CubeIDE 프로젝트 (Import 후 바로 빌드 가능)

#### xm_api_freertos — 백그라운드 태스크 API (신규)
- `XM_CreateBackgroundTask()` — UserTask(1kHz) 방해 없이 무거운 연산을 별도 태스크에서 실행
- 우선순위 `osPriorityNormal`(24) — UserTask(54)보다 낮아 제어 루프 영향 없음
- 백그라운드 태스크에서 `XM_SetAssistTorque` 등 제어 API 호출 금지 (데이터 공유는 volatile)

#### Post-Build 자동화 파이프라인
- STM32CubeIDE 빌드 시 자동 실행:
  - `size_report.py` → `version_generator.py` → `patch_fw_info.py` → `fw_packager.py`
- 최종 출력: `XM10_X_X_X_X.bin` (PhAI Studio FTP 업로드용 패키징 바이너리)
- `boot_fw_info.c` SDK 직접 컴파일 — `.fw_header` 섹션 배치 (링커 GC 회피)

#### 예제 대규모 확장 (20개 → 44개)
- **Physical AI 토크 제어 시리즈** (Ex.20~33): Impedance, Gravity Comp, CPG, Gait Phase Adaptive, Virtual Constraint, Stance Stiffness, ILC, MRAC, Admittance, Bilateral, FF/FB Hybrid, Friction DOB, GRF Gait Intent, Kinesthetic Teaching
- **MSC 로깅 시리즈** (Ex.10~10c): Basic → Custom Struct → Advanced (Rolling, Markers)
- **Gait Analysis 로깅** (Ex.34): H10 보행 데이터 자동 수집 + Python 디코더
- **Ex.35 MultiLayer Transparent Control** — 다층 투명 제어 (신규)
- **Ex.36 OnDevice Kinesthetic Learning** — 온디바이스 동작 학습 (신규)

#### PhAI Studio 연동 강화
- **Total Data Packet** (Module ID 0x20, 365B): 시스템 자동 전송 (1kHz)
- **User Custom 채널** (Module ID 0xF0~0xFE): `XM_SetUsbCustomMeta()` + `XM_SendUsbDataWithId()`
- **Auto-Stream**: USB 연결 시 자동 스트리밍 시작 (레거시 "AGRB MON START" 불필요)

### 개선 사항

#### AGR_MW / IOIF 서브모듈 최신화
- **AGR_MW**: OD Discovery (이름/단위 조회), SDO non-expedited Upload (4바이트 초과 데이터), PDO_MAP_MAX_ENTRIES config 분리
- **IOIF**: TIM PWM/OC 인터럽트, SetCallback 런타임 콜백 주입, ISR-safe SetOCMode/GenerateUpdate

#### Data Map Code-Gen
- `xm_total_data.yaml` → `xm_total_data_packet.h` 자동 생성
- FDCAN Ch1/Ch2 독립 진단 (TEC/REC/LEC/BusStatus/FIFO)
- `timestamp_ms` → `xm_loop_count` 변경 (UserTask 실행 카운트, 블로킹 감지 가능)

#### 빌드 시스템
- **`--whole-archive` 링커 설정**: HAL `__weak` 콜백 오버라이드 보장 (v2.0.1 패턴 계승)
  - `.cproject`: Other flags에 `-Wl,--whole-archive libXM_Lib.a -Wl,--no-whole-archive`
  - `CMakeLists.txt`: 동일 래핑 적용
- **libXM_Lib.a 빌드**: `-Og -g3` (Debug 최적화 + 최대 디버그 심볼)
  - Live Expression + 브레이크포인트 정상 동작 (Rev1.1: 66MB, Rev2.0: 88MB)
  - Flash 바이너리 크기 무관 (디버그 심볼은 .elf에만 포함)
- CMakeLists.txt 정리: dead path 제거, 대소문자 수정 (`Xsens` → `XSENS`)
- `ExitRun0Mode()` 함수 추가 (CubeMX 6.13+ startup 호환)

#### 예제 튜닝
- **Ex.11 Passive Mode / Ex.12 Active Assist Mode**: Homing 가속도 4→2 deg/s², IVectorKpKd (6,1)→(6,6), PVectorReset 추가

### KIT H10 FW 변경 사항 (v2.3.0 기반)

H10 CM 펌웨어 주요 수정:
- **BLE CM Mode 동기화**: BLE 앱에서 모드 변경 시 CM 내부 상태 즉시 동기화
- **XM↔CM CAN-FD SYNC**: 동기화 프로토콜 개선
- **postProcessingCnt PDO 추가**: 보행 분석 후처리 카운터 외부 노출
- **Left IMU 데이터 수정**: 좌측 IMU 데이터 매핑 오류 수정

> H10 FW 업데이트 방법: [docs/kit-h10-firmware/README.md](docs/kit-h10-firmware/README.md) 참조

### 삭제

- `BuffMngr` 모듈 삭제 (AGR_MW에서 제거됨)
- `user_app.c` 루트 복사본 삭제 (XM_Apps/User_Algorithm/에서만 관리)

---

## 호환성 매트릭스

| 컴포넌트 | 최소 버전 | 권장 버전 | 비고 |
|----------|----------|----------|------|
| **XM10 FW (이 SDK)** | v2.1.1 | v2.1.1 | Rev1.1/Rev2.0 별도 libXM_Lib.a |
| **AGR_BOOT (부트로더)** | v1.1.0 | v1.1.0 | 단일 바이너리 (Rev1.1/Rev2.0 공통) |
| **KIT H10 CM** | v2.3.0 | v2.3.0+ | BLE Sync + ExtPDO 수정 포함 |
| **KIT H10 ESP32** | v2.3.0 | v2.3.0+ | |
| **KIT H10 SAM10/MD** | v2.3.0 | v2.3.0+ | |
| **ContentsFiles** | 2025.02 | 최신 | SD카드 업데이트 |
| **STM32CubeIDE** | v1.13.2 | v1.14.1+ | GCC 13.3 필요 |
| **Python** | 3.8+ | 3.12+ | Post-Build 스크립트용 |
| **PhAI Studio** | — | 최신 | [studio.onephai.com](https://studio.onephai.com) |

---

## 빠른 시작 가이드

### 1. 부트로더 설치 (최초 1회)
> 상세: [docs/bootloader/README.md](docs/bootloader/README.md)

1. STM32CubeProgrammer로 **Full Chip Erase**
2. `AGR_Bootloader.bin`을 `0x08000000`에 프로그래밍

### 2. SDK 프로젝트 열기
1. 이 레포를 Clone
2. STM32CubeIDE에서 **Import → Existing Projects into Workspace**
3. `XM10_SDK/Rev1.1/Extension_Module/` 또는 `XM10_SDK/Rev2.0/Extension_Module/` 선택
4. Build (Ctrl+B) → Post-Build 스크립트 자동 실행

### 3. 펌웨어 업로드
- **개발 중**: STM32CubeIDE Debug (F11) — SWD, Start address `0x08040000`
- **배포 시**: PhAI Studio → FW Upload → `XM10_X_X_X_X.bin` 선택

### 4. KIT H10 FW 업데이트
> 상세: [docs/kit-h10-firmware/README.md](docs/kit-h10-firmware/README.md)

첨부된 H10 FW 바이너리를 USB 메모리에 복사 후 H10에 연결하여 업데이트합니다.

---

## 알려진 이슈

- Rev1.1 SDK의 RAM_D2 사용량이 98.65%에 달합니다. 대용량 버퍼 할당 시 주의가 필요합니다.
- `version_generator.py`는 현재 디렉토리의 git tag를 읽습니다. SDK 레포와 개발 레포의 tag가 다를 수 있으므로 `version.h`를 수동으로 확인하세요.
- Post-Build 스크립트 실행 시 Python이 PATH에 있어야 합니다. STM32CubeIDE 재시작 후에도 인식되지 않으면 시스템 환경변수를 확인하세요.
- libXM_Lib.a는 `-Og -g3` (Debug)로 빌드됩니다. 디버그 심볼로 인해 .a 파일이 66~88MB이지만, 최종 Flash 바이너리 크기에는 영향 없습니다.

---

## v2.1.0 (Pre-Release) 사용자 안내

v2.1.0에서 발견된 2가지 문제가 v2.1.1에서 수정되었습니다:
1. **`--whole-archive` 링커 누락** → HAL `__weak` 콜백 미오버라이드 → vPortFree heap corruption
2. **`-O2 -g0` 빌드** → Live Expression, 브레이크포인트 미동작

**반드시 v2.1.1을 사용하세요.**

---

*이 릴리즈는 Angel Robotics의 Physical AI 플랫폼 XM10 Extension Module을 위한 것입니다.*
