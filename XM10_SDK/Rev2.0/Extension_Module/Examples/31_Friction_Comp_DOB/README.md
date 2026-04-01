# 예제 31: 외란 관측기 기반 투명 모드 완성 (DOB — Stage 1)

본 예제는 **외란 관측기(Disturbance Observer, DOB)**를 이용하여
예제 21(중력+마찰 보상)로는 제거하지 못한 **잔류 외란**을 실시간으로 추정·보상합니다.
이로써 진정한 **액추에이터 투명성(Physical Transparency, Stage 1)**이 완성됩니다.

> 📖 API 레퍼런스: [H10 Control & Data](../../docs/api-reference/02-h10-control-n-data.md) · [Task State Machine](../../docs/api-reference/01-task-state-machine.md)
>
> 📄 전제 예제: [Ex.21 중력+마찰 보상](../21_Gravity_Compensation/) (공칭 모델 기반)

---

## Physical AI 5단계 학습 여정에서의 위치

```
Stage 1 ★  Physical Transparency  ← 지금 여기 (DOB로 완성)
Stage 2    Intent Sensing          ← τ_ext_est가 핵심 입력 신호
Stage 3    Adaptive Assistance
Stage 4    Machine Learning
Stage 5    Shared Autonomy
```

> **Stage 1 → Stage 2 연결 고리**: DOB가 추정하는 `τ_ext_est`는
> 인간 의도 힘(human intent force)의 근사값입니다.
> Stage 2(Ex.32)에서 이 신호를 임계값으로 분류하면 "의도 감지"가 됩니다.

---

## 제어 모드 이해 (초급 가이드)

> 이 섹션은 제어공학을 처음 접하는 분을 위한 배경 설명입니다.

### "왜 위치제어가 아닌 토크제어인가?"

외골격에서 제어 방식의 선택은 **착용자 경험을 근본적으로 바꿉니다**.

| 제어 방식 | 로봇의 "의지" | 인간의 힘은? | 투명 모드 가능? |
|----------|-------------|------------|--------------|
| **위치제어** | "여기로 가!" | 방해물(외란)로 처리 | ❌ 불가 |
| **전류제어** | "이 전류를 내!" | 직접 관여 불가 | △ MD 내부 |
| **토크제어** | "이만큼만 도와줘" | 자유롭게 추가 가능 | ✅ 가능 |
| **임피던스제어** | "스프링처럼 느껴지게" | 가상 스프링에 저항 | △ K=0일 때 |

- **위치제어**(Ex.11 P/I-Vector): 목표 자세로 강하게 이끄는 방식. 착용자가 저항하면 더 큰 토크로 맞서 싸움. 재활 초기처럼 착용자의 움직임이 제한되어야 할 때 적합.
- **토크제어**(이 예제들): "특정 힘만 더한다"는 개념. 착용자가 어떤 방향으로 움직이든 지정된 보조만 추가. 투명 모드와 보조 모드 모두에 적합.

### PhAI X1(H10+XM10)의 제어 계층

```
[XM10 User_Loop 1kHz]
  XM_SetAssistTorqueRH(τ)        ← 사용자가 명령하는 레벨
       ↓ DOP V3 (CAN-FD, ~0.5ms 지연)
[H10 CM ~1kHz]
  Assist Torque를 수신 → 자체 제어에 합산
       ↓
[H10 MD ~10kHz]
  i_cmd = τ_total / Kt           ← FOC 전류 제어로 최종 실행
       ↓
[모터]
```

**핵심**: XM10에서 "토크제어"를 켜도 실제 하드웨어는 MD의 FOC(전류제어)가 실행합니다. XM10은 "얼마나 도울지"를 결정하고, "어떻게 전류로 변환할지"는 MD가 담당합니다.

### DOB가 필요한 이유

```
이상적 투명 모드: τ_out = τ_grav + τ_fric          (Ex.21, 공칭 모델)
    ↑
    여기서 남는 오차:
      - 실제 마찰 ≠ 모델 마찰 (비선형성, 온도 의존성)
      - 감속기 백래시, 케이블 탄성
      - 모터 토크 상수 Kt의 오차

DOB 기반 투명 모드: τ_out = τ_grav + τ_fric + d_hat  (이 예제)
    ↑ d_hat = Q-filter(τ_meas - τ_model)
    → 모델이 설명 못한 "나머지"를 실시간 추정하여 상쇄
```

---

## 학습 목표

- **공칭 모델(Nominal Model)의 한계**를 이해합니다.
  Ex.21의 보상 이후에도 잔류하는 외란(HW 비선형성, 모델 오차)이 존재합니다.
- **DOB Q-filter 원리**를 학습합니다:
  `d_hat[k] = α_q · d_hat[k-1] + (1 - α_q) · (τ_meas - τ_model)`
- Q-filter **차단 주파수(f_c)의 트레이드오프**를 체험합니다.
- `τ_ext_est` 신호가 **인간 의도 힘의 추정치**임을 확인합니다.

---

## 동작 원리

### DOB 수식

```
공칭 모델: τ_model = τ_grav + τ_fric
               = M·g·L_eff·sin(θ)  +  B_f·sign(θ̇) + B_v·θ̇

실측 토크: τ_meas = Kt · i_meas  (Kt = 0.8 Nm/A)
  ※ XM.status.h10.rightHipTorque 필드는 모터 전류(A)를 제공합니다.
     필드명에 "Torque"가 붙어있으나 실제 단위는 Ampere(A)입니다.
     (내부 변환: raw_int16 / 60 → 범위 −30~+30 A)

잔류 외란: residual = τ_meas - τ_model  (모델 미설명 성분)

Q-filter:  d_hat[k] = α_q·d_hat[k-1] + (1-α_q)·residual
           α_q = 1 - 2π·f_c·dt   (f_c 기본값: 5 Hz)
           ※ α_q ∈ [0, 1) 조건 필수 — 이탈 시 IIR 필터 불안정

출력 토크: τ_out = τ_model + d_hat  (완전한 투명 모드)
```

### 상태 전이 (TSM)

`OFF → STANDBY → ACTIVE(DOB 투명 모드)`

### 버튼 조작

| 버튼 | 동작 |
|------|------|
| BTN1 클릭 | DOB ON/OFF 토글 (OFF 시: Ex.21과 동일한 공칭 모델만) |
| BTN2 클릭 | Q-filter 차단 주파수 순환 (1 → 5 → 10 → 20 Hz) |
| BTN3 클릭 | 외란 추정치(d_hat) 초기화 (드리프트 보정) |

### USB 스트리밍 (Module ID 0xF1)

| 채널 | 이름 | 단위 | 설명 |
|------|------|------|------|
| 0 | Model Torque | Nm | 공칭 모델 토크 (중력+마찰) |
| 1 | DOB Torque | Nm | DOB 보상 토크 |
| 2 | Output Torque | Nm | 총 출력 토크 |
| 3 | Ext Est | Nm | 추정 외부 토크 ≈ 인간 의도 힘 |

---

## 제어공학적 제약사항과 튜닝 가이드

### ① rightHipTorque 단위 주의 (Critical)

`XM.status.h10.rightHipTorque`는 이름과 달리 **모터 전류(A)**입니다.

```c
// cm_drv.c 내부 변환
dst_buf->rightHipTorque = raw_int16 / 60.0f;  // 단위: A (−30~+30 A)

// Ex.31 코드가 올바르게 사용하는 방법
float current_r_a = XM.status.h10.rightHipTorque;   // A
float tau_meas_r  = KT_NM_PER_A * current_r_a;      // 0.8 Nm/A × A = Nm ✓
```

**주의**: `XM.status.h10.rightHipTorque`에 직접 Nm 단위로 사용하면 Kt 배 오차가 발생합니다.

### ② 통신 지연에 의한 DOB 대역폭 한계

```
XM10 → (CAN-FD 명령, ~0.5ms) → H10 CM → MD → 모터 출력
H10 CM → (PDO 응답, ~0.5ms + CM 처리) → XM10 수신
총 왕복 지연: ~1~2ms → 등가 샘플링 지연
```

이론적으로 안정적인 DOB 대역폭 상한: **f_c < 1/(2×지연) ≈ 250~500 Hz**.
Q-filter 5Hz는 이 한계를 충분히 하회 — ✅ 안전.
f_c를 높일수록 실제 지연과 노이즈에 민감해집니다.

### ③ 속도 추정 노이즈

이 예제는 후향 차분(backward difference)으로 각속도를 추정합니다:

```c
vel = (angle[k] - angle[k-1]) / 0.001f   // 1ms 차분
```

1ms 차분은 각도 해상도(예: 0.01°)의 노이즈를 1000배 증폭합니다.
→ 0.01° 노이즈 → 10 deg/s 속도 노이즈

마찰 보상의 데드존(`VEL_DZ_RADS = 0.01 rad/s`)이 저속 노이즈를 일부 억제하지만,
고속 구간에서는 노이즈가 DOB 입력에 영향을 줍니다. 개선 방향: 속도에 별도 LPF 적용.

### ④ 모터 토크 상수(Kt) 오차

실제 Kt는 전류, 온도, 감속기 효율에 따라 ±10~20% 변동합니다.

```
τ_meas = Kt_nominal × i_meas ≠ τ_actual
오차 = (Kt_actual - Kt_nominal) × i_meas → DOB가 모델 오차로 흡수
```

DOB가 이 오차도 `d_hat`에 포함시켜 자동 보상합니다 — 이것이 DOB의 강점입니다.
단, Kt가 심하게 다를 경우 τ_meas 자체가 부정확해져 DOB 초기값 드리프트 가능.

### ⑤ 모터각 vs 고관절각

`rightHipMotorAngle`은 모터 샤프트 각도입니다.
감속기(기어비 N)가 있는 경우: `θ_hip = θ_motor / N`.
이 예제는 기어비를 고려한 변환 없이 `rightHipMotorAngle`을 직접 사용합니다.
실제 시스템에서는 H10의 기어비를 확인하여 보상이 필요할 수 있습니다.

### ⑥ Q-filter 안정성 조건

```
α_q = 1 - 2π·f_c·dt

f_c = 5 Hz, dt = 0.001s → α_q = 1 - 0.0314 ≈ 0.969   ✅
f_c = 159 Hz → α_q = 0 (임계)
f_c > 159 Hz → α_q < 0 → IIR 발산 ❌
```

코드에서 `_ClampFloat(alpha, 0.0f, 0.999f)`로 보호되어 있습니다. ✅

### ⑦ 파라미터 튜닝 가이드

| 파라미터 | 기본값 | 의미 | 튜닝 방향 |
|---------|--------|------|----------|
| `MGL_EFF` | 17.16 Nm | M·g·L_eff (70kg 기준) | 실제 착용자 체중으로 조정: 체중(kg)×9.81×0.25 |
| `B_COULOMB_NM` | 0.3 Nm | 정마찰 | PhAI Studio에서 저속 토크 관찰 후 조정 |
| `B_VISCOUS_NMS` | 0.01 Nm·s/rad | 속도 비례 마찰 | 고속 운동 중 `Model Torque` 채널로 확인 |
| `KT_NM_PER_A` | 0.8 Nm/A | 모터 토크 상수 | MD 모터 사양서 참조 |
| `DOB_CUTOFF_HZ` | 5 Hz | Q-filter BW | 낮을수록 느리고 안정, 높을수록 빠르고 채터링 |
| `MAX_TORQUE_NM` | 5.0 Nm | 안전 포화 | 착용자 체중·관절 한계에 맞게 조정 |

---

## 실행 방법

1. `friction_comp_dob.c`를 `user_app.c`로 복사 후 빌드하여 XM10에 플래시합니다.
2. H10 전원 ON → ASSIST MODE 전환.
3. LED1 빠른 깜빡임(200ms) → ACTIVE 진입 확인.
4. PhAI Studio에서 Module ID `0xF1` 채널을 열어 4개 채널을 실시간 모니터링합니다.
5. BTN1로 DOB ON/OFF를 전환하며 `tau_out`의 변화를 관찰합니다.
6. BTN2로 Q-filter 주파수를 변경하며 응답 속도와 노이즈의 트레이드오프를 체험합니다.

---

## 직접 해보기

- **DOB 비교 실험**: BTN1로 DOB OFF → Ex.21과 동일. 같은 동작에서 `tau_out`이 달라지는지 PhAI Studio에서 비교하세요.
- **Q-filter 튜닝**: f_c = 1Hz vs 20Hz에서 착용자가 느끼는 응답감의 차이를 체험하세요.
- **τ_ext_est 관찰**: 손으로 외골격을 밀 때 `Ext Est` 채널이 반응하는지 확인하세요. 이것이 Stage 2가 사용할 신호입니다.
- **파라미터 식별**: `Model Torque`와 `Ext Est`를 동시에 관찰하며 공칭 모델 파라미터를 튜닝하세요. `Ext Est`가 0에 가까울수록 모델이 정확합니다.

---

## 핵심 레퍼런스

제어공학 배경 없이도 이해할 수 있는 순서로 정렬했습니다.

**[입문] 왜 외란 관측기가 필요한가**
- **Umeno, T. & Hori, Y. (1991).** "Robust speed control of DC servomotors using modern two degrees-of-freedom controller design." *IEEE Trans. Industrial Electronics, 38*(5), 363–368. (~800 citations)
  → DOB 기반 서보 제어의 원조 논문. 왜 모델만으로는 부족한지 설명.

**[핵심] DOB 이론과 안정성 분석**
- **Ohnishi, K., Shibata, M., & Murakami, T. (1996).** "Motion control for advanced mechatronics." *IEEE/ASME Trans. Mechatronics, 1*(1), 56–67. (~1200 citations)
  → DOB의 체계적 이론 정립. Q-filter 설계 방법론 포함.
- **Sariyildiz, E., Oboe, R., & Ohnishi, K. (2020).** "Disturbance observer-based robust control and its applications: 35th anniversary overview." *IEEE Trans. Industrial Electronics, 67*(3), 2042–2053. (~400 citations)
  → DOB 35년 역사의 종합 리뷰. 안정성 조건과 실제 구현 지침 포함. **가장 먼저 읽을 논문**.

**[응용] 외골격 투명 모드**
- **Hogan, N. (1985).** "Impedance control: An approach to manipulation." *ASME Journal of Dynamic Systems, 107*(1), 1–24. (~3000 citations)
  → 임피던스 제어의 기원. "투명 모드 = 임피던스 0"의 철학적 근거.
- **Keemink, A. Q. L. et al. (2018).** "Admittance control for physical human-robot interaction." *The International Journal of Robotics Research, 37*(11), 1421–1444. (~300 citations)
  → 외골격에서 투명성과 보조를 동시에 달성하는 어드미턴스 제어 프레임워크.
- **Vanderborght, B. et al. (2013).** "Variable impedance actuators: A review." *Robotics and Autonomous Systems, 61*(12), 1601–1614. (~1800 citations)
  → 왜 고정 임피던스가 아닌 가변 임피던스가 필요한지, 외골격 설계 철학의 배경.

---

## 주의사항

> **DOB 안전 초기화**: DOB는 적분기 성격을 가집니다. ACTIVE 진입 시 `d_hat = 0`으로
> 초기화하지 않으면 이전 추정치가 남아 순간적인 큰 토크가 출력될 수 있습니다.

> **이전 예제 권장**: 이 예제는 Ex.21(중력+마찰 보상)이 선행되어야 합니다.
> 공칭 모델 파라미터(`MGL_EFF`, `B_COULOMB_NM`, `B_VISCOUS_NMS`)를 실제 시스템에 맞게 튜닝하세요.

> **rightHipTorque 단위**: 이 필드는 필드명과 달리 **모터 전류(A)**를 제공합니다.
> 토크 추정: `τ = Kt × rightHipTorque`. 직접 Nm으로 사용하면 안 됩니다.
