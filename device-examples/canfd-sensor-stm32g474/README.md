# CAN-FD 내 장치 — STM32G474 참조 구현 (Ex.45 의 상대 장치)

XM10 예제 [Ex.45 CAN-FD My Sensor](../../examples/45_CANFD_My_Sensor/) 와 주고받는 장치 펌웨어입니다. 직접 CAN-FD 장치를 만들 때 출발점으로 쓰세요.

> 🧰 **필요한 것: NUCLEO-G474RE 보드 + CAN-FD 트랜시버 보드.** Nucleo 보드에는 CAN 트랜시버가 없어서, XM10 과 연결하려면 트랜시버 보드가 꼭 필요합니다.
>
> 🟢 XM10 **Rev 2.0** 과 씁니다 (Ex.45 가 Rev 2.0 전용).

---

## 이 장치가 하는 일

| 무엇을 | 언제 | XM10 에서 보이는 것 |
|------|------|------|
| 주기 데이터(카운터 + ADC 2채널) 보내기 | 1 ms 마다 | `xm10` 도구 `0xF0` 탭의 `ADC0` · `ADC1` |
| 살아있음 보내기 | 1 초마다 (켜질 때 재시작 알림 한 번) | XM10 LED 1 켜짐 |
| LED 명령 받기 | XM10 이 보낼 때 (Ex.45 는 100 ms 마다) | 버튼 1 을 누를 때마다 이 보드의 LD2 가 켜지고 꺼짐 |

> ℹ️ 위 동작은 코드가 정한 것입니다. 보드에서는 아직 확인하지 못했습니다. (보드 확인 전) <!-- board-check -->

---

## 준비물

| 준비물 | 설명 |
|------|------|
| NUCLEO-G474RE | ST 의 STM32G474RE 개발 보드. 24 MHz 크리스털이 달려 있어 정확한 클럭을 씁니다 |
| CAN-FD 트랜시버 보드 | **CAN FD 5 Mbit/s 를 지원**하고, MCU 쪽 로직 전압을 **3.3 V** 로 쓸 수 있는 것. 데이터시트에 'CAN FD' 와 '5 Mbit/s' 가 적혀 있는지 확인하세요. 클래식 CAN 전용 트랜시버(최대 1 Mbit/s)로는 동작하지 않습니다 |
| 점퍼선 · 120 Ω 저항 | 버스 끝의 종단 저항(트랜시버 보드에 있으면 그것을 씁니다) |
| STM32CubeIDE | 빌드 · 올리기 (1.13 에서 빌드를 확인했습니다) |

---

## 배선

| NUCLEO-G474RE | 트랜시버 보드 |
|------|------|
| PA11 (FDCAN1_RX) | RXD |
| PA12 (FDCAN1_TX) | TXD |
| 3V3 | 로직 전압(VIO 또는 VCC — 보드 설명서를 따르세요) |
| GND | GND |

| 트랜시버 보드 | XM10 |
|------|------|
| CAN_H · CAN_L · GND | CAN-FD #2 포트의 CAN_H · CAN_L · GND |

- **종단 저항** — 버스 양 끝에 120 Ω 이 하나씩 있어야 합니다. XM10 보드에는 이미 있으므로, 장치 쪽 끝에 하나를 둡니다.
- 트랜시버의 버스 쪽 전원(보통 5 V)은 그 보드 설명서를 따르세요. Nucleo 보드의 5V 핀을 쓸 수 있습니다.
- PA11 · PA12 의 위치는 ST 의 NUCLEO-G474RE 사용자 설명서 핀 배치도에서 찾으세요. PB8 · PB9 로도 FDCAN1 을 쓸 수 있지만, PB8 은 부팅 핀(BOOT0)을 겸해서 이 예제는 쓰지 않았습니다.
- ADC — PA0 → `ADC0`, PA1 → `ADC1` (0 ~ 3.3 V, 12 비트). 가변저항의 가운데 단자를 연결하면 그래프가 움직입니다. 아무것도 연결하지 않으면 흔들리는 값이 보입니다.

---

## 빌드 · 올리기

1. STM32CubeIDE → **File → Import → Existing Projects into Workspace** → 이 폴더의 `STM32CubeIDE` 폴더를 고릅니다.
2. **Build** → ✅ `0 errors`
3. Nucleo 보드를 USB 로 PC 에 연결하고 **Run** (ST-LINK).
4. XM10 에 [Ex.45](../../examples/45_CANFD_My_Sensor/) 를 올리고 두 보드를 연결합니다 → ✅ XM10 LED 1 이 켜집니다. (보드 확인 전) <!-- board-check -->

> LD2 가 켜자마자 빠르게 깜빡이면 초기화에 실패한 것입니다(아래 '흔한 증상').

---

## XM10 과 맞출 것

장치 번호 n = `SENSOR_NODE_ID`(`Inc/canfd_sensor.h`, 기본 `0x20`) — XM10 예제의 `MY_DEVICE_ID` 와 같아야 합니다.

| 항목 | 내용 |
|------|------|
| 버스 | CAN-FD, 11비트 ID, 데이터 구간 고속 전송(BRS), 1 Mbit/s / 5 Mbit/s, 샘플 포인트 75 % |
| 주기 데이터 (보내기) | ID `0x180 + n`, 16 바이트, 1 ms 마다 — 바이트 0~3 카운터(uint32, 작은 쪽 바이트부터, 보낼 때마다 +1) · 4~5 ADC 0(uint16) · 6~7 ADC 1(uint16) · 8~15 은 0 |
| 살아있음 (보내기) | ID `0x700 + n`, 1 바이트 `0x05`, 1 초마다. 켜질 때 `0x00` 을 한 번(재시작 알림) |
| 명령 (받기) | ID `0x200 + n`, 2 바이트 `[순번, LED(0 = 끔 · 1 = 켬)]` |
| 동기 신호 (받기, 선택) | ID `0x080` — 아래 '동기 신호' |

**명령 순번 거르기** — 순번이 지난번보다 1 ~ 127 앞서면 새 명령, 같거나 뒤면 버립니다(중복 · 순서 뒤바뀜). 명령이 1 초 넘게 없었으면 XM10 이나 장치가 다시 켜졌을 수 있어 다음 순번을 새 기준으로 받아들입니다. 그래서 연결이 끊긴 동안 XM10 쪽에 남아 있던 옛 명령이 먼저 오면 그 명령은 걸러지지 않습니다.

---

## 코드 구성

| 파일 | 하는 일 |
|------|------|
| `Src/canfd_sensor.c` · `Inc/canfd_sensor.h` | **XM10 과 주고받는 규칙** — 주기 데이터 · 살아있음 · 명령 순번. MCU 에 묶이지 않습니다(HAL 을 쓰지 않음) |
| `Src/main.c` | G474 의 클럭 · FDCAN · ADC · LED. 규칙이 쓰는 함수 세 개(`Sensor_PortSend` · `Sensor_PortReadAdc` · `Sensor_PortSetLed`)를 HAL 로 채웁니다 |
| `Src/stm32g4xx_hal_msp.c` | FDCAN · ADC 의 클럭과 핀 |
| `Drivers/` · `STM32CubeIDE/` | ST 의 HAL · CMSIS 와 프로젝트 파일 |

---

## 다른 MCU 로 만들 때 — 컨트롤러가 갖출 것

- **CAN FD(ISO) + 데이터 구간 고속 전송(BRS)**, 11비트 ID, 한 메시지 16 바이트 이상
- **속도와 샘플 포인트를 XM10 과 같게** — 중재 1 Mbit/s · 데이터 5 Mbit/s, 둘 다 샘플 포인트 75 %. 이 예제의 값(FDCAN 클럭 80 MHz 기준):

  | 구간 | 분주 | seg1 | seg2 | 재동기 폭 | 속도 · 샘플 포인트 |
  |------|:---:|:---:|:---:|:---:|------|
  | 중재 | 10 | 5 | 2 | 2 | 80 MHz ÷ 10 ÷ 8 = 1 Mbit/s · 6/8 = 75 % |
  | 데이터 | 1 | 11 | 4 | 4 | 80 MHz ÷ 1 ÷ 16 = 5 Mbit/s · 12/16 = 75 % |

  클럭이 다르면 같은 샘플 포인트가 되도록 다시 계산하세요.
- **송신 지연 보상(TDC)** — 5 Mbit/s 에는 꼭 켭니다. 오프셋 = 데이터 분주 × 데이터 seg1 (이 예제는 11).
- **정확한 클럭** — 크리스털을 쓰세요. 내부 RC 클럭은 5 Mbit/s 에서 오차가 큽니다.
- **받기 필터** — `0x200 + n`(명령), `0x080`(동기 신호, 선택).

### 옮기는 방법

- **다른 STM32(FDCAN 이 있는 G0 · G4 · H5 · H7 · L5 · U5)** — `main.c` 의 클럭 · 핀 · FDCAN 설정만 그 MCU 에 맞게 바꾸고 `canfd_sensor.c` 는 그대로 씁니다. HAL 판에 따라 받은 메시지의 길이 값 모양이 다를 수 있습니다 — `_DlcToLen()` 을 확인하세요(이 예제는 길이 코드 0 ~ 15 를 그대로 받는 판).
- **ST 가 아닌 MCU** — 필요한 것은 네 가지입니다: CAN-FD 초기화(위 속도) · 받기 필터 · 보내기 · 받기. `canfd_sensor.h` 의 함수 세 개를 만들고, 받은 메시지를 `Sensor_OnFrame()` 으로 넘기고, 메인 루프에서 `Sensor_Poll()` 을 부릅니다.
- **장치 없이 PC 로 시험** — CAN-FD USB 어댑터와 [PC 시뮬레이터](../canfd-sensor-pc-sim/)를 쓰세요.

---

## 동기 신호 (선택)

XM10 예제에서 `MY_USE_SYNC` 를 `true` 로 바꾸면 XM10 이 1 kHz 동기 신호(ID `0x080`)를 보냅니다. 이 장치에서 `SENSOR_USE_SYNC` 를 `1` 로 바꾸면 신호 직후에 주기 데이터를 보냅니다. 신호는 잠깐 멈추거나 빠질 수 있어서, 5 ms 동안 없으면 자기 주기(1 ms)로 돌아갑니다.

---

## 흔한 증상

| 증상 | 원인 | 확인 |
|------|------|------|
| 켜자마자 LD2 가 빠르게 깜빡임 | 초기화 실패 — 크리스털(HSE)이 돌지 않거나 FDCAN · ADC 설정 실패 | 보드 전원, 다른 Nucleo 보드인지(NUCLEO-G474RE 인지) |
| XM10 LED 1 느린 깜빡임(끊김) | 살아있음이 XM10 에 닿지 않음 — RXD · TXD 가 바뀜 · 트랜시버 전원 · 종단 저항 없음 · 장치 번호가 다름 · 트랜시버가 5 Mbit/s 를 지원하지 않음 | 배선 표, `SENSOR_NODE_ID` 와 `MY_DEVICE_ID` |
| XM10 LED 1 빠른 깜빡임 | XM10 쪽 장치 번호 등록 실패 | [Ex.45 흔한 실수](../../examples/45_CANFD_My_Sensor/) |
| 그래프의 ADC 가 흔들림 | PA0 · PA1 에 아무것도 연결하지 않음 | 가변저항 등을 연결 |
| LD2 가 명령을 따르지 않음 | 명령이 오지 않음, 또는 1 초 안에 옛 순번이 와서 버림 | 디버거로 `Sensor_GetStats()` 의 `cmd_new` · `cmd_ignored` |
| 끊겼다 붙었다 함 | 버스 오류에서 다시 시작하는 중 — 속도 · 샘플 포인트가 다르거나 종단이 없음 | 디버거로 `s_busoff_count`(main.c) |

---

## 라이선스

- `canfd_sensor.*` · `main.c` · `main.h` · `stm32g4xx_hal_msp.c` · 이 README 는 이 레포의 라이선스를 따릅니다.
- `Drivers/STM32G4xx_HAL_Driver` · `Drivers/CMSIS/Device` 는 ST 의 STM32CubeG4 원본이며 각 폴더의 `LICENSE.txt`(BSD-3-Clause)를, `Drivers/CMSIS/Include` 는 `Drivers/CMSIS/LICENSE.txt`(Apache 2.0)를 따릅니다.
- `Src/system_stm32g4xx.c` · `Src/stm32g4xx_it.c` · `Inc/stm32g4xx_it.h` · `Inc/stm32g4xx_hal_conf.h` 와 `STM32CubeIDE/` 의 프로젝트 · 링커 스크립트 · startup · syscalls · sysmem 은 ST 의 NUCLEO-G474RE 템플릿에서 가져왔고 이 폴더의 `LICENSE.txt`(BSD-3-Clause)를 따릅니다.
