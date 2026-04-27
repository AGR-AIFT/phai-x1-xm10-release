# AGR_MW (AGR Middleware)

AGR 모듈 공용 미들웨어 서브모듈. IOIF(HW 추상화)와 Application 사이의 프로토콜/서비스 레이어.

## 구조

```
AGR_MW/
  DOP/          — Data Object Protocol V3 (CiA 301, Core + Transport)
  PnP/          — Plug-and-Play (Master/Slave, CANopen 기반)
  BOOT/         — App-side Boot Core (ConfirmBoot, RequestUpdate)
  TSM/          — Task State Machine
  RiskMngr/     — Risk Manager (에러 사전 + 상태 관리)
  BuffMngr/     — Ring Buffer
```

## 사용법

### 1. Git Submodule 추가

```bash
git submodule add -b Develop https://github.com/AGR-EXO/AGR_MW.git {Module}_FW/AGR_MW
```

### 2. agr_mw_conf.h 생성

`agr_mw_conf_template.h`를 각 모듈의 `System/Config/agr_mw_conf.h`로 복사 후 수정.

> **`agr_mw_conf_template.h`는 참고용 템플릿입니다.**
> AGR_MW 서브모듈 내부의 이 파일은 빌드에 포함되지 않습니다.
> 실제로 사용되는 conf는 각 모듈 레포의 `System/Config/agr_mw_conf.h`입니다.
> (`ioif_conf.h`와 동일한 패턴)

```c
// System/Config/agr_mw_conf.h 예시
#define AGR_MW_MCU_SERIES_H7  1   // 자동 감지 (CubeMX define 기반)
#define AGR_MW_BOOT_ENABLE        // H7 + 부트로더 사용 모듈만
```

### 3. Include Path 추가 (.cproject)

```
AGR_MW/BOOT/Inc
AGR_MW/DOP
AGR_MW/DOP/Core
AGR_MW/DOP/Transport/CAN_FD
AGR_MW/DOP/Transport/CoE
AGR_MW/PnP
AGR_MW/TSM/Inc
AGR_MW/RiskMngr/Inc
AGR_MW/BuffMngr/Inc
```

## MCU 지원

| MCU Series | 매크로 | BOOT | DOP/PnP/TSM/etc |
|-----------|--------|------|-----------------|
| STM32H7 (H743, H750) | `AGR_MW_MCU_SERIES_H7` | O | O |
| STM32G4 (G474, G431) | `AGR_MW_MCU_SERIES_G4` | X (disable) | O |

## 적용 모듈

| 모듈 | MCU | BOOT |
|------|-----|------|
| XM (Extension Module) | STM32H743XI | Enable |
| CM-WH (WalkON H CM) | STM32H743ZI | Disable (향후) |
| SM-IMU (IMU Hub) | STM32G474RE | Disable |
| SM-EMG (EMG Hub) | STM32G474RE | Disable |
| SM-FES (FES Hub) | STM32G474RE | Disable |

## BOOT Core + Trigger 분리

AGR_MW/BOOT/에는 **공통 Core만** 포함:
- `agr_boot_types.h` — 타입, 플래시 레이아웃, enum (BL과 바이너리 호환)
- `agr_boot_core.h/c` — ConfirmBoot(), RequestUpdate()

모듈별 **Trigger** (부트 진입 방식)는 각 모듈의 `System/Boot/`에 위치:
- XM: `boot_ftp_trigger.c` (USB CDC FTP)
- CM: (향후) UART FTP
- SM: (향후) CAN-FD 기반 부트