# Ex.26 — Iterative Learning Control (ILC — 매 보행 주기마다 학습하는 토크 프로파일)

> 🎯 **학습 목표**:
> - **P-type ILC**: `τ_{k+1}(i) = τ_k(i) + L·e_k(i)` — 매 주기 오차로 다음 주기 개선.
> - **이산 위상 인덱싱**: gaitCycle 0~100% → 배열 인덱스 0~99 매핑.
> - **주기 완료 감지** (`prev > 80% && current < 20%`) + BTN3 학습 동결로 평가.
>
> ⏱️ 권장 시간: 50분 | 🔧 난이도: ⭐⭐⭐
> 🧰 사전 예제: [Ex.23 Gait Phase Adaptive](../23_Gait_Phase_Adaptive_Torque/) + [Ex.14 PD](../14_PD_Realtime_Control/) | 📚 관련 docs: [H10 Control](../../docs/api-reference/02-h10-control-n-data.md)
> 📄 논문: Emken, J. L., et al. (2007). *Robotic movement training as an optimization problem.* IEEE ICORR. Bristow, D. A. (2006). *A survey of iterative learning control.* IEEE Control Systems Mag.

---

## ⚠️ Body Data 전제조건 — 필수

ILC 학습 인덱스 = gaitCycle/100. **gaitCycle 부정확 시 학습 붕괴**. `User_Setup` 에서 `XM_SendUserBodyData(bodyData)` 필수.

> [examples/README.md — Body Data 안내](../README.md#part-5)

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

매 보행 주기마다 100-슬롯 토크 프로파일 배열이 **점진적으로 채워짐** → 보행 주기 반복 시 보조감 증가.

| 주기 # | 학습 상태 | 보조감 |
|--------|---------|------|
| 1~3 | `τ_ff` 거의 0 | 약함 (학습 시작) |
| 5~10 | 프로파일 형성 중 | 점점 강해짐 |
| 20+ | 수렴 | 안정적 보조 |
| BTN 3 동결 후 | 동일 프로파일 반복 | 학습된 결과 평가 |

USB CDC `ILC | cyc:8 i:45 e:1.2 τ_ff:0.85 τ_cmd:1.40` 매 200 ms.

> 📸 `![ILC 수렴 곡선](../assets/img/26_ilc_convergence.png)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

### P-type ILC 업데이트 법칙

```
보행 주기 k 의 i 번째 슬롯:
  e_k(i)     = θ_d(i) − θ_k(i)              (오차)
  τ_{k+1}(i) = τ_k(i) + L · e_k(i)          (학습 업데이트)

L: 학습 게인 (0 < L < 2 / G, G=플랜트 DC 게인)
  L 작음 → 학습 느림, 안정
  L 큼   → 학습 빠름, 발산 위험
```

### 수렴 조건 (Bristow 2006)

```
|1 − L · G| < 1
→ 0 < L < 2/G (DC 게인 G 가 작으면 L 도 작아야 함)
→ 실용적으로 L = 0.05 ~ 0.3 권장
```

### 주기 완료 감지

```
new_cycle = (s_prev_gait > 80) && (gaitCycle < 20)
→ heel strike 직후 → gaitCycle 점프 → 새 주기 시작 판정
→ 이 시점에서 100 슬롯 학습 업데이트 한꺼번에 수행
```

### 위상 인덱싱

```
i = gaitCycle / 100 × (ILC_SAMPLES − 1)   /* 99 */
→ 정수 변환 시 lost precision 무시 (100 → 99 슬롯)
→ 선형 보간 적용 가능 (선택사항)
```

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
#define ILC_SAMPLES   100U                                          // 0~99 슬롯
#define L_LEARN_RATE  0.1f                                          // ① 학습 게인
#define A_REF         15.0f                                         //    참조 진폭 (deg)

static float s_ilc_torque[ILC_SAMPLES] = {0};                       // ② 학습 토크 프로파일
static float s_ilc_error[ILC_SAMPLES]  = {0};                       //    오차 이력
static uint8_t s_prev_gait = 0;
static bool s_learning_frozen = false;
static uint32_t s_cycle_count = 0;

static float _RefAngle(uint16_t i)                                  // ③ 참조 궤도 (정현파)
{
    return A_REF * sinf(M_PI * (float)i / (float)(ILC_SAMPLES - 1));
}

static void Active_Loop(void)
{
    uint8_t gc = _EstimateGaitCycle();                              /* Body Data 필수 */
    if (gc == 0) {                                                  /* 정지 → 0 출력 */
        XM_SetAssistTorqueRH(0); XM_SetAssistTorqueLH(0);
        return;
    }

    /* ④ 위상 인덱스 매핑 */
    uint16_t i = (uint16_t)((float)gc / 100.0f * (ILC_SAMPLES - 1));

    /* ⑤ 오차 기록 (현재 토크 대비 참조) */
    float theta = XM.status.h10.rightHipAngle;
    s_ilc_error[i] = _RefAngle(i) - theta;

    /* ⑥ 학습된 피드포워드 토크 송신 */
    float tau = s_ilc_torque[i];
    XM_SetAssistTorqueRH(_ClampFloat(tau, -MAX_TORQUE_NM, MAX_TORQUE_NM));
    XM_SetAssistTorqueLH(_ClampFloat(tau, -MAX_TORQUE_NM, MAX_TORQUE_NM));

    /* ⑦ 주기 완료 감지 → 학습 업데이트 */
    if (!s_learning_frozen && s_prev_gait > 80 && gc < 20) {
        for (uint16_t k = 0; k < ILC_SAMPLES; k++) {
            s_ilc_torque[k] += L_LEARN_RATE * s_ilc_error[k];        /* P-type ILC */
            s_ilc_torque[k] = _ClampFloat(s_ilc_torque[k], -MAX_TORQUE_NM, MAX_TORQUE_NM);
        }
        s_cycle_count++;
    }
    s_prev_gait = gc;
}

static void _HandleButtons(void)
{
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {              /* BTN1: L 순환 */
        L_idx = (L_idx + 1) % 4;
        L_active = L_presets[L_idx]; /* 0.05, 0.1, 0.2, 0.3 */
    }
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK)                /* BTN2: 리셋 */
        memset(s_ilc_torque, 0, sizeof(s_ilc_torque));
    if (XM_GetButtonEvent(XM_BTN_3) == XM_BTN_CLICK)                /* BTN3: 동결 토글 */
        s_learning_frozen = !s_learning_frozen;
}
```

전체 코드: [`iterative_learning_control.c`](iterative_learning_control.c) (449 줄)

> 🧒 ⑦ 의 **주기 완료 감지** 가 핵심. 매 cycle 학습이 아니라 보행 1 주기 완료 후 100 슬롯 한꺼번에 업데이트.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **Body Data 설정** + 빌드/플래시 + ASSIST + 보행 시작
2. **첫 3 주기** → ✅ 토크 거의 0 (학습 초기)
3. **5 주기 후** → ✅ 프로파일 형성 시작, USB `τ_ff:0.30`
4. **10~15 주기** → ✅ 수렴, `τ_ff:0.85~1.20`
5. **USB CDC** → `ILC | cyc:12 i:50 e:0.85 τ_ff:1.10 τ_cmd:1.10` 매 200 ms
6. **PhAI 0xF0** → 100 슬롯 토크 배열의 evolution 관찰
7. **BTN 3 클릭 (동결)** → ✅ 학습 멈춤. 동일 프로파일 반복 적용
8. **BTN 3 다시 (해제)** → ✅ 학습 재개. 다시 수렴 가속
9. **BTN 2 클릭 (리셋)** → ✅ 모든 슬롯 0. 처음부터 재학습 시작
10. **BTN 1 클릭** → ✅ L 0.05 / 0.1 / 0.2 / 0.3 순환. 수렴 속도 비교
11. **변형 1 — L = 0.5+ (발산)**: 큰 게인으로 학습 불안정 직접 체험
12. **변형 2 — 슬롯 100 → 50**: 위상 해상도 ↓ 비교
13. **변형 3 — D-type ILC**: `e_k(i+1) − e_k(i)` 미분 항 추가
14. **변형 4 — 참조 궤도 변경**: `_RefAngle` 정현파 → 톱니파

---

## 5️⃣ 다음 단계

- MRAC (모델 참조 적응): [Ex.27 MRAC](../27_MRAC/)
- 정현파 프로파일 직접 정의: [Ex.23 Gait Phase Adaptive](../23_Gait_Phase_Adaptive_Torque/)
- Kinesthetic Teaching (전문가 시연 학습): [Ex.33 Kinesthetic Teaching](../33_Kinesthetic_Teaching/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| 토크 계속 0 (학습 안 됨) | Body Data 미설정 → gaitCycle 0 | `XM_SendUserBodyData` 호출 |
| 학습 발산 (토크 폭주) | L 너무 큼 (0.5+) | L 0.05~0.2 권장 |
| 수렴 매우 느림 | L 너무 작음 (0.01) | L 0.1 → 0.2 |
| 주기 완료가 자주 감지됨 | `s_prev_gait > 80 && < 20` 조건 노이즈 | 히스테리시스 추가 (90/10) |
| 학습 동결 후에도 토크 변함 | 동결 플래그 미체크 | `if (!s_learning_frozen)` 가드 확인 |
| 슬롯 인덱스 100 접근 (out of range) | `gaitCycle = 100` → `i = 99` 확인 | 클램프 `i = min(i, 99)` |
| 보행 멈췄는데 마지막 토크 유지 | gaitCycle 0 시 토크 0 출력 안 됨 | `if (gc == 0) return` 가드 추가 |
| 매 cycle 학습 시도 (안 됨) | 주기 감지 패턴 오류 | `prev > 80 && now < 20` 조건 확인 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
