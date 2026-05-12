# 02 — 개발 환경 구축

STM32CubeIDE 설치와 레포 clone 까지. 다운로드 시간 포함해 20 분 정도 걸립니다. [01 하드웨어 연결](01-hardware-setup.md) 이 끝났다는 전제로 시작합니다.

XM10 보드의 프로세서는 PC 와 종류가 다르기 때문에, C 코드를 보드용 기계어 (`.elf`) 로 바꿔주는 컴파일러 + 보드에 쓸 수 있는 드라이버 + 코드 편집기가 필요합니다. 이 세트를 통째로 묶어주는 게 ST 사의 무료 IDE 인 **STM32CubeIDE** 예요. 이거 하나만 깔면 끝납니다.

> Claude Code 사용자는 `"환경 구축 도와줘"` 한 줄로 이 페이지의 전 과정을 AI 가 자동 안내합니다 → [Claude Code 와 함께 시작](00-claude-code-quickstart.md)

---

## 설치할 것

| 항목 | 용도 | 다운로드 |
|------|------|---------|
| STM32CubeIDE v2.0.0+ | C 코드 → 보드 기계어 변환 + 디버거 | [st.com](https://www.st.com/en/development-tools/stm32cubeide.html) (ST 계정 무료 가입 필요) |
| GitHub Desktop (선택) | GUI 로 레포 clone + 업데이트 관리 | [desktop.github.com](https://desktop.github.com/) |
| Git for Windows (대안) | 커맨드라인으로 clone 하는 경우 | [git-scm.com](https://git-scm.com/download/win) |

CubeIDE 가 컴파일러 + ST-Link USB 드라이버 + J-Link 드라이버까지 같이 깔아줍니다.

---

## 단계

### 1. STM32CubeIDE 설치

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

### 2. 레포 clone

**옵션 A — GitHub Desktop (GUI)**:
1. [GitHub Desktop 설치](https://desktop.github.com/) + 로그인
2. 브라우저에서 [Extension_Module 레포](https://github.com/AGR-EXO/Extension_Module) → `<> Code` → `Open with GitHub Desktop`
3. Local Path 를 짧고 한글 없는 경로로 지정 (`C:\dev\`, `C:\xm10\` 등) → Clone

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

### 3. VS Code settings.json 적용 (선택)

VS Code + clangd 환경을 쓰는 경우에만. 이 레포에는 `.vscode/settings.json.template` 가 있어요 (개발자 PC 의 절대경로가 노출되지 않도록).

```powershell
Copy-Item C:\dev\Extension_Module\.vscode\settings.json.template `
          C:\dev\Extension_Module\.vscode\settings.json
```

복사 후 `settings.json` 을 열어 `<STM32CUBEIDE_INSTALL_PATH>` 를 실제 설치 경로로 바꾸면 됩니다. CubeIDE 만 쓰는 분은 이 단계 건너뛰세요.

---

## 자주 막히는 부분

### Clone 경로

| 경로 예시 | 상태 |
|----------|------|
| `C:\dev\Extension_Module\` | 권장 |
| `C:\xm10\` | 권장 (가장 짧음) |
| `D:\Projects\Extension_Module\` | OK |
| `C:\Users\사용자\Documents\GitHub\Extension_Module\` | 주의 (한글 + 깊은 경로) |
| `C:\Users\...\OneDrive - 회사\...\Extension_Module\` | 오류 위험 (MAX_PATH 260 초과) |

세 가지만 지키면 됩니다.

1. 드라이브 루트와 가까운 짧은 경로
2. 한글·공백·특수문자 피하기
3. OneDrive / iCloud 같은 클라우드 동기화 폴더 안에 두지 않기 (파일 잠금 충돌)

근본 해결법은 [Windows Long Path 활성화](../troubleshooting.md#경로-길이-문제-windows-max_path) 참조.

### 설치 / 다운로드

- **ST 다운로드 페이지에서 "no eligible files"** — ST 계정 로그인 안 됨. 학교 이메일로 무료 가입 가능합니다.
- **설치 중 백신이 차단합니다** — 백신을 잠시 끄고 재시도.
- **`where STM32CubeIDE.exe` 가 못 찾습니다** — PATH 등록이 안 된 상태. 시작 메뉴에서 한 번 실행한 뒤 다시 시도.
- **GitHub Desktop 로그인 실패** — 2단계 인증을 켜뒀다면 personal access token 이 필요합니다.
- **clone 할 때 SSL 에러** — 사내 프록시 환경일 가능성. `git config --global http.sslVerify false` 는 임시 우회용이지 보안상 권장하지는 않습니다.

---

## 다음으로

두 도구 설치 + clone 이 끝났으면 → [03 첫 빌드 & 실행](03-first-build.md)
