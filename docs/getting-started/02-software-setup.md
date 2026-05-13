# 02 — 개발 환경 구축

STM32CubeIDE 설치와 SDK ZIP 다운로드까지. 다운로드 시간 포함해 20 분 정도 걸립니다. [01 하드웨어 연결](01-hardware-setup.md) 이 끝났다는 전제로 시작합니다.

XM10 보드의 프로세서는 PC 와 종류가 다르기 때문에, C 코드를 보드용 기계어 (`.elf`) 로 바꿔주는 컴파일러 + 보드에 쓸 수 있는 드라이버 + 코드 편집기가 필요합니다. 이 세트를 통째로 묶어주는 게 ST 사의 무료 IDE 인 **STM32CubeIDE** 예요. 이거 하나만 깔면 끝납니다.

> Claude Code 사용자는 `"환경 구축 도와줘"` 한 줄로 이 페이지의 전 과정을 AI 가 자동 안내합니다 → [Claude Code 와 함께 시작](00-claude-code-quickstart.md)

---

## 설치할 것

| 항목 | 필수 여부 | 용도 | 다운로드 |
|------|---------|------|---------|
| STM32CubeIDE v2.0.0+ | 필수 | C 코드 → 보드 기계어 변환 + 디버거 | [st.com](https://www.st.com/en/development-tools/stm32cubeide.html) (ST 계정 무료 가입 필요) |
| 7-Zip | 선택 | ZIP 압축 해제 (Windows 기본 도구가 깨질 때 대안) | [7-zip.org](https://www.7-zip.org/) |

CubeIDE 가 컴파일러 + ST-Link USB 드라이버 + J-Link 드라이버까지 같이 깔아줍니다. git 은 필요하지 않습니다 — SDK 는 GitHub Releases 의 ZIP 으로 받습니다.

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

### 2. SDK ZIP 다운로드 + 압축 해제

본 SDK 는 **GitHub Releases 에서 per-Rev ZIP 으로 배포**됩니다. 본인 보드 리비전의 ZIP 만 받으면 됩니다 — git clone 불필요.

**a. 본인 보드 Rev 확인**:
- 보드 라벨 또는 박스에 `Rev 1.1` / `Rev 2.0` 표기
- 외관: RJ45 Ethernet 포트가 있으면 Rev 2.0, 없으면 Rev 1.1
- 모호하면 → [docs/architecture/](../architecture/) Rev 비교표

**b. Release 페이지에서 다운로드**:

📦 https://github.com/AGR-EXO/Extension_Module/releases/latest

Assets 섹션에서 본인 Rev 선택:
- **Rev 2.0** → `Rev2.0.zip`
- **Rev 1.1** → `Rev1.1.zip`

**c. 압축 해제 (PowerShell)**:

```powershell
New-Item -ItemType Directory -Force -Path C:\dev

# 다운로드한 ZIP 압축 해제 (Downloads 폴더 기준)
Expand-Archive -Path "$HOME\Downloads\Rev2.0.zip" -DestinationPath C:\dev\ -Force
```

✅ 검증:
```powershell
Test-Path C:\dev\Extension_Module\.project      # CubeIDE 프로젝트 파일
Test-Path C:\dev\Extension_Module\CLAUDE.md     # AI 진입점
```
둘 다 `True` 면 성공. 압축 풀린 폴더 (`C:\dev\Extension_Module\`) 자체가 **CubeIDE 가 Import 할 프로젝트 root** 이자 **Claude Code 가 켜질 진입 폴더** 입니다.

압축 푼 폴더 안에 다음이 모두 있어야 정상:

| 항목 | 역할 |
|------|------|
| `.project`, `.cproject`, `*.ld`, `startup_*.s` | CubeIDE 프로젝트 + 빌드 설정 |
| `CLAUDE.md` | AI 도구 자동 안내 진입점 (Rev 특화) |
| `.claude/skills/` | Claude Code 학생 온보딩/예제 트러블 스킬 |
| `docs/`, `examples/` | 학습 문서 + 41 개 실습 예제 |
| `Drivers/`, `XM_API/`, `XM_Apps/`, `XM_FW/`, `XM_Lib/` | SDK 코드 |
| `Middlewares/`, `FATFS/`, `LWIP/` (Rev 2.0 만) | HAL/CMSIS/STM32 미들웨어 |

> 💡 ZIP 내부 최상위 폴더명이 다르면 (`Extension_Module-Rev2.0/` 같은) `Get-ChildItem C:\dev\` 로 실제 이름을 확인하고 후속 경로를 조정하세요. 필요하면 `Rename-Item` 으로 `Extension_Module` 로 통일해도 됩니다.

### 3. VS Code settings.json 적용 (선택)

VS Code + clangd 환경을 쓰는 경우에만. 본 SDK 에는 `.vscode/settings.json.template` 가 포함되어 있어요 (개발자 PC 의 절대경로가 노출되지 않도록).

```powershell
Copy-Item C:\dev\Extension_Module\.vscode\settings.json.template `
          C:\dev\Extension_Module\.vscode\settings.json
```

복사 후 `settings.json` 을 열어 `<STM32CUBEIDE_INSTALL_PATH>` 를 실제 설치 경로로 바꾸면 됩니다. CubeIDE 만 쓰는 분은 이 단계 건너뛰세요.

---

## 자주 막히는 부분

### 압축 해제 경로

| 경로 예시 | 상태 |
|----------|------|
| `C:\dev\Extension_Module\` | 권장 |
| `C:\xm10\` | 권장 (가장 짧음) |
| `D:\Projects\Extension_Module\` | OK |
| `C:\Users\사용자\Documents\GitHub\Extension_Module\` | 주의 (한글 + 깊은 경로) |
| `C:\Users\...\OneDrive - 회사\...\Extension_Module\` | 오류 위험 (MAX_PATH 260 초과 + 클라우드 동기화 충돌) |

세 가지만 지키면 됩니다.

1. 드라이브 루트와 가까운 짧은 경로
2. 한글·공백·특수문자 피하기
3. OneDrive / iCloud 같은 클라우드 동기화 폴더 안에 두지 않기 (파일 잠금 충돌)

근본 해결법은 [Windows Long Path 활성화](../troubleshooting.md#경로-길이-문제-windows-max_path) 참조.

### 설치 / 다운로드

- **ST 다운로드 페이지에서 "no eligible files"** — ST 계정 로그인 안 됨. 학교 이메일로 무료 가입 가능합니다.
- **설치 중 백신이 차단합니다** — 백신을 잠시 끄고 재시도.
- **`where STM32CubeIDE.exe` 가 못 찾습니다** — PATH 등록이 안 된 상태. 시작 메뉴에서 한 번 실행한 뒤 다시 시도.
- **GitHub Releases 페이지가 안 열림** — 사내 프록시·방화벽이 차단. 다른 네트워크에서 시도하거나 IT 부서에 `github.com` 허용 요청.
- **`Expand-Archive` 가 일부 파일 누락 / 멈춤** — Windows 내장 압축 도구가 큰 ZIP 에서 깨질 수 있음. [7-Zip](https://www.7-zip.org/) 으로 압축 해제 재시도 권장.
- **잘못된 Rev ZIP 다운로드** — 보드 라벨 재확인 후 올바른 ZIP 재다운로드. Rev 끼리 호환되지 않습니다.

---

## 다음으로

CubeIDE 설치 + SDK 압축 해제가 끝났으면 → [03 첫 빌드 & 실행](03-first-build.md)
