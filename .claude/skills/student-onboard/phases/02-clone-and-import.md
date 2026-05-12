# Phase 2 — 레포 Clone + CubeIDE Import

> 📌 **이 Phase 가 끝나면**: CubeIDE Project Explorer 에 `Extension_Module` 프로젝트가 표시됩니다.
> ⏱️ 예상 시간: 10 분
> 🧰 사전 조건: Phase 1 통과 (CubeIDE 설치 검증)

## 💡 WHY — 왜 clone 위치가 중요한가

XM10 SDK 는 깊은 폴더 구조 (`XM10_SDK/Rev2.0/Extension_Module/...`) 라 Windows 경로 길이 제한 (MAX_PATH = 260자) 에 쉽게 닿습니다. 그래서 clone 위치는 가능한 **얕고 (drive 직하 권장), 영문 짧게** 잡아야 합니다.

> 🧒 비유: 책장이 너무 깊으면 안쪽 책에 손이 안 닿는 것과 같음.

## 📖 WHAT — 무엇을 하나

1. `git clone` 으로 본 레포를 로컬에 복사
2. CubeIDE 의 **Import Existing Projects** 기능으로 프로젝트 인식 시키기

## 🔧 HOW — 단계별 진행

### 1. clone 위치 결정 + 실행

권장 위치: `C:\dev\`

```powershell
# 폴더가 없다면 생성
New-Item -ItemType Directory -Force -Path C:\dev
cd C:\dev

# clone
git clone https://github.com/AGR-EXO/Extension_Module.git

# 결과 확인
Test-Path C:\dev\Extension_Module\README.md
```

✅ `True` 가 출력되면 통과.

### 2. STM32CubeIDE 실행 + workspace 선택

- 시작 메뉴 → STM32CubeIDE 실행
- 첫 실행 시 workspace 경로 묻습니다: `C:\dev\stm32-workspace` 권장 (한글 X)
- "Use this as default and do not ask again" 체크 → Launch

### 3. 프로젝트 Import

CubeIDE 메뉴:
- `File` → `Import...` → `General` → `Existing Projects into Workspace` → `Next`
- `Select root directory` → `Browse` → 본인 보드 리비전 폴더 선택:
  - **Rev 2.0 보드** (최신): `C:\dev\Extension_Module\XM10_SDK\Rev2.0\Extension_Module\`
  - **Rev 1.1 보드**: `C:\dev\Extension_Module\XM10_SDK\Rev1.1\Extension_Module\`
- 자동으로 프로젝트 1개 감지됨 → 체크박스 활성화 확인 → `Finish`

### 4. 검증

✅ CubeIDE 좌측 **Project Explorer** 패널에 `Extension_Module` 항목이 표시되어야 합니다.
✅ 클릭해서 펼치면 `Application/`, `Drivers/`, `Middlewares/`, `XM_API/`, `XM_Apps/` 등이 보입니다.

## ⚠️ 흔한 실수 / 막혔다면

- **clone 실패: "fatal: unable to access"** → 인터넷/프록시 문제. VPN 끄고 재시도.
- **`Test-Path` 가 False** → clone 이 다른 경로로 됨. `pwd` 로 현재 위치 확인 후 재시도.
- **CubeIDE 가 프로젝트를 못 잡음** → `XM10_SDK/Rev2.0/Extension_Module/` 안에 `.project` `.cproject` 파일이 있어야 함. 없으면 잘못된 폴더 선택.
- **워크스페이스 진입 시 메시지: "Workspace in use"** → 다른 CubeIDE 인스턴스가 열려있음. 모두 종료 후 재시작.
- **MAX_PATH 에러** ("Filename too long") → clone 위치를 더 짧게: `C:\xm10\` 같은.

## ➡️ 다음 단계

✅ Phase 2 완료. Phase 3 (빌드) 로 진행:
→ [03-build-firmware.md](03-build-firmware.md)
