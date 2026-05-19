# 하드웨어 스펙

XM10 보드의 외부 인터페이스를 한 곳에 모았습니다. 보드에 뭐가 몇 개 있는지, 어디에 꽂으면 되는지 처음 시작할 때 가장 먼저 보게 되는 페이지입니다.

> 보드 외관 사진과 리비전별 GPIO 헤더 사진은 [`assets/img/`](../../assets/img/) 에서 관리합니다.

---

## 한눈에

| 분류 | 개수 / 종류 | 비고 |
|------|-----------|------|
| **보드 LED** | 3 개 | LED 1 (좌) · LED 2 (중) · LED 3 (우) |
| **채널 LED** | 7 개 RGB | EMG / FES / IMU / HMMG / GRF_L / GRF_R / USB (모듈 상태 표시용) |
| **버튼** | 3 개 | BTN 1 (좌) · BTN 2 (중) · BTN 3 (우) |
| **외부 GPIO** | DIO 8 + ADC 4 (+ 동적 ADC 8) | [Rev 별 핀맵 참조](#외부-gpio-핀맵) |
| **CAN-FD 포트** | 2 개 | KIT H10 연결 + 센서 허브 확장 |
| **USB-C** | 1 개 | 시리얼 통신 (CDC) + 메모리 로깅 (MSC) 겸용 |
| **외부 UART** | 1 개 | 외부 IMU 등 직렬 통신용 |
| **SWD 디버그** | 4-pin | ST-Link 펌웨어 업로드·디버깅 |
| **메인 커넥터** | 1 개 | KIT H10 본체 연결 (24V 전원 + CAN-FD) |

> 각 인터페이스의 정확한 위치는 아래 보드 외관 사진을 참고하세요.

---

## 보드 외관

![XM10 보드 외관 — 인터페이스 위치](../../assets/img/board-photo.png)

LED 3 개 / 버튼 3 개 / 메인 커넥터 / SWD / USB-C / CAN-FD / 외부 GPIO 헤더 / 외부 UART 의 물리적 위치를 한눈에 보여주는 사진입니다. 리비전별 외부 GPIO 헤더 핀맵은 [Rev 1.1](external-gpio-rev1.1.md) / [Rev 2.0](external-gpio-rev2.0.md) 페이지를 따로 참조하세요.

---

## 외부 인터페이스

### 메인 커넥터 (KIT H10 연결)

XM10 의 전원 + 통신을 한 가닥으로 받습니다. Molex 1053081206 6-pin 커넥터.

| 핀 | 기능 |
|---|---|
| 1 | NC |
| 2 | 24 V (전원) |
| 3 | GND |
| 4 | GND |
| 5 | CAN HIGH |
| 6 | CAN LOW |

> 연결 절차: [Getting Started — 01 하드웨어 연결](../getting-started/01-hardware-setup.md)

### CAN-FD 포트

총 2 개. 하나는 메인 커넥터로 KIT H10 과 통신, 다른 하나는 센서 허브 (EMG/GRF/FSR 등) 확장용.

| 포트 | 용도 |
|------|------|
| CAN-FD #1 | KIT H10 외골격 ↔ XM10 (메인 커넥터에 포함) |
| CAN-FD #2 | 센서 허브 모듈 확장 |

### USB-C 포트

PC 와 데이터를 주고받는 데 사용합니다. 시리얼 통신 (CDC) + USB 메모리 로깅 (MSC) 두 모드를 같은 포트로 지원해요.

| 모드 | 용도 | 사용하는 함수 |
|------|------|--------------|
| 시리얼 (CDC) | PC 터미널 / PhAI Studio 로 디버그·데이터 전송 | `XM_SendUsbDebugMessage`, `XM_SendUsbDataWithId` |
| 메모리 (MSC) | USB 메모리에 데이터 로깅 (FAT32, 32 KB cluster) | `XM_StartUsbDataLog`, `XM_LogBinaryData` |

> 한 번에 하나의 모드만 활성됩니다. 시리얼 사용 중 USB 메모리 꽂으면 모드 전환 동작.

### 외부 UART 포트

외부 직렬 장치 (예: XSENS MTi 시리즈 IMU) 연결용 1 개. 자세한 설정은 외부 IMU 사용 예제 참조.

### SWD 디버그 포트

ST-Link V2/V3 디버거 연결용 4-pin 헤더. 펌웨어를 처음 올릴 때 + 디버깅할 때 사용.

> 핀 배치: [Getting Started — 01 하드웨어 연결](../getting-started/01-hardware-setup.md#2-st-link-디버거-pc--xm10)

---

## 보드 입력·출력

### 버튼

| 버튼 | 위치 | API | MCU 핀 (Rev 1.1) | MCU 핀 (Rev 2.0) |
|------|------|-----|------------------|------------------|
| BTN 1 | 좌측 | `XM_GetButtonEvent(XM_BTN_1)` | PC10 | PC11 |
| BTN 2 | 중앙 | `XM_GetButtonEvent(XM_BTN_2)` | PC11 | PC12 |
| BTN 3 | 우측 | `XM_GetButtonEvent(XM_BTN_3)` | PC12 | PC13 |

지원 이벤트: 눌림 (`XM_BTN_PRESSED`), 떼짐 (`XM_BTN_RELEASED`), 클릭 (`XM_BTN_CLICK`), 1 초 이상 길게 누름 (`XM_BTN_LONG_PRESS`).

> ⚠️ Rev 1.1 → Rev 2.0 으로 가면서 내장 버튼 3 개가 한 핀씩 옮겨졌습니다 (PC10 자리는 Rev 2.0 에서 외장 SRAM (PSRAM) 용으로 빠졌습니다). API 이름 (`XM_BTN_1/2/3`) 은 양쪽에서 그대로 좌·중·우 의미를 유지하므로, **본인 보드와 같은 Rev 의 SDK ZIP** 만 쓰면 신경 쓸 필요 없습니다. 다른 Rev 의 ZIP 을 올리면 버튼 매핑이 한 칸씩 어긋납니다.

### LED

| LED | 위치 | API |
|-----|------|-----|
| LED 1 | 좌측 | `XM_SetLedState(XM_LED_1, XM_ON)` 등 |
| LED 2 | 중앙 | `XM_SetLedState(XM_LED_2, ...)` |
| LED 3 | 우측 | `XM_SetLedState(XM_LED_3, ...)` |

지원 효과: 켜기·끄기 (`XM_LED_SOLID`/`OFF`), 깜빡이기 (`BLINK`), 두근두근 (`HEARTBEAT`), 한 번만 (`ONESHOT`).

### 채널 LED (RGB 7 개)

각 센서 모듈의 연결 상태를 색으로 표시하는 RGB LED. 시스템이 자동으로 제어하지만, 필요하면 `XM_SetChannelLedRGB()` 로 사용자가 직접 색을 지정할 수도 있습니다.

| 채널 LED | 표시 모듈 |
|---------|---------|
| `XM_CH_LED_EMG` | EMG 모듈 |
| `XM_CH_LED_FES` | FES 모듈 |
| `XM_CH_LED_IMU` | IMU 모듈 |
| `XM_CH_LED_HMMG` | HMMG 모듈 |
| `XM_CH_LED_GRF_L` / `_GRF_R` | 좌·우 GRF 슈즈 |
| `XM_CH_LED_USB` | USB 연결 상태 |

> LED + 버튼 API 전체: [LED & 버튼](../api-reference/03-led-btn-control.md)

---

## 외부 GPIO 핀맵

외부 GPIO 헤더는 학생이 가장 자주 들여다보게 되는 부분입니다. 보드 리비전에 따라 커넥터 위치·라벨이 다르므로 사용 중인 보드에 맞는 페이지를 보세요.

| 보드 리비전 | 핀맵 페이지 |
|------------|-----------|
| **Rev 1.1** | [external-gpio-rev1.1.md](external-gpio-rev1.1.md) |
| **Rev 2.0** | [external-gpio-rev2.0.md](external-gpio-rev2.0.md) |

> 핀 자체의 사용법 (어떻게 입출력 함수를 호출하는지) 은 [외부 IO API](../api-reference/04-external-io.md) 에 정리되어 있습니다.

---

## 보드 리비전 비교

본인 보드가 어느 리비전인지부터 확인하세요. 보드 라벨에 `Rev 1.1` / `Rev 2.0` 표기가 있고, 외관상 가장 쉽게 구별되는 차이는 **RJ45 Ethernet 포트의 유무** (Rev 2.0 만 있음) 입니다.

| 항목 | Rev 1.1 | Rev 2.0 |
|------|---------|---------|
| RJ45 Ethernet 포트 | 없음 | **있음** (외관 식별의 가장 빠른 지표) |
| 채널 LED (RGB 7 개) | 없음 (내장 LED 3 개만) | **있음** (PCA9957, `XM_SetChannelLedRGB` API) |
| 외장 SRAM (PSRAM) | 없음 | 있음 (MCU 핀 PC10 사용) |
| 내장 버튼 MCU 핀 | PC10 / PC11 / PC12 | PC11 / PC12 / PC13 (한 핀 시프트) |
| 외부 GPIO 커넥터 위치/라벨 | [Rev 1.1 핀맵](external-gpio-rev1.1.md) | [Rev 2.0 핀맵](external-gpio-rev2.0.md) |
| 배포 ZIP | `Rev1.1.zip` | `Rev2.0.zip` |

> 본인 보드의 Rev 와 다른 ZIP 을 풀어 빌드하면 — 빌드 자체는 통과하지만 — 내장 버튼/LED 의 MCU 핀 매핑이 한 칸씩 어긋나 동작하지 않습니다. 보드 라벨을 먼저 확인하세요.

---

## 관련 문서

- [Getting Started — 01 하드웨어 연결](../getting-started/01-hardware-setup.md) — 보드 케이블 연결 순서
- [외부 IO API](../api-reference/04-external-io.md) — GPIO/ADC 제어 함수
- [LED & 버튼 API](../api-reference/03-led-btn-control.md) — LED·버튼 함수
- [USB 시리얼 통신 API](../api-reference/05-usb-connectivity.md) — USB-C 사용법
- [USB 메모리 로깅 API](../api-reference/06-usb-data-logging.md) — USB 메모리 로깅
- [KIT H10 Firmware](../kit-h10-firmware/) — H10 본체 펌웨어 업데이트
- [Bootloader](../bootloader/) — XM10 펌웨어 업로드 방법
