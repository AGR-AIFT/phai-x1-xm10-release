# Ex.23 — Gait Phase Adaptive Torque (보행 위상별 정현파 토크 프로파일)

> 🎯 **학습 목표**:
> - **보행 주기 (gaitCycle 0~100%)** 를 위상 변수로 → 시간 기반이 아닌 **위상 기반 제어**.
> - 입각기 / 유각기 4구간 분할 + **정현파 토크 프로파일** (Quinlivan 2017 Science Robotics).
> - 보행 속도 변화에 **자동 적응** — 위상 기반이므로 시간 무관.
>
> ⏱️ 권장 시간: 45분 | 🔧 난이도: ⭐⭐⭐
> 🧰 사전 예제: [Ex.17 FSM Gait Intent](../17_FSM_Gait_Intent/) (FSM 단계 vs 연속 정현파) | 📚 관련 docs: [H10 Control](../../docs/api-reference/02-h10-control-n-data.md)
> 📄 논문: Quinlivan, B. T., et al. (2017). *Assistance magnitude versus metabolic cost reductions.* Science Robotics, 2(2), eaah4416.

---

## ⚠️ Body Data 전제조건 — 필수

`gaitCycle` 은 `footContact` 기반 heel strike detection 으로 추정됩니다. **Body Data 없이는 동작하지 않습니다**:

```c
uint32_t bodyData[8] = { 700, 1750, 0, 0, 0, 0, 0, 0 };  // 70.0 kg, 175.0 cm
XM_SendUserBodyData(bodyData);
```

> [examples/README.md — Body Data 안내](../README.md#part-5)

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

보행 1주기 (0~100%) 를 4구간으로 나눠 정현파 토크 적용:

| 위상 범위 | 구간 | 토크 | 의미 |
|-----------|------|------|------|
| 0~30% | 초기 입각기 (Loading) | 0 | 체중 수용 — 보조 없음 |
| 30~60% | 중·말 입각기 | **+A·sin(π·(φ−0.3)/0.3)** | **신전 보조** (전방 추진) |
| 60~80% | 전·초기 유각기 | **−A·sin(π·(φ−0.6)/0.2)** | **굴곡 보조** (다리 들어올림) |
| 80~100% | 말기 유각기 | 0 | 착지 준비 |

USB CDC `GAIT | φ:0.45 A:2.0 τR:1.85 τL:-0.62` + PhAI 0xF0 5축.

> 📸 `![보행 위상 토크 프로파일](../assets/img/23_gait_phase_profile.png)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

### 시간 기반 vs 위상 기반 제어

| | 시간 기반 (Ex.14 PD) | 위상 기반 (이 예제) |
|--|---------------------|--------------------|
| 독립 변수 | t (초) | s = gaitCycle / 100 (0~1) |
| 보행 속도 변화 시 | 적응 불가 (사전 정의된 시간) | **자동 적응** (s 는 측정값) |
| 정지 시 | 시간 계속 흐름 → 오작동 | s = 0 으로 고정 |

### Heel Strike Detection 알고리즘

```
정지 판정: forwardVelocity < 0.1 m/s → gaitCycle = 0
left foot contact rising edge → Heel Strike
stride 주기 = 연속 HS 간 시간 차 (400~3000 ms 유효)
gaitCycle = 100 × (now − last_HS) / stride_period
```

### 정현파 프로파일의 장점 (Quinlivan 2017)

- 구간 경계에서 **C¹ 연속** (sin 의 도함수 = cos 도 부드러움)
- 정현파 적분 가능 → 일 (work) 계산 용이
- 대사 비용 절감 효과 입증 (Soft exosuit)

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
#define VELOCITY_THRESHOLD  0.1f
#define MAX_TORQUE_NM       5.0f

static float s_amplitude = 1.0f;                                    // ① 진폭 A
static float s_phase_offset = 0.0f;                                 //    위상 오프셋
static SymmetryMode_t s_sym_mode = SYM_BALANCED;                    //    좌·우 비대칭

static float _ComputePhaseTorque(float phi, float A)                // ② 4-구간 정현파
{
    if      (phi < 0.30f)         return 0.0f;                       /* Loading: 0 */
    else if (phi < 0.60f)         return  A * sinf(M_PI * (phi - 0.30f) / 0.30f);  /* 신전 */
    else if (phi < 0.80f)         return -A * sinf(M_PI * (phi - 0.60f) / 0.20f);  /* 굴곡 */
    else                          return 0.0f;                       /* Terminal Swing: 0 */
}

static void Active_Loop(void)
{
    /* ③ 정지 판정 — Body Data 필수 */
    if (XM.status.h10.forwardVelocity < VELOCITY_THRESHOLD) {
        XM_SetAssistTorqueRH(0.0f);
        XM_SetAssistTorqueLH(0.0f);
        return;
    }

    /* ④ Heel Strike 기반 gaitCycle 추정 (좌측 기준) */
    uint8_t gc_left  = _EstimateGaitCycle();
    float phi_left   = (float)gc_left / 100.0f;
    float phi_right  = phi_left + 0.5f;                              /* 좌·우 50% 위상차 */
    if (phi_right >= 1.0f) phi_right -= 1.0f;

    /* ⑤ 토크 프로파일 계산 */
    float tau_left  = _ComputePhaseTorque(phi_left,  s_amplitude);
    float tau_right = _ComputePhaseTorque(phi_right, s_amplitude);

    /* ⑥ 좌·우 대칭 모드 적용 */
    if (s_sym_mode == SYM_RIGHT_BIAS) tau_left  *= 0.5f;
    if (s_sym_mode == SYM_LEFT_BIAS)  tau_right *= 0.5f;

    XM_SetAssistTorqueRH(_ClampFloat(tau_right, -MAX_TORQUE_NM, MAX_TORQUE_NM));
    XM_SetAssistTorqueLH(_ClampFloat(tau_left,  -MAX_TORQUE_NM, MAX_TORQUE_NM));
}
```

전체 코드: [`gait_phase_adaptive_torque.c`](gait_phase_adaptive_torque.c) (459 줄)

> 🧒 ②의 **구간별 정현파** 가 핵심. 30~60% 에서 sin 의 0~π 가 0→1→0 의 종 모양 → 매끄러운 신전 보조.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **Body Data 설정 확인** + 빌드/플래시 + ASSIST 진입
2. **정지 상태** → ✅ τ = 0, `forwardVelocity < 0.1` 표시
3. **보행 시작** → ✅ gaitCycle 0~100% 갱신, USB CDC `GAIT | φ:0.45 ...`
4. **신전 구간 (φ ≈ 0.3~0.6)** → ✅ 양의 토크 출력, 추진 보조감
5. **굴곡 구간 (φ ≈ 0.6~0.8)** → ✅ 음의 토크, 다리 들어올림 보조
6. **BTN 1** 5회 클릭 → ✅ A 0.5 → 1.0 → ... → 3.0. 보조 강도 증가 체감
7. **BTN 2 클릭** → ✅ 위상 오프셋 -10% / 0% / +10% 순환. 보조 타이밍 변화
8. **BTN 3 클릭** → ✅ 대칭 모드 (균형 / 우측강화 / 좌측강화) 순환
9. **변형 1 — Body Data 미설정 비교**: 모든 토크 0 → 학생이 직접 체험
10. **변형 2 — 구간 경계 조정**: 30% → 25% (조기 신전), 60% → 65% (지연 굴곡)
11. **변형 3 — 진폭 동적 변화**: `s_amplitude = 1.0f + 0.5f * sinf(now * 0.001f)` 시간에 따라 진폭 변조
12. **변형 4 — 5구간으로 분할**: Pre-Swing 60~70% 추가 (음의 작은 토크)
13. **변형 5 — Sigmoid 프로파일**: sin 대신 시그모이드 (부드러운 ON/OFF)

---

## 5️⃣ 다음 단계

- FSM 7-phase 단계별 차등 토크: [Ex.17 FSM Gait Intent](../17_FSM_Gait_Intent/)
- Bézier + HZD 위상 기반 궤도 추종: [Ex.24 Virtual Constraint](../24_Virtual_Constraint/)
- GRF 기반 위상 추정: [Ex.32 GRF Gait Intent](../32_GRF_Gait_Intent/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| gaitCycle 항상 0 | Body Data 미설정 → footContact 인식 X | `XM_SendUserBodyData` 호출 필수 |
| 보행해도 토크 0 | `forwardVelocity < 0.1` 판정 → 정지로 인식 | 조금 더 빠르게 걷거나 임계치 ↓ |
| 좌·우 토크 동기 (역위상 X) | `phi_right = phi_left + 0.5` 누락 또는 모듈로 X | 0.5 위상차 + wrap 가드 확인 |
| 토크 점프 (구간 경계) | sin 도함수 불연속 (구간 끝에서 ≠ 0) | 구간 경계에서 sin 값이 0 인지 확인 (수식상 보장됨) |
| 진폭 너무 작아 효과 없음 | A = 0.5 → 너무 약함 | A 1.5~2.5 권장 |
| 진폭 너무 커서 강제 보행 | A > 4.0 → 자연 보행 방해 | A 3.0 이하 |
| Heel Strike 놓침 → gaitCycle 점프 | 노이즈로 인한 false negative | `GAIT_EST_MIN_STRIDE_MS` 적절히 조정 |
| 좌·우 비대칭 모드인데 효과 미미 | `s_sym_mode` 분기 누락 | 토크 계산 후 곱셈 적용 확인 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
