# Troubleshooting — FAQ & 문제 해결

XM10 개발 중 자주 발생하는 문제와 해결 방법을 정리한 문서입니다.

---

## 빌드 오류

### 경로 길이 문제 (Windows MAX_PATH)

**증상:** 빌드 시 `No such file or directory` 또는 `file name too long` 오류 발생

**원인:** Windows의 기본 경로 길이 제한(260자)을 초과하는 경우 발생합니다. 프로젝트가 깊은 경로에 위치하거나, OneDrive 동기화 폴더 안에 있을 때 빌드 출력 경로가 260자를 초과할 수 있습니다.

**해결 방법:**

**방법 1: 짧은 경로에 SDK 압축 해제 (권장)**

본 SDK 는 GitHub Releases 의 ZIP 으로 받습니다. 압축 해제 위치를 드라이브 루트에 가깝게 잡으면 경로 길이 문제를 피할 수 있습니다.

```powershell
# Releases 페이지에서 본인 Rev ZIP 다운로드 후
Expand-Archive -Path "$HOME\Downloads\Rev2.0.zip" -DestinationPath C:\XM_SDK\ -Force
```

| 구분 | 경로 예시 | 빌드 최대 경로 |
| :--- | :--- | :---: |
| 권장 | `C:\XM_SDK\` | ~130자 |
| 일반 | `C:\Users\Name\Documents\GitHub\Extension_Module\` | ~190자 |
| 위험 | `C:\Users\...\OneDrive - Company\...\Extension_Module\` | 260자 초과 가능 |

**방법 2: Windows Long Path 지원 활성화 (근본 해결)**

Windows 10 (1607+) / Windows 11에서 260자 제한을 해제할 수 있습니다.

PowerShell (관리자 권한):
```powershell
New-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem" -Name "LongPathsEnabled" -Value 1 -PropertyType DWORD -Force
```

또는 그룹 정책:
`컴퓨터 구성 > 관리 템플릿 > 시스템 > 파일 시스템 > Win32 긴 경로 사용` → **사용**

> 설정 후 PC 재시작이 필요합니다. 이 설정은 1회만 하면 되며 Git, CMake, GCC 등 모든 도구에 적용됩니다.

---

### 한글/공백 경로 오류

**증상:** 빌드 시 `No such file or directory` 또는 인코딩 관련 오류

**원인:** 프로젝트 경로 또는 STM32CubeIDE 설치 경로에 한글, 공백, 특수문자가 포함된 경우

**해결:**
* 프로젝트를 영문 경로로 이동 (예: `C:\XM_SDK\`)
* STM32CubeIDE를 영문 경로에 설치 (예: `C:\dev\STM32CubeIDE`)
* Windows 사용자 이름이 한글인 경우: 프로젝트를 사용자 폴더가 아닌 드라이브 루트에 배치

---

### nano.specs 중복 오류

**증상:** `fatal error: nano.specs: attempt to rename spec 'link' to already defined spec 'nano_link'`

**원인:** CMake 빌드 설정에서 `--specs=nano.specs` 플래그가 중복 적용

**해결:** `CMakeLists.txt`에서 `CMAKE_C_FLAGS`와 `CMAKE_C_FLAGS_DEBUG/RELEASE`가 동일한 플래그를 중복 포함하지 않는지 확인하세요. `CMAKE_C_FLAGS`에 공통 플래그를 넣고, `DEBUG/RELEASE`에는 빌드 타입별 플래그만 설정합니다.

---

### IOIF 매크로 재정의 경고

**증상:** `warning: "AGRB_IOIF_FDCAN_ENABLE" redefined` 등의 경고

**원인:** `AGRB_IOIF_*_ENABLE` 매크로가 CMakeLists.txt의 `-D` 플래그와 `ioif_conf.h`에서 중복 정의

**해결:** CMakeLists.txt의 `PROJECT_DEFINES`에서 `AGRB_IOIF_*` 관련 정의를 제거하고, `ioif_conf.h`에서만 관리하세요. 경고 자체는 동작에 영향을 주지 않지만, 제거하는 것이 깔끔합니다.

---

## 보드 리비전 / SDK ZIP 불일치

### 가운데·우측 버튼이 안 눌리거나, 누르지 않은 버튼이 항상 눌린 것으로 잡힘

**증상:**
- `XM_GetButtonEvent(XM_BTN_2)` 가 가운데 버튼을 눌러도 반응 없음
- 한 버튼만 동작하고 나머지는 다른 ID 로 잡히거나 stuck (`XM_PRESSED` 가 고정)
- `XM_GetButtonState` 와 HAL `HAL_GPIO_ReadPin` 비교 결과 매핑이 한 칸씩 어긋남

**원인:** Rev 1.1 보드와 Rev 2.0 보드는 내장 버튼 3 개의 MCU 핀이 다릅니다 — Rev 2.0 으로 가면서 외장 SRAM (PSRAM) 자리를 만들기 위해 버튼 핀이 한 칸 시프트되었어요.

| | BTN 1 (좌) | BTN 2 (중) | BTN 3 (우) |
|---|---|---|---|
| Rev 1.1 | PC10 | PC11 | PC12 |
| Rev 2.0 | PC11 | PC12 | PC13 |

API 이름 (`XM_BTN_1/2/3`) 은 양쪽 Rev 에서 좌·중·우 의미를 그대로 유지하지만, 그 안에서 어느 MCU 핀을 읽을지는 SDK 가 본인 Rev 의 `main.h` 핀 정의를 따라갑니다. 따라서 **보드 Rev 와 다른 ZIP 으로 빌드하면** 빌드는 통과해도 라이브러리가 잘못된 핀을 읽어 매핑이 어긋납니다.

**해결:**
1. 보드 라벨에서 `Rev 1.1` 또는 `Rev 2.0` 확인. 라벨이 모호하면 외관상 RJ45 Ethernet 포트의 유무로 구별 (Rev 2.0 만 있음). 자세한 비교는 [docs/hardware/README.md - 보드 리비전 비교](hardware/README.md#보드-리비전-비교).
2. [Releases](https://github.com/AGR-EXO/Extension_Module/releases) 에서 본인 Rev 와 같은 ZIP 을 받아 STM32CubeIDE 에 다시 import.
3. 재빌드·플래시 후 Ex.01 (Button & LED Basic) 의 세 버튼 모두 정상 동작 확인.

**진단 코드 (선택사항):** 본인 보드의 실제 핀을 직접 확인하고 싶다면:

```c
// Run_Loop 안에서 PC10~PC13 을 동시에 읽어 비교
uint8_t pc10 = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_10);
uint8_t pc11 = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_11);
uint8_t pc12 = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_12);
uint8_t pc13 = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13);
// 좌·중·우 버튼을 한 번씩 눌러 어느 핀이 떨어지는지 확인
// → Rev 1.1 보드면 PC10/11/12, Rev 2.0 보드면 PC11/12/13 이 떨어짐
```

> ⚠️ HAL 로 직접 읽는 코드는 진단용입니다. 정상 동작이 확인되면 `XM_GetButtonEvent` / `XM_GetButtonState` 로 돌아오세요 — 디바운싱과 클릭/롱프레스 인식은 라이브러리가 처리합니다.

---

## USB 연결 문제

### USB-CDC가 PC에서 인식되지 않음

**체크리스트:**
1. USB-C 케이블이 **데이터 전송용**인지 확인 (충전 전용 케이블은 불가)
2. Windows 장치 관리자에서 `Ports (COM & LPT)` 아래에 `STMicroelectronics Virtual COM Port` 확인
3. 드라이버가 없는 경우: [STM32 Virtual COM Port Driver](https://www.st.com/en/development-tools/stsw-stm32102.html) 설치
4. XM10 펌웨어에서 USB CDC 초기화가 정상적으로 완료되었는지 확인

### USB-MSC가 인식되지 않음

**체크리스트:**
1. USB 메모리가 XM10 보드의 USB Host 포트에 올바르게 삽입되었는지 확인
2. USB 메모리가 **FAT32** 포맷인지 확인 (NTFS, exFAT는 지원하지 않음)
3. 메모리 용량이 **32GB 이하**인지 확인 (권장: Sandisk Ultra Dual Drive Type C 32GB)
4. XM10 펌웨어에서 MSC 초기화 완료 후 파일 시스템 마운트 상태 확인

---

## CAN-FD 통신 문제

### KIT H10과 통신이 안 됨

**체크리스트:**
1. XM10 ↔ KIT H10 케이블이 단단히 연결되었는지 확인
2. KIT H10의 전원이 켜져 있는지 확인 (24V 전원 공급)
3. 커넥터 핀맵 확인: CAN HIGH(5번), CAN LOW(6번)이 올바르게 연결되었는지 검증
4. CAN-FD 보레이트 설정이 일치하는지 확인

### 센서 허브 모듈 연동 오류

**체크리스트:**
1. 센서 허브 모듈의 CAN 주소(Node ID)가 충돌하지 않는지 확인
2. AGR PnP V2를 통한 디바이스 검색이 정상 수행되는지 확인
3. 센서 허브 모듈의 펌웨어 버전이 XM10 SDK와 호환되는지 확인

---

## 디버깅 관련

### ST-Link 연결 실패

**체크리스트:**
1. ST-Link 디버거의 USB 연결 상태 확인
2. SWD 4핀 케이블의 핀 배치가 올바른지 확인 (SWDIO, SWCLK, GND, 3.3V)
3. STM32CubeIDE의 Debug Configuration에서 ST-Link가 감지되는지 확인
4. ST-Link Firmware를 최신 버전으로 업데이트 (`Help > ST-Link Upgrade`)

### 디버깅 시 변수 값이 Optimized Out

**원인:** Release 빌드 설정(`-O2`)에서 컴파일러 최적화로 변수가 제거됨

**해결:** Debug 빌드 설정(`-Og -g3`)으로 빌드하세요. `Project > Properties > C/C++ Build > Settings > Optimization` 에서 확인할 수 있습니다.

---

## 자주 마주치는 함정

### 코드 작성 패턴

#### `sprintf("%.2f", val)` 결과가 정수처럼 출력됨

**원인:** newlib-nano (기본 ARM 라이브러리) 가 float 형식 미지원

**해결:** STM32CubeIDE → `Project > Properties > C/C++ Build > Settings > MCU Settings` → **"Use float with printf"** 체크.

#### `static` 누락으로 변수가 매 호출 0 으로 초기화

**증상:** 토글 동작이 안 되거나, 타이머가 매번 reset

**원인:** 함수 내부 변수는 매 호출 스택에 새로 생성. `static` 키워드로 보존 필요.

```c
// ❌ 잘못된 예
static void Run_Loop(void) {
    bool toggle = false;          // 매 호출 false 로 reset
    if (clicked) toggle = !toggle;  // 절대 true 가 안 됨
}

// ✅ 올바른 예
static void Run_Loop(void) {
    static bool toggle = false;    // 다음 호출까지 유지
    if (clicked) toggle = !toggle;
}
```

#### `XM_TSM_Run()` 누락으로 상태 콜백 미실행

**증상:** Add_State 등록했는데 `on_loop` 함수가 호출 안 됨

**해결:** `User_Loop()` 안에서 반드시 `XM_TSM_Run(handle);` 호출.

#### Active Low vs Active High 혼동

**증상:** 외부 스위치 누름인데 LED 가 반대 동작

**규칙:**
- 풀업 (`INPUT_PULLUP`) + 스위치 한쪽 GND → 눌림 = `XM_LOW` (Active Low)
- 풀다운 (`INPUT_PULLDOWN`) + 스위치 한쪽 3.3V → 눌림 = `XM_HIGH` (Active High)

본 SDK 의 내장 버튼 (`XM_GetButtonState`) 은 추상화되어 항상 `XM_PRESSED`.

#### Edge Detection 누락

**증상:** 버튼 한 번 눌렀는데 토글이 여러 번 발생

**해결:** 직전 상태 (`s_btn_prev`) 와 현재 상태 비교로 "방금 막 눌린 순간" 만 감지.

```c
bool btn_now = (XM_DigitalRead(BTN) == XM_LOW);
bool pressed = (btn_now && !s_btn_prev);  // Leading edge
s_btn_prev = btn_now;
if (pressed) { /* 1회만 실행 */ }
```

또는 `XM_GetButtonEvent()` 의 read-clear 동작 활용 (Ex.02 참조).

### USB / 통신 함정

#### USB-CDC 메시지가 PhAI Studio 또는 터미널에 안 보임

**원인 1:** 다른 시리얼 클라이언트가 동일 COM 포트 점유 (PhAI Studio + PuTTY 동시 실행)

**원인 2:** USB-C 케이블이 데이터 전송 불가 (충전 전용)

**원인 3:** Windows 장치 관리자에서 `STMicroelectronics Virtual COM Port` 미인식

**해결 순서:**
1. 모든 시리얼 클라이언트 종료 → 하나만 단독 실행
2. 데이터 전송 가능 USB-C 케이블 (가능하면 PC 후면 USB-A 직결, 허브 X)
3. [STM32 VCP 드라이버 설치](https://www.st.com/en/development-tools/stsw-stm32102.html)

#### Python 디코더가 .bin 파일을 못 읽음 ("size mismatch")

**원인:** 구조체 크기와 `metadata.txt` 의 합산이 어긋남 (padding 또는 정렬 차이)

**해결:**
1. 보드에서 `XM_SendUsbDebugMessage` 로 `printf("size=%u", sizeof(MyStruct))` 출력
2. metadata 의 필드 크기 합산이 일치하는지 검증
3. 비정렬 시 `_pad(Nbytes)` 명시 또는 `__attribute__((packed))` 사용

### KIT H10 / 로봇 제어 함정

#### `XM.status.h10.*` 가 모두 0

- KIT H10 본체 전원 OFF → 24 V 입력 확인
- CAN-FD HIGH/LOW 핀 거꾸로 → 핀맵 ([01-hardware-setup.md Figure 1](getting-started/01-hardware-setup.md)) 확인
- KIT H10 FW v2.3.0 미만 → [kit-h10-firmware/](kit-h10-firmware/) 가이드

#### `gaitCycle`, `forwardVelocity`, `footContact` 가 항상 0

**원인:** Body Data 전제조건 미충족 — `XM_SendUserBodyData()` 미호출

**해결:** [examples/README.md — Body Data 안내](../examples/README.md#part-5) 참조.

#### `SetAssistTorque` 호출했는데 H10 안 움직임

- `XM_SetControlMode(XM_CTRL_TORQUE)` 누락 → Active 진입 시 1회 호출
- 안전 스위치 트리거 상태 → ERROR 상태 점검
- H10 본체 모터 활성화 안 됨 → 본체 LED / 토크 큐 확인

### 환경 / 시스템 함정

#### Windows 사용자명에 한글 포함

**증상:** `C:\Users\홍길동\...` 경로에서 빌드 시 한글 인코딩 또는 MAX_PATH 문제

**해결:** 사용자 폴더 대신 **드라이브 루트** 에 SDK 압축 해제:
```powershell
Expand-Archive -Path "$HOME\Downloads\Rev2.0.zip" -DestinationPath C:\dev\ -Force
```

#### 클라우드 동기화 폴더 (OneDrive, iCloud) 에 clone

**증상:** 빌드 중 파일 잠금 충돌, 동기화 충돌 파일 생성

**해결:** 클라우드 비동기 폴더 (예: `C:\dev\`) 로 이동.

---

문제가 해결되지 않으면 [GitHub Issues](https://github.com/AGR-EXO/Extension_Module/issues)에 문의해주세요.

> 🤖 Claude Code 사용자: `"Ex.XX 가 안 돼"` 또는 `"빌드 에러 났어"` 한 줄로 `example-helper` 스킬이 본 페이지의 해당 항목 + 예제 README 의 ⚠️ 섹션을 인용해 답합니다.
