# Troubleshooting — FAQ & 문제 해결

XM10 개발 중 자주 발생하는 문제와 해결 방법을 정리한 문서입니다.

---

## 빌드 오류

### 경로 길이 문제 (Windows MAX_PATH)

**증상:** 빌드 시 `No such file or directory` 또는 `file name too long` 오류 발생

**원인:** Windows의 기본 경로 길이 제한(260자)을 초과하는 경우 발생합니다. 프로젝트가 깊은 경로에 위치하거나, OneDrive 동기화 폴더 안에 있을 때 빌드 출력 경로가 260자를 초과할 수 있습니다.

**해결 방법:**

**방법 1: 짧은 경로에 프로젝트 이동 (권장)**

```bash
# 드라이브 루트에 가까운 짧은 경로로 Clone
git clone https://github.com/angel-robotics/Extension_Module.git C:\XM_SDK
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

문제가 해결되지 않으면 [GitHub Issues](https://github.com/angel-robotics/Extension_Module/issues)에 문의해주세요.
