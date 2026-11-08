# Ex.45 — CAN-FD My Sensor (CAN-FD 로 연결한 내 장치와 주고받기)

> 🎯 **학습 목표**:
> - 직접 만든 CAN-FD 장치의 번호를 등록하고, 장치가 보내는 주기 데이터의 최신값을 읽습니다 (`XM_CanAttachDevice` · `XM_CanReadLatest`).
> - 장치에 명령을 보내고 결과를 확인합니다 (`XM_CanSend` · `XM_CAN_OK`).
> - 연결 상태와 통계를 봅니다 (`XM_CanIsConnected` · `XM_CanGetStats` · `XM_CanGetBusStats`).
> - 프로토콜 없이 ID 와 데이터만으로 주고받습니다 (`XM_CanSendFrame` · `XM_CanReadFrame`).
>
> ⏱️ 권장 시간: 60분 | 🔧 난이도: ⭐⭐⭐ | 🟢 **Rev 2.0 전용**
> 🧰 사전 예제: [Ex.41 IMU Hub Dashboard](../41_IMU_Hub_Dashboard/) | 📚 관련 docs: [CAN 통신](../../docs/api-reference/11-canbus.md) · [`xm_api_canbus.h` 레퍼런스](../../docs/api-reference/ref/xm-api-canbus.md) · [하드웨어 — CAN-FD 포트](../../docs/hardware/README.md#can-fd-포트)

> 🟢 **Rev 2.0 전용** — Rev 1.1 은 CAN-FD #2 포트로 내 장치와 주고받는 기능을 지원하지 않습니다. 본인 보드의 리비전은 [보드 리비전 비교](../../docs/hardware/README.md#보드-리비전-비교)에서 확인하세요.

---

## 🧰 준비물

| 준비물 | 설명 |
|------|------|
| XM10 (Rev 2.0) | 이 예제를 올릴 보드 |
| 내 CAN-FD 장치 | 아래 '장치가 지킬 것' 을 따르는 장치. 처음이면 둘 중 하나를 쓰세요 |
| ① 참조 장치 | [`device-examples/canfd-sensor-stm32g474/`](../../device-examples/canfd-sensor-stm32g474/) — **Nucleo-G474 보드와 CAN-FD 트랜시버가 필요합니다** |
| ② PC 시뮬레이터 | [`device-examples/canfd-sensor-pc-sim/`](../../device-examples/canfd-sensor-pc-sim/) — CAN-FD 를 지원하는 USB 어댑터와 Python 이 필요합니다 |

---

## ⚠️ USB-CDC 단독 점유

- PC 프로그램은 **한 번에 하나만** 연결하세요 (PhAI Studio · `xm10` 도구 · 시리얼 터미널 중 하나).
- **SDK 에 함께 들어 있는 `xm10` 도구**(`pc-data-tool` 폴더)로 보세요.

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

장치를 연결하고 `xm10` 도구로 포트를 열면 아래처럼 보입니다. (보드 확인 전) <!-- board-check -->

| 어디서 | 무엇이 | 주기 |
|------|------|:---:|
| `xm10` 도구 `0xF0` 탭 | `ADC0` · `ADC1`(장치의 ADC 값) · `Skipped`(건너뛴 값 수) · `Connected`(1 = 연결) | 100 Hz |
| `xm10` 도구 메시지 창 | `[CAN] dev=0x20 connected=1 rx=… alive=… …` (장치 통계) · `[CAN] mode=0 rx=… free_rx=… …` (포트 통계) | 1 초 |
| 내 장치 | 명령 `[순번, LED]` — 버튼 1 을 누를 때마다 장치 LED 를 켜고 끕니다 | 100 ms |
| 버스 | 자유 송신 — ID `0x321`, 4 바이트 카운터 | 1 초 |

| LED | 뜻 |
|:---:|------|
| LED 1 켜짐 | 장치가 연결되었습니다 |
| LED 1 느린 깜빡임 | 끊김 — 3 초 동안 장치의 살아있음 신호를 받지 못했습니다 |
| LED 1 빠른 깜빡임 | 장치 번호를 등록하지 못했습니다. `MY_DEVICE_ID` 를 확인하세요 |
| LED 2 | 장치에 보내는 LED 명령 (켬 · 끔) |

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

- **두 가지 방법** — ① **장치 API**: 장치 번호를 등록해 두면 그 장치의 주기 데이터와 살아있음 신호를 XM10 이 알아서 받아 둡니다. 명령도 보낼 수 있습니다. ② **자유 송수신**: 프로토콜 없이 ID 와 데이터만으로 보내고 받습니다. 둘을 함께 써도 됩니다.
- **장치가 지킬 것** (장치 번호 n = `MY_DEVICE_ID`, 이 예제는 `0x20`)

  | 항목 | 내용 |
  |------|------|
  | 버스 | CAN-FD, 11비트 ID, 데이터 구간 고속 전송(BRS), 1 Mbit/s / 5 Mbit/s, 샘플 포인트 75 % |
  | 주기 데이터 | ID `0x180 + n`, 길이 16, 최대 1 kHz — 바이트 0~3 카운터(uint32, 작은 쪽 바이트부터, 보낼 때마다 +1) · 4~5 ADC 채널 0(uint16) · 6~7 ADC 채널 1(uint16) · 8~15 은 0 |
  | 살아있음 | ID `0x700 + n`, 1 바이트 `0x05`, 1 초마다 — 3 초 동안 없으면 XM10 은 '끊김' 으로 봅니다. 켜질 때 `0x00` 을 한 번 보내면(선택) XM10 은 그동안 받아 둔 값을 버립니다 |
  | 명령(받기) | ID `0x200 + n`, 2 바이트 `[순번, LED(0 = 끔 · 1 = 켬)]` — 순번이 지난번보다 1 ~ 127 앞서면 새 명령, 아니면 버립니다 |

- **장치 번호** — 1 ~ 127 중 예약 번호(`0x01` · `0x02` · `0x0C` · `0x0D` · `0x0F` · `0x10` · `0x11`)를 뺀 번호를 씁니다. `XM_CanIsDeviceIdAllowed()` 로 확인할 수 있습니다. 고정 번호와 쓸 수 있는 번호는 [CAN 통신 — 번호 한눈에 보기](../../docs/api-reference/11-canbus.md#3-번호-한눈에-보기) 표에 있습니다. 장치는 4 대까지 등록합니다. 같은 버스에서 번호가 겹치면 XM10 은 알아채지 못하니 장치마다 다른 번호를 쓰세요.
- **최신값 하나** — XM10 은 장치별로 최신값 하나만 보관합니다. 같은 값을 다시 읽어도 `true` 이므로 새 값인지는 `seq` 로 가립니다. 끊긴 뒤에도 옛 값이 읽히니, 연결 여부는 `XM_CanIsConnected()` 로 따로 봅니다.
- **`XM_CAN_OK` 는 '보내기 큐에 실림'** 입니다 — 장치가 받았다는 뜻이 아닙니다. `XM_CAN_OK` 가 0 이므로 결과는 꼭 `== XM_CAN_OK` 로 확인하세요.
- **같은 포트에 센서 허브(IMU Hub · EMG Hub)를 함께 연결해도 됩니다** — 장치 번호만 겹치지 않게 쓰세요.
- **배선** — XM10 CAN-FD #2 포트의 CAN_H · CAN_L · GND 를 장치 트랜시버의 같은 이름에 연결합니다. 버스 양 끝에는 120 Ω 종단 저항이 있어야 합니다. [하드웨어 — CAN-FD 포트](../../docs/hardware/README.md#can-fd-포트)를 보세요.

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
#define MY_DEVICE_ID  (0x20U)                                   // 장치 펌웨어와 같은 번호

void Control_Setup(void)
{
    s_attached = XM_CanAttachDevice(MY_DEVICE_ID);              // ① 등록 — Control_Setup() 안에서만
}

void Control_Loop(void)
{
    XmCanFrame_t f;
    if (XM_CanReadLatest(MY_DEVICE_ID, 1, &f) && (f.seq != s_last_seq)) {   // ② 새 값일 때만
        s_last_seq = f.seq;
        uint32_t counter = (uint32_t)f.data[0] | ((uint32_t)f.data[1] << 8)
                         | ((uint32_t)f.data[2] << 16) | ((uint32_t)f.data[3] << 24);
        /* … ADC 값은 f.data[4..7] … */
    }

    const uint8_t cmd[2] = { s_cmd_seq, s_led_cmd };            // ③ [순번, LED]
    if (XM_CanSend(MY_DEVICE_ID, cmd, sizeof(cmd)) == XM_CAN_OK) {
        s_cmd_seq++;                                            //    수락됐을 때만 순번을 올립니다
    }

    bool connected = XM_CanIsConnected(MY_DEVICE_ID);           // ④ 3 초 안에 살아있음을 받았나
}
```

전체 코드: [`canfd_my_sensor.c`](canfd_my_sensor.c)

> 🧒 ② `XM_CanReadLatest` 는 같은 값을 다시 읽어도 `true` 입니다. `seq` 가 지난번과 같으면 새 메시지가 아닙니다.
>
> 🧒 ③ 끊긴 동안에는 `XM_CAN_NOT_CONNECTED` 로 거부됩니다. 거부된 명령은 버스에 나가지 않았으므로 다음에 같은 순번으로 다시 보내면 됩니다.
>
> 🧒 예제 파일에서는 100 ms 마다 명령을 보내고, 1 초마다 통계를 보냅니다.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

아래 ✅ 는 코드가 의도한 동작이며, 보드에서는 아직 확인하지 못했습니다. (보드 확인 전) <!-- board-check -->

1. **장치 준비** — 참조 장치(G474) 또는 PC 시뮬레이터를 장치 번호 `0x20` 으로 준비합니다(각 폴더의 README).
2. **배선** — CAN_H · CAN_L · GND 를 연결하고 버스 양 끝의 종단 저항을 확인합니다.
3. **빌드 + 플래시** → ✅ `0 errors`
4. **전원 켜기** → ✅ 장치가 살아있음을 보내면 LED 1 이 켜집니다
5. **`xm10` 도구로 포트 열기** → ✅ `0xF0` 탭에 `ADC0` · `ADC1` 이 움직이고 `Connected` 가 1 입니다. 1 초마다 `[CAN]` 통계 두 줄이 옵니다
6. **버튼 1 누르기** → ✅ LED 2 가 바뀌고, 장치의 LED 도 따라 바뀝니다
7. **장치 전원을 끄기** → ✅ 3 초 뒤 LED 1 이 느리게 깜빡이고 `Connected` 가 0 이 됩니다. 다시 켜면 LED 1 이 켜집니다
8. **변형 1 — 동기 신호**: 파일 위쪽의 `MY_USE_SYNC` 를 `true` 로 바꾸면 XM10 이 1 kHz 동기 신호(ID `0x080`)를 보냅니다. 장치는 이 신호를 샘플링 정렬에만 쓰고, 신호가 없으면 자기 주기로 계속 보내게 만드세요.
9. **변형 2 — 장치 번호 바꾸기**: `MY_DEVICE_ID` 와 장치 펌웨어의 번호를 함께 바꿉니다. `XM_CanIsDeviceIdAllowed()` 가 `true` 인 번호만 됩니다.
10. **변형 3 — 자유 수신 보기**: 장치가 다른 ID 로 보내는 메시지는 `XM_CanReadFrame()` 로 받은 순서대로 꺼냅니다. 통계의 `free_rx` · `last_id` 가 바뀝니다.

---

## 5️⃣ 다음 단계

- 클래식 CAN 모터 드라이버(CANopen)를 설정하고 움직이기: [Ex.46 CANopen Drive](../46_CANopen_Drive/) · [Ex.47 CANopen Drive PDO](../47_CANopen_Drive_PDO/)
- 함수 · 반환값 · 한 포트에서 지킬 것을 자세히: [CAN 통신](../../docs/api-reference/11-canbus.md) · [`xm_api_canbus.h` 레퍼런스](../../docs/api-reference/ref/xm-api-canbus.md)
- 장치 펌웨어를 다른 MCU 로 옮기기: [참조 장치 README](../../device-examples/canfd-sensor-stm32g474/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| LED 1 빠른 깜빡임 | 장치 번호 등록 실패 — 예약 번호 · 같은 번호 두 번 · 5 대째 · `Control_Setup()` 밖에서 등록 | `MY_DEVICE_ID` 를 `XM_CanIsDeviceIdAllowed()` 가 `true` 인 번호로, 장치 펌웨어와 같게 씁니다 |
| LED 1 느린 깜빡임 (끊김) | 살아있음(`0x700 + n`, `0x05`)을 3 초 동안 못 받음 — 배선 · 종단 저항 · 버스 설정(1 / 5 Mbit/s, 75 %) · 장치 번호가 서로 다름 | 장치 번호와 버스 설정을 맞추고 배선과 종단 저항을 확인합니다 |
| 그래프가 움직이지 않음 | 주기 데이터(`0x180 + n`)를 아직 못 받았거나 길이가 8 보다 짧음 | 장치가 보내는 ID 와 길이(16)를 확인합니다 |
| `Skipped` 가 조금씩 늘어남 | 장치가 1 kHz 로 보내면 XM10 이 읽기 전에 새 값으로 바뀌는 일이 있습니다. 버스에서 잃은 값도 함께 셉니다 | 장치 통계의 `overwrite`(읽기 전에 바뀐 수)와 비교해 봅니다 |
| 장치 통계의 `last_fail=3` | 끊긴 동안 명령을 보냄(`XM_CAN_NOT_CONNECTED`) | 다시 연결되면 수락됩니다. 순번은 수락됐을 때만 올립니다 |
| 포트 통계의 `pending` 이 계속 큼 · `status` 의 bit0 | 받아 주는 장치가 없거나 버스 오류(bus-off) | 배선 · 종단 저항 · 버스 설정을 확인합니다 |
| `if (XM_CanSend(...))` 로 확인했더니 실패를 성공으로 읽음 | `XM_CAN_OK` 가 0 입니다 | `== XM_CAN_OK` 로 비교합니다 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)

> ℹ️ **아직 보드에서 확인되지 않은 예제입니다** (v2.10.0 시점). 처음 돌려보실 때 위 4️⃣의 순서로 단계별로 확인해 보시고, 결과를 알려 주시면 다음 사람이 덜 헤맵니다. (보드 확인 전) <!-- board-check -->
