# Phase 2 — SDK ZIP 다운로드 + CubeIDE Import

> 📌 **이 Phase 가 끝나면**: CubeIDE Project Explorer 에 `Extension_Module` 프로젝트가 표시됩니다.
> ⏱️ 예상 시간: 10 분
> 🧰 사전 조건: Phase 1 통과 (CubeIDE 설치 검증)

## 💡 WHY — 왜 ZIP 다운로드 + 압축 해제 위치가 중요한가

XM10 SDK 는 GitHub Releases 에서 **per-Rev ZIP** 으로 배포됩니다 (`Rev1.1.zip` / `Rev2.0.zip`). 본인 보드 리비전 ZIP 만 받으면 됩니다 — git clone 으로 전체 레포를 받을 필요가 없습니다.

ZIP 안에는 **본인 Rev SDK 한 벌이 ZIP root 에 평탄화** 되어 있습니다 (`.project`, `CLAUDE.md`, `.claude/`, `docs/`, `examples/`, SDK 코드 모두 ZIP root 에 함께). 그래서 압축 푼 폴더 자체가 곧 CubeIDE Import root + Claude Code 진입 폴더입니다.

압축 해제 위치도 중요합니다 — 한글·공백 없는 짧은 경로 (`C:\dev\Extension_Module\`) 를 권장합니다. Windows 경로 길이 제한 (MAX_PATH = 260자) 회피 + CubeIDE 빌드 출력 경로 안전 확보.

> 🧒 비유: 책장이 너무 깊으면 안쪽 책에 손이 안 닿는 것과 같음.

## 📖 WHAT — 무엇을 하나

1. 본인 보드 리비전의 SDK ZIP 을 GitHub Releases 에서 다운로드
2. `C:\dev\` 같은 짧은 영문 경로에 압축 해제
3. CubeIDE 의 **Import Existing Projects** 기능으로 프로젝트 인식 시키기

## 🔧 HOW — 단계별 진행

### 1. 보드 리비전 확인

먼저 본인 보드의 리비전을 확인하세요:
- **보드 라벨**: PCB 표면 또는 박스 라벨에 `Rev 1.1` 또는 `Rev 2.0` 표기
- **외관 차이**: RJ45 Ethernet 포트가 있으면 Rev 2.0, 없으면 Rev 1.1
- 모호하면 → [docs/architecture/](../../../docs/architecture/) Rev 비교표

### 2. SDK ZIP 다운로드

GitHub Releases 페이지에서 본인 Rev 의 ZIP **만** 받습니다:

📦 다운로드: https://github.com/AGR-EXO/Extension_Module/releases/latest

- **Rev 2.0 보드** → `Rev2.0.zip` (대부분 여기)
- **Rev 1.1 보드** → `Rev1.1.zip`

AI 가 다운로드 페이지를 자동 오픈할 수 있습니다 (권한 1회):

```powershell
Start-Process "https://github.com/AGR-EXO/Extension_Module/releases/latest"
```

- `Allow` → 기본 브라우저에서 Release 페이지 열림 → Assets 섹션에서 본인 Rev ZIP 클릭
- 거부 시 위 URL 을 직접 클릭

### 3. 압축 해제

권장 위치: `C:\dev\` (한글·공백 없음, 짧음, 클라우드 동기화 X)

```powershell
# 작업 폴더 준비
New-Item -ItemType Directory -Force -Path C:\dev

# 다운로드한 ZIP 위치 (보통 Downloads 폴더)
$zip = "$HOME\Downloads\Rev2.0.zip"   # Rev1.1 보드면 Rev1.1.zip 으로 변경

# 압축 해제
Expand-Archive -Path $zip -DestinationPath C:\dev\ -Force

# 결과 확인 — 압축 푼 폴더 자체가 CubeIDE 프로젝트 + AI 진입점
Test-Path C:\dev\Extension_Module\.project      # CubeIDE 프로젝트 파일
Test-Path C:\dev\Extension_Module\CLAUDE.md     # AI 진입점 (Rev 특화)
```

✅ 둘 다 `True` 면 통과. 이 폴더 (`C:\dev\Extension_Module\`) 가 곧 **CubeIDE Import root** 이자 **Claude Code 가 켜질 진입 폴더** 입니다.

> 💡 ZIP 은 Rev 별로 평탄화되어 배포됩니다. Rev2.0.zip 을 풀면 Rev 2.0 SDK 한 벌이 ZIP root 에 그대로 들어있고, Rev1.1.zip 도 마찬가지로 Rev 1.1 SDK 만 들어있습니다. ZIP root 폴더 이름이 다르면 (예: `Extension_Module-Rev2.0/`) `Get-ChildItem C:\dev\` 로 실제 이름 확인 후 `Rename-Item` 으로 `Extension_Module` 로 통일.

### 4. STM32CubeIDE 실행 + workspace 선택

- 시작 메뉴 → STM32CubeIDE 실행
- 첫 실행 시 workspace 경로 묻습니다: `C:\dev\stm32-workspace` 권장 (한글 X)
- "Use this as default and do not ask again" 체크 → Launch

### 5. 프로젝트 Import

CubeIDE 메뉴:
- `File` → `Import...` → `General` → `Existing Projects into Workspace` → `Next`
- `Select root directory` → `Browse` → **압축 해제한 폴더 자체** 선택:
  - 권장: `C:\dev\Extension_Module\` (`.project` / `.cproject` 가 이 폴더 root 에 있음)
- 자동으로 프로젝트 1개 감지됨 → 체크박스 활성화 확인 → `Finish`

> 💡 본 ZIP 은 Rev 별로 평탄화 배포라 **Rev 폴더 안으로 들어갈 필요 없습니다**. ZIP root 가 곧 SDK root. (사내 개발 레포의 `XM10_SDK/Rev*/Extension_Module/` 깊은 구조와는 다릅니다.)

### 6. 검증

✅ CubeIDE 좌측 **Project Explorer** 패널에 `Extension_Module` 항목이 표시되어야 합니다.
✅ 클릭해서 펼치면 `Application/`, `Drivers/`, `Middlewares/`, `XM_API/`, `XM_Apps/` 등이 보입니다.

## ⚠️ 흔한 실수 / 막혔다면

- **ZIP 다운로드 실패** → 인터넷/프록시 문제. VPN/사내 프록시 끄고 재시도. 또는 다른 네트워크에서 시도.
- **`Test-Path` 가 False** → 압축 해제 위치가 예상과 다름. `Get-ChildItem C:\dev\` 로 실제 폴더명 확인 후 경로 조정.
- **CubeIDE 가 프로젝트를 못 잡음** → 압축 푼 폴더 root 에 `.project` / `.cproject` 가 있어야 함 (`Test-Path C:\dev\Extension_Module\.project` → True). False 면 ZIP 손상 또는 잘못된 ZIP → 재다운로드.
- **워크스페이스 진입 시 메시지: "Workspace in use"** → 다른 CubeIDE 인스턴스가 열려있음. 모두 종료 후 재시작.
- **MAX_PATH 에러** ("Filename too long") → 압축 해제 위치를 더 짧게: `C:\xm10\` 같은.
- **잘못된 Rev ZIP 다운로드** → 보드 라벨 재확인 후 올바른 ZIP 재다운로드. Rev 끼리 호환 안 됨.
- **Expand-Archive 가 멈춤 / 일부 파일 누락** → Windows 내장 압축이 깨질 수 있음. [7-Zip](https://www.7-zip.org/) 으로 재시도 권장.

## ➡️ 다음 단계

✅ Phase 2 완료. Phase 3 (빌드) 로 진행:
→ [03-build-firmware.md](03-build-firmware.md)
