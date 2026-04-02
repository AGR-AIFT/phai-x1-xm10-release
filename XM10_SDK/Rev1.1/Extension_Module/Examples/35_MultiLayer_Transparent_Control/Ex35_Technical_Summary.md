# Ex.35 Multi-Layer Transparent Control — 기술 요약서

> XM10 Extension Module 제어 시연용 | Angel Robotics | 2026-04-03

---

## 1. 시스템 구성

### 1.1 하드웨어

| 항목 | 사양 |
|------|------|
| 플랫폼 | SUIT H10 고관절 외골격 (거치대 고정) |
| 제어 모듈 | XM10 Extension Module (STM32H743, 480MHz) |
| 구동기 | SAM10 ×2 (좌/우 고관절, 감속비 18.75:1) |
| 토크 상수 | K_t = 0.085 Nm/A (관절 등가: 1.594 Nm/A) |
| 역구동 토크 | < 0.3 Nm (시스템 식별 기반 마찰보상 후 잔류) |
| 정격/최대 토크 | 10 Nm / 18.3 Nm (소프트웨어 제한: 8 Nm) |
| 제어 주기 | 1 kHz (1 ms) |

### 1.2 링크 물성치 (3D CAD 실측)

| 항목 | 값 | 비고 |
|------|-----|------|
| 질량 *m* | 0.184 kg | 좌우 동일 |
| CoM 거리 *L_com* | 126.4 mm | 관절축 → 질량 중심 |
| 링크 전장 | 284.5 mm | 관절축 → 끝점 |
| 관절축 관성 *J* | 0.00414 kg·m² | I_yy(CoM) + m·d² (평행축 정리) |
| 최대 중력 토크 | 0.228 Nm | m·g·L_com |

---

## 2. 제어 아키텍처

### 2.1 계층 구조

```
┌──────────────────────────────────────────────────────┐
│  State Estimation: Alpha-Beta Tracker (모델프리)       │
│  · 등속 예측 + 측정 혁신 보정                            │
│  · 속도 추정 대역폭 ≈ 40 Hz                            │
│  · 전 모드 공통 적용                                    │
├──────────┬───────────────┬───────────────────────────┤
│ Zero-Imp │ Virtual Wall  │ Bilateral Coupling        │
│ (투명성)  │ (가상 강성 벽)  │ (양방향 텔레오퍼레이션)     │
├──────────┴───────────────┴───────────────────────────┤
│  Layer 1: Gravity Compensation                        │
│  τ_grav = m·g·L_com · sin(θ)                         │
│  · 단순 진자 모델 (θ=0: 수직 수하, 안정 평형)            │
│  · 마찰보상은 SAM10 내부에서 수행 (이중보상 방지)         │
└──────────────────────────────────────────────────────┘
```

### 2.2 상태 추정 — Alpha-Beta Tracker

모터 엔코더로부터 위치만 측정 가능한 상황에서, 모델 기반 Kalman Filter 적용 시 액추에이터 내부 동역학(전류 루프, 감속기 비선형성)과의 모델 불일치가 속도 추정을 오염시키는 문제가 확인되었습니다.

이를 해결하기 위해 **모델프리 Alpha-Beta Tracker**를 채택하였습니다:

```
Predict:  θ̂⁻ = θ̂ + θ̂̇·dt          (등속 가정)
          θ̂̇⁻ = θ̂̇                 (가속도 = 0 가정)
Update:   θ̂  = θ̂⁻ + L₁·dt·(y - θ̂⁻)
          θ̂̇  = θ̂̇⁻ + L₂·dt·(y - θ̂⁻)
```

| 파라미터 | 값 | 의미 |
|---------|-----|------|
| L₁ | 22.49 | 위치 보정 이득 (α ≈ 0.022) |
| L₂ | 248.0 | 속도 보정 이득 (β ≈ 0.248) |

액추에이터 모델에 의존하지 않으므로 SAM10의 내부 제어 구조 변경에 robust하며, 위상 지연이 1차 LPF 대비 적어 댐핑 토크의 안정성이 향상됩니다.

---

## 3. 제어 모드 상세

### 3.1 Zero-Impedance Mode (임피던스 영 렌더링)

**목적**: 액추에이터의 기계적 임피던스를 최소화하여, 착용자 또는 외부 조작자가 관절을 자유롭게 움직일 수 있는 투명 모드를 구현합니다.

**제어 법칙**:
```
τ_total = τ_gravity + τ_DOB

τ_gravity = m·g·L_com · sin(θ)        [중력보상]
τ_DOB     = d̂[k]                       [외란 추정치, 선택적 활성화]

d̂[k] = α_q · d̂[k-1] + (1-α_q) · (τ_meas - τ_gravity)
α_q  = 1 - 2π·f_c·dt,  f_c = 5 Hz
```

| 요소 | 역할 |
|------|------|
| 중력보상 | 링크 자중에 의한 위치 의존적 토크를 상쇄 |
| DOB | 공칭 모델(중력)로 설명되지 않는 잔류 외란(비선형 마찰, 케이블 장력 등)을 1차 Q-filter로 추정·보상 |

DOB는 BTN1 Long Press로 토글 가능합니다. 비활성 시 순수 중력보상만 적용됩니다.

**이론적 배경**: Ohnishi (1996) — Disturbance Observer 기반 모션 제어에서 제안된 프레임워크를 단일 관절 외골격에 적용한 것입니다. 이상적으로 DOB가 완전 보상되면 유효 임피던스 Z_eff → 0이 되어, 조작자는 액추에이터의 존재를 인지하지 못합니다.

---

### 3.2 Virtual Wall Mode (가상 강성 벽 렌더링)

**목적**: 버튼을 누른 순간의 관절 각도를 평형점으로 캡처하고, 임피던스 제어로 해당 위치를 유지합니다.

**제어 법칙**:
```
τ_total = τ_gravity + τ_impedance

τ_impedance = K·(θ_eq - θ) - B·θ̇_filtered
```

| 파라미터 | 값 | 설계 근거 |
|---------|-----|----------|
| K (가상 강성) | 0.2 Nm/deg | 10° 외란 시 2 Nm 복원력 |
| B (가상 감쇠) | 0.02 Nm·s/deg | 과감쇠 (ζ > 1): B > B_cr = 2√(K_rad·J) ≈ 0.012 |

댐핑 항에 사용되는 속도는 Alpha-Beta Tracker 출력을 사용하여 수치 미분 노이즈를 억제합니다. 감쇠비를 임계감쇠 이상(과감쇠)으로 설정하여 오버슈트 없이 단조 수렴을 보장합니다.

**이론적 배경**: Hogan (1985) — 임피던스 제어는 위치와 힘을 동시에 독립 제어할 수 없다는 인식 하에, 위치-힘 관계(임피던스)를 가상 스프링-댐퍼로 매개변수화하여 안정적 상호작용을 구현합니다.

---

### 3.3 Bilateral Coupling Mode (양방향 힘 반사 텔레오퍼레이션)

**목적**: 좌우 고관절을 양방향 커플링하여, 한쪽 관절의 움직임이 반대쪽에 실시간으로 반영되고 그 역도 성립하는 텔레오퍼레이션을 구현합니다.

**제어 법칙**:
```
τ_R = τ_gravity_R + K_c·(θ_L - θ_R) + B_c·(θ̇_L - θ̇_R)
τ_L = τ_gravity_L - K_c·(θ_L - θ_R) - B_c·(θ̇_L - θ̇_R)
```

| 파라미터 | 값 | 의미 |
|---------|-----|------|
| K_c (커플링 강성) | 0.15 Nm/deg | 위치 동기화 |
| B_c (커플링 감쇠) | 0.01 Nm·s/deg | 속도 동기화 |

좌우 토크가 작용-반작용 관계(τ_R = -τ_L + gravity)를 만족하므로 에너지 보존이 성립하며, 커플링 계수를 조절하여 동기화 강도와 조작 자유도 사이의 트레이드오프를 설정합니다.

---

## 4. 시스템 운용 절차

### 4.1 초기화 시퀀스

```
전원 ON → OFF (CM 연결 대기)
       → STANDBY (H10 ASSIST 대기)
       → ACTIVE: Homing 수행
           ├─ I-Vector: Kp=80 임피던스 설정
           ├─ P-Vector: 0° 위치로 궤적 이동
           ├─ 완료 대기 → 임피던스 해제 (Kp=0)
           └─ Zero-Impedance 모드 진입
```

호밍 절차는 SAM10의 임피던스 제어기를 초기화하고 관절을 기준 위치(0°)로 정렬하기 위한 것입니다.

### 4.2 모드 전환

| 입력 | 동작 | LED 표시 |
|------|------|---------|
| BTN1 클릭 | Zero-Impedance | LED1 heartbeat |
| BTN1 길게 | DOB ON/OFF 토글 | LED3 = DOB 상태 |
| BTN2 클릭 | Virtual Wall (현재 위치 캡처) | LED1 + LED2 ON |
| BTN3 클릭 | Bilateral Coupling | LED1 blink(200ms) + LED2 + LED3 ON |

모든 모드 전환 시 DOB 추정치(d̂)가 0으로 초기화되어 토크 불연속을 방지합니다.

---

## 5. 설계 시 고려한 제약사항

### 5.1 마찰보상 분리

SAM10 모터 드라이버가 시스템 식별 기반 마찰보상을 내부적으로 수행하고 있습니다. XM10에서 추가로 마찰보상을 적용하면 이중보상으로 인한 불안정이 발생하므로, XM10 제어 레이어에서는 중력보상만 수행합니다.

### 5.2 상태 추정기 선택

초기 설계에서 선형화 진자 모델(`J·θ̈ = τ - m·g·L·sin(θ)`) 기반 Kalman Filter를 적용하였으나, SAM10의 내부 전류/속도 제어 루프가 유효 플랜트 동역학을 변형시켜 모델 불일치가 발생했습니다. 이로 인해 속도 추정이 편향되어 댐핑 토크의 불안정을 초래하였고, 모델에 의존하지 않는 Alpha-Beta Tracker로 전환하여 해결하였습니다.

### 5.3 토크 제어의 한계

XM10은 보조 토크(auxiliary torque)를 PDO를 통해 1 kHz로 CM에 전송하고, CM이 이를 SAM10에 전달하는 구조입니다. SAM10의 쿨롱 마찰 잔류분(~0.3 Nm)은 토크 명령으로 직접 상쇄할 수 없어, 정밀 위치 추종 시 stick-slip이 발생합니다. 위치 정밀도가 필요한 응용에서는 SAM10의 내부 위치 제어(P-Vector/I-Vector)와의 하이브리드 접근이 필요합니다.

---

## 6. 향후 확장 방향

| 방향 | 내용 |
|------|------|
| **Adaptive Parameter Identification** | 링크 물성치(m, L_com, J)를 온라인 추정하여 중력보상 정확도 향상 |
| **Force Amplification** | DOB 추정 외란(≈인간 의도 힘)을 증폭하여 착용자의 움직임을 보조 |
| **Phase-dependent Impedance Scheduling** | 보행 위상에 따라 임피던스를 실시간 전환 (착용 보행 시연) |
| **SAM10 시스템 식별** | 내부 동역학 모델 확보 후 정밀 Kalman Filter 재적용 |

---

## 참고 문헌

1. Hogan, N. (1985). "Impedance Control: An Approach to Manipulation." *ASME J. Dynamic Systems, Measurement, and Control*, 107(1), 1–24.
2. Ohnishi, K. et al. (1996). "Microprocessor-Controlled DC Motor for Load-Insensitive Position Servo System." *IEEE Trans. Industrial Electronics*, 33(1).
3. Sariyildiz, E. & Ohnishi, K. (2015). "Stability and Robustness of Disturbance-Observer-Based Motion Control Systems." *IEEE Trans. Industrial Electronics*, 62(1).
