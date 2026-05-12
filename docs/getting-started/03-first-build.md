# 03 — 첫 빌드 & 실행

> 📌 **이 페이지를 읽고 나면**: 보드에서 첫 펌웨어가 실행되고 LED 가 점등됩니다.
> ⏱️ 예상 학습 시간: 20분
> 🧰 사전 지식: [01. 하드웨어 연결](01-hardware-setup.md) + [02. 개발 환경 구축](02-software-setup.md) 완료

---

## 💡 WHY — 왜 빌드/플래시 사이클을 익혀야 하나

학생이 작성한 C 코드는 그저 텍스트입니다. 보드의 MCU 는 텍스트를 못 읽고 **기계어 (binary)** 만 실행합니다.
- **빌드 (Build)** = C → 기계어 (`.elf`) 변환
- **플래시 (Flash)** = `.elf` 를 보드 flash 메모리에 쓰기

이 두 단계가 모든 임베디드 개발의 기본 사이클입니다. 학생은 앞으로 수십, 수백 번 반복할 거예요.

> 🧒 비유: 책 쓰기 = 빌드 (한국어 → 영어 번역), 도서관 진열 = 플래시 (책장에 꽂기).

---

## 📖 WHAT — 무엇이 만들어지나

빌드 산출물 (CubeIDE 가 자동 생성):
- `Debug/Extension_Module.elf` — 디버그 정보 포함 (학생용 권장)
- `Debug/Extension_Module.bin` / `.hex` — raw 바이너리 (양산 배포용)
- Console 로그 — 컴파일 에러/경고 + 메모리 사용량 (`text data bss`)

---

## 🔧 HOW — 단계별 진행

### 1단계 — 프로젝트 Import

`File` → `Import...` → `General` → **`Existing Projects into Workspace`** → `Next`

`Browse...` → 본인 보드 리비전 폴더:
- **Rev 2.0 보드** (최신): `C:\dev\Extension_Module\XM10_SDK\Rev2.0\Extension_Module\`
- **Rev 1.1 보드**: `C:\dev\Extension_Module\XM10_SDK\Rev1.1\Extension_Module\`

`Projects:` 리스트에 `Extension_Module` 표시 → `Finish`

✅ Project Explorer 좌측 패널에 `Extension_Module` 항목 등장

### 2단계 — 예제 코드 적용 (선택)

기본 상태 그대로 빌드해도 보드는 부팅합니다 (Status LED Heartbeat). 학생 코드를 시험하려면:

**옵션 A — `user_app.c` 에 복사 (권장)**: 원하는 예제의 `.c` 내용 (예: `examples/00_Quick_Start/quick_start.c`) 을 통째 복사해서 `XM_Apps/User_Algorithm/user_app.c` 에 붙여넣기

**옵션 B — 예제 파일로 교체**: 예제 `.c` 를 `User_Algorithm` 폴더로 옮기고 기존 `user_app.c` 삭제

### 3단계 — 빌드

`Project` → `Build All` (단축키 `Ctrl + B`) 또는 툴바의 **망치 아이콘**

Console 패널에 컴파일 로그 실시간 출력. 첫 빌드는 5분 가량 (수백 개 파일).

✅ 마지막에:
```
   text    data     bss     dec     hex filename
 XXXXXX   YYYYY   ZZZZZ  AAAAAA  BBBBBB Extension_Module.elf

13:23:45 Build Finished. 0 errors, N warnings. (took XmYs)
```
**"0 errors"** 가 핵심. warning 은 무시 OK.

### 4단계 — 플래시 + 실행

**방법 A — 디버그 모드 (권장)**:
1. 툴바의 **벌레 아이콘** (Debug) 클릭
2. ST-Link 가 인식되면 자동으로 플래시 + halt
3. CubeIDE 가 **Debug perspective** 로 자동 전환
4. `Resume` (`F8`) → 보드 코드 실행 시작

**방법 B — 일반 실행 모드**:
1. 툴바의 **재생 아이콘** (Run) 클릭
2. 플래시 완료 후 자동으로 보드에서 실행

### 5단계 — 동작 확인

✅ XM10 보드의 LED 1 이 Heartbeat 패턴 (1초 주기 두근-두근) → 펌웨어 정상 동작 중

예제 코드를 적용한 경우 해당 예제 README 의 "실험" 단계 따라가기 (예: [Ex.00](../../examples/00_Quick_Start/README.md) 의 BTN 1 클릭 → USB 메시지).

---

## ⚠️ 흔한 실수 / 막혔다면

### 빌드 단계

- **`fatal error: 'xxx.h' file not found`** → Include Path 설정 누락. Project Properties → C/C++ Build → Settings 에서 include 경로 확인
- **`undefined reference to 'xxx'`** → 라이브러리 (`.a`) 누락. SDK 폴더 손상 의심 → 02 단계부터 재 clone
- **`region 'RAM' overflowed by N bytes`** → 사용자 코드가 메모리 한도 초과. 큰 배열/구조체 축소
- **빌드 너무 느림** → 백신 SW 가 임시 파일 스캔. CubeIDE workspace 폴더 백신 예외 등록
- **`1 errors`** → Console 상단 빨간 메시지 그대로 AI 에게 붙여넣기 ("이 에러 뭐야?")

### 플래시 단계

- **"No ST-Link detected"** → USB 케이블/포트. 허브 X, PC 후면 직결
- **"Target no device found"** → 보드 전원 또는 SWD 4핀 정렬. 1번 핀 마커 확인
- **"Old firmware on ST-Link, please update"** → ST-Link 펌웨어 업데이트 권유. `Yes` 클릭
- **플래시 도중 멈춤 → "Connection error"** → Debug Configurations → Debugger 탭 → `Reset Behaviour: Connect under reset` 로 변경

### 동작 단계

- **LED 1 점등 안 됨** → 플래시 실패 또는 부팅 후 hang. Reset 버튼 1회 누름
- **LED 가 너무 빨리 깜빡 (error blink)** → fault handler 진입. Debug perspective `Suspend` → PC 가 가리키는 함수 확인
- **USB 시리얼 메시지 안 보임** → CDC 단일 점유 (PhAI Studio 등 다른 클라이언트 종료 후 재시도)

---

## ➡️ 다음 단계

🎉 환경 구축 완료. 이제부터는 코드 작성 중심으로 전환:

- **첫 예제**: [Ex.00 Quick Start](../../examples/00_Quick_Start/README.md) — 보드 smoke test 의 5-step 실험
- **학습 로드맵**: [tutorials/README.md](../tutorials/README.md) — 41 예제 트랙 (난이도/시간 별 표시)
- **추천 진행**: Ex.00 → Ex.01 → Ex.02 → Ex.03 (Button & LED 4종, ⭐~⭐⭐)
