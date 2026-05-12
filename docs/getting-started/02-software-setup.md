# 02 — 개발 환경 구축

> 📌 **이 페이지를 읽고 나면**: STM32CubeIDE 설치 + 본 레포 clone 까지 완료됩니다.
> ⏱️ 예상 학습 시간: 20분 (다운로드 시간 포함)
> 🧰 사전 지식: [01. 하드웨어 연결](01-hardware-setup.md) 완료

---

## 💡 WHY — 왜 이 도구들이 필요한가

XM10 의 MCU (STM32H743) 는 ARM Cortex-M7 — 학생 PC 의 x86 과 다른 아키텍처입니다. **C 코드 → ARM 기계어 (.elf)** 로 변환하려면 ARM GCC 컴파일러 + 보드에 쓰기 위한 ST-Link 드라이버 + 코드 편집기가 필요합니다.
이 세트를 통째 묶어 제공하는 것이 ST 사의 무료 IDE **STM32CubeIDE** 입니다.

> 🧒 비유: 영문 책 (.elf) 을 한국어 책 (C 코드) 으로 쓰려면 영영사전 + 한영사전 + 종이가 다 있어야 함. CubeIDE = 세트.

> 🤖 **AI 와 함께 진행하기**: Claude Code 사용자는 `"환경 구축 도와줘"` 한 줄로 본 페이지의 전 과정을 AI 가 자동 안내합니다. → [00-claude-code-quickstart](00-claude-code-quickstart.md)

---

## 📖 WHAT — 무엇을 설치하나

| 항목 | 용도 | 다운로드 |
|------|------|---------|
| **STM32CubeIDE v2.0.0+** | C 코드 → ARM 기계어 변환 + 디버거 | [st.com](https://www.st.com/en/development-tools/stm32cubeide.html) (ST 계정 무료 가입 필요) |
| **GitHub Desktop** (선택) | 본 레포 clone + 업데이트 관리 GUI | [desktop.github.com](https://desktop.github.com/) |
| **Git for Windows** (대안) | CLI 로 clone 하는 경우 | [git-scm.com](https://git-scm.com/download/win) |

함께 자동 설치되는 것: ARM GCC 컴파일러, ST-Link USB 드라이버, J-Link 옵션 드라이버.

---

## 🔧 HOW — 단계별 진행

### 1단계 — STM32CubeIDE 설치

1. ST 공식 페이지 접속 → v2.0.0 이상 선택 → ST 계정 로그인 → Windows 버전 다운로드
2. 설치 마법사 진행 — **반드시 다음 항목 확인**:

| 항목 | 권장 | 비권장 |
|------|------|--------|
| 설치 경로 | `C:\ST\STM32CubeIDE_x.y.z\` (기본) | 경로에 한글/공백 (`C:\내 도구\`) |
| 사용자 권한 | 관리자 권한 | 일반 사용자 (일부 드라이버 설치 실패) |
| ST-Link 드라이버 | 함께 설치 ✅ | 체크 해제 시 보드 인식 실패 |
| 백신 SW | 알림 발생 시 허용 | 차단 시 다운로드 중단 |

3. 설치 후 확인:
```powershell
where STM32CubeIDE.exe
```
✅ 경로가 출력되면 성공.

### 2단계 — 레포 clone

**옵션 A — GitHub Desktop (GUI 친화)**:
1. [GitHub Desktop 설치](https://desktop.github.com/) + 로그인
2. 브라우저에서 [Extension_Module 레포](https://github.com/AGR-EXO/Extension_Module) → `<> Code` → `Open with GitHub Desktop`
3. **Local Path** 를 **얕고 한글 없는 경로** 로 지정 (`C:\dev\`, `C:\xm10\` 등) → `Clone`

**옵션 B — Git CLI (PowerShell)**:
```powershell
New-Item -ItemType Directory -Force -Path C:\dev
cd C:\dev
git clone https://github.com/AGR-EXO/Extension_Module.git
```

✅ 검증:
```powershell
Test-Path C:\dev\Extension_Module\README.md
```
`True` 가 출력되면 성공.

### 3단계 — VS Code settings.json 적용 (선택, VS Code + clangd 사용 시)

본 레포는 `.vscode/settings.json.template` 를 제공합니다 (개발자 PC 절대경로 누출 방지).

```powershell
Copy-Item C:\dev\Extension_Module\.vscode\settings.json.template `
          C:\dev\Extension_Module\.vscode\settings.json
```

그 후 `settings.json` 을 열고 `<STM32CUBEIDE_INSTALL_PATH>` 를 실제 설치 경로로 변경.

CubeIDE 만 사용하는 학생은 이 단계를 건너뛰어도 됩니다.

---

## ⚠️ 흔한 실수 / 막혔다면

### clone 경로 관련

| 경로 예시 | 상태 |
|----------|------|
| `C:\dev\Extension_Module\` | ✅ 권장 |
| `C:\xm10\` | ✅ 권장 (가장 짧음) |
| `D:\Projects\Extension_Module\` | ✅ OK |
| `C:\Users\사용자\Documents\GitHub\Extension_Module\` | ⚠️ 주의 (한글 + 깊은 경로) |
| `C:\Users\...\OneDrive - 회사\...\Extension_Module\` | ❌ 오류 가능 (MAX_PATH 260 초과) |

**규칙:**
1. 드라이브 루트에 가까운 짧은 경로
2. 한글 · 공백 · 특수문자 금지
3. OneDrive / iCloud 등 클라우드 동기화 폴더 피하기 (파일 잠금 충돌)

근본 해결: Windows Long Path 활성화 → [troubleshooting](../troubleshooting.md#경로-길이-문제-windows-max_path)

### 설치 / 다운로드 관련

- **ST 다운로드 페이지에서 "no eligible files"** → ST 계정 로그인 안 됨. 학교 이메일로 무료 가입
- **설치 도중 백신 차단** → 백신 SW 임시 비활성화 후 재시도
- **`where STM32CubeIDE.exe` 가 못 찾음** → PATH 미등록. 시작 메뉴에서 한 번 실행 후 다시 시도
- **GitHub Desktop 로그인 실패** → 2FA 활성화 시 personal access token 필요
- **clone 시 SSL 에러** → 사내 프록시. `git config --global http.sslVerify false` (보안 우려, 일시적 우회만)

---

## ➡️ 다음 단계

✅ 두 도구 설치 + clone 완료 → [03. 첫 빌드 & 실행](03-first-build.md) 으로 진행
