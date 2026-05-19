# XM10 Extension Module — Claude Code Entry Point

> 이 파일은 **Claude Code 가 자동으로 읽는 안내문** 입니다. 학생/개발자가 이 레포를 처음 받았다면, 레포 디렉토리에서 Claude Code 를 실행하기만 해도 본 안내가 자동 로드됩니다.

---

## 🚀 처음 시작하는 학생 — 한 줄 안내

Claude Code 에 다음 중 하나를 입력하세요:

- `"처음 시작할게"`
- `"환경 구축 도와줘"`
- `"XM10 시작하려고 해"`
- `"/student-onboard"`

→ AI 가 **STM32CubeIDE 설치 → 프로젝트 import → 빌드 → 플래시 → LED 점등** 까지 6 단계로 자동 안내합니다.
Claude Code 미사용자는 [docs/getting-started/00-claude-code-quickstart.md](docs/getting-started/00-claude-code-quickstart.md) 의 수동 절차를 따라가세요.

---

## 레포 정체성

| 항목 | 값 |
|------|----|
| **모듈** | XM10 (eXtension Module 10) |
| **풀네임** | angel Robotics KIT H10 알고리즘 개발 플랫폼 |
| **MCU** | STM32H743XIH6 (Cortex-M7, 480 MHz, BGA265) |
| **OS** | FreeRTOS + CMSIS-OS2 |
| **레포 역할** | 학생/연구자 대상 **공개 릴리즈** (SDK + 예제 + 문서) |
| **개발 원본** | 사내 `ARC_ExtensionBoard` (private) — 본 레포는 동기화된 공개판 |
| **License** | MIT |
| **Latest** | [`v2.2.1`](https://github.com/AGR-EXO/Extension_Module/releases/tag/v2.2.1) (Releases → `Rev2.0.zip` / `Rev1.1.zip` 다운로드) |

---

## 학습 경로

본 레포는 다음 순서로 학습하도록 설계되었습니다 (각 단계 30 분 ~ 2 시간):

1. **[처음 시작 (AI 자동 안내)](docs/getting-started/00-claude-code-quickstart.md)** — Claude Code 가 환경 구축까지 동반
2. **[하드웨어 셋업](docs/getting-started/01-hardware-setup.md)** → **[소프트웨어 셋업](docs/getting-started/02-software-setup.md)** → **[첫 빌드](docs/getting-started/03-first-build.md)** — 수동 절차
3. **[Ex.00 Quick Start](examples/00_Quick_Start/)** — 보드 smoke test (⭐)
4. **[Ex.01~03 Button & LED](examples/01_Button_LED_Basic/)** — 디지털 IO 기본 (⭐~⭐⭐)
5. **[전체 학습 로드맵](docs/tutorials/README.md)** — 41 예제 트랙 (난이도 별 표시)

---

## AI 사용 안내 (Claude Code)

본 레포에는 다음 학생용 스킬이 정의되어 있습니다 (`.claude/skills/`):

- **`student-onboard`** — 신규 학생 환경 구축 자동화 (Phase 1~6: 설치 → import → 빌드 → 플래시 → LED 확인 → 핸드오프)
- **`example-helper`** — "Ex.XX 빌드 안 돼" / "LED 안 켜져" 등 예제 트러블슈팅

### 트리거되는 문구

학생이 다음 중 하나를 말하면 AI 가 자동으로 해당 스킬을 호출합니다:

| 의도 | 트리거 문구 예시 | 호출 스킬 |
|------|-----------------|----------|
| 환경 구축 | "처음 시작", "환경 구축", "XM10 시작", "수업에서 받았어", URL 공유 직후 | `student-onboard` |
| 예제 트러블 | "Ex.07 안 돼", "예제 빌드 실패", "LED 안 켜져", "CDC 연결 실패" | `example-helper` |

### AI 에게 추가 권한이 필요한 단계

`student-onboard` Phase 1 (CubeIDE 설치) 에서 다음 명령 권한이 1회 요청됩니다 — `Allow` 를 눌러주세요:
- `Bash(Start-Process *)` — STM32CubeIDE 다운로드 페이지 자동 오픈용
- `Bash(where *)` — 설치 검증용

거부 시 markdown 링크 fallback 으로 자동 전환됩니다.

---

## 절대 룰 (학생 / AI 모두 준수)

1. **한글·공백 경로 금지**
   - STM32CubeIDE 설치 경로, 본 레포 clone 경로 모두 영문/숫자/언더스코어만.
   - 권장: `C:\dev\Extension_Module`. 비권장: `C:\Users\홍길동\내 폴더\Extension_Module`.

2. **USB-CDC 포트 단일 점유**
   - PhAI Studio 와 시리얼 터미널 (PuTTY, RealTerm 등) 을 같은 COM 포트로 **동시에 열지 마세요**.
   - 같은 COM 포트 충돌 → 접속 실패 / 데이터 손실. (`examples/07~09` 헤더 `@warning` 참조)

3. **사용자 코드 영역**
   - 학생이 수정하는 곳: `XM_Apps/User_Algorithm/` 또는 `examples/<번호>_<이름>/*.c`
   - 라이브러리 (`XM_Lib`, `IOIF`, `AGR_MW`, 시스템 코드) 는 **봉인** — 수정 시 SDK 일관성 깨짐.

4. **HW Rev 호환**
   - `XM10_SDK/Rev1.1/` 와 `XM10_SDK/Rev2.0/` 은 HW 가 다른 **독립 SDK**.
   - 본인 보드 리비전을 보드 라벨 또는 [docs/hardware/README.md - 보드 리비전 비교](docs/hardware/README.md#보드-리비전-비교) 로 확인 후 진입. 다른 Rev 의 SDK 로 빌드 시 내장 버튼 MCU 핀이 한 칸 시프트되어 동작이 어긋남.

5. **bootloader 영역 비건드림**
   - 부트로더 (별도 region) 는 학생이 직접 flash 하지 않습니다. SWD 로 한 번 설치 후, 이후 FW 업로드는 PhAI Studio USB FTP. ([docs/bootloader/](docs/bootloader/))

---

## 다른 사내 도구와의 관계

| 도구 | 역할 | XM10 과의 관계 |
|------|------|---------------|
| **PhAI Studio** | 실시간 데이터 모니터링 + FW 업로드 | USB-CDC 로 본 보드와 통신 |
| **angel Sensor Studio** | 사내 진단/검증 GUI (Python/PySide6) | FES/EMG/IMU Hub 등 — XM10 과 CAN-FD 로 연계 가능 |
| **PythonDecoder** (`PythonDecoder/`) | USB MSC 로그 CSV 후처리 | 본 레포 내장 |

---

## 도움 받기

- 공식 문서: [docs/](docs/README.md)
- 트러블슈팅: [docs/troubleshooting.md](docs/troubleshooting.md)
- 이슈 제출: GitHub Issues (`.github/ISSUE_TEMPLATE/` 4종 제공 — bug / feature / hardware / config)
- 학습용 외부 자료: [onephai.com](https://onephai.com) (사내 온라인 학습 플랫폼)

---

## AI 동작 정책 (Claude Code 메타)

본 레포에서 Claude Code 는:
- 학생/개발자의 **사용자 코드 영역만** 수정합니다 (라이브러리 봉인).
- 빌드/플래시 명령은 **사용자 명시 호출 후에만** 실행합니다.
- 외부 다운로드는 **권한 프롬프트 1회 후** 진행합니다.
- 본 레포 외 외부 서비스 (GitHub, web)에 데이터를 전송하지 않습니다 — `WebFetch` 는 사용자 명시 요청 시에만.
