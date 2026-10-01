# AGENTS.md — XM10 Extension Module

> 보조 AI 진입점. **주 진입점은 [CLAUDE.md](CLAUDE.md)** — 이 파일은 핵심만 간추린 요약본입니다.

## 빠른 안내 (처음 시작하는 분)

이 레포는 angel Robotics XM10 보드의 알고리즘 개발 SDK + 예제 + 문서입니다. 처음 시작하는 분은 다음을 입력하세요:

- `"처음 시작할게"` / `"환경 구축 도와줘"` / `"/student-onboard"`

→ AI 가 STM32CubeIDE 설치 → 프로젝트 import → 빌드 → 플래시 → LED 점등까지 6 단계 자동 안내.

## 정체성

- **모듈**: XM10 (STM32H743XIH6, FreeRTOS)
- **레포 역할**: 공개 릴리즈 (SDK + 예제 + 문서)
- **License**: MIT

## 학습 경로 요약

1. [docs/getting-started/00-claude-code-quickstart.md](docs/getting-started/00-claude-code-quickstart.md) — AI 자동 안내
2. [examples/00_Quick_Start/](examples/00_Quick_Start/) — Hello board
3. [docs/tutorials/README.md](docs/tutorials/README.md) — 45 개 예제 전체 로드맵

## 절대 룰

1. 한글·공백 경로 금지 (CubeIDE 설치 경로 + SDK ZIP 압축 해제 경로 모두)
2. USB-CDC 단일 점유 — PC 프로그램(PhAI Studio · xm10 도구 · 시리얼 터미널)은 한 번에 하나만 연결하세요.
3. 사용자 코드 영역: `XM_Apps/Control_Task/` 또는 `examples/*/` 만 수정. XM_API 밖의 라이브러리·시스템 폴더는 봉인.
4. HW Rev 호환: `Rev1.1` vs `Rev2.0` 독립 SDK — 본인 보드 리비전 확인 필요
5. 부트로더 영역은 SWD 로 1회 flash 이후 사용자가 직접 건드리지 않음

## 문서 작성 원칙

- 이 레포의 문서·예제 README·커밋·PR 은 모두 공개됩니다. XM API(`XM_FW/XM_API/` 헤더에 선언된 것)만 안내합니다.
- 내부 구현·내부 용어 대신 결과를 쉬운 말로, 짧게 씁니다. (예: "CAN-FD 로 연결된 IMU Hub")
- 개발 중인 도구와 앞으로의 기능은 약속하지 않습니다. PhAI Studio 는 아직 개발 중이라, 직접 정의한 데이터 구조체(커스텀 구조체)는 우선 `xm10` 도구로 보고 저장하세요.

## AI 정책

- 본 레포에서 AI 는 사용자 코드 영역만 수정. 라이브러리·시스템 코드 봉인.
- 빌드/플래시는 사용자 명시 호출 시에만.
- 외부 다운로드는 권한 프롬프트 1회 후.

## 도움말

- 트러블슈팅: [docs/troubleshooting.md](docs/troubleshooting.md)
- API 레퍼런스: [docs/api-reference/](docs/api-reference/)
- 이슈: GitHub Issues (`.github/ISSUE_TEMPLATE/`)
