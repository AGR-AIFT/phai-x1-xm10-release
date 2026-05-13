# XM10 Extension Module Rev 2.0 SDK — Claude Code Entry Point

> 이 파일은 **Claude Code 가 자동으로 읽는 안내문** 입니다. 본 SDK ZIP 을 압축 풀고 그 폴더에서 Claude Code 를 실행하면 자동 로드됩니다.

---

## 🚀 처음 시작하는 학생 — 한 줄 안내

Claude Code 에 다음 중 하나를 입력하세요:

- `"처음 시작할게"`
- `"환경 구축 도와줘"`
- `"XM10 시작하려고 해"`
- `"/student-onboard"`

→ AI 가 **STM32CubeIDE 설치 확인 → 프로젝트 Import → 빌드 → 플래시 → LED 점등** 까지 6 단계로 자동 안내합니다 (Phase 1 은 Studio 안내로 이미 완료된 경우 건너뜀).

Claude Code 미사용자는 [docs/getting-started/00-claude-code-quickstart.md](docs/getting-started/00-claude-code-quickstart.md) 의 수동 절차를 따라가세요.

---

## 이 SDK 의 정체성

| 항목 | 값 |
|------|----|
| **모듈** | XM10 Extension Module |
| **보드 리비전** | **Rev 2.0** (CAN-FD + Ethernet + PSRAM + RTC) |
| **MCU** | STM32H743XIH6 (Cortex-M7, 480 MHz, BGA265) |
| **OS** | FreeRTOS + CMSIS-OS2 |
| **이 ZIP 의 출처** | https://github.com/AGR-EXO/Extension_Module/releases |
| **License** | MIT |
| **본 SDK 의 역할** | 학생/연구자용 공개 릴리즈 — 본인 알고리즘을 `XM_Apps/User_Algorithm/` 또는 `examples/` 에 작성 |

---

## Rev 2.0 특화 사항 — Rev 1.1 과의 차이

| 항목 | Rev 2.0 (이 SDK) | Rev 1.1 (참고) |
|------|-----------------|---------------|
| **CAN-FD** | ✅ FDCAN1 / FDCAN2 | ✅ |
| **Ethernet (RJ45)** | ✅ LWIP 미들웨어 포함 | ❌ 없음 |
| **PSRAM** | ✅ 외부 PSRAM (memory-mapped) | ❌ |
| **RTC** | ✅ 내장 RTC + 배터리 백업 | ❌ |
| **USB Type-C** | ✅ C-to-C DRP 지원 (CDC/MSC 모두) | A-to-C + MSC 만 |
| **RAM_D2 여유** | 충분 | 99.21% 사용 (Rev 1.1 위험 주의) |

**Rev 2.0 만 활성화된 페리페럴**:
- ETH (LWIP) — `LWIP/` 미들웨어
- PSRAM — `XM_FW/Drivers/PSRAM/`
- RTC — `XM_FW/System/RTC/`

코드에서 `STM32H743xx` 매크로는 두 Rev 공통이지만, **Ethernet/PSRAM/RTC 페리페럴 코드는 Rev 2.0 SDK 에만 존재**합니다. Rev 1.1 학생이 본 SDK 를 잘못 쓰면 빌드 실패 또는 페리페럴 초기화 hang.

---

## 학습 경로

1. **[처음 시작 (AI 자동 안내)](docs/getting-started/00-claude-code-quickstart.md)** — Claude Code 가 환경 구축까지 동반
2. **[Ex.00 Quick Start](examples/00_Quick_Start/)** — 보드 smoke test (⭐)
3. **[Ex.01~03 Button & LED](examples/01_Button_LED_Basic/)** — 디지털 IO 기본 (⭐~⭐⭐)
4. **[전체 학습 로드맵](docs/tutorials/README.md)** — 41 예제 트랙
5. **[Rev 2.0 전용 예제]** — Ethernet (`05_USB_Connectivity` 외), PSRAM (`19_Memory_*`), RTC 활용

---

## CubeIDE 통합

본 ZIP 압축 푼 폴더 자체가 CubeIDE 프로젝트 root 입니다.

- **Import 대상**: 이 폴더 (`.project` / `.cproject` 가 ZIP root 에 있음)
- **빌드**: `Ctrl+B` (Post-Build 스크립트가 자동으로 `XM10_2_X_X_X.bin` 패키징 생성)
- **디버그/플래시**: `F11` (ST-Link 필요, Start address `0x08040000`)
- **PhAI Studio FW 업로드**: 패키징된 `XM10_2_X_X_X.bin` 을 PhAI Studio FTP 로 업로드

---

## AI 사용 안내 (Claude Code)

본 SDK 에 다음 학생용 스킬이 정의되어 있습니다 (`.claude/skills/`):

- **`student-onboard`** — 신규 학생 환경 구축 자동화 (Phase 1~6: 설치 → Import → 빌드 → 플래시 → LED 확인 → 핸드오프)
- **`example-helper`** — "Ex.XX 빌드 안 돼" / "LED 안 켜져" / "CDC 연결 실패" 등 예제 트러블슈팅

### 트리거되는 문구

| 의도 | 트리거 문구 예시 | 호출 스킬 |
|------|-----------------|----------|
| 환경 구축 | "처음 시작", "환경 구축", "XM10 시작", "수업에서 받았어" | `student-onboard` |
| 예제 트러블 | "Ex.07 안 돼", "예제 빌드 실패", "LED 안 켜져", "CDC 연결 실패" | `example-helper` |

---

## 절대 룰 (학생 / AI 모두 준수)

1. **한글·공백 경로 금지**
   - STM32CubeIDE 설치 경로, 본 ZIP 압축 해제 경로 모두 영문/숫자/언더스코어만.
   - 권장: `C:\dev\Extension_Module\`. 비권장: `C:\Users\홍길동\내 폴더\Extension_Module\`.

2. **USB-CDC 포트 단일 점유**
   - PhAI Studio 와 시리얼 터미널 (PuTTY, RealTerm 등) 을 같은 COM 포트로 **동시에 열지 마세요**.
   - 같은 COM 포트 충돌 → 접속 실패 / 데이터 손실. (`examples/07~09` 헤더 `@warning` 참조)

3. **사용자 코드 영역**
   - 학생이 수정하는 곳: `XM_Apps/User_Algorithm/` 또는 `examples/<번호>_<이름>/*.c`
   - 라이브러리 (`XM_FW`, `XM_Lib`, `Drivers/`, `Middlewares/`, `LWIP/`, `Compatible/`) 는 **봉인** — 수정 시 SDK 일관성 깨짐.

4. **HW Rev 호환**
   - 본 SDK 는 **Rev 2.0 전용** 입니다. Rev 1.1 보드에 본 SDK 빌드 결과물을 플래시하지 마세요.
   - 본인 보드 리비전은 PCB 라벨 또는 [docs/architecture/](docs/architecture/) 비교표로 확인.

5. **bootloader 영역 비건드림**
   - 부트로더 (별도 region, `0x08000000`) 는 학생이 직접 flash 하지 않습니다. SWD 로 한 번 설치 후, 이후 FW 업로드는 PhAI Studio USB FTP. ([docs/bootloader/](docs/bootloader/))

6. **Rev 2.0 특화 페리페럴 주의**
   - **PSRAM**: cache 정책 + memory-mapped 접근 시 D-Cache invalidate 필요. 직접 접근 시 `XM_FW/Drivers/PSRAM/` API 사용.
   - **ETH (LWIP)**: TX/RX descriptor 가 `.eth_data` 섹션에 배치. 임의 ETH 버퍼 위치 변경 금지.
   - **RTC**: 배터리 미장착 시 매 전원 인가 후 시간 재설정 필요.

---

## 다른 사내 도구와의 관계

| 도구 | 역할 | XM10 과의 관계 |
|------|------|---------------|
| **PhAI Studio** | 실시간 데이터 모니터링 + FW 업로드 | USB-CDC 로 본 보드와 통신 |
| **angel Sensor Studio** | 사내 진단/검증 GUI (Python/PySide6) | FES/EMG/IMU Hub 등 — XM10 과 CAN-FD 로 연계 가능 |
| **PythonDecoder** | USB MSC 로그 CSV 후처리 | 본 ZIP 에 포함 (있다면) |

---

## 도움 받기

- 공식 문서: [docs/](docs/README.md)
- 트러블슈팅: [docs/troubleshooting.md](docs/troubleshooting.md)
- 새 버전 확인: https://github.com/AGR-EXO/Extension_Module/releases
- 이슈 제출: https://github.com/AGR-EXO/Extension_Module/issues
- 학습용 외부 자료: [onephai.com](https://onephai.com)

---

## AI 동작 정책 (Claude Code 메타)

본 SDK 에서 Claude Code 는:
- 학생/개발자의 **사용자 코드 영역만** 수정합니다 (라이브러리 봉인).
- 빌드/플래시 명령은 **사용자 명시 호출 후에만** 실행합니다.
- 외부 다운로드는 **권한 프롬프트 1회 후** 진행합니다.
- 본 SDK 외 외부 서비스 (GitHub, web)에 데이터를 전송하지 않습니다 — `WebFetch` 는 사용자 명시 요청 시에만 (예: 최신 release 확인).
