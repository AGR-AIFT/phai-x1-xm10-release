# Ex.16 — TinyAI Sensor Fusion (MCU 위 3-layer NN 자세 분류)

> 🎯 **학습 목표**:
> - MCU 위에서 직접 **3-layer 퍼셉트론 추론** (forward pass 만, 양자화 X, 정수형 변환 X).
> - **두 가지 자세 데이터 소스 선택** — H10 전처리 (default) vs IMU Hub raw + **상보 필터**.
> - **Tiny ML 워크플로우** — Python 학습 → C 배열 export → MCU 추론.
>
> ⏱️ 권장 시간: 45분 | 🔧 난이도: ⭐⭐⭐
> 🧰 사전 예제: [Ex.14 PD](../14_PD_Realtime_Control/) | 📚 관련 docs: [H10 Control](../../docs/api-reference/02-h10-control-n-data.md)

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

H10 가 ASSIST 모드일 때 사용자의 자세를 **3개 클래스** 로 실시간 분류 (50 Hz):

| 클래스 | 의미 | LED 표시 |
|--------|------|---------|
| 0 UPRIGHT | 직립 자세 | LED 1 Heartbeat (1.5초) |
| 1 FORWARD_LEAN | 전방 경사 | LED 2 Blink (느림, 500 ms) |
| 2 BACKWARD_LEAN | 후방 경사 | LED 3 Blink (빠름, 200 ms) |

USB CDC 500 ms 주기: `AI | P:5.2 R:-1.3 Class:UPRIGHT Conf:78.4%` + PhAI 0xF0 5축 (pitch / roll / pitch_rate / class_id / confidence).

> 📸 `![NN 자세 분류 실시간](../assets/img/16_tinyai_posture.png)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

### 두 가지 자세 데이터 소스 (`#define` 으로 선택)

| 매크로 | 사용 데이터 | HW 요구 | 시연 포인트 |
|--------|-----------|---------|------------|
| `USE_H10_PRECOMPUTED` (default) | `XM.status.h10.leftHipImuSagittalPitch/FrontalRoll` (H10 내부 센서 퓨전 결과) | H10 만 | "이미 처리된 자세 + MCU NN 분류" |
| `USE_IMU_HUB_FUSION` | IMU Hub body-frame raw → 상보 필터 | + IMU Hub 모듈 | "raw IMU 부터 센서 퓨전 + NN" |

### 상보 필터 (Mode 2 만)

```
pitch = ALPHA · (pitch + gyro_y · dt) + (1 − ALPHA) · pitch_acc
ALPHA = 0.98, dt = 0.001 s → 차단 주파수 ≈ 3.2 Hz
```
- 고주파 (빠른 자세 변화) ← 자이로 적분
- 저주파 (드리프트 보정) ← 가속도계 atan2
- 시작 시 가속도계 값으로 초기화 (드리프트 없는 출발점)

### 3-layer NN

```
Input(4) → Dense(8) ReLU → Dense(4) ReLU → Dense(3) argmax
입력: [pitch, roll, pitch_rate, roll_rate]
```
- 파라미터 수: 32+8 + 32+4 + 12+3 = **91 floats** (~364 B)
- 추론 비용: 50 Hz × ~88 곱셈 = 4400 ops/s (Cortex-M7 무시 가능)
- Softmax 대신 **argmax** (exp() 회피 — MCU 친화 + 분류 결과 동일)

### 본 예제의 한계

- 가중치가 **하드코딩 임의 값** — 실제 학습 결과 X. Python 으로 학습 후 가중치 export 필요.
- 양자화 (int8) 미적용 — 추후 메모리 75% 절감 가능.

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
#define USE_H10_PRECOMPUTED                                    // ① 모드 선택
// #define USE_IMU_HUB_FUSION

/* ② 사전 학습 가중치 (Python → C 배열) */
static const float s_w1[NN_INPUT_SIZE * NN_HIDDEN1_SIZE] = { ... };  // 32개
static const float s_b1[NN_HIDDEN1_SIZE] = { ... };                   // 8개
/* ... w2, b2, w3, b3 ... */

#ifdef USE_H10_PRECOMPUTED
static void _UpdatePostureFromH10(void)                       // ③ 모드 1: H10 직접
{
    s_pitch_deg = XM.status.h10.leftHipImuSagittalPitch;
    s_roll_deg  = XM.status.h10.leftHipImuFrontalRoll;
    s_pitch_rate_dps = XM.status.h10.leftHipImuGlobalGyrY;
    s_roll_rate_dps  = XM.status.h10.leftHipImuGlobalGyrX;
}
#endif

#ifdef USE_IMU_HUB_FUSION
static void _UpdatePostureFromImuHub(void)                    // ④ 모드 2: 상보 필터
{
    /* IMU Hub body-frame raw → 가속도계 기반 pitch_acc/roll_acc */
    float pitch_acc = atan2f(acc_x, sqrtf(acc_y*acc_y + acc_z*acc_z)) * RAD_TO_DEG;
    float roll_acc  = atan2f(acc_y, sqrtf(acc_x*acc_x + acc_z*acc_z)) * RAD_TO_DEG;

    if (!s_cf_initialized) { /* 시작 시 가속도계로 초기화 */ }
    else {
        s_pitch_deg = CF_ALPHA * (s_pitch_deg + gyro_y * CF_DT)
                    + (1 - CF_ALPHA) * pitch_acc;             // 상보 필터
        s_roll_deg  = ... 동일 패턴 ...;
    }
}
#endif

static void _RunInference(void)                               // ⑤ NN forward pass
{
    float input[4] = { s_pitch_deg, s_roll_deg, s_pitch_rate_dps, s_roll_rate_dps };

    /* Layer 1: 4 → 8 + ReLU */
    for (i=0..7) { h1[i] = ReLU(b1[i] + sum(input[j] * w1[j*8+i])); }

    /* Layer 2: 8 → 4 + ReLU */
    for (i=0..3) { h2[i] = ReLU(b2[i] + sum(h1[j] * w2[j*4+i])); }

    /* Layer 3: 4 → 3 (no activation) + argmax */
    for (i=0..2) { output[i] = b3[i] + sum(h2[j] * w3[j*3+i]); }
    s_result.class_id = argmax(output);
}

static void Active_Loop(void)
{
    /* [매 1 ms] 자세 데이터 갱신 */
#ifdef USE_H10_PRECOMPUTED
    _UpdatePostureFromH10();
#else
    _UpdatePostureFromImuHub();
#endif

    /* [매 50 ms] NN 추론 — 자세 변화 대역폭 ~5 Hz, 20 Hz 충분 */
    if (now - s_inference_timer >= 50) {
        _RunInference();
        _UpdateLedByPosture(s_result.class_id);
    }
}
```

전체 코드: [`tinyai_sensor_fusion.c`](tinyai_sensor_fusion.c)

> 🧒 ⑤ 의 `for` 루프 3개 = full forward pass. **역전파 (backward) 없음** — MCU 는 추론 전용.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **빌드 + 플래시** (Mode 1 default) → CM + ASSIST 진입
2. **착용자 직립** → ✅ LED 1 Heartbeat (UPRIGHT)
3. **앞으로 기울임** → ✅ 0.5 초 내 LED 2 깜빡 (FORWARD)
4. **뒤로 기울임** → ✅ LED 3 빠른 깜빡 (BACKWARD)
5. **USB CDC** → `AI | P:5.2 R:-1.3 Class:UPRIGHT Conf:78.4%` 매 500 ms
6. **변형 1 — Mode 2 활성화**: `#define USE_H10_PRECOMPUTED` 주석 + `USE_IMU_HUB_FUSION` 활성. IMU Hub 연결.
7. **변형 2 — Python 학습 + Export**:
   ```python
   # Python 학습 (의사 코드)
   model = Sequential([Dense(8, 'relu'), Dense(4, 'relu'), Dense(3)])
   model.fit(X_train, y_train)
   # 가중치 export → s_w1/b1/w2/b2/w3/b3 C 배열로
   ```
8. **변형 3 — 클래스 추가**: NN_OUTPUT_SIZE 3 → 5 (LEFT_LEAN, RIGHT_LEAN 추가). w3/b3 크기 확장 + 학습 데이터 라벨링.
9. **변형 4 — 양자화 (Quantization)**: float32 → int8 변환. 메모리 75% 절감. STM32Cube.AI 또는 TFLite Micro 참조.
10. **변형 5 — 추론 주기**: 50 → 100 ms (10 Hz, 더 저전력) 또는 20 ms (50 Hz, 빠른 반응)

---

## 5️⃣ 다음 단계

- Phase-Dependent 보조 (보행 단계별): [Ex.17 FSM Gait Intent](../17_FSM_Gait_Intent/)
- CPG 진동자 + 자세 보조: [Ex.22 CPG](../22_CPG_Oscillator/) (Phase 2D)
- GRF 기반 보행 위상 추정: [Ex.32 GRF Gait Intent](../32_GRF_Gait_Intent/) (Phase 2D)
- Kinesthetic Teaching (전문가 스킬 데이터화): [Ex.33 Kinesthetic](../33_Kinesthetic_Teaching/) (Phase 2D)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| 두 모드 모두 활성화 시 `#error` | 의도된 방어 | 한 모드만 `#define`, 다른 건 주석 |
| Mode 1: pitch 가 항상 0 | `XM.status.h10.leftHipImuSagittalPitch` 미지원 H10 FW | KIT H10 FW v2.3.0+ 사용 |
| Mode 2: pitch 가 항상 0 | IMU Hub 미연결 또는 sensor[0] 미응답 | `XM.status.imu_hub.is_connected` 확인 |
| 분류 결과가 무작위로 변함 | 가중치가 하드코딩 임의 값 (학습 X) | Python 실제 학습 후 가중치 교체 |
| 추론 결과가 항상 UPRIGHT | NN bias 가 그쪽으로 편향 | 다양한 자세 데이터로 재학습 |
| `Confidence` 가 100 이상 또는 음수 | argmax raw score 라 정규화 X (Softmax 미적용) | 의도된 동작 — 절대값보다 상대 비교에 사용 |
| 50 Hz 추론이 너무 빠름 | 자세 변화는 5 Hz 정도 | INFERENCE_PERIOD_MS 늘려도 OK |
| Mode 2 atan2 가 0 nan | 가속도 거의 0 (자유낙하 또는 정지) | 본 예제는 `acc_yz < 0.01` 가드 |
| 가중치 메모리 너무 큼 | 큰 네트워크로 확장 시 | int8 양자화 + STM32Cube.AI 사용 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
