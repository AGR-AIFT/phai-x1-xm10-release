# 03 — 첫 빌드 & 실행

마지막 단계입니다. 20 분 정도면 보드에서 첫 펌웨어가 돌면서 LED 가 깜빡이는 걸 볼 수 있어요. [01 하드웨어 연결](01-hardware-setup.md) + [02 환경 구축](02-software-setup.md) 이 끝났다는 전제.

내가 작성한 C 코드는 그저 텍스트입니다. 보드 안의 프로세서는 텍스트를 읽지 못하고 기계어만 실행할 수 있어요. 그래서 두 단계가 필요합니다.

- **빌드 (Build)** — C 코드를 기계어 (`.elf`) 로 번역
- **플래시 (Flash)** — 그 기계어를 보드 메모리에 써넣기

앞으로 코드 한 줄 바꿀 때마다 이 두 단계를 수십, 수백 번 반복하게 됩니다.

---

## 빌드 결과물

CubeIDE 가 자동으로 만들어 줍니다.

- `Debug/Extension_Module.elf` — 디버그 정보 포함 (개발용)
- `Debug/Extension_Module.bin` / `.hex` — 양산 배포용 바이너리
- Console 로그 — 컴파일 에러·경고 + 메모리 사용량

---

## 단계

### 1. 프로젝트 Import

`File` → `Import...` → `General` → **`Existing Projects into Workspace`** → `Next`

`Browse...` → **압축 푼 폴더 자체** 선택:
- 권장: `C:\dev\Extension_Module\` (`.project` 가 폴더 root 에 있음)

본 SDK ZIP 은 Rev 별로 평탄화 배포라 (Rev1.1.zip / Rev2.0.zip), 압축 풀면 본인 Rev SDK 한 벌이 ZIP root 에 그대로 들어있습니다. Rev 폴더 안으로 들어갈 필요 없습니다.

`Projects:` 리스트에 `Extension_Module` 표시 → `Finish`

✅ Project Explorer 좌측 패널에 `Extension_Module` 항목 등장

### 2. 예제 코드 적용 (선택)

기본 상태 그대로 빌드해도 보드는 부팅합니다 (LED 1 이 두근두근 깜빡임). 직접 예제 코드를 시험하려면:

- **옵션 A (권장)** — `examples/00_Quick_Start/quick_start.c` 같은 예제의 내용을 통째로 복사해서 `XM_Apps/User_Algorithm/user_app.c` 에 붙여넣기
- **옵션 B** — 예제 `.c` 파일을 `User_Algorithm` 폴더로 옮기고 기존 `user_app.c` 는 삭제

### 3. 빌드

`Project` → `Build All` (단축키 `Ctrl + B`) 또는 툴바의 **망치 아이콘**

Console 패널에 컴파일 로그 실시간 출력. 첫 빌드는 5분 가량 (수백 개 파일).

✅ 마지막에:
```
   text    data     bss     dec     hex filename
 XXXXXX   YYYYY   ZZZZZ  AAAAAA  BBBBBB Extension_Module.elf

13:23:45 Build Finished. 0 errors, N warnings. (took XmYs)
```
**"0 errors"** 가 핵심. warning 은 무시 OK.

### 4. 플래시 + 실행

**디버그 모드 (권장)**:
1. 툴바의 벌레 아이콘 (Debug) 클릭
2. ST-Link 가 인식되면 자동으로 플래시 + 일시정지
3. CubeIDE 가 Debug perspective 로 전환됨
4. `Resume` (F8) → 보드 코드 실행 시작

**일반 실행 모드**:
1. 툴바의 재생 아이콘 (Run) 클릭
2. 플래시 완료 후 자동으로 보드에서 실행

### 5. 동작 확인

XM10 보드의 LED 1 이 1 초 주기로 두근-두근 깜빡이면 → 펌웨어가 정상 동작 중입니다.

예제 코드를 적용했다면 해당 예제 README 의 "실험" 단계를 따라가세요 (예: [Ex.00](../../examples/00_Quick_Start/README.md) 의 BTN 1 클릭 → USB 메시지 확인).

---

## 자주 막히는 부분

### 빌드

- **`fatal error: 'xxx.h' file not found`** — Include Path 가 빠졌습니다. Project Properties → C/C++ Build → Settings 에서 확인.
- **`undefined reference to 'xxx'`** — 라이브러리 (`.a`) 파일이 없습니다. SDK 폴더가 손상됐을 가능성 — 02 단계부터 다시 ZIP 다운로드 + 압축 해제.
- **`region 'RAM' overflowed by N bytes`** — 코드가 메모리 한계를 넘었어요. 큰 배열·구조체부터 줄여보세요.
- **빌드가 너무 느립니다** — 백신이 임시 파일을 계속 스캔하는 경우. CubeIDE workspace 폴더를 백신 예외에 등록하세요.
- **`1 errors`** — Console 상단의 빨간 에러 메시지를 통째로 AI 에게 붙여넣고 물어보세요.

### 플래시

- **"No ST-Link detected"** — USB 케이블/포트 문제. USB 허브 말고 PC 후면 직결.
- **"Target no device found"** — 보드 전원이 안 들어오거나 SWD 4 핀이 어긋난 경우. 1 번 핀 마커 확인.
- **"Old firmware on ST-Link, please update"** — ST-Link 펌웨어 업데이트 권유. Yes 클릭.
- **플래시 도중 "Connection error"** — Debug Configurations → Debugger 탭에서 Reset Behaviour 를 `Connect under reset` 으로 바꿔보세요.

### 동작

- **LED 1 이 안 켜집니다** — 플래시 실패 또는 부팅 후 멈춤. 보드의 Reset 버튼을 한 번 눌러보세요.
- **LED 가 너무 빨리 깜빡 (에러 신호)** — fault handler 진입. Debug perspective 에서 Suspend → PC 가 가리키는 함수 확인.
- **USB 시리얼 메시지가 안 보입니다** — USB 시리얼 (CDC) 포트는 한 번에 한 프로그램만 점유 가능. PhAI Studio 등 다른 클라이언트를 닫고 재시도.

---

## 다음으로

환경 구축 끝났습니다. 이제 본격적으로 코드 작성으로 넘어가요.

- 첫 예제 → [Ex.00 Quick Start](../../examples/00_Quick_Start/README.md) (보드 동작 확인용)
- 학습 경로 → [tutorials/README.md](../tutorials/README.md) (41 예제 + 추천 순서)
- 추천 시작: Ex.00 → Ex.01 → Ex.02 → Ex.03 (버튼 + LED 4 종, ⭐~⭐⭐)
