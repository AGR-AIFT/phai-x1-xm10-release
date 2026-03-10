# 예제 31: 외란 관측기 기반 투명 모드 완성 (DOB — Stage 1)

본 예제는 **외란 관측기(Disturbance Observer, DOB)**를 이용하여
예제 21(중력+마찰 보상)로는 제거하지 못한 **잔류 외란**을 실시간으로 추정·보상합니다.
이로써 진정한 **액추에이터 투명성(Physical Transparency, Stage 1)**이 완성됩니다.

> 📖 API 레퍼런스: [H10 Control & Data](../../docs/api-reference/02-h10-control-n-data.md) · [Task State Machine](../../docs/api-reference/01-task-state-machine.md)
>
> 📄 전제 예제: [Ex.21 중력+마찰 보상](../21_Gravity_Compensation/) (공칭 모델 기반)
>
> 📄 논문 레퍼런스: Ohnishi, K. et al. (1996). *Microprocessor-controlled DC motor for load-insensitive position servo system.* IEEE Trans. Industrial Electronics, 33(1).

---

## Physical AI 5단계 학습 여정에서의 위치

```
Stage 1 ★  Physical Transparency  ← 지금 여기 (DOB로 완성)
Stage 2    Intent Sensing          ← τ_ext_est가 핵심 입력 신호
Stage 3    Adaptive Assistance
Stage 4    Machine Learning
Stage 5    Shared Autonomy
```

> **Stage 1 → Stage 2 연결 고리**: DOB가 추정하는 `τ_ext_est`(외부 토크 추정치)는
> 인간 의도 힘(human intent force)의 근사값입니다.
> Stage 2(Ex.32)에서 이 신호를 임계값으로 분류하면 "의도 감지"가 됩니다.

---

## 학습 목표

- **공칭 모델(Nominal Model)의 한계**를 이해합니다.
  Ex.21의 중력+마찰 보상 이후에도 잔류하는 외란(HW 비선형성, 모델 오차)이 존재합니다.
- **DOB Q-filter 원리**를 학습합니다:
  `d_hat[k] = α_q · d_hat[k-1] + (1 - α_q) · (τ_meas - τ_model)`
- Q-filter **차단 주파수(f_c)의 트레이드오프**를 체험합니다:
  낮을수록 노이즈 억제 강, 응답 지연 / 높을수록 빠른 추적, 채터링 위험
- `τ_ext_est` 신호가 **인간 의도 힘의 추정치**임을 확인합니다 (Stage 2 연결).

---

## 동작 원리

### DOB 수식

```
공칭 모델: τ_model = τ_grav + τ_fric
               = M·g·L_eff·sin(θ)  +  B_f·sign(θ̇) + B_v·θ̇

실측 토크: τ_meas = Kt · i_meas  (Kt = 0.8 Nm/A)

잔류 외란: residual = τ_meas - τ_model  (모델 미설명 성분)

Q-filter:  d_hat[k] = α_q·d_hat[k-1] + (1-α_q)·residual
           α_q = 1 - 2π·f_c·dt   (f_c 기본값: 5 Hz)

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

## 실행 방법

1. `friction_comp_dob.c`를 `user_app.c`로 복사 후 빌드하여 XM10에 플래시합니다.
2. H10 전원 ON → ASSIST MODE 전환.
3. LED1 빠른 깜빡임(200ms) → ACTIVE 진입 확인.
4. PhAI Studio에서 Module ID `0xF1` 채널을 열어 4개 채널을 실시간 모니터링합니다.
5. BTN1로 DOB ON/OFF를 전환하며 `tau_out`의 변화를 관찰합니다.
6. BTN2로 Q-filter 주파수를 변경하며 응답 속도와 노이즈의 트레이드오프를 체험합니다.

---

## 직접 해보기

- **DOB 비교 실험**: BTN1로 DOB를 OFF하면 Ex.21과 동일합니다. 같은 동작에서 `tau_out`이 달라지는지 PhAI Studio에서 비교하세요.
- **Q-filter 튜닝**: f_c = 1Hz vs 20Hz에서 착용자가 느끼는 응답감의 차이를 체험하세요.
- **τ_ext_est 관찰**: 손으로 외골격을 밀 때 `Ext Est` 채널이 반응하는지 확인하세요. 이것이 Stage 2가 사용할 신호입니다.
- **Stage 2 연결**: `Ext Est > 임계값`일 때 이벤트 트리거를 추가하면 Ex.32(보행 의도 감지)의 DOB 버전이 됩니다.

---

## 주의사항

> **DOB 안전 초기화**: DOB는 적분기 성격을 가집니다. ACTIVE 진입 시 `d_hat = 0`으로
> 초기화하지 않으면 이전 추정치가 남아 순간적인 큰 토크가 출력될 수 있습니다.

> **이전 예제 권장**: 이 예제는 Ex.21(중력+마찰 보상)이 선행되어야 합니다.
> 공칭 모델 파라미터(`MGL_EFF`, `B_COULOMB_NM`, `B_VISCOUS_NMS`)를 실제 시스템에 맞게 튜닝하세요.
