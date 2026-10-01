# Changelog

모든 주요 변경 사항은 이 문서에 기록됩니다. [Semantic Versioning](https://semver.org/)을 따릅니다.

---

## [v2.8.0] — 2026-09-10

> **컴퓨터에서 XM10 데이터를 받아보는 도구가 새로 생긴 릴리즈.** 펌웨어 자체는 거의 바뀌지 않았고, 대신 SDK 안에 PC 에서 데이터를 받아 그래프로 보고 파일로 저장하는 `xm10` 도구가 새로 들어갔습니다. 보드가 없어도 전체 흐름을 먼저 체험해볼 수 있습니다.

### Added
* **PC 데이터 도구 `xm10`** — SDK 안 `PythonDecoder/` 에 새로 포함됩니다(v2.7.0 까지는 없었습니다). `xm10.py demo` 로 **보드 없이** 전체 흐름을 먼저 체험할 수 있고, `xm10.py recv` 로 실시간 그래프를 띄우거나(`--cli` 옵션으로 콘솔만), `xm10.py soak --minutes 30` 으로 장시간 수신 중 빠지는 데이터가 없는지 확인하고, `xm10.py export` 로 저장한 파일을 표(CSV)로 뽑을 수 있습니다. `build_exe.py` 로 파이썬 설치 없이 쓰는 실행 파일도 만들 수 있습니다(그래프 포함 55.6 MB / 그래프 제외 23.0 MB). 사용법은 `docs/getting-started/04-pc-data-tool.md` 에 정리했습니다.
* **받은 데이터를 그대로 저장하는 `.xmlog` 파일** — 값으로 풀기 전의 원본을 그대로 저장해 두고 나중에 `export` 명령으로 표로 뽑아낼 수 있습니다. 화면에 안 그리는 채널도 파일에는 전부 들어가고, 중간에 놓친 데이터는 그 자리에 표시가 남으며, 도중에 프로그램이 꺼져도 그 전까지 받은 내용은 읽을 수 있습니다. 그래프 창에는 "Save .xmlog" 체크박스가 기본으로 켜져 있습니다.
* **Total Data(0x20) 197 개 채널을 PC 에서 값으로 풀 수 있게 됨** — 지금까지는 정수와 소수가 섞여 있어 값이 깨졌는데, 이제 각 채널의 실제 형식대로 제대로 읽습니다. CSV 로 뽑으면 `leftHipAngle` 처럼 실제 채널 이름이 열 제목으로 나옵니다.
* **사용자 채널에 등록한 이름이 화면에 표시됨** — `XM_SetUsbCustomMeta` 로 이름을 등록해 보내면, 전에는 예전 수신 프로그램이 이를 쓰지 않고 `ch0, ch1…` 로만 보여줬는데 이제 그래프 제목과 CSV 열 제목에 등록한 이름이 그대로 나옵니다. 다만 값의 형식(정수인지 소수인지)까지는 아직 함께 오지 않아 소수로 가정해 표시하며, 화면과 표 양쪽에 "⚠ 타입 미상 — float32 가정" 으로 안내됩니다.

### Changed
* **SDK 안의 폴더 이름 2 개가 바뀌었습니다 (이번에는 Rev 1.1)** — `XM_FW/IOIF/` → **`XM_FW/phoundation-ioif/`**, `XM_FW/AGR_MW/` → **`XM_FW/phoundation-mw/`**. Rev 2.0 은 v2.7.0 에서 이미 바뀌었고, 이번에 Rev 1.1 도 같은 이름으로 맞췄습니다. 동봉된 `CMakeLists.txt` 와 `.cproject` 는 새 경로로 맞춰 두었으므로 **SDK 를 그대로 쓰시면 하실 일이 없습니다.** 본인 빌드 스크립트에 옛 경로를 직접 적어 두셨다면 바꿔 주세요. `#include "ioif_agrb_uart.h"` 처럼 파일명만 쓰는 방식은 영향받지 않습니다.
* **펌웨어 동작은 바뀌지 않았습니다** — 이번 릴리즈의 펌웨어 쪽 변경은 위 폴더 이름 정리와, 동작에 영향이 없는 내부 정리뿐입니다.

### Fixed
* **Ex.35 MultiLayer Transparent Control 의 채널 7 개가 값을 보내지 않던 문제** — 채널 이름은 등록해 두고 실제로 값을 보내는 호출을 빠뜨려서, PC 쪽에는 이름만 있고 값이 영영 안 오는 채널로 보였습니다. Rev 2.0 · Rev 1.1 양쪽 다 고쳤습니다.

### Notes
* **이번 PC 도구는 아직 실제 XM10 보드로 확인하지 못했습니다.** 준비된 샘플 데이터와 도구 자체 점검으로만 확인했습니다.
* **사용자 채널의 값 형식(정수/소수)은 아직 PC 로 전해지지 않습니다.** 지금 보드는 채널 이름만 보내므로 PC 는 항상 float32 로 가정합니다.
* 그래프를 뺀 실행 파일(`--no-gui`)은 실시간 화면 표시 경로까지는 확인하지 못합니다(그래프를 그리는 부분이 빠져 있어서입니다) — `selftest` 를 실행하면 이 점이 함께 안내됩니다.
* USB-CDC 를 쓰는 다른 예제들에는 채널 표기와 실제로 보내는 값이 어긋나는 문제가 없습니다.
* 삭제되거나 이름이 바뀐 공개 API 는 없습니다.

---

## [v2.7.0] — 2026-09-08

> **외부 장비와 시리얼로 통신할 수 있게 된 릴리즈.** External UART(PD5/PD6)를 XM_API 만으로 쓸 수 있는 범용 Serial 포트로 열었습니다. XM10 두 대를 직접 잇거나, 아두이노·PC·다른 MCU 와 자유로운 형식으로 통신할 수 있습니다. **SDK 안의 폴더 이름 두 개가 바뀌었으니(아래 Changed) 직접 include 경로를 쓰시던 분은 확인이 필요합니다.**

### Added
* **범용 Serial API (Rev 2.0)** — External UART(USART2, PD5/PD6)를 XM_API 로 개방했습니다. 5 개 함수: `XM_AttachExternalUart()` / `XM_SetExternalUartBaudrate()` / `XM_SendExternalUartData()`(논블로킹) / `XM_SendExternalUartDataBlocking()`(Control_Setup 전용) / `XM_EnsureExternalUartRxArmed()`. **하드웨어 설정은 이미 고정**되어 있습니다 — **USART2, TX=PD5(`EXT_UART_TX`), RX=PD6(`EXT_UART_RX`), 3.3 V, 921600 8N1, 흐름제어 없음**. 상대 장비를 이 값에 맞추세요. 속도만 `XM_SetExternalUartBaudrate()` 로 9600~921600 에서 바꿀 수 있고 **8N1 은 바꿀 수단이 없습니다**. 한 번에 보낼 수 있는 양은 **128 바이트**(`XM_EXT_UART_TX_MAX_BYTES`)입니다. GPIO 에는 **풀업/풀다운이 없습니다**.
* **Ex.43 External UART Ping-Pong (Rev 2.0 전용)** — XM10 두 대를 선으로 이어 서로 데이터를 주고받는 예제. 🛑 **Rev 1.1 보드에서는 이 배선을 하지 마세요** — Rev 1.1 에는 External UART 가 없고 **PD6 이 `USB_PWR_ON`(USB 전원 제어 출력)** 이라 상대 TX 와 출력끼리 맞부딪칩니다. 배선도(TX↔RX 교차·공통 GND·3.3V), 수신 콜백에서 복사만 하는 이유, 프레임 경계를 찾는 바이트 상태기계, 증상별 원인표까지 README 에 담았습니다.

### Changed
* **SDK 안의 폴더 이름 2 개가 바뀌었습니다 (Rev 2.0 만)** — `XM_FW/IOIF/` → **`XM_FW/phoundation-ioif/`**, `XM_FW/AGR_MW/` → **`XM_FW/phoundation-mw/`**. 동봉된 `CMakeLists.txt` 와 `.cproject` 는 이미 새 경로로 맞춰 두었으므로 **SDK 를 그대로 쓰시면 아무 것도 하실 게 없습니다.** 다만 본인 빌드 스크립트에 옛 경로를 직접 적어 두셨다면 바꿔 주세요. `#include "ioif_agrb_uart.h"` 처럼 파일명만 쓰는 방식은 영향받지 않습니다. Rev 1.1 SDK 는 종전 이름 그대로입니다.
* **센서 허브 연결 안정성 개선 (Rev 2.0)** — 센서 데이터가 몰릴 때 IMU·EMG 허브와의 CAN-FD 연결이 간헐적으로 끊기던 현상이 줄어들도록 바꿨습니다. ⚠️ **아직 실제 보드에서는 확인하지 못했습니다** — 아래 Notes 참고.
* **예제 개수** — Rev 2.0 **46 개**(Ex.43 추가) / Rev 1.1 42 개(변동 없음).

* **⚠️ Xsens MTi-630 API 가 기본 빌드에서 빠졌습니다** — `XM_AttachXsensMTi630()` / `XM_ConfigureXsensMTi630()` 를 쓰시던 분은 v2.7.0 에서 **컴파일 에러**가 납니다. External UART 는 포트가 하나뿐이라 범용 Serial API 와 Xsens 드라이버를 동시에 쓸 수 없습니다 — 그래서 빌드할 때 둘 중 하나만 고르게 막았습니다(조용한 오작동 대신 빌드 실패). `XM_FW/System/Config/module.h` 의 `XM_EXTERNAL_UART_XSENS_ENABLE` 을 `1` 로 바꾸고 다시 빌드하면 Xsens API 가 돌아오고, 대신 범용 Serial API 가 사라집니다. **기본값은 `0`(범용 Serial)** 입니다. Xsens 를 안 쓰시면 영향 없습니다.

### Fixed
* **IMU 채널 LED 가 늦게 꺼지던 문제** — 허브 연결이 끊겼을 때 LED 상태가 즉시 반영되지 않던 것을 고쳤습니다.
* **Ex.09 문서의 패킷 크기 표기** — 425 B 로 적혀 있던 Total Data 패킷 크기를 실제값 **365 B** 로 정정했습니다(동작 변경 없음, 표기만).
* **리눅스 환경 빌드** — 헤더 파일명 대소문자가 실제 파일과 어긋난 곳 7 군데를 정정했습니다. 대소문자를 구분하는 파일시스템(리눅스·macOS 일부)에서만 나던 컴파일 실패입니다.

### Notes
* **삭제되거나 이름이 바뀐 공개 API 는 없습니다.** 다만 위 Changed 의 Xsens 항목만 예외로, 기본 빌드에서 두 함수가 **선언되지 않습니다**(빌드 스위치로 되돌릴 수 있습니다). 그 외 기존 코드는 재빌드만 하면 됩니다.
* **센서 허브 연결 안정성 개선은 아직 실제 보드에서 확인하지 못했습니다.** IMU·EMG 허브를 쓰시는 분은 v2.6.0 대비 이 부분이 바뀌었다는 점을 알아 두시고, 이상이 있으면 알려 주세요.
* **Ex.43 도 아직 실제 보드에서 확인하지 못했습니다.** 두 보드가 실제로 바이트를 주고받는 것은 확인 전이며, 예제 README 에도 적어 두었습니다.
* **Rev 1.1 은 펌웨어 변경이 없습니다.** 버전만 v2.7.0 으로 맞췄습니다(SDK ZIP 은 항상 양 리비전을 함께 배포합니다).

---

## [v2.6.0] — 2026-08-20

> **Rev 1.1 · Rev 2.0 공통 안전성 릴리즈.** 안전 동작을 중심으로 손봐 "제어를 끄면 확실히 꺼지고, 펌웨어가 멈추면 스스로 되살아나는" 동작을 갖췄습니다. 삭제된 API 는 없으며 사용자 코드는 재빌드만 하면 됩니다 — 다만 **`MONITOR` 전환 동작과 `forwardVelocity` 값이 바뀌었습니다**(아래 Changed 참고). Rev 1.1 은 v2.5.0 이후 첫 업데이트입니다.

### Added
* **`XM_EmergencyDisengage()`** — 램프다운을 생략하고 즉시 토크 0 을 확정 전송합니다.
* **`XM_GetAppliedControlMode()`** — 요청(`XM_SetControlMode`)이 아닌 **실제 적용** 모드(`CONTROL` / `MONITOR` / `TRANSITION`)를 반환합니다.
* **`xm_api_safety.h`** — header-only 공통 안전 헬퍼. `XM_SafeTorque_Init/Reset/Step`(유한값 가드 + 클램프 + 진입 소프트스타트 + slew), `XM_SafeAssistLevel()`, `XM_SafeIsFresh()`. `xm_api.h` 가 이미 포함합니다.
* **하드웨어 워치독 (약 8 초)** — `Control_Loop` 가 1 kHz 로 돌며 갱신합니다. 초기화 단계에서 실패하면 그 자리에서 멈추고, 정상 동작 중에 멈춘 경우에만 보드가 스스로 재시작해 회복합니다.
* **Ext_Sync — 외부 동기화 TTL 입력** — Total Data 에 `sync_save_active` / `sync_din_level` / `sync_din_edge_count` 3 채널 추가. 기존 예약(reserved) 영역을 사용해 **패킷 크기 365 B 와 뒤쪽 채널 오프셋이 그대로**이므로 기존 수신 프로그램은 무수정으로 동작합니다.
* **진단 정보 확장 (Rev 2.0)** — 오류 알림 누적 횟수, 부팅 단계별 실패 정보, 태스크 스택 여유 값이 추가되었고, 펌웨어에 빌드한 Git 커밋 정보가 들어갑니다.

### Changed
* **`XM_SetControlMode(XM_CTRL_MONITOR)` 가 즉시 끊지 않고 안전하게 전환합니다** — 토크를 부드럽게 0 까지 내리고 → 0 을 여러 번 확실히 보내고 → P/I 벡터를 해제한 뒤 출력을 끕니다. 총 0.3~0.6 초 걸리며 그동안 사용자 토크 명령은 반영되지 않습니다. 이전에는 전송이 바로 끊겨 H10 에 **마지막 토크가 남아 있을 수 있었습니다.**
* **`MONITOR` 모드가 완전 차단이 되었습니다** — 토크·벡터 어떤 제어 명령도 전송하지 않습니다.
* **예제 15 종 안전 강화** — 켜는 순간 튀지 않는 소프트스타트·기울기 제한(Ex.14·21·27·30·33), 정지 스위치 단선 대응·출력 클램프·정지 램프·센서 끊김 타임아웃(Ex.06·13·17·26·32), 모드 전환 정리·호밍이 끝나기 전에 제어로 들어가는 경로 차단·전류(A)→토크(Nm) 이중 변환 제거(Ex.11·12·15·35·36). 예제 번호와 학습 내용은 그대로입니다. 별도로 Ex.03·08·31 은 설명 문구만 정정(동작 불변).
* **착용 전제 예제 4 종에 벤치 파라미터 안내 주석** (Ex.15·21·30·33) — 미착용 상태는 관성이 작아 진동/발산하므로 링크 단독 물성(0.184 kg / 0.1264 m)으로 치환하는 방법을 파일 헤더에 명시했습니다.
* **부트로더 바이너리 갱신** — 설정 영역이 손상돼도 스스로 복구해 정상 부팅합니다(신규). **v2.6.0 펌웨어는 구 부트로더에서도 동작하므로 재설치는 권장이지 필수가 아닙니다.**

### Fixed
* **`XM.status.h10.forwardVelocity` 60 배 과대 정정** — 분당→초당 환산 누락. 실보행 ~1 m/s 에서 1.0 부근 수신 확인. 이 값을 제어에 사용했다면 **게인 재조정이 필요**하며, 과거 데이터와 혼용하면 안 됩니다. 부수 효과로 **Ex.22 · 23 · 24 · 26 의 정지 판정(`forwardVelocity < 0.1`)이 이제 성립**합니다 — 종전에는 60 배 확대값이라 사실상 항상 "보행 중" 으로 처리됐습니다.
* **Ex.14 미분킥 제거** — PD 의 미분항을 오차가 아닌 **측정값**에 걸어(derivative-on-measurement) 목표 스텝 시 D 항이 튀던 원인을 제거했습니다. 목표가 일정한 구간에서는 기존과 수학적으로 동일합니다.
* **무한 재부팅 방지** — 초기화 실패 시 재부팅을 반복하던 문제를 고쳤습니다.
* **EMG 허브 데이터 길이 검증 보강** — 길이가 맞지 않는 데이터를 해석하기 전에 걸러냅니다.
* **USB-CDC 안정화 · USB 로거 무한 대기 해소 (Rev 2.0)** — USB-CDC 연결·전송 안정성을 개선했고, USB 로거가 끝없이 대기하던 문제를 해소했습니다.
* **문서 정정** — 착용 안전 수칙의 "워치독 없음" 문구를 "워치독이 있지만 비상 정지는 아니다" 로 바로잡았고, task 생성 가이드에 8 초 제약을 추가했으며, Ex.31 README 의 `rightHipTorque` 단위(전류 A → 관절 토크 Nm)를 정정했습니다.

### Notes
* 삭제된 공개 API 없음. 옛 이름 `XM_CTRL_TORQUE` 는 `XM_CTRL_CONTROL` 의 alias 로 유지됩니다.
* 예제 개수 변동 없음 (Rev 2.0 45 개 / Rev 1.1 42 개).
* **KIT H10 펌웨어 v2.4.0 동봉** (`SUIT_H10_Binary_20260820.zip`) — CM · MD 가 2.3.0 → 2.4.0, ESP32 는 2.3.0 그대로. 컨텐츠 파일(`SUIT_ContentsFiles_20260820.zip`)은 내용 동일, 파일명만 날짜 표기로 변경.

---

## [v2.5.1] — 2026-07-29

> **Rev 2.0 전용 긴급 수정 (Rev 1.1 영향 없음).** v2.4.1 · v2.5.0 의 Rev 2.0 펌웨어가 부팅 도중 멈추던 문제를 고쳤습니다 (LED 무반응 · PC 에 COM 포트 미출현). 기능 변경은 없으며, Rev 2.0 사용자는 재빌드·업로드만 하면 됩니다.

### Fixed
* **부팅 정지 수정 (Rev 2.0)** — 부팅 초반의 내부 설정이 제때 풀리지 않아 USB 준비 단계에서 멈추던 문제를 고쳤습니다. 증상은 LED 무반응 · PC 에 COM 포트 미출현이었습니다.
* **재발 방지** — 부팅을 시작할 때 같은 설정을 한 번 더 확인해, 비슷한 일이 생겨도 멈추지 않게 했습니다.

### Notes
* Rev 1.1 은 원인이 된 코드가 없어 이 결함의 영향을 받지 않습니다. `Rev1.1.zip` 은 v2.5.0 과 동일합니다.
* v2.4.0 이하는 해당 없습니다 (이 결함은 v2.4.1 에서 생겼습니다).

---

## [v2.5.0] — 2026-07-24

> **Rev 1.1 · Rev 2.0 공통.** USB 메모리(MSC)에 데이터를 저장하던 기능을 제거했습니다. 저장 속도 한계로 데이터가 조용히 누락될 수 있어(loop count 는 멀쩡해 보여 발견 어려움), 데이터 수집을 **USB-CDC 실시간 스트리밍(PhAI Studio / PythonDecoder CDC)** 으로 일원화했습니다.

### Removed
* **USB-MSC 파일 로깅 제거** — 파일 로깅 예제 5종(`10`/`10a`/`10b`/`10c`/`34`), 관련 public API(`XM_SetUsbLogSource` / `XM_StartUsbDataLog` / `XM_StopUsbDataLog` / `XM_GetUsbLogStats` / `XM_InsertUsbLogMarker` 등), "USB 메모리 로깅" 문서 페이지를 제거했습니다. CDC(실시간 스트리밍) API 는 전부 유지됩니다.

### Changed
* **예제 11 · 12 · 17 데이터 캡처 CDC 전환** — USB 메모리 세션 로깅 대신 USB-CDC 실시간 스트리밍(`XM_SetUsbStreamSource` + `XM_SetUsbAutoStream`)으로 전환했습니다.
* **예제 개수** — Rev 2.0 45 개 / Rev 1.1 42 개 (MSC 예제 5 종 제거 반영).

### Docs
* **데이터 수집 안내 CDC 일원화** — API 레퍼런스 · 튜토리얼 · AI 데이터 파이프라인을 USB-CDC 기준으로 정리하고, 삭제된 예제로 향하던 링크를 CDC(Ex.09)·PhAI Studio 로 연결했습니다. `PythonDecoder/README` 를 CDC 실시간 수신 샘플 전용으로 재작성했습니다.

---

## [v2.4.1] — 2026-07-24

> **Rev 2.0 전용** 패치 릴리즈. C 표준 라이브러리(실수 `printf` · `malloc`)가 Rev 2.0 에서 정상 동작하도록 고치고, 멀티태스크에서도 안전하게 했습니다. 새 예제/API 없음. Rev 1.1 은 v2.3.1 그대로.

### Fixed (펌웨어 — Rev 2.0)
* **`printf("%f", ...)` 실수 출력과 `malloc()` 이 정상 동작합니다** — 이전에는 `malloc()` 과 이를 쓰는 실수 `printf`(`%f`)가 조용히 실패했습니다. (Rev 1.1 은 애초에 영향 없음)
* **C 표준 라이브러리 멀티태스크 안전화** — `malloc` / `free` / `printf` 계열을 여러 태스크에서 동시에 불러도 안전합니다.

### Fixed (SDK 빌드)
* **CMake · CubeIDE 빌드 결과 일치** — CMake 로 빌드할 때 실수 `printf` 관련 설정이 빠지던 문제를 바로잡아, 두 빌드 방법이 동일한 결과를 내도록 했습니다.

### Changed (도구)
* **USB 로그 디코더가 독립 실행 파일로 바뀌었습니다** — 기존 `PythonDecoder/` 안의 파이썬 스크립트(MSC · Legacy) 대신, 파이썬 설치 없이 바로 쓰는 **XM10 Log Decoder** 설치 파일을 Releases 에 제공합니다. 임의의 사용자 로그 구조를 자동 인식하도록 범용화했습니다. 실시간 USB-CDC 수신 샘플(`PythonDecoder/CDC/`)은 그대로 유지됩니다.

### Docs
* **디버그 정지 안내 보강** — 디버그 시작 시 `main()` 정지 및 `Break at address "0x0800xxxx" ... no debug information`(부트로더 영역) 팝업이 정상 동작임을 트러블슈팅 문서에 추가했습니다.

---

## [v2.4.0] — 2026-07-21

> USB 시리얼 사용성을 개선한 **Rev 2.0 전용** 릴리즈. 일반 터미널에서 텍스트 예제가 보이지 않던 문제 해결 + USB 통신 모드 API 단순화. Rev 1.1 은 v2.3.1 그대로.

### Changed (동작 변경 — 주의)
* **USB 통신 모드 API 교체 (Rev 2.0, breaking)** — `XM_USB_GetMode()` / `XM_USB_SetMode()` / `XM_USB_RegisterModeChangeCallback()` 및 타입 `XM_USB_Mode_e` / `XM_USB_ModeChangeCb_t` **제거**. 대체: `XM_USB_SetHostProfile(XM_USB_HostProfile_e)` — 선택지 2개(`XM_USB_HOST_PHAI_STUDIO`(기본) / `XM_USB_HOST_TERMINAL`). 마이그레이션: `XM_USB_SetMode(XM_USB_MODE_TERMINAL)` → `XM_USB_SetHostProfile(XM_USB_HOST_TERMINAL)`. 미호출 코드는 기본 PhAI Studio 스트리밍으로 변경 없이 동작.

### Added
* **Ex.07 / Ex.08 텍스트 예제 터미널 프로파일 자동 지정** — `Control_Setup()` 에서 `XM_USB_SetHostProfile(XM_USB_HOST_TERMINAL)` 호출. 1kHz Total Data 자동 전송을 꺼 일반 시리얼 터미널(Tera Term · VS Code Serial Monitor · PuTTY)에 깨끗한 텍스트만 출력되며, 터미널 설정을 따로 바꿀 필요가 없습니다. 프로파일은 재접속에도 유지됩니다.

### Fixed (펌웨어)
* **GRF 센서 모듈 안정성 추가 개선** — 불필요한 리셋 반복을 줄였습니다 (v2.3.1 GRF 부팅 안정화 후속).
* **의도하지 않은 리셋 방지** — MCU 리셋 핀 처리를 정리해, 외부 요인으로 보드가 리셋될 여지를 없앴습니다.

### Fixed (SDK 빌드)
* **CubeIDE 빌드 안정화** — CubeMX 로 코드를 다시 생성해도 빌드가 깨지지 않도록 정리했습니다.

### Docs
* **부트로더 디버그 문서 보강** — 디버그 설정에 벡터 테이블 주소 `0x08040400` 명시(미지정 시 디버거가 엉뚱한 곳에서 멈추는 현상 방지), `main()` 자동 정지가 정상 동작임을 안내.

---

## [v2.3.1] — 2026-07-18

> 부팅·통신 안정성과 SDK 사용성을 다듬은 패치 릴리즈. 새 예제/API 없음.

### Changed (동작 변경 — 주의)
* **고관절 토크 단위** — `XM.status.h10.leftHipTorque`/`rightHipTorque` 가 모터 전류(A) 대신 **관절 토크(Nm)** 를 담습니다. 직접 환산(×0.085×18.75)하던 코드는 환산 제거 필요. Ex.31/34 예제 동반 갱신.

### Removed
* `XM_GetUserPSRAM()` / `XM_GetUserPSRAMSize()` 공개 API 제외 — 대체: `XM_GetUserWorkspace()`(200KB).

### Fixed (펌웨어)
* **GRF 센서 모듈 부팅 안정성** — 모듈을 꽂은 채 전원을 켤 때 반복 재부팅하던 현상을 완화했습니다. GRF 데이터 수신 누락도 없앴습니다.
* **CAN-FD 안정화** — 버스 오류 상태에서 스스로 회복하도록 보강했습니다.
* **USB** — 케이블 분리/재연결 처리를 개선했습니다.
* **저장** — 로그와 설정 저장 동작의 안정성을 높였습니다.
* **제어 안전** — Ex.06 리미트 스위치 입력 설정을 안전하게 바꾸고, 센서 데이터와 RTC 시각 설정 값을 더 엄격하게 검사합니다.

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
* **Ex.41 IMU Hub Dashboard** 🛑 Rev 2.0 전용 — IMU Hub Module 최대 6채널의 방위(쿼터니언)를 받아 오일러 각으로 변환, 연결 자동감지 + USB 로 18채널 스트리밍. (관찰형, 모터 없음)
* **Ex.42 EMG Hub Biofeedback** 🛑 Rev 2.0 전용 — EMG Hub Module의 근활성도를 받아 MVC 정규화 + LED 와 PC 화면 바이오피드백. (모터 없음)
* IMU Hub·EMG Hub 는 내부에서 개발 중인 모듈입니다. 사용하려면 https://huphailab.com/contact 로 문의해 주세요.

### Fixed (예제 동작)
* **Ex.28 Admittance** — 어드미턴스 토크가 좌/우 계산 완료 전에 인가되던 순서 정정(좌우 각각 계산 후 인가).
* **Ex.17 FSM Gait Intent** — 체중 파라미터 단위(g) 및 레벨 스케일 계수 정정.
* **Ex.12 Active Assist** — 앵커 각도 스케일(×10) 정정.
* **Ex.10 / 10c MSC Log** — 로그 시작 시점을 상태 진입(on_entry)으로 통일 + 시작 실패 시 안전 복귀.

### Changed
* **SDK 펌웨어·시스템 코드 최신화** — `libXM_Lib.a` 를 v2.3.0 펌웨어로 새로 빌드하고, 이전 버전 그대로 남아 있던 `Core/`·`Compatible/`·`LWIP/` 를 라이브러리와 같은 버전으로 맞췄습니다.
* **예제 갱신** — 진입점 주석(Control_Setup/Loop)·난이도 표기 일관화, Ex.40 을 Rev 2.0 SDK 번들에도 포함.
* **문서 갱신** — 예제 수 표기 갱신, Ex.41/42 카탈로그 반영, 문서 안의 오래된 경로 참조 정리.

### Documentation
* **API 참조 정확화** — P-Vector 목표각 스케일, task 우선순위표, RTC / PSRAM Rev 2.0 전용 표기 정정.
* **`version.h` 2.3.0 정정** — v2.2.1 / v2.2.2 패치 때 버전 숫자가 갱신되지 않아 펌웨어가 2.2.0 으로 표시되던 문제를 바로잡음.

### Compatibility
* Ex.40 / 41 / 42 는 **Rev 2.0 전용**(센서 허브 · 외부 전원 API). Rev 1.1 SDK 에는 포함되지 않습니다.
* 사용자 코드 / 공개 API 시그니처 변경 없음.

---

## [v2.2.2] — 2026-05-20

> SDK 안의 예제 코드에서 사용자가 헷갈릴 만한 부분 9 건을 정리한 패치 릴리즈. Ex.36 이 Rev 2.0 전용임을 4 곳에 일관 표기. 사용자 코드 / API / KIT H10 펌웨어 변경 없음.

### Fixed (예제 코드 정리)

* **Ex.31 Friction_Comp_DOB** — `MAX_TORQUE_NM` 이중 `#define` (`8.0f` → `5.0f`) 정리. 5.0 Nm 단일 정의로 통합, redefine 컴파일러 경고 제거.
* **Ex.08 CDC_Sensor_Print · Ex.13 Resistive_Mode** — 64 byte 스택 버퍼의 `sprintf` → `snprintf(buf, sizeof(buf), ...)` 로 교체 (버퍼를 조용히 넘쳐 쓸 위험 차단).
* **Ex.15 Inverted_Pendulum** — 디버그 라인의 고정소수점 출력에서 음수 부호가 사라지던 문제 (`-0.35` → `0.35` 로 표시되던) 수정. 부호를 `%s` prefix 로 분리.
* **Ex.33 Kinesthetic_Teaching** — loop-count 매크로 `RECORD_DOWNSAMPLE` 를 ms 임계값으로 재활용하던 부분을 `REPLAY_STEP_MS = 10U` 매크로로 분리.
* **Ex.36 OnDevice_Kinesthetic_Learning** — `Active_Entry` 에서 값을 명시적으로 초기화 (다시 들어올 때 곧바로 STANDBY 로 빠지던 가능성 차단) + 학습 task 와 제어 루프 사이 데이터 공유 주의점을 헤더 주석에 보강.
* **Ex.22 CPG_Oscillator** — AFO frequency 적응 식이 phase advance 이후의 `sin(φ)` 를 쓰던 한 스텝 lag 수정 (`sin(φ[k]) / cos(φ[k])` 미리 계산).
* **Ex.24 Virtual_Constraint** — 1 kHz 루프의 5 차 Bézier basis `powf` 12 회 호출 → 곱셈 체인으로 교체.
* **Ex.26 ILC** — 보행 주기 1 회 (≈ 1 Hz) 단위 호출임을 헤더 주석으로 명시 (1 kHz 루프 부담 오해 차단).
* **Ex.16 TinyAI Sensor_Fusion** — NN raw logit 을 `× 100` 으로 확률처럼 표시하던 부분 → 라벨 `Conf:%` → `Score:`, 단위 그대로 출력.

### Documentation

* **Ex.36 Rev 2.0 전용 표기** — 소스 헤더 `@warning` + `examples/36/README.md` 상단 🛑 배너 + `docs/troubleshooting.md` 신규 섹션 + `docs/tutorials/README.md` 예제 로드맵 표기. 4 곳 일관.
* **release-notes** — `docs/release-notes/v2.2.2.md` 신규.

### Compatibility

* **사용자 코드** — v2.2.1 코드 그대로 빌드 가능. `Control_Setup` / `Control_Loop` / 모든 API 변경 없음.
* **KIT H10 펌웨어** — v2.3.0 그대로.
* **부트로더** — v1.1.0 그대로.

---

## [v2.2.1] — 2026-05-19

> Rev 2.0 SDK 다운로드 직후 빌드 실패 (undefined reference 22 건) 수정. 사용자 코드 / API 변경 없음.

### Fixed

* **Rev 2.0 SDK link error 22 건** — 라이브러리 빌드 단계에서 일부 USB 통신 모듈의 소스가 누락되어 있던 문제. 새 `libXM_Lib.a` 로 교체했고 clean build 가 링크 오류 없이 통과하는 것을 확인했습니다.
* **Rev 1.1 SDK 안정성 보강** — 링크 실패는 없었지만, SDK 에서 빠져 있던 부분 2 건(함수 1 건, 설정 1 건)을 채웠습니다 (사용자 코드에는 영향 없음).

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
* **`docs/api-reference/09-task-creation.md`** — 보드 안 task 구성·우선순위와, 보조 task 를 넣는 방법을 한 그림으로 정리한 가이드
* **`docs/release-notes/v2.2.0.md`** — 본 릴리즈 노트

### Changed

* **폴더 이름** — `XM_Apps/User_Algorithm/` → `XM_Apps/Control_Task/`
* **`libXM_Lib.a` 양 Rev 재빌드** — 위 변경을 반영했습니다 (사용자가 따로 할 일 없음)
* **Ex.36 (OnDevice Kinesthetic Learning)** — 옛 `XM_BgTask_Create` → 새 `XM_Task_CreateOneShot` 마이그레이션

### Compatibility

* **기존 v2.1.1 코드** — 새 SDK 로 그대로 컴파일 가능 (옛 함수 이름·옛 API 자동 인식)
* **API surface** — Rev 1.1 / Rev 2.0 동일 (이전까지는 Rev 2.0 에만 일부 task API 존재)

---

## [unreleased — 2026-05] — Docs UX Overhaul

> 코드 변경 없음. 사용자 친화 문서 전면 개편 + Claude Code 온보딩 도구.

### Added

* **Claude Code 온보딩** — 처음 시작하는 분을 6 단계로 안내하는 `student-onboard`, 예제별 문제 해결을 돕는 `example-helper` (`CLAUDE.md`, `AGENTS.md` 포함)
* **`docs/getting-started/00-claude-code-quickstart.md`** — AI 자동 안내 진입 페이지
* **`docs/hardware/`** — 보드 외부 인터페이스 통합 안내 + Rev 1.1 / Rev 2.0 별 외부 GPIO 핀맵
* **`docs/advanced/ai-data-pipeline.md`** — 보드 데이터 → PyTorch / sklearn 학습 흐름 한 페이지 정리 (3 가지 길 + PhAI Studio 연동)
* **`docs/tutorials/README.md` 16 주 수업 진도표** — 한 학기 수업 운영용 참고 진도표
* **`docs/find-it.md`** — 키워드 → 페이지 빠른 찾기 인덱스 (자주 묻는 질문 통합)
* **`docs/release-notes/`** — 버전별 첨부 파일 + 호환성 매트릭스 (루트 `RELEASE_v*.md` 이전 위치)
* **`assets/img/README.md`** — 문서에 들어갈 이미지 자료 안내

### Changed

* **41 개 예제 README 통일** — 5 단계 lab manual 포맷 (목표 / 사전 지식 / 핵심 코드 / 실험 / 다음 단계 + 흔한 실수)
* **`docs/` 4-tier 재구성** — getting-started · tutorials · api-reference · architecture · advanced · bootloader · kit-h10-firmware · troubleshooting 사용자 친화 톤
* **루트 `README.md` 재설계** — Claude Code 우선 + 수동 3 단계 간단 명령
* **사용자 친화 용어 교체** — 내부 약어를 처음 보는 사람도 이해할 수 있는 자연스러운 표현으로 바꿨습니다
* **문서 톤 정리** — 장식용 박스와 영문 헤더를 걷어내고 자연스러운 한국어로 다듬었습니다
* **`examples/README.md`** — Rev 1.1 / Rev 2.0 호환성 통합 안내 (41 개 예제 모두 빌드 호환, 외부 GPIO 핀맵만 리비전별 확인)

### Fixed

* **`docs/api-reference/04-external-io.md`** — 잘못된 ADC 핀 정보 (PA0/PA1) → 실제 (PB0/PB1/PF11/PF12) 로 정정, Rev 2.0 누락 보강

### Moved

* **`RELEASE_v2.1.1.md`** → **`docs/release-notes/v2.1.1.md`** (루트 정리)
* 루트 README 에 **폴더 구조 시각 가이드 + 길 찾기 박스** 추가 — "어디부터 봐야 하지" 마찰 감소

---

## [v2.1.1] — 2026-04-04

> v2.1.0 의 문제 수정 + 디버깅 복원 + 보조 task API(`xm_api_freertos`) + Ex.35~36 추가

### Fixed (from v2.1.0)

* **실행 중 메모리 손상 수정** — v2.0.1 에서 고쳤던 문제가 v2.1.0 에서 다시 생겨, 링커 설정을 복원해 바로잡았습니다
* **디버깅 복원** — `libXM_Lib.a` 에 디버그 심볼을 다시 넣어 Live Expression 과 브레이크포인트가 동작합니다
* **CMakeLists.txt 파손 방지 (Rev2.0)** — 빌드할 때 SDK 의 `CMakeLists.txt` 가 망가질 수 있던 문제를 막았습니다
* **Rev1.1 헤더 정리** — `xm_api.h` 에서 Rev2.0 전용 헤더(`xm_api_memory.h`, `xm_api_rtc.h`) include 를 제거했습니다

### Added (from v2.1.0)

* **xm_api_freertos.h/c** — 백그라운드 태스크 API
* **Ex.35 MultiLayer Transparent Control** — 다층 투명 제어
* **Ex.36 OnDevice Kinesthetic Learning** — 온디바이스 동작 학습
* Ex.11/12 homing 튜닝: accel 4→2 deg/s², IVectorKpKd (6,1)→(6,6)

---

## [v2.1.0] — 2026-04-02 ⚠️ Pre-Release — v2.1.1 사용 권장

### Highlights

* **새 부트로더 최초 도입** — PhAI Studio 로 USB 를 통해 펌웨어 업로드(USB FTP), 자동 백업/롤백, CRC-32 검증
* **듀얼 HW 리비전 SDK 동시 배포** — `XM10_SDK/Rev1.1/` + `XM10_SDK/Rev2.0/` 폴더 구조
* **예제 42개** — 입문부터 Physical AI 고급 제어까지 완전한 학습 경로

### Added

* **부트로더 지원**
  * 빌드하면 부트로더용 펌웨어 헤더가 자동으로 붙고, 마무리 단계가 업로드용 파일을 만듭니다 (Python 필요)
  * 최종 출력: `XM10_X_X_X_X.bin` (PhAI Studio 업로드용 패키징 바이너리)
  * 부트로더 매뉴얼: [docs/bootloader/README.md](docs/bootloader/README.md)
* **Rev2.0 SDK 신규**
  * Ethernet, PSRAM (8MB), RTC, LED 드라이버 지원
  * 신규 XM API: `xm_api_memory.h` (PSRAM/Workspace), `xm_api_rtc.h` (RTC 시간 관리)
* **예제 대규모 확장 (20개 → 42개)**
  * Physical AI 토크 제어 시리즈 (Ex.20~33): Impedance, Gravity Comp, CPG, ILC, MRAC, Admittance, Bilateral, DOB, Kinesthetic Teaching 등
  * Gait Analysis 로깅 (Ex.34): H10 보행 데이터 자동 수집 + Python 디코더
* **Total Data Packet (365B)**: CAN-FD 채널 1·2 의 통신 상태 진단 값, `xm_loop_count` 도입
* **PhAI Studio 연동 강화**
  * Total Data Packet (Module ID 0x20) 시스템 자동 전송 (1kHz)
  * User Custom 채널 (0xF0~0xFE): `XM_SetUsbCustomMeta()` + `XM_SendUsbDataWithId()`
  * Auto-Stream 모드 (별도 시작 명령 불필요)

### Changed

* **SDK 폴더 구조**: `XM10_SDK/Extension_Module/` → `XM10_SDK/Rev1.1/` + `XM10_SDK/Rev2.0/`
* **`libXM_Lib.a` 를 최적화 빌드(디버그 심볼 없음)로 전환** — 이 때문에 Live Expression·브레이크포인트가 동작하지 않아 v2.1.1 에서 되돌렸습니다
* **CubeMX 6.13+ 호환** — 새 CubeMX 로 만든 프로젝트와도 호환되도록 시작 코드를 보강했습니다

### Fixed

* **펌웨어 헤더 누락 수정** — 빌드한 펌웨어에 부트로더가 확인하는 헤더가 빠지던 문제를 고쳤습니다

### Removed

* `user_app.c` 루트 복사본 (`XM_Apps/User_Algorithm/`에서만 관리)

### Compatibility

| 컴포넌트 | 최소 버전 | 권장 버전 |
|----------|----------|----------|
| XM10 부트로더 | v1.1.0 | v1.1.0 |
| KIT H10 CM | v2.3.0 | v2.3.0+ |
| KIT H10 ESP32 | v2.3.0 | v2.3.0+ |
| KIT H10 SAM10/MD | v2.3.0 | v2.3.0+ |
| STM32CubeIDE | v1.13.2 | v1.14.1+ |
| Python | 3.8+ | 3.12+ |
| PhAI Studio | — | 최신 ([studio.onephai.com](https://studio.onephai.com)) |

---

## [v2.0.1] — 2026-03-09

### Fixed

* **런타임 크래시 수정** — `libXM_Lib.a` 를 다시 빌드해, 실행 중 메모리가 손상되어 멈추던 문제를 고쳤습니다
* **SDK 링커 설정 수정** — CubeIDE 빌드에서도 같은 문제가 없도록 링커 설정을 바로잡았습니다
* **SDK XM_FW 헤더 동기화**: 헤더를 최신으로 맞춤
* **CMake 로 빌드하는 도구 추가**

### Note

* `libXM_Lib.a` 는 디버그 심볼이 포함된 빌드로 제공됩니다.
* v2.0.0의 libXM_Lib.a는 동작하지 않습니다. **반드시 v2.0.1을 사용하세요.**

---

## [v2.0.0] — 2026-02-24 ⚠️ Deprecated — v2.0.1 사용 권장

### Breaking Changes

* **통신 방식 전면 교체**
  * 기존 Links 기반 통신 코드는 v2.0.0과 호환되지 않습니다.
  * 마이그레이션 필요: `Links_*` API → `XM_*` API로 전환
* **하드웨어 접근 방식 변경**
  * 기존 직접 HAL 호출 코드는 SDK 가 제공하는 함수로 전환 필요
* **XM_FW 정적 라이브러리(libXM_Lib.a)로 제공**
  * 사용자는 `XM_Apps/User_Algorithm/user_app.c`만 수정
  * XM_FW 소스 코드 직접 수정 불가 (헤더만 제공)

### Added

* **디바이스 자동 검색 및 구성:** 연결된 센서 허브·모듈을 자동으로 찾아 구성
* **IMU Hub Module 디바이스 드라이버:** IMU 센서 허브 연동 지원
* **USB CDC 개선:** PhAI Studio 와의 USB 통신을 개선했습니다
* **USB MSC 개선:** 자동 타임스탬프, 롤링 파일, 구조체 등록 기반 로깅
* **XM API 모듈화:**
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
* 매크로 재정의 경고 제거

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
