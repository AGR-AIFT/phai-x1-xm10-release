# Phase 1 — STM32CubeIDE 설치

> 📌 **이 Phase 가 끝나면**: `where STM32CubeIDE.exe` 가 설치 경로를 출력합니다.
> ⏱️ 예상 시간: 10~20 분 (다운로드 시간 포함)
> 🧰 사전 조건: Phase 0 통과 (한글 경로 X, 관리자 권한 OK)

## 💡 WHY — 왜 CubeIDE 가 필요한가

XM10 보드는 STM32H7 MCU 를 씁니다. MCU 코드를 작성하고, 컴파일하고, 보드에 다운로드하려면 전용 통합 개발 환경 (IDE) 이 필요합니다.
STMicroelectronics 가 무료로 제공하는 **STM32CubeIDE** 가 그 도구입니다. ARM 컴파일러 + Eclipse 기반 에디터 + ST-Link 디버거 드라이버까지 한 번에 설치됩니다.

> 🧒 비유: 자동차 정비소처럼 — 차를 고치려면 렌치·진단기·리프트가 한 세트로 필요. CubeIDE 가 그 세트.

## 📖 WHAT — 무엇을 설치하나

- **STM32CubeIDE** (Windows v2.0.0 이상 권장)
- 함께 자동 설치되는 것: ARM GCC 컴파일러, ST-Link USB 드라이버, J-Link 드라이버 옵션

다운로드 페이지: https://www.st.com/en/development-tools/stm32cubeide.html
(ST 계정 무료 가입 필요 — 학교 이메일로 가입 가능)

## 🔧 HOW — 단계별 진행

### 1. 이미 설치되어 있는지 먼저 확인

```powershell
where STM32CubeIDE.exe
```

- ✅ 경로가 출력되면 → Phase 2 로 건너뛰기
- ❌ "INFO: 파일을 찾을 수 없습니다" → 다음 단계 진행

### 2. 다운로드 페이지 자동 오픈 (AI 가 실행)

Claude Code 가 다음 명령을 실행합니다 (권한 프롬프트 1회):

```powershell
Start-Process "https://www.st.com/en/development-tools/stm32cubeide.html"
```

- `Allow` 를 눌러주세요. 기본 브라우저에서 ST 다운로드 페이지가 열립니다.
- 거부 시 fallback: [STM32CubeIDE 다운로드 (수동)](https://www.st.com/en/development-tools/stm32cubeide.html) 링크를 직접 클릭.

### 3. 설치 시 반드시 지킬 것

| 항목 | 권장 | 비권장 |
|------|------|--------|
| 설치 경로 | `C:\ST\STM32CubeIDE_x.y.z\` (기본) | 경로에 한글/공백 (`C:\프로그램\…`) |
| 사용자 계정 | 관리자 권한 | 일반 사용자로 설치 시 일부 드라이버 실패 |
| ST-Link 드라이버 | 함께 설치 ✅ | 체크 해제 시 보드 인식 실패 |
| 방화벽 | 설치 중 알림 허용 | 차단 시 ST 서버 통신 실패 |

### 4. 설치 후 검증

```powershell
where STM32CubeIDE.exe
```

✅ 경로가 출력되면 통과. 안 나오면 PATH 등록 누락 — 시작 메뉴에서 한 번 실행하면 PATH 가 잡힙니다.

## ⚠️ 흔한 실수 / 막혔다면

- **다운로드가 안 됨** → ST 계정 미가입. 가입 후 재시도.
- **설치 도중 멈춤** → 백신 SW 가 차단. 잠시 비활성화 후 재시도.
- **`where` 가 못 찾음** → 설치는 됐는데 PATH 미등록. 시작 메뉴 → STM32CubeIDE 한 번 실행 → 다시 `where` 시도.
- **한글 경로 사용자** → `C:\Users\홍길동\…` 자체는 OK (CubeIDE 설치 폴더가 한글이면 안 됨). 그래도 워크스페이스는 한글 없는 경로 (`C:\dev\`) 권장.
- **"This installer requires .NET …" 메시지** → Windows .NET Runtime 누락. ST 페이지의 "Prerequisites" 따라 설치.

## ➡️ 다음 단계

✅ Phase 1 완료. Phase 2 (SDK ZIP 다운로드 + 프로젝트 import) 로 진행:
→ [02-download-and-import.md](02-download-and-import.md)
