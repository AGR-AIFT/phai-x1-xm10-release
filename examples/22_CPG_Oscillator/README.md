# Ex.22 — CPG Oscillator (적응 주파수 진동자 — 보행 리듬 자동 동기화)

> 🎯 **학습 목표**:
> - **AFO (Adaptive Frequency Oscillator)** 의 위상 φ + 각주파수 ω **동시 적응** 메커니즘.
> - 사용자 보행 주기를 **사전 설정 없이 실시간 학습** → 동기화된 정현파 토크 출력.
> - **컴파일 타임 피드백 소스 전환** — `gaitCycle` (Body Data 필요) vs `motorAngle` (불필요).
>
> ⏱️ 권장 시간: 50분 | 🔧 난이도: ⭐⭐⭐
> 🧰 사전 예제: [Ex.17 FSM Gait Intent](../17_FSM_Gait_Intent/) (단계별 vs 연속 위상 비교) | 📚 관련 docs: [H10 Control](../../docs/api-reference/02-h10-control-n-data.md)
> 📄 논문: Ronsse, R., et al. (2011). *Oscillator-based assistance of cyclical movements.* Med. Biol. Eng. Comput., 49(10).

---

## ⚠️ Body Data 전제조건 (default 모드 사용 시)

기본 `USE_GAIT_CYCLE_FEEDBACK` 모드는 `footContact` + `forwardVelocity` 를 사용 → **Body Data 필수**.
`User_Setup()` 에 `XM_SendUserBodyData(bodyData)` 호출. 또는 매크로 전환:

```c
/* #define USE_GAIT_CYCLE_FEEDBACK */   // 기본 — Body Data 필요
#define USE_MOTOR_ANGLE_FEEDBACK         // Body Data 불필요 (각도 직접 사용)
```

> [examples/README.md — Body Data 안내](../README.md#part-5)

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

착용자가 걷기 시작하면 진동자가 **자동으로 보행 주기에 락온 (lock-on)**, 동기화된 토크 출력:

```
τ = A · sin(φ)        — 위상 φ 는 사용자 보행에 자동 추종
```

| 단계 | 동작 |
|------|------|
| 정지 (속도 < 0.1 m/s) | τ = 0, 진동자 위상 유지 |
| 보행 시작 | ω 가 사용자 보행 주파수로 수렴 (5~10초) |
| 정상 보행 | τ = A·sin(φ) 출력, 자연스러운 리듬 보조 |

USB CDC `CPG | φ:1.57 ω:6.28 F:-0.32 τ:0.95` + PhAI 0xF0 4ch.

> 📸 `![AFO 위상 락온 곡선](../assets/img/22_afo_locking.png)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

### CPG (Central Pattern Generator) 개념

- 인간 척수에는 보행 리듬을 생성하는 신경 회로 (CPG) 가 있음
- 의식적 제어 없이 다리가 교대로 움직이는 원리
- 로봇 제어에 응용: 외부 동기 신호 없이 리드믹 패턴 생성

### AFO 핵심 수식 (이산 시간 업데이트, dt = 1 ms)

```
F[k]   = x_feedback[k] − A₀·sin(φ[k])       (피드백 오차)
φ[k+1] = φ[k] + dt·(ω + ε·F·cos φ)            (위상 업데이트)
ω[k+1] = ω[k] − dt·ε·F·sin φ                  (주파수 자동 적응)
φ      = fmod(φ, 2π)                          (정규화)
τ      = A · sin(φ)                           (토크 출력)
```

- **φ**: 진동자 위상 (rad)
- **ω**: 진동자 각주파수 (rad/s) — **사용자 보행 주파수로 자동 수렴**
- **ε**: 커플링 강도 (0.1~0.5 권장) — 클수록 빠르지만 불안정
- **A**: 토크 진폭 (0.5~4.0 Nm)
- **F**: 피드백 오차 (측정값 − 진동자 예측값)

### 두 가지 피드백 소스

| 매크로 | 입력 신호 | HW 요구 | 동기 정확도 |
|--------|---------|---------|----------|
| `USE_GAIT_CYCLE_FEEDBACK` (default) | `gaitCycle` 0~100% | Body Data | 높음 (heel strike 기반) |
| `USE_MOTOR_ANGLE_FEEDBACK` | `rightHipMotorAngle` | 불필요 | 보통 (관절각 진동) |

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
/* 피드백 모드 선택 (컴파일 타임) */
#define USE_GAIT_CYCLE_FEEDBACK     // 또는 USE_MOTOR_ANGLE_FEEDBACK

#define CPG_DT       0.001f                                         // 1 kHz
#define CPG_OMEGA_0  6.28f                                          // 초기 ω ≈ 1 Hz
#define CPG_A0       1.0f                                           // 피드백 진폭 (정규화)

static float s_phi   = 0.0f;                                        // ① 위상 (rad)
static float s_omega = CPG_OMEGA_0;                                 //    각주파수
static float s_epsilon = 0.3f;                                      //    커플링 강도
static float s_torque_amp = 1.0f;                                   //    출력 진폭 A

static void _UpdateAFO(float x_feedback)
{
    /* ② 피드백 오차 F = x − A₀·sin(φ) */
    float F = x_feedback - CPG_A0 * sinf(s_phi);

    /* ③ 위상 + 주파수 동시 적응 */
    float phi_dot   = s_omega + s_epsilon * F * cosf(s_phi);
    float omega_dot = -s_epsilon * F * sinf(s_phi);

    s_phi   += CPG_DT * phi_dot;
    s_omega += CPG_DT * omega_dot;

    /* ④ 위상 정규화 (0 ~ 2π) */
    s_phi = fmodf(s_phi, 2.0f * M_PI);
    if (s_phi < 0) s_phi += 2.0f * M_PI;
}

static void Active_Loop(void)
{
    /* ⑤ 피드백 소스 선택 */
#ifdef USE_GAIT_CYCLE_FEEDBACK
    uint8_t gc = _EstimateGaitCycle();   /* footContact 기반, Body Data 필수 */
    float x_fb = sinf(2.0f * M_PI * (float)gc / 100.0f);    /* 위상 변환 */
#else
    float x_fb = sinf(DEG_TO_RAD(XM.status.h10.rightHipMotorAngle));
#endif

    _UpdateAFO(x_fb);

    /* ⑥ 토크 출력: τ = A · sin(φ) */
    float tau = s_torque_amp * sinf(s_phi);
    XM_SetAssistTorqueRH(_ClampFloat(tau, -MAX_TORQUE, MAX_TORQUE));
    XM_SetAssistTorqueLH(_ClampFloat(-tau, -MAX_TORQUE, MAX_TORQUE)); /* 좌·우 역위상 */
}
```

전체 코드: [`cpg_oscillator.c`](cpg_oscillator.c) (585 줄)

> 🧒 ③ 의 **위상 + 주파수 동시 업데이트** 가 AFO 의 핵심. PLL 과 유사하지만 진폭 추정도 가능.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **Body Data 설정** + 빌드/플래시 (default 모드)
2. **CM ASSIST 진입 + 정지 상태** → ✅ τ = 0, USB `φ:0.00 ω:6.28`
3. **보행 시작 (5~10 초)** → ✅ ω 가 사용자 보행 주파수로 수렴 (약 5~7 rad/s = 0.8~1.1 Hz)
4. **USB CDC 출력** → `CPG | φ:1.57 ω:5.95 F:-0.21 τ:1.00` 매 200 ms
5. **PhAI 0xF0** → φ 톱니파, ω 곡선 (락온 후 평탄), τ 정현파
6. **BTN 1 클릭** → ✅ 진폭 A 0.5 → 1.0 → ... → 4.0 → 0.0 순환. 보조감 증감
7. **BTN 2 클릭** → ✅ ε 0.1 → 0.2 → 0.3 → 0.5 순환. 큰 ε = 빠른 락온 (불안정 가능)
8. **BTN 3 클릭** → ✅ φ=0, ω=초기값 리셋. 동기 실패 시 재시작
9. **변형 1 — Motor Angle 모드**: `USE_GAIT_CYCLE_FEEDBACK` 주석 + `USE_MOTOR_ANGLE_FEEDBACK` 활성. Body Data 없이도 동작
10. **변형 2 — 빠른 보행 → 느린 보행 전환**: ω 가 추적하는 데 걸리는 시간 측정
11. **변형 3 — 좌·우 동위상 (역위상 제거)**: `XM_SetAssistTorqueLH(tau)` 동일 → 강제 양다리 모드
12. **변형 4 — 초기 ω 0.5 Hz 로 설정**: `CPG_OMEGA_0` 3.14. 락온 시간 비교
13. **변형 5 — ε 너무 큼 (1.0+)**: 진동자가 발산. 안정 한계 체험

---

## 5️⃣ 다음 단계

- FSM 단계별 vs 연속 위상 비교: [Ex.17 FSM Gait Intent](../17_FSM_Gait_Intent/)
- Gait Phase Adaptive Torque (연속 위상 4구간): [Ex.23](../23_Gait_Phase_Adaptive_Torque/)
- GRF 기반 위상 추정 (대안): [Ex.32 GRF Gait Intent](../32_GRF_Gait_Intent/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| ω 가 수렴 안 함 | Body Data 미설정 → gaitCycle 항상 0 | `XM_SendUserBodyData` 호출 or Motor Angle 모드 |
| 락온은 됐는데 발 위상과 어긋남 | 좌·우 위상차 부호 오류 | LH 부호 반전 (`-tau`) 확인 |
| ω 가 진동 (안정 안 됨) | ε 너무 큼 (>0.5) | ε 0.2~0.3 으로 ↓ |
| 정지 시 토크 계속 출력 | 정지 판정 없음 | `forwardVelocity < 0.1` 시 τ=0 |
| 진폭 A 가 너무 크면 강제 보행 | 자연스러운 보조 한계 초과 | A 1.0~2.0 권장, 4.0 이상 위험 |
| 초기 락온 너무 오래 걸림 | 초기 ω 가 사용자 주파수와 크게 다름 | `CPG_OMEGA_0` 를 ~6.28 (1 Hz) 부근으로 |
| Motor Angle 모드인데 안 락온 | 모터 각도가 충분히 진동 안 함 (예: ASSIST 미진입) | 보행 시작 + ASSIST 모드 확인 |
| `fmodf` 결과가 음수 | C 표준 — float 음수 입력 시 음수 반환 | 후처리에서 `+= 2π` 가드 추가 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
