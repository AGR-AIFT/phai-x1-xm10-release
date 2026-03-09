# 예제 14: PD 실시간 토크 제어 (PD Realtime Control)

본 예제는 **PD(비례-미분) 제어기**를 직접 구현하여 H10 고관절 모터에 실시간 토크를 인가하는 방법을 다룹니다. 제어 이론의 가장 기본적인 피드백 제어 기법을 2ms(500Hz) 제어 루프에서 실습합니다.

> 📖 API 레퍼런스: [H10 Control & Data](../../docs/api-reference/02-h10-control-n-data.md) · [Task State Machine](../../docs/api-reference/01-task-state-machine.md)

## 🎯 학습 목표 (Objective)

* **PD 제어 수식**(`tau = Kp * e + Kd * de/dt`)을 코드로 구현하는 방법을 학습합니다.
* **이산 미분 근사**(후향 차분법: `de = (e[k] - e[k-1]) / dt`)의 원리를 이해합니다.
* **토크 포화(Saturation)**: 모터 보호를 위해 출력을 `[-MAX, +MAX]` 범위로 클램핑하는 방법을 학습합니다.
* 2ms 주기의 **실시간 제어 루프** 안에서 센서 읽기 → 연산 → 명령 전송까지의 흐름을 이해합니다.

---

## ⚙️ 동작 원리 (How it Works)

이 예제는 OFF → STANDBY → **ACTIVE**(PD 제어)의 3단계 상태 머신으로 동작합니다.

### 1. 상태 전환

1.  **OFF:** CM(Central Module)과 CAN-FD 연결을 대기합니다.
2.  **STANDBY:** H10이 ASSIST 모드로 전환되기를 대기합니다. LED 1 Heartbeat.
3.  **ACTIVE:** PD 토크 제어를 실행합니다. LED 1 빠른 Blink.

### 2. PD 제어 루프 (ACTIVE 상태, 2ms 주기)

매 루프마다 다음을 수행합니다:

1.  **센서 읽기:** `XM.status.h10.rightHipMotorAngle`에서 현재 각도를 읽습니다.
2.  **오차 계산:** `e[k] = target_angle - current_angle`
3.  **미분 근사:** `de[k] = (e[k] - e[k-1]) / dt` (후향 차분법)
4.  **PD 법칙:** `tau = Kp * e + Kd * de`
5.  **토크 포화:** `clamp(tau, -5.0, +5.0)` Nm
6.  **명령 전송:** `XM_SetAssistTorqueRH/LH`로 좌/우 동일 토크 인가

### 3. 버튼 조작

*  **BTN 1 클릭:** 목표 각도의 부호를 반전합니다 (양 ↔ 음). LED 2로 방향을 표시합니다.
*  **BTN 2 클릭:** 목표 각도를 5도씩 증가시킵니다 (5 → 10 → 15 → 20 → 25 → 5 래핑).

---

## 🚀 실행 방법 (How to Use)

1.  `STM32CubeIDE`에서 본 예제 소스파일을 `user_app.c`으로 옮겨와서 빌드하고 펌웨어를 `XM10`에 업로드합니다.
2.  `KIT H10`의 전원을 켜고 `XM10`과 연결합니다.
3.  `angel'a DEV` 또는 다른 제어 수단을 통해 `KIT H10`의 모드를 **`ASSIST_MODE`로 변경**합니다.
4.  LED 1이 빠르게 깜빡이면 **PD 제어가 시작**된 것입니다.
5.  모터가 목표 각도(기본 10도)를 향해 토크를 인가하는지 확인합니다.
6.  **BTN 1**을 눌러 방향을 반전하고, **BTN 2**를 눌러 목표 각도를 변경하며 동작을 관찰합니다.
7.  USB CDC 터미널에서 500ms마다 `PD | Tgt:... Cur:... Err:... Tau:...` 디버그 메시지를 확인합니다.

---

## 💡 직접 해보기 (Things to Try)

* **Kp/Kd 튜닝:** `KP_GAIN`(기본 0.5)과 `KD_GAIN`(기본 0.02) 값을 변경하며 오버슈트, 진동, 수렴 속도의 변화를 관찰해보세요. Kp를 키우면 응답이 빨라지지만 진동이 커집니다.
* **D항 제거:** `KD_GAIN`을 0으로 설정하면 P-Only 제어가 됩니다. 목표 각도 근처에서 진동이 발생하는지 관찰하고, Kd의 감쇠(damping) 역할을 체감해보세요.
* **토크 한계 조절:** `MAX_TORQUE_NM`(기본 5.0)을 줄여서 포화가 더 자주 발생하도록 한 후, 응답 특성이 어떻게 변하는지 확인해보세요.
