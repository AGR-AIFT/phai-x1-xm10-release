# Changelog

모든 주요 변경 사항은 이 문서에 기록됩니다. [Semantic Versioning](https://semver.org/)을 따릅니다.

---

## [v2.6.0] — 2026-08-20

> **Rev 1.1 · Rev 2.0 공통 안전성 릴리즈.** 전수 리뷰 결과를 반영해 "제어를 끄면 확실히 꺼지고, 펌웨어가 멈추면 스스로 되살아나는" 동작을 갖췄습니다. 삭제된 API 는 없으며 사용자 코드는 재빌드만 하면 됩니다 — 다만 **`MONITOR` 전환 동작과 `forwardVelocity` 값이 바뀌었습니다**(아래 Changed 참고). Rev 1.1 은 v2.5.0 이후 첫 업데이트입니다.

### Added
* **`XM_EmergencyDisengage()`** — 램프다운을 생략하고 즉시 토크 0 을 확정 전송합니다.
* **`XM_GetAppliedControlMode()`** — 요청(`XM_SetControlMode`)이 아닌 **실제 적용** 모드(`CONTROL` / `MONITOR` / `TRANSITION`)를 반환합니다.
* **`xm_api_safety.h`** — header-only 공통 안전 헬퍼. `XM_SafeTorque_Init/Reset/Step`(유한값 가드 + 클램프 + 진입 소프트스타트 + slew), `XM_SafeAssistLevel()`, `XM_SafeIsFresh()`. `xm_api.h` 가 이미 포함합니다.
* **하드웨어 워치독 (IWDG, 약 8 초)** — `Control_Loop` 가 도는 UserTask 가 1 kHz 로 갱신합니다. 초기화 단계 실패는 그 자리에서 halt 하고, 정상 동작 중의 행(hang)만 리셋으로 회복시킵니다. 부팅 단계·리셋 원인 마커도 함께 기록합니다.
* **Ext_Sync — 외부 동기화 TTL 입력** — Total Data 에 `sync_save_active` / `sync_din_level` / `sync_din_edge_count` 3 채널 추가. 기존 예약(reserved) 영역을 사용해 **패킷 크기 365 B 와 뒤쪽 채널 오프셋이 그대로**이므로 기존 수신 프로그램은 무수정으로 동작합니다.
* **진단 확장 (Rev 2.0)** — EMCY 누적 카운터, 부팅 단계별 실패 비트맵, 태스크 스택 watermark. 펌웨어 빌드에 Git 커밋 해시를 주입합니다.

### Changed
* **`XM_SetControlMode(XM_CTRL_MONITOR)` 가 즉시 차단이 아니라 3 단계 안전 전환을 거칩니다** — 토크 지수 감쇠(τ=100 ms) → 0 확정 전송(프레임 유실 대비 반복) → P/I 벡터 해제 → 출력 차단. 총 0.3~0.6 초 소요되며 그동안 사용자 토크 명령은 반영되지 않습니다. 기존에는 전송이 즉시 끊겨 **CM/MD 에 마지막 비-0 토크가 latch** 되는 ZOH silent-drop 이 있었습니다.
* **`MONITOR` 모드가 완전 차단이 되었습니다** — 토크·벡터 어떤 제어 명령도 전송하지 않습니다 (Control/Monitor 경로 분리).
* **예제 15 종 안전 강화** — 진입 소프트스타트/slew(Ex.14·21·27·30·33), NC 정지 스위치·출력 클램프·정지 램프·센서 stale 타임아웃(Ex.06·13·17·26·32), 모드 전환 정합·호밍 fail-closed·이중 A→Nm 변환 제거(Ex.11·12·15·35·36). 예제 번호와 학습 내용은 그대로입니다. 별도로 Ex.03·08·31·37 은 설명 문구만 정정(동작 불변).
* **착용 전제 예제 4 종에 벤치 파라미터 안내 주석** (Ex.15·21·30·33) — 미착용 상태는 관성이 작아 진동/발산하므로 링크 단독 물성(0.184 kg / 0.1264 m)으로 치환하는 방법을 파일 헤더에 명시했습니다.
* **부트로더 바이너리 갱신** — BootConfig 영역 ECC 손상 시 BusFault 를 감지해 설정을 초기화하고 정상 부팅합니다(신규). 앱 진입 시 pending 시스템 예외(SysTick/PendSV) 정리는 v2.5.1 첨부본에 이미 포함된 항목입니다. **v2.6.0 펌웨어는 구 부트로더에서도 동작하므로 재설치는 권장이지 필수가 아닙니다.**

### Fixed
* **`XM.status.h10.forwardVelocity` 60 배 과대 정정** — 분당→초당 환산 누락. 실보행 ~1 m/s 에서 1.0 부근 수신 확인. 이 값을 제어에 사용했다면 **게인 재조정이 필요**하며, 과거 데이터와 혼용하면 안 됩니다. 부수 효과로 **Ex.22 · 23 · 24 · 26 의 정지 판정(`forwardVelocity < 0.1`)이 이제 성립**합니다 — 종전에는 60 배 확대값이라 사실상 항상 "보행 중" 으로 처리됐습니다.
* **Ex.14 미분킥 제거** — PD 의 미분항을 오차가 아닌 **측정값**에 걸어(derivative-on-measurement) 목표 스텝 시 D 항이 튀던 원인을 제거했습니다. 목표가 일정한 구간에서는 기존과 수학적으로 동일합니다.
* **무한 재부팅 방지** — 초기화 실패 시 재부팅을 반복하던 `Error_Handler` 경로를 halt 로 재설계했습니다.
* **EMG TPDO1 길이 검증 위치 정정** — 디코드 이전 호출부에서 검증하도록 옮겼습니다.
* **USB-CDC 안정화 (Rev 2.0)** — TX epoch/디스크립터 처리, role 전환 가드, Boot FTP task defer.
* **USB 로거 무한 스핀 해소 (Rev 2.0)** — 게이트 없는 유한 drain 으로 분리했습니다.
* **문서 정정** — `wearable-safety` 의 "워치독 없음" 문구를 IWDG 도입 사실 + "워치독은 비상 정지가 아니다" 로 정정, Task 생성 가이드에 8 초 CPU 독점 제약 추가, Ex.31 README 의 `rightHipTorque` 단위(전류 A → 관절 토크 Nm) 자기모순 정정.

### Notes
* 삭제된 공개 API 없음. 옛 이름 `XM_CTRL_TORQUE` 는 `XM_CTRL_CONTROL` 의 alias 로 유지됩니다.
* 예제 개수 변동 없음 (Rev 2.0 45 개 / Rev 1.1 42 개).
* **KIT H10 펌웨어 v2.4.0 동봉** (`SUIT_H10_Binary_20260820.zip`) — CM · MD 가 2.3.0 → 2.4.0, ESP32 는 2.3.0 그대로. 컨텐츠 파일(`SUIT_ContentsFiles_20260820.zip`)은 내용 동일, 파일명만 날짜 표기로 변경.

---

## [v2.5.1] — 2026-07-29

> **Rev 2.0 전용 긴급 수정 (Rev 1.1 영향 없음).** v2.4.1 · v2.5.0 의 Rev 2.0 펌웨어가 부팅 도중 멈추던 문제를 고쳤습니다 (LED 무반응 · PC 에 COM 포트 미출현). 기능 변경은 없으며, Rev 2.0 사용자는 재빌드·업로드만 하면 됩니다.

### Fixed
* **부팅 정지 수정 (Rev 2.0)** — `agr_retarget.c` 의 newlib 락 뮤텍스를 만드는 startup 생성자가 스케줄러 시작 전에 실행되면서, FreeRTOS 크리티컬 섹션이 남긴 인터럽트 마스크(BASEPRI)가 복구되지 않았습니다. 스케줄러 시작 전에는 `uxCriticalNesting` 이 초기 sentinel 값이라 `vPortExitCritical()` 이 마스크를 푸는 분기에 도달하지 못합니다. 그 결과 HAL 틱(TIM1_UP)이 차단되어 `uwTick` 이 증가하지 않고, 부팅 경로 최초의 `HAL_Delay()`(`MX_USB_OTG_FS_PCD_Init()` 내 `USB_SetCurrentMode()`)에서 무한 대기했습니다. 생성자에서 마스크를 명시적으로 복구하도록 수정했습니다.
* **심층 방어** — `main()` 진입 시점에도 인터럽트 마스크를 한 번 초기화합니다. `SystemInit()` 의 `__enable_irq()` 는 PRIMASK 만 다루고 생성자보다 먼저 실행되어 이 경로를 덮지 못합니다.

### Notes
* Rev 1.1 에는 `agr_retarget.c` 가 없어 이 결함의 영향을 받지 않습니다. `Rev1.1.zip` 은 v2.5.0 과 동일합니다.
* v2.4.0 이하는 해당 없습니다 (원인 파일이 v2.4.1 에서 도입).

---

## [v2.5.0] — 2026-07-24

> **Rev 1.1 · Rev 2.0 공통.** USB 메모리(MSC)에 데이터를 저장하던 기능을 제거했습니다. 저장 속도 한계로 데이터가 조용히 누락될 수 있어(loop count 는 멀쩡해 보여 발견 어려움), 데이터 수집을 **USB-CDC 실시간 스트리밍(PhAI Studio / PythonDecoder CDC)** 으로 일원화했습니다. 온보드 저장(SD카드)은 향후 HW 리비전에서 지원 예정.

### Removed
* **USB-MSC 파일 로깅 제거** — 파일 로깅 예제 5종(`10`/`10a`/`10b`/`10c`/`34`), 관련 public API(`XM_SetUsbLogSource` / `XM_StartUsbDataLog` / `XM_StopUsbDataLog` / `XM_GetUsbLogStats` / `XM_InsertUsbLogMarker` 등), "USB 메모리 로깅" 문서 페이지를 제거했습니다. CDC(실시간 스트리밍) API 는 전부 유지됩니다.

### Changed
* **예제 11 · 12 · 17 데이터 캡처 CDC 전환** — USB 메모리 세션 로깅 대신 USB-CDC 실시간 스트리밍(`XM_SetUsbStreamSource` + `XM_SetUsbAutoStream`)으로 전환했습니다.
* **예제 개수** — Rev 2.0 45 개 / Rev 1.1 42 개 (MSC 예제 5 종 제거 반영).

### Docs
* **데이터 수집 안내 CDC 일원화** — API 레퍼런스 · 튜토리얼 · AI 데이터 파이프라인을 USB-CDC 기준으로 정리하고, 삭제된 예제로 향하던 링크를 CDC(Ex.09)·PhAI Studio 로 연결했습니다. `PythonDecoder/README` 를 CDC 실시간 수신 샘플 전용으로 재작성했습니다.

---

## [v2.4.1] — 2026-07-24

> **Rev 2.0 전용** 패치 릴리즈. C 표준 라이브러리(실수 `printf` · `malloc`)가 Rev 2.0 에서 정상 동작하도록 힙 설정을 바로잡고, 멀티태스크 안전성을 확보했습니다. 새 예제/API 없음. Rev 1.1 은 v2.3.1 그대로.

### Fixed (펌웨어 — Rev 2.0)
* **`printf("%f", ...)` 실수 출력과 `malloc()` 이 정상 동작합니다** — 이전에는 newlib 힙 상한이 실제 힙 영역(RAM_D1)이 아니라 스택 꼭대기(DTCMRAM)를 기준으로 잡혀, `_sbrk()` 가 첫 호출부터 실패했습니다. 그 결과 `malloc()` 과 이를 내부적으로 사용하는 실수 `printf`(`%f`)가 조용히 실패했습니다. 힙 상한을 RAM_D1 최상단으로 바로잡아 해결했습니다. (Rev 1.1 은 스택·힙이 같은 영역이라 애초에 영향 없음)
* **C 표준 라이브러리 멀티태스크 안전화** — `malloc` / `free` / `printf` 계열을 여러 FreeRTOS 태스크에서 동시에 호출해도 안전하도록 newlib 락을 FreeRTOS 뮤텍스에 연결했습니다.

### Fixed (SDK 빌드)
* **CMake · CubeIDE 빌드 동작 일치** — CMake 빌드 파일 생성기가 실수 `printf` 링커 옵션과 링커 Other-flags(`--wrap`)를 누락하던 문제를 바로잡아, 두 빌드 시스템이 동일한 결과를 내도록 했습니다.

### Changed (도구)
* **USB 로그 디코더가 독립 실행 파일로 바뀌었습니다** — 기존 `PythonDecoder/` 안의 파이썬 스크립트(MSC · Legacy) 대신, 파이썬 설치 없이 바로 쓰는 **XM10 Log Decoder** 설치 파일을 Releases 에 제공합니다. 임의의 사용자 로그 구조를 자동 인식하도록 범용화했습니다. 실시간 USB-CDC 수신 샘플(`PythonDecoder/CDC/`)은 그대로 유지됩니다.

### Docs
* **디버그 정지 안내 보강** — 디버그 시작 시 `main()` 정지 및 `Break at address "0x0800xxxx" ... no debug information`(부트로더 영역) 팝업이 정상 동작임을 트러블슈팅 문서에 추가했습니다.

---

## [v2.4.0] — 2026-07-21

> USB 시리얼 사용성을 개선한 **Rev 2.0 전용** 릴리즈. 일반 터미널에서 텍스트 예제가 보이지 않던 문제 해결 + USB 통신 모드 API 단순화. Rev 1.1 은 v2.3.1 그대로.

### Changed (동작 변경 — 주의)
* **USB 통신 모드 API 교체 (Rev 2.0, breaking)** — `XM_USB_GetMode()` / `XM_USB_SetMode()` / `XM_USB_RegisterModeChangeCallback()` 및 타입 `XM_USB_Mode_e` / `XM_USB_ModeChangeCb_t` **제거**. 대체: `XM_USB_SetHostProfile(XM_USB_HostProfile_e)` — 선택지 2개(`XM_USB_HOST_PHAI_STUDIO`(기본) / `XM_USB_HOST_TERMINAL`). 마이그레이션: `XM_USB_SetMode(XM_USB_MODE_TERMINAL)` → `XM_USB_SetHostProfile(XM_USB_HOST_TERMINAL)`. 미호출 코드는 기본 PhAI Studio 스트리밍으로 변경 없이 동작. 생산검사(PRODUCTION) 모드는 내부 자동 처리로 전환되어 사용자 비노출.

### Added
* **Ex.07 / Ex.08 텍스트 예제 터미널 프로파일 자동 지정** — `Control_Setup()` 에서 `XM_USB_SetHostProfile(XM_USB_HOST_TERMINAL)` 호출. 1kHz auto-pump 를 꺼 일반 시리얼 터미널(Tera Term · VS Code Serial Monitor · PuTTY)에 깨끗한 텍스트만 출력, DTR 조작 불필요. 프로파일은 재접속에도 유지.

### Fixed (펌웨어)
* **GRF 센서 모듈 안정성 추가 개선** — fault 발생 시 센서 전원 재순환(power cycling) 비활성화로 불필요한 리셋 반복 완화 (v2.3.1 GRF 부팅 안정화 후속).
* **MCU 리셋 핀 처리 정리** — 리셋 핀 released 유지로 외부 리셋 오동작 여지 제거.

### Fixed (SDK 빌드)
* **CubeIDE 빌드 파이프라인 견고화** — pre-build 스크립트 단일 진입점 통합(CubeMX 코드 재생성 시 빌드 붕괴 방지), 미사용 USB 클럭 설정 자동 정리, LwIP(Ethernet) include 경로 자동 반영.

### Docs
* **부트로더 디버그 문서 보강** — 디버그 설정에 벡터 테이블 주소 `0x08040400` 명시(미지정 시 signal handler 정지 방지), `main()` 자동 정지가 정상 동작임을 안내.

---

## [v2.3.1] — 2026-07-18

> 부팅·통신 안정성과 SDK 사용성을 다듬은 패치 릴리즈. 새 예제/API 없음.

### Changed (동작 변경 — 주의)
* **고관절 토크 단위** — `XM.status.h10.leftHipTorque`/`rightHipTorque` 가 모터 전류(A) 대신 **관절 토크(Nm)** 를 담습니다. 직접 환산(×0.085×18.75)하던 코드는 환산 제거 필요. Ex.31/34 예제 동반 갱신.

### Removed
* `XM_GetUserPSRAM()` / `XM_GetUserPSRAMSize()` 공개 API 제외 — 대체: `XM_GetUserWorkspace()`(RAM_D1, 200KB).

### Fixed (펌웨어)
* **GRF 센서 모듈 부팅 안정성** — 센서 전원 레일을 부팅 후 지연·단계 인가, 모듈 장착 상태 전원 인가 시 반복 재부팅 완화. GRF 데이터 수신 누락 제거.
* **CAN 견고화** — 버스 오류 자동 회복 경로 정비, 전송 상태 관리 보강.
* **USB** — 케이블 분리/재연결 처리 개선, 패킷 길이 검사.
* **저장·메모리** — 로그 버퍼 배치 재정리(네트워크 기능과 메모리 충돌 예방), 설정 저장(Flash) 경계 검사.
* **제어 안전** — Ex.06 리미트 스위치 입력 설정 안전화, 자세 센서 파싱 경계 검사, RTC 시각 설정 검증 등.

### Fixed (SDK 포장·빌드)
* ZIP 압축 해제 → CubeIDE import 직후 빌드 실패 결함 수정 — 누락됐던 헤더 경로와 빌드 스크립트(`tools/build/`)를 ZIP 에 정상 포함.
* 빌드 후 펌웨어 버전이 1.0.0.0 으로 바뀌던 문제 수정 — ZIP 프로젝트에서도 릴리즈 버전 유지.
* 중복 빌드 설정 제거 + 미사용 변수 경고 제거 — CubeIDE 빌드 0 error / 0 warning.

### Docs
* 개발 환경 구축에 **Python 3.10+ 준비물** 명시 (빌드 마무리 단계가 사용).
* 메모리 문서/함수 레퍼런스에서 PSRAM API 제거 반영.

### 첨부/호환
* **부트로더 · KIT H10 펌웨어 · 컨텐츠 파일** — v2.3.0 그대로 (v2.3.0 릴리즈 첨부 사용).

---

## [v2.3.0] — 2026-07-15

> 센서 허브 연동 예제 2 종(IMU Hub · EMG Hub)을 새로 추가하고, SDK 안의 펌웨어·시스템 코드·예제를 모두 최신 버전으로 다시 맞춘 마이너 릴리즈. Rev 1.1 / Rev 2.0 두 버전 모두 적용했으며, 펌웨어 버전 표기(version.h)를 실제 릴리즈에 맞춰 정정했습니다.

### Added (예제)
* **Ex.41 IMU Hub Dashboard** 🛑 Rev 2.0 전용 — IMU Hub Module 최대 6채널의 방위(쿼터니언)를 받아 오일러 각으로 변환, 연결 자동감지 + PhAI Studio 18채널 스트리밍. (관찰형, 모터 없음)
* **Ex.42 EMG Hub Biofeedback** 🛑 Rev 2.0 전용 — EMG Hub Module의 근활성도를 받아 MVC 정규화 + LED / PhAI Studio 바이오피드백. (모터 없음)

### Fixed (예제 동작)
* **Ex.28 Admittance** — 어드미턴스 토크가 좌/우 계산 완료 전에 인가되던 순서 정정(좌우 각각 계산 후 인가).
* **Ex.17 FSM Gait Intent** — 체중 파라미터 단위(g) 및 레벨 스케일 계수 정정.
* **Ex.12 Active Assist** — 앵커 각도 스케일(×10) 정정.
* **Ex.10 / 10c MSC Log** — 로그 시작 시점을 상태 진입(on_entry)으로 통일 + 시작 실패 시 안전 복귀.

### Changed
* **SDK 펌웨어·시스템 코드 최신화** — `libXM_Lib.a` 를 v2.3.0 펌웨어로 새로 빌드 + 이전 버전 그대로 남아 있던 `Core/`(NVIC 우선순위·fault 핸들러·RTOS 설정)·`Compatible/`·`LWIP/` 를 라이브러리와 일치시킴. 양 Rev SDK 를 처음부터 다시 빌드해 오류 0 확인.
* **예제 갱신** — 진입점 주석(Control_Setup/Loop)·난이도 표기 일관화, Ex.40 을 Rev 2.0 SDK 번들에도 포함.
* **문서 갱신** — 예제 수 표기 50 개로 갱신(Rev 1.1 SDK 는 47 개), Ex.41/42 카탈로그 반영, 문서 안의 오래된 경로 참조 정리.

### Documentation
* **API 참조 정확화** — P-Vector 목표각 스케일, RTOS task 우선순위표, RTC / PSRAM Rev 2.0 전용 표기 정정.
* **`version.h` 2.3.0 정정** — v2.2.1 / v2.2.2 패치 때 버전 숫자가 갱신되지 않아 펌웨어가 2.2.0 으로 표시되던 문제를 바로잡음.

### Compatibility
* Ex.40 / 41 / 42 는 **Rev 2.0 전용**(센서 허브 · 외부 전원 API). Rev 1.1 SDK 에는 포함되지 않습니다.
* 사용자 코드 / 공개 API 시그니처 변경 없음.

---

## [v2.2.2] — 2026-05-20

> SDK 안의 예제 코드 47 개를 다시 한 번 훑어 사용자 혼선 포인트 9 건을 정리한 패치 릴리즈. Ex.36 이 Rev 2.0 전용임을 4 곳에 일관 표기. 사용자 코드 / API / KIT H10 펌웨어 변경 없음.

### Fixed (예제 코드 정리)

* **Ex.31 Friction_Comp_DOB** — `MAX_TORQUE_NM` 이중 `#define` (`8.0f` → `5.0f`) 정리. 5.0 Nm 단일 정의로 통합, redefine 컴파일러 경고 제거.
* **Ex.08 CDC_Sensor_Print · Ex.13 Resistive_Mode** — 64 byte 스택 버퍼의 `sprintf` → `snprintf(buf, sizeof(buf), ...)` 로 교체 (silent stack overflow 위험 차단).
* **Ex.15 Inverted_Pendulum** — 디버그 라인의 고정소수점 출력에서 음수 부호가 사라지던 문제 (`-0.35` → `0.35` 로 표시되던) 수정. 부호를 `%s` prefix 로 분리.
* **Ex.33 Kinesthetic_Teaching** — loop-count 매크로 `RECORD_DOWNSAMPLE` 를 ms 임계값으로 재활용하던 부분을 `REPLAY_STEP_MS = 10U` 매크로로 분리.
* **Ex.36 OnDevice_Kinesthetic_Learning** — `Active_Entry` 에서 `s_mode_lost_tick` 워치도그 명시 리셋 (재진입 시 즉시 STANDBY 빠지던 가능성 차단) + BG task ↔ foreground 다중 워드 volatile 한계 헤더 주석 보강.
* **Ex.22 CPG_Oscillator** — AFO frequency 적응 식이 phase advance 이후의 `sin(φ)` 를 쓰던 한 스텝 lag 수정 (`sin(φ[k]) / cos(φ[k])` 미리 계산).
* **Ex.24 Virtual_Constraint** — 1 kHz 루프의 5 차 Bézier basis `powf` 12 회 호출 → 곱셈 체인으로 교체.
* **Ex.26 ILC** — 보행 주기 1 회 (≈ 1 Hz) 단위 호출임을 헤더 주석으로 명시 (1 kHz 루프 부담 오해 차단).
* **Ex.16 TinyAI Sensor_Fusion** — NN raw logit 을 `× 100` 으로 확률처럼 표시하던 부분 → 라벨 `Conf:%` → `Score:`, 단위 그대로 출력.

### Documentation

* **Ex.36 Rev 2.0 전용 표기** — 소스 헤더 `@warning` + `examples/36/README.md` 상단 🛑 배너 + `docs/troubleshooting.md` 신규 섹션 + `docs/tutorials/README.md` 41 예제 로드맵 표기. 4 곳 일관.
* **release-notes** — `docs/release-notes/v2.2.2.md` 신규.

### Compatibility

* **사용자 코드** — v2.2.1 코드 그대로 빌드 가능. `Control_Setup` / `Control_Loop` / 모든 API 변경 없음.
* **KIT H10 펌웨어** — v2.3.0 그대로.
* **부트로더** — v1.1.0 그대로.

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
* **`docs/api-reference/09-task-creation.md`** — 시스템 task 인벤토리 + prio_hint 가이드 + 데이터 흐름 다이어그램
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

> 코드 변경 없음. 사용자 친화 문서 전면 개편 + Claude Code 온보딩 인프라.

### Added

* **Claude Code 온보딩 인프라** — `CLAUDE.md`, `AGENTS.md`, `.claude/skills/student-onboard/` (6 단계 phased 안내), `.claude/skills/example-helper/` (예제별 트러블 응답)
* **`docs/getting-started/00-claude-code-quickstart.md`** — AI 자동 안내 진입 페이지
* **`docs/hardware/`** — 보드 외부 인터페이스 통합 안내 + Rev 1.1 / Rev 2.0 별 외부 GPIO 핀맵
* **`docs/advanced/ai-data-pipeline.md`** — 보드 데이터 → PyTorch / sklearn 학습 흐름 한 페이지 정리 (3 가지 길 + PhAI Studio 연동)
* **`docs/tutorials/README.md` 16 주 수업 진도표** — 한 학기 수업 운영용 참고 진도표
* **`docs/student-walkthrough-simulations.md`** — 가상 사용자 5 명 UX 시뮬레이션 (멘토·강사용 체크리스트)
* **`docs/find-it.md`** — 키워드 → 페이지 빠른 찾기 인덱스 (자주 묻는 질문 통합)
* **`docs/release-notes/`** — 버전별 첨부 파일 + 호환성 매트릭스 (루트 `RELEASE_v*.md` 이전 위치)
* **`assets/img/README.md`** — 이미지 자료 우선순위 가이드 (Tier 1~6, 35 개 placeholder 목록)

### Changed

* **41 개 예제 README 통일** — 5 단계 lab manual 포맷 (목표 / 사전 지식 / 핵심 코드 / 실험 / 다음 단계 + 흔한 실수)
* **`docs/` 4-tier 재구성** — getting-started · tutorials · api-reference · architecture · advanced · bootloader · kit-h10-firmware · troubleshooting 사용자 친화 톤
* **루트 `README.md` 재설계** — Claude Code 우선 + 수동 3 단계 간단 명령
* **사용자 친화 용어 교체** — 내부 약어 (AGR DOP V2, IOIF V3.0, Facade Layer, RTOS Task, PI-Vector, TSM, PDO, CDC, MSC, Cortex-M7 등) → 처음 보는 사람도 이해할 수 있는 자연스러운 표현
* **AI 틱한 표현 제거** — 📌/⏱️/🧰 메타 박스 + WHY/WHAT/HOW 영문 헤더 + 🧒 비유 박스 정리, 한국어 자연스러운 톤
* **`examples/README.md`** — Rev 1.1 / Rev 2.0 호환성 통합 안내 (41 개 예제 모두 빌드 호환, 외부 GPIO 핀맵만 리비전별 확인)

### Fixed

* **`docs/api-reference/04-external-io.md`** — 잘못된 ADC 핀 정보 (PA0/PA1) → 실제 (PB0/PB1/PF11/PF12) 로 정정, Rev 2.0 누락 보강
* **`.vscode/settings.json`** — 개발자 절대 경로 박힌 파일을 추적에서 제외 (`.template` 만 추적)

### Moved

* **`RELEASE_v2.1.1.md`** → **`docs/release-notes/v2.1.1.md`** (루트 정리, 향후 릴리즈도 동일 폴더로 일관성)
* 루트 README 에 **폴더 구조 시각 가이드 + 길 찾기 박스** 추가 — "어디부터 봐야 하지" 마찰 감소

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
* **SDK XM_FW 헤더 동기화**: 내부 개발 레포 원본과 완전 동기화
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
