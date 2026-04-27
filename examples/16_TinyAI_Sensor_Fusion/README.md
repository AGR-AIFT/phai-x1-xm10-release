# 예제 16: Tiny AI — 자세 분류 (H10 자세 데이터 + IMU Hub 센서 퓨전)

본 예제는 두 가지 IMU 데이터 소스 중 하나를 선택하여 자세(Pitch/Roll)를 획득하고, **3층 퍼셉트론(Tiny Neural Network)** 으로 실시간 자세를 분류하는 고급 기법을 다룹니다. MCU 위에서 직접 AI 추론을 수행하는 **Tiny ML** 워크플로우를 실습합니다.

> 📖 API 레퍼런스: [H10 Control & Data](../../docs/api-reference/02-h10-control-n-data.md)

## 🎯 학습 목표 (Objective)

* **두 가지 자세 데이터 획득 방법**을 비교 학습합니다: H10 전처리 데이터 vs IMU Hub 상보 필터
* **상보 필터**(`pitch = ALPHA * (pitch + gyro*dt) + (1-ALPHA) * pitch_acc`)로 가속도계와 자이로의 장점을 결합하는 **센서 퓨전** 개념을 학습합니다. (Mode B)
* MCU(Cortex-M7)에서 **신경망 순전파(forward pass)** 를 직접 수행하는 방법을 이해합니다.
* 4→8→4→3 구조의 **Tiny NN**에서 ReLU 활성화, argmax 분류의 원리를 학습합니다.
* **Global-frame vs Body-frame IMU 데이터**의 차이를 이해합니다.

---

## ⚙️ 동작 모드 선택 (Mode Selection)

이 예제는 **컴파일 타임**에 데이터 소스를 선택합니다. 소스 코드 상단의 `#define`을 변경하세요:

### Mode A — `USE_H10_PRECOMPUTED` (기본값, 추가 HW 불필요)

H10 CM이 내부에서 이미 센서 퓨전을 완료한 자세 데이터를 직접 사용합니다:
*  `leftHipImuSagittalPitch` — Sagittal 면 Pitch 각도 (deg)
*  `leftHipImuFrontalRoll` — Frontal 면 Roll 각도 (deg)

상보 필터가 필요 없으며, H10 연결만으로 동작합니다.

### Mode B — `USE_IMU_HUB_FUSION` (IMU Hub Module 필요)

IMU Hub Module의 **body-frame** 원시 가속도/자이로 데이터에 상보 필터를 적용하여 자세를 직접 추정합니다:
*  `sensor[0].acc_x/y/z` (g) — Body-frame 보정 가속도
*  `sensor[0].gyr_x/y/z` (deg/s) — Body-frame 보정 자이로
*  ALPHA=0.98, dt=0.001s (1ms = 1kHz) → 차단 주파수 약 3.2Hz

> ⚠️ **왜 H10 Global IMU에 상보 필터를 쓰면 안 되는가?**
> H10의 `leftHipImuGlobalAccX/Y/Z`는 이미 글로벌 좌표계로 회전 변환된 값입니다.
> Global frame에서 `acc_z ≈ 9.81` (항상), `acc_x ≈ 0`, `acc_y ≈ 0`이므로
> `atan2`로 Pitch/Roll을 추정하면 항상 ~0°가 나옵니다.
> 상보 필터에는 반드시 **body-frame** 가속도가 필요합니다.

---

## ⚙️ 동작 원리 (How it Works)

이 예제는 OFF → STANDBY → **ACTIVE**(자세 추정 + AI 추론)의 3단계 상태 머신으로 동작합니다. 토크를 인가하지 않는 **모니터링 전용** 예제입니다.

### Part A — 자세 데이터 갱신 (매 1ms, 1kHz)

*  **Mode A:** H10 전처리 Pitch/Roll을 그대로 읽기 (필터링 불필요)
*  **Mode B:** IMU Hub body-frame 데이터에 상보 필터 적용
    1.  **가속도계 기반:** `pitch_acc = atan2(acc_x, sqrt(acc_y² + acc_z²))` — 정적 자세 기준
    2.  **자이로 적분:** `pitch_gyro = pitch + gyro_y * dt` — 빠른 변화 추적
    3.  **상보 결합:** `pitch = 0.98 * pitch_gyro + 0.02 * pitch_acc`

### Part B — Tiny Neural Network (매 50ms, 20Hz)

자세 변화는 느리므로(~5Hz), 추론은 50ms 주기로 충분합니다.

1.  **입력:** `[pitch, roll, pitch_rate, roll_rate]` (4차원)
2.  **은닉층 1:** 4 → 8 뉴런, ReLU 활성화
3.  **은닉층 2:** 8 → 4 뉴런, ReLU 활성화
4.  **출력층:** 4 → 3 클래스 (활성화 없음, argmax로 분류)
5.  **분류 결과:** `UPRIGHT`(직립), `FORWARD_LEAN`(전방 경사), `BACKWARD_LEAN`(후방 경사)

### LED 표시

*  **LED 1 Heartbeat:** 직립 자세 (안정)
*  **LED 2 느린 Blink:** 전방 경사
*  **LED 3 빠른 Blink:** 후방 경사

---

## 🚀 실행 방법 (How to Use)

1.  `STM32CubeIDE`에서 본 예제 소스파일을 `user_app.c`으로 옮겨와서 빌드하고 펌웨어를 `XM10`에 업로드합니다.
2.  **모드 선택:** 소스 상단에서 `USE_H10_PRECOMPUTED` 또는 `USE_IMU_HUB_FUSION` 중 하나를 `#define`합니다.
3.  `KIT H10`의 전원을 켜고 `XM10`과 연결합니다. (Mode B는 IMU Hub도 연결)
4.  `KIT H10`의 모드를 **`ASSIST_MODE`로 변경**합니다.
5.  슈트를 입고 **똑바로 서면** LED 1이 Heartbeat 합니다.
6.  **앞으로 기울이면** LED 2가 깜빡이고, **뒤로 기울이면** LED 3이 빠르게 깜빡입니다.
7.  USB CDC 터미널에서 500ms마다 `AI[H10] | P:... R:... Class:... Score:...` 디버그 메시지를 확인합니다.

---

## 💡 직접 해보기 (Things to Try)

* **Mode B 전환:** `USE_IMU_HUB_FUSION`을 활성화하여 상보 필터를 직접 체험해보세요. H10 모드와 분류 결과를 비교해보세요.
* **ALPHA 값 변경 (Mode B):** `CF_ALPHA`를 0.90~0.99 범위에서 변경해보세요. 값이 작을수록 가속도계 비중이 높아져 드리프트는 줄지만 노이즈가 커집니다.
* **가중치 수정:** `s_w1`, `s_b1` 등의 NN 가중치 값을 임의로 변경하여 분류 결과가 어떻게 달라지는지 관찰해보세요.
* **Python 학습 → C export:** 실제 IMU 데이터를 USB CDC로 로깅한 뒤, Python에서 모델을 학습하고 가중치를 C 배열로 export하는 파이프라인을 설계해보세요.
* **IMU Hub 센서 인덱스 변경:** `IMU_HUB_SENSOR_IDX`를 0~5로 변경하여 다른 위치의 IMU 데이터로 분류해보세요.
