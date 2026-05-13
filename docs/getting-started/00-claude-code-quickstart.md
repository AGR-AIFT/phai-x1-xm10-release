# Claude Code 와 함께 시작하기

임베디드도, STM32CubeIDE 도 처음이어도 괜찮습니다. AI 가 설치부터 보드 LED 점등까지 옆에서 안내해줘요. 5 분만 읽으면 어떻게 시작하는지 감이 잡힙니다.

> **SDK ZIP 을 아직 안 받은 상태여도 괜찮아요.** Claude Code 만 설치하고 임의의 빈 폴더에서 켠 다음, GitHub URL 만 알려주면 AI 가 ZIP 다운로드부터 같이 진행합니다. 자세한 흐름은 아래 [URL 만 받은 학생 (다운로드 전)](#url-만-받은-학생-다운로드-전) 섹션.

---

## 왜 AI 와 함께 진행하면 좋나

수동 가이드 (`01-hardware-setup.md` → `02-software-setup.md` → `03-first-build.md`) 도 잘 정리되어 있긴 하지만, 학생 입장에서는 몇 가지 불편이 있습니다.

- 어느 단계까지 왔는지 스스로 챙겨야 합니다.
- 에러가 나면 트러블슈팅 문서를 따로 찾아야 합니다.
- "한글 경로 금지" 같은 함정은 한 번 읽고 잊어버리기 쉽습니다.

Claude Code 를 쓰면 똑같은 내용이 대화로 진행됩니다. 매 단계 결과를 확인하기 전까지 다음으로 안 넘어가고, 막히면 그 자리에서 같이 원인을 짚어줍니다. 책으로 혼자 운전 배우기 vs 강사가 옆에 앉아 매번 봐주기 — 그 차이입니다.

---

## Claude Code 가 뭔가요

Anthropic 사가 만든 AI 코딩 도구입니다. 터미널 또는 VS Code 안에서 동작하고, 한국어로 "처음 시작할게" 라고만 말해도 이 SDK 의 `CLAUDE.md` 와 `.claude/skills/` 를 읽고 알아서 안내를 시작합니다.

- 설치: https://claude.com/claude-code
- Anthropic 계정으로 무료 체험 가능

### 이 SDK 에서 AI 가 해주는 일

| 단계 | AI 가 하는 일 | 내가 하는 일 |
|-------|--------------|--------------|
| 0 | OS / 한글 경로 / 권한 점검 명령 실행 | 결과 보고 |
| 1 | STM32CubeIDE 다운로드 페이지 자동 오픈, 설치 검증 | 설치 마법사 진행 |
| 2 | SDK ZIP 다운로드 페이지 안내 → 압축 해제 → CubeIDE Import 단계 안내 | 클릭 따라가기 |
| 3 | Build 명령 안내, `.elf` 생성 검증 | `Ctrl + B` |
| 4 | ST-Link 인식 확인, 플래시 단계 안내 | 보드 + 케이블 연결 |
| 5 | LED 점등 확인 질문 | 보드 보고 답하기 |
| 6 | Ex.00 으로 핸드오프 | 첫 코드 실험 |

세부 단계는 `.claude/skills/student-onboard/SKILL.md` 와 `phases/01~06-*.md` 에 있습니다.

---

## 사용법 (3 단계)

### 1. Claude Code 설치

공식 사이트의 안내를 따라가세요. Windows / macOS / Linux 모두 지원.
- https://claude.com/claude-code

### 2. SDK ZIP 다운로드 + 압축 해제

GitHub Releases 페이지에서 본인 보드 리비전의 ZIP 만 받습니다:

📦 https://github.com/AGR-EXO/Extension_Module/releases/latest

- **Rev 2.0 보드** → `Rev2.0.zip` (대부분 여기)
- **Rev 1.1 보드** → `Rev1.1.zip`

```powershell
# 한글 없는 짧은 경로 권장
New-Item -ItemType Directory -Force -Path C:\dev

# 다운로드한 ZIP 압축 해제 (Downloads 폴더 기준)
Expand-Archive -Path "$HOME\Downloads\Rev2.0.zip" -DestinationPath C:\dev\ -Force

cd C:\dev\Extension_Module
```

압축 푼 폴더 (`C:\dev\Extension_Module\`) 한 곳이 **모든 역할의 단일 진입점** 입니다:

- ✅ **Claude Code 진입 폴더** — `claude` 실행 시 이 폴더의 `CLAUDE.md` (Rev 특화) + `.claude/skills/` 자동 로드
- ✅ **CubeIDE Import root** — 이 폴더 자체를 import (`.project` 가 폴더 root 에 있음)
- ✅ **코드 작성 폴더** — `XM_Apps/User_Algorithm/user_app.c` 가 같은 폴더 안에서 바로 접근

> 💡 ZIP 내부 최상위 폴더명이 다르면 (`Extension_Module-Rev2.0/` 같은) `Get-ChildItem C:\dev\` 로 실제 이름 확인 후 `cd` 명령 조정하세요.

### 3. Claude Code 실행 + 자동 안내 트리거

압축 푼 폴더에서:
```powershell
claude
```

Claude Code 가 켜지면 `CLAUDE.md` 가 자동 로드됩니다. 학생이 다음 중 하나를 입력:

- `처음 시작할게`
- `환경 구축 도와줘`
- `XM10 시작하려고 해`
- `/student-onboard`

→ AI 가 Phase 0 부터 차례로 진행합니다. 매 단계 ✅ 체크포인트.

### URL 만 받은 학생 (다운로드 전)

학교에서 GitHub URL (`https://github.com/AGR-EXO/Extension_Module`) 만 받은 상태라면:

1. Claude Code 를 설치 후 임의 디렉토리에서 실행
2. AI 에게 다음을 입력:
   ```
   https://github.com/AGR-EXO/Extension_Module 이걸로 처음 시작하려고 해
   ```
3. AI 가 `WebFetch` 로 본 페이지를 읽고 → "본인 보드 Rev 가 무엇인가요?" 부터 시작 → Releases 페이지에서 본인 Rev ZIP 다운로드 단계로 안내

---

## 자주 막히는 부분

- **Claude Code 설치가 어렵습니다** — 운영체제별 설치 가이드는 Anthropic 공식 문서를 보세요. 학교/회사 PC 라면 관리자 권한이 필요할 수 있습니다.
- **AI 가 한국어를 못 알아듣나요?** — 한국어 입력 됩니다. 다만 명령어 자체 (`claude`, `Expand-Archive` 등) 는 영어로 입력하세요.
- **권한 프롬프트 (Allow/Deny) 가 무섭습니다** — AI 가 요청하는 권한은 브라우저 열기 (`Start-Process`), 설치 검증 (`where`), 압축 해제 (`Expand-Archive`) 정도예요. 모두 로컬·읽기 작업입니다.
- **AI 안내를 멈추고 직접 하고 싶어요** — 언제든 "여기까지 할게" 라고 말하면 멈춥니다. 수동 절차는 [01-hardware-setup.md](01-hardware-setup.md) 부터 똑같이 적혀 있습니다.
- **어디서 막혔는지 까먹었어요** — Claude Code 세션을 종료해도 `~/.xm10-onboard-done` sentinel 파일이 남아있어서 다음에 다시 켤 때 진행 위치를 추적합니다. 완전 처음부터 하려면 sentinel 을 삭제 (Phase 6 참조).

---

## 다음으로

- AI 와 함께 진행: Claude Code 실행 후 `"처음 시작할게"` 입력
- 수동 진행: [01 하드웨어 연결](01-hardware-setup.md) → [02 환경 구축](02-software-setup.md) → [03 첫 빌드](03-first-build.md)
- 환경 구축 끝났다면: [Ex.00 Quick Start](../../examples/00_Quick_Start/) → [학습 경로](../tutorials/README.md)
