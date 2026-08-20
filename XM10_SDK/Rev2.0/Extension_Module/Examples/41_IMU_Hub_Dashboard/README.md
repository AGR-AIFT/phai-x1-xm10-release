# Ex.41 — IMU Hub Dashboard (6축 자세 대시보드)

> 🎯 **학습 목표**:
> - **IMU Hub Module**에서 최대 **6개 IMU**의 방위(쿼터니언)를 받아온다.
> - 쿼터니언을 사람이 읽는 **오일러 각(roll/pitch/yaw)**으로 on-device 변환한다.
> - 연결 개수 자동 감지 + **PhAI Studio 18채널 실시간 스트리밍**까지 한 흐름.
>
> ⏱️ 권장 시간: 40분 | 🔧 난이도: ⭐⭐⭐ | 🟢 **Rev 2.0 전용** (FDCAN2 센서허브 버스)
> 🧰 사전 예제: [Ex.09 CDC Stream](../09_CDC_Stream/) · [Ex.16 TinyAI Sensor Fusion](../16_TinyAI_Sensor_Fusion/)
> 📚 관련 docs: [USB 연결성 (0xF0 스트리밍)](../../docs/api-reference/05-usb-connectivity.md) · [LED/BTN 제어](../../docs/api-reference/03-led-btn-control.md)

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

IMU Hub Module에 연결된 IMU들의 **자세(기울기·방향)**를 실시간으로 읽어, 전신 자세 대시보드처럼 PhAI Studio에 뿌립니다. 모터를 쓰지 않는 **순수 관찰형** 예제라 안전하게 IMU 데이터 파이프라인을 익힐 수 있습니다.

| 요소 | 동작 |
|---|---|
| 데이터 | 각 IMU의 쿼터니언 `q_w,q_x,q_y,q_z` → 오일러 `roll/pitch/yaw`(deg) |
| 연결 감지 | `connected_mask`(bit0~5)로 0~6개 자동 인식. 미연결 슬롯은 0으로 전송 |
| LED1 | 연결된 IMU가 많을수록 빠르게 점멸 (0개 = 느린 대기 점멸) |
| 스트리밍 | `0xF0` 채널 18개(6 IMU × r/p/y), 50Hz |

신호 흐름:
```
IMU Hub (FDCAN2) → XM.status.imu_hub.sensor[i].q_*  (core 가 자동 갱신)
                 → 쿼터니언→오일러 변환 (_QuatToEulerDeg)
                 → 50Hz 스로틀 → PhAI Studio 0xF0
```

> 🟢 **Rev 2.0 전용**: IMU Hub Module은 **FDCAN2 센서허브 버스**로 연결되며, 이 버스는 Rev 2.0 보드에만 있습니다. Rev 1.1에서는 `XM.status.imu_hub.is_connected`가 항상 `false`입니다.

---

## 2️⃣ IMU Hub Module이란? — 무엇을 할 수 있는 모듈인가

**IMU Hub Module**은 여러 개의 관성센서(IMU)를 한 곳에 모아 XM10으로 넘겨주는 **센서 허브**입니다. XM10은 개별 IMU와 직접 씨름하지 않고, 허브가 정리해 준 자세 데이터를 받기만 하면 됩니다.

| 특징 | 내용 |
|---|---|
| **채널** | 최대 **6채널** IMU 동시 사용 |
| **지원 센서** | **EBIMU**(EBIMU-9DOFV6) · **Xsens MTi-630** — 서로 다른 두 종류를 **섞어서** 연결 가능 |
| **Auto-sense** | 연결된 IMU를 **자동 감지** — 몇 번 슬롯에 무엇을 꽂았는지 일일이 설정할 필요 없음 |
| **출력** | 각 IMU의 방위(쿼터니언) + 가속도 + 각속도 |
| **연결** | XM10과 **FDCAN2**(DOP V3) 센서허브 버스로 통신 |

### 🔌 연결만 하면 끝 — 자동 데이터 수신

IMU를 허브에 꽂고, 허브를 XM10에 연결하기만 하면 됩니다. 별도 설정 없이 **XM10이 자동으로 자세 데이터를 받기 시작**합니다(Auto-sense로 센서 인식 + 자동 연결). 사용자 코드는 그저 `XM.status.imu_hub`를 읽으면 됩니다.

### 🛠️ PhEEL Studio 연동 (공개 예정)

IMU의 **캘리브레이션**과 **센서 설정 변경**은 Angel Robotics 자체 개발 도구인 **PhEEL Studio**로 합니다. PhEEL Studio는 IMU Hub에 직접 붙어, 각 채널의 자세를 실시간으로 보면서 영점·축 방향·출력 설정 등을 조정할 수 있습니다. *(PhEEL Studio는 공개 예정입니다.)*

> 📸 **PhEEL Studio — IMU Hub 6채널 자세 모니터링** — 사진·영상 준비 중

> 💡 이 예제는 "IMU 데이터로 **무엇을 할 수 있는가**"의 가장 기본 — **자세 시각화** — 를 보여줍니다. 쿼터니언→오일러 변환은 모든 자세 표시의 출발점입니다.

---

## 3️⃣ 사전 지식 — 시작 전 알아둘 것

- **쿼터니언 vs 오일러** — IMU Hub가 XM으로 보내는 방위는 **쿼터니언**(q_w,q_x,q_y,q_z, 4개 값)입니다. roll/pitch/yaw 형태로는 오지 않으므로, 이 예제가 직접 변환합니다. 쿼터니언은 짐벌락이 없어 IMU가 방위를 안정적으로 표현하는 표준 방식입니다.
- **데이터는 읽기만** — `core_process`가 FDCAN2 수신을 받아 `XM.status.imu_hub`를 자동 갱신합니다. 사용자 코드는 `XM.status.imu_hub.sensor[i]`를 **읽기만** 하면 됩니다(드라이버 직접 호출 불필요).
- **connected_mask** — `bit0`=IMU0 … `bit5`=IMU5. 연결된 슬롯만 1. 미연결 슬롯의 데이터는 신뢰하지 말 것(이 예제는 0으로 채웁니다).
- **0xF0 User Custom 스트리밍** — `XM_SetUsbCustomMeta()`로 채널 라벨을 등록하고 `XM_SendUsbDataWithId()`로 float 배열을 보냅니다. ([Ex.09](../09_CDC_Stream/))
  - ⚠️ `XM_SetUsbCustomMeta`는 **USB 연결당 Module ID 1개**만 라벨링됩니다. 그래서 6개 IMU를 **하나의 0xF0 채널 그룹(18채널)**으로 묶어 보냅니다.

---

## 4️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
#define IMU_STREAM_PERIOD_MS  20U   // 50Hz 스로틀 (자세 관찰에 충분 + USB 절약)
#define RAD_TO_DEG            57.2957795130823f

/* ① 쿼터니언(w,x,y,z) → 오일러(deg). Aerospace ZYX. pitch는 asin 정의역 포화로 NaN 방지 */
static void _QuatToEulerDeg(const XmImuHubSensor_t* s, float* roll, float* pitch, float* yaw) {
    float w = s->q_w, x = s->q_x, y = s->q_y, z = s->q_z;
    *roll  = atan2f(2.0f*(w*x + y*z), 1.0f - 2.0f*(x*x + y*y)) * RAD_TO_DEG;
    float sinp = 2.0f*(w*y - z*x);
    if (sinp >  1.0f) sinp =  1.0f;          // 짐벌 근처 포화
    if (sinp < -1.0f) sinp = -1.0f;
    *pitch = asinf(sinp) * RAD_TO_DEG;
    *yaw   = atan2f(2.0f*(w*z + x*y), 1.0f - 2.0f*(y*y + z*z)) * RAD_TO_DEG;
}

/* ② Control_Loop: 연결 IMU 만 변환, 미연결은 0. 50Hz 로 0xF0 스트림 */
for (uint8_t i = 0; i < XM_IMU_HUB_SENSOR_COUNT; i++) {
    float* slot = &s_posture[i * 3];
    if (hub->connected_mask & (1U << i)) _QuatToEulerDeg(&hub->sensor[i], &slot[0], &slot[1], &slot[2]);
    else                                 { slot[0] = slot[1] = slot[2] = 0.0f; }
}
XM_SendUsbDataWithId(s_posture, sizeof(s_posture), 0xF0);
```

전체 코드: [`imu_hub_dashboard.c`](imu_hub_dashboard.c)

> 🧒 핵심: IMU Hub는 쿼터니언만 주고, "사람이 읽는 각도"로 만드는 건 여러분 몫입니다. ①이 그 변환입니다.

---

## 5️⃣ 실험 — 절차

1. **HW**: IMU Hub Module을 XM10의 FDCAN2 센서허브 포트에 연결. IMU를 1~6개 허브에 연결.
2. **빌드 + 플래시** → 부팅. 허브 미연결이면 LED1이 느리게(1초) 점멸.
3. **연결 확인** — IMU를 꽂으면 Auto-sense로 인식됩니다. 연결 개수가 늘수록 LED1 점멸이 빨라집니다.
4. **PhAI Studio** — USB Connect → `0xF0` 채널 선택. `IMU0 Roll/Pitch/Yaw … IMU5 …` 18채널이 뜹니다.
5. **자세 확인** — IMU를 손으로 기울여 보며 roll/pitch/yaw가 실제 움직임과 맞는지 확인. 미연결 슬롯은 평평한 0.
6. **혼합 사용 테스트** — EBIMU와 Xsens를 섞어 꽂아도 동일하게 표시되는지 확인(허브가 종류를 흡수).

> 🔧 변형: `IMU_STREAM_PERIOD_MS`를 바꿔 스트리밍 주기를 조절(10ms=100Hz ~ 50ms=20Hz)하며 부드러움 비교.

> 📸 **PhAI Studio — 0xF0 18채널 자세 그래프** — 사진·영상 준비 중

---

## 6️⃣ 확장 아이디어 — 여기서 무엇을 더 할 수 있나

이 예제는 **자세를 읽어오는 배관**만 제공합니다. IMU 데이터로 할 수 있는 것은 훨씬 많습니다:

- **A. 관절각 추정** — 두 IMU의 상대 자세 차이로 무릎·고관절 각도를 계산(세그먼트 간 각도).
- **B. 보행 위상 검출** — 정강이/허벅지 IMU의 pitch 주기에서 stance/swing 위상 판별.
- **C. 낙상·기울기 감지** — 몸통 IMU의 roll/pitch가 임계 초과 시 경고 LED.
- **D. 제스처·동작 분류** — 자세 시퀀스를 [Ex.16 TinyAI](../16_TinyAI_Sensor_Fusion/)의 온디바이스 NN에 넣어 동작 인식.
- **E. 자세 기반 보조 트리거** — 특정 자세/구간에서만 제어를 켜는 상위 조건으로 활용.

> 📝 IMU Hub는 "여러 관절의 방위를 한 번에" 주는 창구입니다. 무엇을 계산해 무엇에 쓸지가 여러분의 설계 영역입니다.

---

## 7️⃣ 다음 단계

- User Custom 스트리밍 기초: [Ex.09 CDC Stream](../09_CDC_Stream/)
- 온디바이스 AI 센서 융합: [Ex.16 TinyAI Sensor Fusion](../16_TinyAI_Sensor_Fusion/)
- 보행 의도 FSM: [Ex.17 FSM Gait Intent](../17_FSM_Gait_Intent/)
- EMG Hub 버전(근활성도 바이오피드백): [Ex.42 EMG Hub Biofeedback](../42_EMG_Hub_Biofeedback/)

---

## ❓ 흔한 실수

| 증상 | 원인 | 해결 |
|---|---|---|
| 데이터가 전부 0 / 그래프 평평 | Rev 1.1 보드 (FDCAN2 센서허브 없음) | **Rev 2.0** 보드에서 실행 |
| `is_connected`가 false | 허브 미연결 / 케이블 / 전원 | FDCAN2 포트·허브 전원 확인 |
| 특정 슬롯만 0 | 그 슬롯 IMU 미연결/미인식 | `connected_mask` 확인, IMU 재연결(Auto-sense 재감지) |
| roll/pitch/yaw가 움직임과 안 맞음 | IMU 장착 축 방향 문제 | **PhEEL Studio**로 축·영점 캘리브 |
| PhAI Studio에 채널 없음 | Connect 안 함 / 라벨 미등록 | 포트 선택 → **Connect**, 0xF0 채널 선택 |
| 값이 튀거나 지연 | 스트리밍 주기 부적절 | `IMU_STREAM_PERIOD_MS` 조정 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
