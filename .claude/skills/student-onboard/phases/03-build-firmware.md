# Phase 3 — 펌웨어 빌드

> 📌 **이 Phase 가 끝나면**: `Debug/Extension_Module.elf` 파일이 생성됩니다.
> ⏱️ 예상 시간: 5~10 분 (첫 빌드 시 5분, 이후는 수십 초)
> 🧰 사전 조건: Phase 2 통과 (Project Explorer 에 프로젝트 표시)

## 💡 WHY — 왜 빌드가 필요한가

학생이 작성한 C 코드는 그냥 텍스트입니다. 보드의 MCU 는 텍스트를 못 읽고, **기계어 (binary)** 만 실행합니다. **빌드** 는 C → 기계어 변환 (컴파일 + 링크) 과정이고, 최종 산출물이 `.elf` (Executable and Linkable Format) 파일입니다.

> 🧒 비유: 한국어 책 (C 코드) 을 영어 책 (.elf) 으로 번역하는 작업. 보드는 영어만 읽음.

## 📖 WHAT — 무엇이 만들어지나

- `Debug/Extension_Module.elf` — 디버그 정보 포함 (학생용 권장)
- `Debug/Extension_Module.bin` / `.hex` — 디버그 정보 없는 raw 바이너리 (양산 배포용)
- Build console 로그 — 컴파일 에러/경고 + 메모리 사용량 (`text data bss`)

## 🔧 HOW — 단계별 진행

### 1. 빌드 명령

CubeIDE 메뉴:
- `Project` → `Build All` (단축키 `Ctrl + B`)
- 또는 Project Explorer 에서 `Extension_Module` 우클릭 → `Build Project`

### 2. 진행 상황 관찰

- 하단 **Console** 패널에 컴파일 로그가 실시간 출력됩니다
- 첫 빌드는 수백 개 파일을 컴파일하므로 5 분 가량 소요
- 우상단 진행률 표시 (`Build Workspace: 30% ...`)

### 3. 성공 검증

빌드가 끝나면 Console 마지막 줄에 다음 형태가 보여야 합니다:

```
   text    data     bss     dec     hex filename
 XXXXXX   YYYYY   ZZZZZ  AAAAAA  BBBBBB Extension_Module.elf

13:23:45 Build Finished. 0 errors, N warnings. (took XmYs)
```

✅ **"0 errors"** 가 핵심. warning 은 있어도 OK.

파일 검증:
```powershell
Test-Path C:\dev\stm32-workspace\Extension_Module\Debug\Extension_Module.elf
```
✅ `True` → 통과.

## ⚠️ 흔한 실수 / 막혔다면

- **`fatal error: 'xxx.h' file not found`** → 프로젝트 Include Path 설정 누락. Project Explorer 우클릭 → `Properties` → `C/C++ Build` → `Settings` 에서 include 경로 확인.
- **`undefined reference to 'xxx'`** → 링커 에러. 라이브러리 (`.a`) 파일 누락. SDK 폴더 구조 손상되었을 가능성 → Phase 2 부터 다시 clone.
- **`region 'RAM' overflowed by N bytes`** → 사용자 코드가 메모리 한도를 초과. `XM_Apps/` 에서 큰 배열/구조체를 줄여야 함. 첫 빌드 시에는 거의 발생 안 함.
- **빌드가 너무 오래 걸림** → 백신 SW 가 임시 파일 스캔. CubeIDE workspace 폴더를 백신 예외에 추가.
- **`Build Finished. 1 errors`** → Console 상단으로 스크롤하여 빨간색 에러 메시지 확인 → 그 메시지를 그대로 AI 에 붙여넣기 ("이 에러 뭐야?").

## ➡️ 다음 단계

✅ Phase 3 완료. Phase 4 (보드 플래시) 로 진행:
→ [04-flash-firmware.md](04-flash-firmware.md)
