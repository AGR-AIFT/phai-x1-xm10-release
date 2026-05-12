# AGR_BOOT V2 — XM10 부트로더 가이드

> 📌 **이 페이지를 읽고 나면**: STM32CubeIDE SWD 디버깅 vs PhAI Studio USB FTP 두 가지 펌웨어 업로드 방법 + 부트로더 파괴 방지 + 자동 롤백 메커니즘을 이해합니다.
> ⏱️ 예상 학습 시간: 30분 (실습 포함 60분)
> 🧰 사전 지식: STM32CubeIDE 설치 ([docs/getting-started/02-software-setup.md](../getting-started/02-software-setup.md))
> 🎯 핵심: **Debug Configuration Start address = `0x08040000`** (기본값 `0x08000000` 으로 두면 부트로더 파괴)

> ⚠️ **절대 금지**: STM32CubeIDE Debug → Start address `0x08000000` 설정 → 부트로더 파괴. 항상 `0x08040000` 사용.

> **버전**: v1.1.0 | **대상 모듈**: XM10 Extension Module (STM32H743XI) | **최초 작성**: 2026-04-02

---

## 목차

1. [개요](#1-개요)
2. [Flash 메모리 맵](#2-flash-메모리-맵)
3. [최초 설치 (부트로더 없는 상태에서)](#3-최초-설치-부트로더-없는-상태에서)
4. [앱 펌웨어 업로드 — STM32CubeIDE (SWD 디버깅)](#4-앱-펌웨어-업로드--stm32cubeide-swd-디버깅)
5. [앱 펌웨어 업로드 — PhAI Studio (USB FTP)](#5-앱-펌웨어-업로드--phai-studio-usb-ftp)
6. [빌드 후 바이너리 생성 과정](#6-빌드-후-바이너리-생성-과정)
7. [부트로더 동작 원리](#7-부트로더-동작-원리)
8. [문제 해결 (Troubleshooting)](#8-문제-해결-troubleshooting)
9. [주의사항](#9-주의사항)

---

## 1. 개요

XM10 Extension Module은 **AGR_BOOT V2** 부트로더를 탑재하여 USB를 통한 펌웨어 업데이트를 지원합니다.

### 부트로더가 하는 일
- 전원 ON 시 앱 펌웨어의 **유효성 검증** (시그니처 + CRC-32 + HW 리비전)
- **USB CDC FTP** 프로토콜을 통한 앱 펌웨어 업로드 수신
- 업데이트 실패 시 **자동 롤백** (최대 3회 부팅 실패 시 백업 펌웨어로 복원)
- Rev1.1 / Rev2.0 **단일 부트로더 바이너리**로 양쪽 HW 지원 (GPIO 자동 감지)

### 필요한 도구
| 도구 | 용도 | 다운로드 |
|------|------|----------|
| **STM32CubeProgrammer** | 부트로더 최초 설치 (SWD/JTAG) | [st.com](https://www.st.com/en/development-tools/stm32cubeprog.html) |
| **STM32CubeIDE** | 앱 펌웨어 빌드 + SWD 디버깅 | [st.com](https://www.st.com/en/development-tools/stm32cubeide.html) |
| **PhAI Studio** | USB FTP 펌웨어 업로드 | [studio.onephai.com](https://studio.onephai.com) |
| **ST-Link V3** | SWD 디버깅 프로브 | ST-Link V2/V3 호환 |

---

## 2. Flash 메모리 맵

STM32H743XI는 2MB 내부 Flash (Bank1 + Bank2)를 가집니다.

```
┌──────────────────────────────────────────────────────────────┐
│ Address         │ Size    │ Region         │ 설명              │
├──────────────────────────────────────────────────────────────┤
│ 0x08000000      │ 256 KB  │ Bootloader     │ AGR_BOOT V2       │
│                 │         │                │ (Bank1 S0-S1)     │
├──────────────────────────────────────────────────────────────┤
│ 0x08040000      │ 1 KB    │ FW Header      │ AGR_FwInfo_t      │
│                 │         │                │ (시그니처+CRC+버전) │
├──────────────────────────────────────────────────────────────┤
│ 0x08040400      │ 767 KB  │ App (Active)   │ 사용자 앱 펌웨어    │
│                 │         │                │ (Bank1 S2-S7)     │
├──────────────────────────────────────────────────────────────┤
│ 0x08100000      │ 768 KB  │ Backup Slot    │ 이전 펌웨어 백업    │
│                 │         │                │ (Bank2 S0-S5)     │
├──────────────────────────────────────────────────────────────┤
│ 0x081C0000      │ 128 KB  │ Boot Config    │ AGR_BootConfig_t  │
│                 │         │                │ (Bank2 S6)        │
├──────────────────────────────────────────────────────────────┤
│ 0x081E0000      │ 128 KB  │ Reserved       │ 향후 확장용        │
│                 │         │                │ (Bank2 S7)        │
└──────────────────────────────────────────────────────────────┘
```

> **중요**: 부트로더(0x08000000)와 앱(0x08040000)은 완전히 분리된 영역입니다.  
> SWD로 앱을 플래시해도 부트로더는 손상되지 않습니다.

---

## 3. 최초 설치 (부트로더 없는 상태에서)

부트로더가 전혀 설치되지 않은 새 보드이거나, 이전에 부트로더 없이 사용하던 보드에 처음 설치하는 경우입니다.

### Step 1: Full Chip Erase

> **왜 필요한가?** 기존 펌웨어가 0x08000000부터 시작하는 단일 바이너리였다면, 부트로더 설치 후 주소가 충돌합니다. 반드시 전체 삭제 후 진행하세요.

1. **STM32CubeProgrammer** 실행
2. ST-Link를 보드에 연결
3. 좌측 패널에서 **Connect** 클릭 (SWD 또는 JTAG)
4. 좌측 메뉴에서 **Erasing & Programming** (다운로드 아이콘) 선택
5. 상단 **Full chip erase** 버튼 클릭
6. "Erase complete" 메시지 확인

### Step 2: 부트로더 바이너리 업로드

1. **Erasing & Programming** 탭에서:
   - **File path**: `AGR_Bootloader.bin` 선택 (Release에서 다운로드)
   - **Start address**: `0x08000000` (기본값 그대로)
   - **Skip flash erase before programming**: 체크 해제 (기본값)
   - **Verify programming**: 체크
2. **Start Programming** 클릭
3. "Download complete" + "Verification OK" 메시지 확인

> **Hex 파일 사용 시**: `AGR_Bootloader.hex`를 선택하면 주소가 자동으로 설정됩니다.

### Step 3: 확인

1. 보드 전원을 리셋 (또는 전원 재투입)
2. 부트로더가 실행되며:
   - 유효한 앱 펌웨어가 없으므로 **FTP 모드**로 진입
   - USB CDC 포트가 PC에 나타남 (COM 포트)
3. 이제 **Step 4 또는 Step 5**로 앱 펌웨어를 업로드

---

## 4. 앱 펌웨어 업로드 — STM32CubeIDE (SWD 디버깅)

개발 중 가장 많이 사용하는 방법입니다. SWD를 통해 직접 Flash에 기록합니다.

### 사전 설정 (최초 1회)

**Debug Configuration 설정:**

1. **Run → Debug Configurations → STM32 C/C++ Application**
2. 새 설정 생성 또는 기존 설정 선택
3. **Debugger** 탭에서:
   - **Download**: ✅ 체크
   - **Start address**: `0x08040000` (⚠️ 기본값 0x08000000이 아닙니다!)
   - **Size**: `0x000C0000` (768 KB = Active Slot)
   - **Reset behaviour**: Software Reset

> **⚠️ 핵심 주의사항**: Start address를 `0x08040000`으로 설정해야 합니다!  
> 기본값(0x08000000)으로 두면 **부트로더를 덮어씁니다**.

### 빌드 & 디버깅

1. STM32CubeIDE에서 프로젝트 **Build** (Ctrl+B)
2. Post-Build 스크립트가 자동 실행:
   - `size_report.py` → 메모리 사용량 리포트
   - `version_generator.py` → git tag에서 version.h 생성
   - `patch_fw_info.py` → ELF의 .fw_header 섹션에 fw_size/CRC 패치
   - `fw_packager.py` → FTP 업로드용 패키징 바이너리 생성
3. **Debug** (F11) 클릭 → SWD를 통해 0x08040000에 앱 펌웨어 기록
4. 디버깅 시작 (브레이크포인트, 변수 감시 등 정상 사용 가능)

### SWD 디버깅이 부트로더와 호환되는 이유

- 링커 스크립트(`STM32H743XIHX_FLASH.ld`)가 앱 코드를 `0x08040400`에 배치
- `.fw_header` 섹션이 `0x08040000`에 배치되어 부트로더가 시그니처를 인식
- `boot_fw_info.c`가 ELF에 `AGRBOOT` 시그니처를 포함시켜 SWD 플래시 후에도 부트로더가 앱을 정상 인식

---

## 5. 앱 펌웨어 업로드 — PhAI Studio (USB FTP)

SWD 디버거 없이 USB 케이블만으로 펌웨어를 업로드하는 방법입니다.  
**양산 환경**, **현장 업데이트**, **일반 사용자** 대상으로 권장됩니다.

### 필요한 파일

빌드 완료 후 `Debug/` 폴더에 생성되는 파일:

```
Debug/
├── Extension_Module.elf        ← SWD 디버깅용
├── Extension_Module.bin        ← SWD 플래시용 (FW Header 포함)
├── Extension_Module.hex        ← SWD 플래시용
├── Extension_Module_app.bin    ← FTP 업로드용 (FW Header 미포함)
└── XM10_X_X_X_X.bin           ← FTP 업로드용 패키징 바이너리 ★
```

> **PhAI Studio에서 사용할 파일**: `XM10_X_X_X_X.bin` (예: `XM10_2_0_1_0.bin`)  
> 이 파일은 `fw_packager.py`가 생성하며, 1KB AGR_FwInfo_t 헤더 + 앱 바이너리로 구성됩니다.

### 업로드 절차

1. **PhAI Studio** 접속: [studio.onephai.com](https://studio.onephai.com)
2. XM10 보드를 USB 케이블로 PC에 연결
3. PhAI Studio에서 디바이스 자동 인식 확인
4. **FW Upload** 메뉴 선택
5. `XM10_X_X_X_X.bin` 파일 선택
6. **Upload** 시작
7. 진행률 표시 → 완료 후 자동 리부팅
8. 부트로더가 새 펌웨어 검증 → 앱 실행

### 부트로더 FTP 모드 강제 진입

앱이 정상 실행 중일 때 FTP 모드로 전환하려면:
- PhAI Studio에서 **Enter Bootloader** 명령 전송
- 또는 앱 코드에서 `AGR_Boot_RequestUpdate()` 호출

---

## 6. 빌드 후 바이너리 생성 과정

STM32CubeIDE에서 **Build**를 실행하면 다음 순서로 Post-Build 스크립트가 실행됩니다:

```
[Build]  GCC 컴파일 + 링크 → Extension_Module.elf
           │
[Step 1]  size_report.py → 메모리 사용량 리포트 (콘솔 출력)
           │
[Step 2]  version_generator.py → git tag → version.h 자동 생성
           │
[Step 3]  patch_fw_info.py → ELF .fw_header 패치 (fw_size, fw_crc32)
           │                  → .bin, .hex, _app.bin 생성
           │
[Step 4]  fw_packager.py → _app.bin + FwInfo 헤더 → XM10_X_X_X_X.bin
```

### 각 단계 상세

| 단계 | 스크립트 | 입력 | 출력 | 설명 |
|------|---------|------|------|------|
| 1 | `size_report.py` | `.elf` | 콘솔 | FLASH/RAM 사용량 시각화 |
| 2 | `version_generator.py` | git tag | `version.h` | `FW_VER_MAJOR/MINOR/PATCH/DEBUG` 정의 |
| 3 | `patch_fw_info.py` | `.elf` | `.bin`, `.hex`, `_app.bin` | fw_size/CRC를 .fw_header에 패치 |
| 4 | `fw_packager.py` | `_app.bin` | `XM10_X_X_X_X.bin` | 1KB 헤더 + 앱 바이너리 = FTP용 |

> **Python 3.x 필수**: Post-Build 스크립트는 Python으로 작성되어 있습니다.  
> `pip install pyyaml` 필요 (Data Map Code-Gen용).

---

## 7. 부트로더 동작 원리

### 부팅 시퀀스

```
전원 ON → 부트로더 시작 (0x08000000)
    │
    ├─ Boot Config 읽기 (Bank2 S6)
    │   └─ 최초 부팅: 기본값으로 Config 자동 생성
    │
    ├─ 롤백 체크 (boot_count >= 3?)
    │   └─ Yes: 백업 슬롯에서 복원
    │
    ├─ 앱 펌웨어 검증
    │   ├─ "AGRBOOT\x01" 시그니처 확인
    │   ├─ CRC-32 검증 (FTP 업로드 시에만)
    │   └─ HW 리비전 호환성 확인
    │
    ├─ 검증 성공 → 앱으로 점프 (0x08040400)
    │   └─ 앱에서 AGR_Boot_ConfirmBoot() 호출 → boot_count 리셋
    │
    └─ 검증 실패 → FTP 대기 모드
        └─ USB CDC로 새 펌웨어 수신 대기
```

### FTP 업데이트 시퀀스

```
PhAI Studio에서 FW Upload 시작
    │
    ├─ [1] 앱에서 BL로 전환 (RTC BKP0R에 매직값 기록 + NVIC Reset)
    ├─ [2] 현재 Active FW → Backup Slot 복사
    ├─ [3] Active Slot 삭제
    ├─ [4] 새 FW 수신 + Active Slot 기록
    ├─ [5] CRC-32 검증
    ├─ [6] Boot Config 업데이트 (PENDING_CONFIRM)
    └─ [7] 리셋 → 새 앱 시작 → ConfirmBoot() → 완료
```

---

## 8. 문제 해결 (Troubleshooting)

### "부트로더를 설치했는데 앱이 실행되지 않아요"

**원인**: 앱 펌웨어가 없거나 유효하지 않습니다.
1. STM32CubeIDE에서 빌드 후 Debug(F11)로 앱 업로드
2. 또는 PhAI Studio에서 `XM10_X_X_X_X.bin` 업로드

### "SWD 디버깅 후 부트로더가 사라졌어요"

**원인**: Debug Configuration의 Start address가 `0x08000000`으로 되어 있었습니다.
1. `0x08040000`으로 수정 (섹션 4 참조)
2. 부트로더 재설치 필요 (섹션 3 참조)

### "PhAI Studio에서 디바이스를 못 찾아요"

1. USB 케이블 연결 확인
2. Windows 장치 관리자에서 COM 포트 확인
3. 부트로더가 FTP 모드인지 확인 (앱 실행 중이면 PhAI Studio에서 "Enter Bootloader" 먼저 실행)

### "FW 업로드 후 부팅이 반복되다가 이전 버전으로 돌아갔어요"

**원인**: 새 펌웨어에서 `AGR_Boot_ConfirmBoot()`가 호출되지 않아 3회 부팅 후 자동 롤백.
- `system_startup.c`에서 부팅 직후 `AGR_Boot_ConfirmBoot()` 호출이 포함되어 있는지 확인
- SDK 기본 코드에는 이미 포함되어 있으므로, 사용자가 `system_startup.c`를 수정하지 않았는지 확인

### "빌드는 되는데 XM10_X_X_X_X.bin이 생성되지 않아요"

1. Python 3.x 설치 확인: `python --version`
2. Post-Build 스크립트 콘솔 출력 확인 (Build Console에 에러 메시지)
3. `tools/build/` 폴더에 스크립트 파일이 있는지 확인

### "STM32CubeProgrammer에서 Read 하면 0x08040000에 데이터가 없어요"

**원인**: SWD 디버깅이 아닌 STM32CubeProgrammer로 직접 `.bin`을 업로드할 때 주소를 잘못 설정한 경우.
- `.bin` 파일 업로드 시 Start address: `0x08040000`
- `.hex` 파일은 주소가 내장되어 있으므로 자동

---

## 9. 주의사항

### 절대 하면 안 되는 것

| 금지 사항 | 결과 |
|----------|------|
| STM32CubeIDE Debug에서 Start address를 0x08000000으로 설정 | **부트로더 파괴** |
| STM32CubeProgrammer에서 Full chip erase 후 앱만 업로드 | **부트로더 없음 → 부팅 불가** |
| 부트로더 영역(0x08000000~0x0803FFFF)에 직접 쓰기 | **부트로더 파괴** |
| Boot Config 영역(0x081C0000)을 임의로 삭제 | **부팅 설정 초기화 (자동 복구는 됨)** |

### Rev1.1 / Rev2.0 공통 사항

- **동일한 부트로더 바이너리**를 사용합니다 (HW 리비전 GPIO 자동 감지)
- **동일한 Flash 메모리 맵**을 사용합니다
- 앱 펌웨어는 각 리비전별 **별도 libXM_Lib.a**로 빌드하지만, 부트로더는 공통

### Python 환경 설정

Post-Build 스크립트 실행을 위해:
```bash
# Python 3.x 설치 후
pip install pyyaml
```

STM32CubeIDE의 Build Console에서 Python 경로가 인식되지 않는 경우:
- Windows: 시스템 환경변수 PATH에 Python 경로 추가
- STM32CubeIDE 재시작 필요
