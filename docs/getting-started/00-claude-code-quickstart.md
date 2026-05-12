# 00 — Claude Code 와 함께 시작하기 🤖

> 📌 **이 페이지를 읽고 나면**: AI 자동 안내로 STM32CubeIDE 설치 + 보드 LED 점등까지 한 번에 도달할 수 있습니다.
> ⏱️ 예상 학습 시간: 5 분 (읽기) + 30~60 분 (실제 진행)
> 🧰 사전 지식: 없음 (MCU/CubeIDE 경험 0 도 OK)

---

## 💡 WHY — 왜 AI 안내가 더 좋은가

기존 환경 구축 가이드 (`01-hardware-setup.md` → `02-software-setup.md` → `03-first-build.md`) 는 사람이 순서대로 읽고 따라하도록 작성되었습니다. 잘 되어 있지만:

- 학생이 **어느 단계에서 멈춰있는지 추적 안 됨** — 본인이 직접 챕터를 찾아야 함
- 에러가 나면 **트러블슈팅 문서를 별도로 열어야 함**
- "한글 경로 금지" 같은 함정을 **읽고 넘기기 쉬움**

**Claude Code (AI 코딩 도구)** 를 쓰면 같은 내용이 대화형으로 진행되고, 매 단계 결과를 학생이 확인할 때까지 다음 단계로 넘어가지 않습니다. 막히면 그 자리에서 진단합니다.

> 🧒 비유: 운전 학원에서 책으로 혼자 배우기 vs. 강사가 옆에 앉아 매번 확인해주기.

---

## 📖 WHAT — Claude Code 는 무엇이고, 무엇을 해주나

### Claude Code 란

Anthropic 사가 만든 **AI 코딩 도구**. 터미널 또는 VS Code 안에서 동작하고, 학생이 한국어로 "처음 시작할게" 라고만 입력하면 본 레포의 `CLAUDE.md` 와 `.claude/skills/` 를 자동으로 읽어 환경 구축을 진행합니다.

- 설치: https://claude.com/claude-code
- 무료 체험 가능 (Anthropic 계정)

### 본 레포에서 AI 가 자동으로 해주는 일

| Phase | AI 가 하는 일 | 학생이 하는 일 |
|-------|--------------|--------------|
| 0 | OS/한글 경로/권한 점검 명령 실행 | 결과 보고 |
| 1 | STM32CubeIDE 다운로드 페이지 자동 오픈, 설치 검증 | 설치 마법사 진행 |
| 2 | clone 명령, CubeIDE Import 단계 안내 | 클릭 따라가기 |
| 3 | Build 명령 안내, `.elf` 생성 검증 | `Ctrl + B` |
| 4 | ST-Link 인식 확인, 플래시 단계 안내 | 보드 + 케이블 연결 |
| 5 | LED 점등 확인 질문 | 보드 보고 답하기 |
| 6 | Ex.00 Quick Start 핸드오프 | 첫 코드 실험 |

전체 단계 상세: `.claude/skills/student-onboard/SKILL.md` 와 `phases/01~06-*.md`

---

## 🔧 HOW — 사용법 (3 단계)

### 1. Claude Code 설치

공식 사이트의 안내를 따라가세요. Windows / macOS / Linux 모두 지원.
- https://claude.com/claude-code

### 2. 본 레포 위치로 진입

```powershell
# 한글 없는 짧은 경로 권장
mkdir C:\dev
cd C:\dev
git clone https://github.com/AGR-EXO/Extension_Module.git
cd C:\dev\Extension_Module
```

> 💡 Git 도 없다면? https://git-scm.com/download/win

### 3. Claude Code 실행 + 자동 안내 트리거

본 레포 디렉토리에서:
```powershell
claude
```

Claude Code 가 켜지면 `CLAUDE.md` 가 자동 로드됩니다. 학생이 다음 중 하나를 입력:

- `처음 시작할게`
- `환경 구축 도와줘`
- `XM10 시작하려고 해`
- `/student-onboard`

→ AI 가 Phase 0 부터 차례로 진행합니다. 매 단계 ✅ 체크포인트.

### URL 만 받은 학생 (clone 전)

학교에서 GitHub URL (`https://github.com/AGR-EXO/Extension_Module`) 만 받은 상태라면:

1. Claude Code 를 임의 디렉토리에서 실행
2. AI 에게 다음을 입력:
   ```
   https://github.com/AGR-EXO/Extension_Module 이걸로 처음 시작하려고 해
   ```
3. AI 가 `WebFetch` 로 본 페이지를 읽고 → "어디에 clone 할까요?" 부터 시작

---

## ⚠️ 흔한 실수 / 막혔다면

- **"Claude Code 설치가 어려워요"** — 운영체제 별 설치 가이드는 Anthropic 공식 문서 참조. 학교 PC 라면 관리자 권한 필요할 수 있음.
- **"AI 가 한국어를 이해 못 해요"** — Claude Code 는 한국어 입력 OK. 다만 명령어 자체 (예: `claude`, `git`) 는 영어.
- **"권한 프롬프트 (Allow/Deny) 가 무서워요"** — 본 레포에서 AI 가 요청하는 권한은 `Start-Process` (브라우저 열기), `where` (설치 검증), `git clone` (레포 복사) 정도. 모두 안전한 read/공개 작업.
- **"AI 안내를 멈추고 직접 하고 싶어요"** — 언제든지 학생이 "여기까지 할게" 라고 말하면 AI 가 멈춤. 수동 절차는 `01-hardware-setup.md` ~ `03-first-build.md` 에 동일하게 적혀있음.
- **"Phase 어디서 막혔는지 까먹었어요"** — Claude Code 세션을 종료해도 `~/.xm10-onboard-done` sentinel 파일은 유지됩니다. AI 가 다시 시작 시 Phase 위치 추적 가능. 완전 처음부터 다시 하려면 sentinel 삭제 (Phase 6 참조).

---

## ➡️ 다음 단계

- **AI 와 함께**: Claude Code 실행 후 `"처음 시작할게"` 입력 → Phase 1 자동 진행
- **수동 진행 (AI 미사용)**: [01-hardware-setup.md](01-hardware-setup.md) → [02-software-setup.md](02-software-setup.md) → [03-first-build.md](03-first-build.md)
- **환경 구축 완료 후**: [Ex.00 Quick Start](../../examples/00_Quick_Start/) → [학습 로드맵](../tutorials/README.md)
