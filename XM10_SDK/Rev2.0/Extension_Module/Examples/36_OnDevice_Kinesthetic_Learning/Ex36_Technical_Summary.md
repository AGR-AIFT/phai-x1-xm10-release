# Ex.36 On-Device Kinesthetic Learning — 기술 요약서

> XM10 Extension Module 제어 시연용 | Angel Robotics | 2026-04-03

---

## 1. 시스템 구성

### 1.1 하드웨어

| 항목 | 사양 |
|------|------|
| 플랫폼 | SUIT H10 고관절 외골격 (거치대 고정) |
| 제어 모듈 | XM10 Extension Module (STM32H743, 480MHz, Cortex-M7 FPU) |
| 구동기 | SAM10 x2 (좌/우 고관절, 감속비 18.75:1) |
| 토크 상수 | K_t = 0.085 Nm/A (관절 등가: 1.594 Nm/A) |
| 제어 주기 | 1 kHz (UserTask), 백그라운드 학습 별도 RTOS 태스크 |

### 1.2 링크 물성치

| 항목 | 값 | 비고 |
|------|-----|------|
| 질량 *m* | 0.184 kg | 좌우 동일 |
| CoM 거리 *L_com* | 142.25 mm | 관절축 → 질량 중심 |
| 링크 전장 | 284.5 mm | 관절축 → 끝점 |
| 링크 관성 *J_link* | 0.00372 kg·m² | m·L_com² |
| 모터 반영 관성 *J_motor_ref* | 0.00116 kg·m² | J_motor·GR² |
| 총 관성 *J_total* | 0.00488 kg·m² | J_link + J_motor_ref |
| 최대 중력 토크 | 0.257 Nm | m·g·L_com |

---

## 2. 아키텍처

### 2.1 실시간 / 백그라운드 분리

```
UserTask (1kHz, 우선순위 54) — 제어 전용
├─ TEACH: 적응 중력보상 + 100Hz 궤적 기록
├─ LEARN: 중력보상만 (학습은 BgTask에서)
└─ REPLAY: NN 추론 + Gravity + LQR-PD

BgTask (우선순위 24, XM_BgTask_Create) — 학습 전용
└─ SGD 5000 epoch (시간 제한 없음)
   UserTask가 sleep 중일 때 CPU 사용
```

**설계 근거**: 1ms 실시간 틱에서 NN 학습(batch 100 × forward+backward)을 실행하면 SYNC/Heartbeat 지연 → PnP 끊김이 발생합니다. RTOS 우선순위 기반 분리로 제어 루프의 결정론적 타이밍을 보장합니다.

### 2.2 NN 사양

| 항목 | 값 |
|------|-----|
| 구조 | 3 → 16 (ReLU) → 2 (Linear) |
| 입력 | phase (0~1), sin(2πφ), cos(2πφ) |
| 출력 | θ_R, θ_L (deg) |
| 파라미터 | 3×16 + 16 + 16×2 + 2 = 114개 (456B) |
| 추론 시간 | < 2μs @ 480MHz |

**Phase Encoding**: 단순 phase(0~1) 입력은 ReLU 네트워크에서 주기적 패턴 학습이 어렵습니다. sin/cos 인코딩으로 주기성을 명시적으로 제공하여 수렴 속도와 정확도를 향상시킵니다 (Vaswani et al. 2017, Positional Encoding 패턴 차용).

### 2.3 학습 알고리즘

| 항목 | 값 |
|------|-----|
| Optimizer | SGD (Stochastic Gradient Descent) |
| 학습률 | 0.001 → 0.0001 (선형 감쇠) |
| Epochs | 5000 |
| Batch Size | 20 (랜덤 미니배치) |
| Loss | MSE (Mean Squared Error) |
| 초기화 | Xavier-like (입력 차원 기반 스케일링) |

**Backprop 구현**: Hidden gradient를 W2 업데이트 전에 계산하여 stale gradient 문제를 방지합니다 (정석적 역전파 순서).

---

## 3. 제어 상세

### 3.1 TEACH — 적응 중력보상

```
τ_total = MGL_eff · sin(θ)

MGL_eff 온라인 추정:
  residual = τ_measured - MGL_eff · sin(θ)
  MGL_eff += α · residual · sin(θ)     (α = 0.001)
```

초기값 MGL_eff = 0.257 Nm (CAD 기반)을 실측 토크로 보정합니다. 거치대 조립 공차, 케이블 장력 등에 의한 실제 중력 토크 편차를 적응적으로 흡수합니다.

### 3.2 REPLAY — LQR 최적 게인 + Gravity Comp

```
τ_total = τ_gravity + τ_PD

τ_gravity = MGL_eff · sin(θ)                [Feedforward]
τ_PD      = Kp · (θ_target - θ) - Kd · θ̇   [LQR-PD Feedback]
```

#### LQR 게인 유도

중력보상 후 잔여 동역학을 선형화:

```
J_total · θ̈ = τ
```

상태: x = [θ - θ_ref, θ̇], 입력: τ. LQR cost Q = diag(q_pos, q_vel), R = r로부터:

| 설계 파라미터 | 값 | 의미 |
|-------------|-----|------|
| ω_n | 15 rad/s | 자연 주파수 (반응시간 ~0.2s) |
| ζ | 0.8 | 댐핑비 (약간 과감쇠) |
| **Kp** | J_total · ω_n² · (π/180) ≈ **0.019 Nm/deg** | |
| **Kd** | 2·ζ·J_total·ω_n · (π/180) ≈ **0.002 Nm·s/deg** | |

### 3.3 속도 추정 — Alpha-Beta Tracker

```
Predict:  θ̂⁻ = θ̂ + θ̂̇ · dt
Update:   θ̂  = θ̂⁻ + α · (y - θ̂⁻)
          θ̂̇  = θ̂̇  + (β/dt) · (y - θ̂⁻)
```

| 파라미터 | 값 | 의미 |
|---------|-----|------|
| α | 0.2 | 위치 보정 가중치 |
| β | 0.01 | 속도 보정 가중치 (강한 노이즈 필터링) |

Ex.35에서 검증된 모델프리 속도 추정을 동일하게 적용합니다.

### 3.4 Ping-Pong 재생

궤적 끝에서 loop restart 대신 **방향 반전**으로 부드러운 전환:

```
정방향: idx 0 → N-1
역방향: idx N-1 → 0
반전 시: Alpha-Beta 속도 리셋 (미분 스파이크 방지)
```

---

## 4. NV Flash 비휘발성 저장

| 영역 | 오프셋 | 크기 | 내용 |
|------|--------|------|------|
| Magic | 0 | 4B | 0xCA5E0A01 |
| NN 가중치 | 4 | 456B | TinyNN_t 구조체 |
| MGL_eff | 460 | 4B | 적응 추정된 중력 파라미터 |
| teach_count | 464 | 4B | 교시 샘플 수 (재생 속도 복원용) |

**저장**: BTN1 Long Press (REPLAY 중)
**복원**: 부팅 시 자동 — Magic 검증 후 NN 가중치 + MGL 로드

---

## 5. 안전

| 항목 | 값 |
|------|-----|
| 토크 포화 | ±8 Nm (소프트웨어 클램핑) |
| NN 출력 클램핑 | 교시 범위 ±5° 이내 |
| ASSIST 모드 이탈 | 500ms 유예 후 STANDBY 전환 |
| Active_Exit | 토크 0 + MONITOR 모드 복귀 |

---

## 6. 성능 지표

| 항목 | 값 |
|------|-----|
| 학습 시간 | ~1-5초 (CPU 여유 시간 의존) |
| 추론 시간 | < 2μs per tick |
| REPLAY 제어 부하 | < 10μs per tick (1ms 예산의 1%) |
| NN 메모리 | 456B (가중치) + 16KB (교시 버퍼) |
| Flash 사용량 | 468B / 128KB (0.4%) |
