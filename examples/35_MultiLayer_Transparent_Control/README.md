# 예제 35: 다층 투명제어 — Zero-Impedance / Virtual Wall / Bilateral Coupling

거치대에 고정된 H10의 다리 링크(184g)를 **3가지 제어 모드**로 실시간 전환하며 시연하는 예제입니다. 3개의 제어 레이어를 조합하여 하나의 예제에서 극적으로 다른 물리적 느낌을 렌더링합니다.

## 학습 목표

* 중력보상 + DOB + 임피던스를 **레이어 구조**로 조합하는 다층 제어 아키텍처를 이해합니다.
* Alpha-Beta Tracker를 이용한 **모델프리 속도 추정**을 학습합니다.
* 버튼 하나로 제어 모드를 전환하는 **TSM + 모드 FSM** 패턴을 익힙니다.
* Bilateral Coupling을 통한 **양방향 텔레오퍼레이션** 원리를 체험합니다.

## 제어 아키텍처

```
                      ┌─────────────────────────────────────────────┐
                      │  Layer 1: Gravity Compensation (항상 ON)      │
                      │  τ_grav = m·g·L_com · sin(θ)                 │
                      │  (마찰보상은 SAM10이 시스템식별 기반으로 수행)    │
                      └──────────────┬──────────────────────────────┘
                                     │
              ┌──────────────────────┼──────────────────────┐
              ▼                      ▼                      ▼
   ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────────┐
   │ Layer 2: DOB      │  │ Layer 3a:         │  │ Layer 3b: Bilateral   │
   │ (Zero-Impedance)  │  │ Impedance         │  │ Coupling              │
   │                    │  │ (Virtual Wall)    │  │ (텔레오퍼레이션)       │
   │ d_hat = Q-filter   │  │ τ = K·e - B·θ̇    │  │ τ_R = Kc·(θL-θR)     │
   │ 외란관측기          │  │ 가상 강성 벽       │  │    + Bc·(θ̇L-θ̇R)     │
   └──────────────────┘  └──────────────────┘  └──────────────────────┘
```

## 3가지 제어 모드

### MODE 0: Zero-Impedance (BTN1)

중력보상 + DOB(토글) 조합으로 **로봇이 사라진 듯한 느낌**을 렌더링합니다.

* **BTN1 클릭**: Zero-Impedance 모드 진입 (DOB OFF 상태)
* **BTN1 길게 누르기**: DOB ON/OFF 토글
* DOB OFF: 중력보상만 → 역구동성(~0.3 Nm)이 약간 느껴짐
* DOB ON: 잔류 외란까지 보상 → 완전히 자유로운 느낌
* Q-filter 차단주파수: 5 Hz

### MODE 1: Virtual Wall (BTN2)

현재 위치를 캡처하여 **임피던스 제어로 고정**합니다.

* **BTN2 클릭**: 현재 각도를 평형점으로 캡처, 임피던스 활성화
* 스프링 강성: K = 0.2 Nm/deg
* 댐핑 계수: B = 0.02 Nm·s/deg (과감쇠 설정)
* 밀어도 원래 자리로 복귀 → 보이지 않는 벽 느낌

### MODE 2: Bilateral Coupling (BTN3)

한쪽 다리를 움직이면 반대쪽이 따라오는 **양방향 텔레오퍼레이션**입니다.

* **BTN3 클릭**: Bilateral Coupling 모드 진입
* 커플링 강성: Kc = 0.15 Nm/deg
* 커플링 감쇠: Bc = 0.01 Nm·s/deg
* 원리: `τ_R = Kc·(θ_L - θ_R) + Bc·(θ̇_L - θ̇_R)`, `τ_L = -τ_R` (작용-반작용)
* 한쪽을 잡으면 이쪽에서 저항이 느껴지고, 놓으면 양쪽이 동기화됨

## 하드웨어 파라미터 (CAD 실측)

거치대 기준: 골반 베이스 고정, 다리 링크만 구동.

| 파라미터 | 값 | 비고 |
|----------|-----|------|
| 링크 질량 | 0.184 kg | 좌우 동일, CAD 실측 |
| CoM 거리 | 126.4 mm | 관절축 → 질량중심 |
| 링크 길이 | 284.5 mm | - |
| 관절축 관성 | 0.00414 kgm² | Iyy(CoM) + m·d² |
| MGL (유효 중력 모멘트) | 0.228 Nm | m·g·L_com |
| SAM10 역구동성 | ~0.3 Nm | DOB 없이 느껴지는 잔류력 |
| 감속비 | 18.75:1 | SAM10 스펙 |
| Kt (관절) | 1.594 Nm/A | Kt_motor × 감속비 |
| 최대 토크 제한 | ±8 Nm | 정격 10 Nm 대비 보수적 |

## 속도 추정: Alpha-Beta Tracker

LPF 대신 **모델프리 Alpha-Beta Tracker**를 사용합니다.

* 모델 의존성 없음: 등속 가정(θ̈ ≈ 0)으로 예측, 측정 혁신으로 보정
* 대역폭: ~40 Hz
* 게인: L1 = 22.49 (위치 보정), L2 = 248.0 (속도 추정)
* Kalman 속도를 사용하므로 LPF 대비 깨끗한 댐핑 응답 제공

```
[Predict]  θ̂⁻ = θ̂ + ω̂·dt,   ω̂⁻ = ω̂  (등속 가정)
[Update]   e = θ_meas - θ̂⁻
           θ̂ = θ̂⁻ + L1·dt·e
           ω̂ = ω̂⁻ + L2·dt·e
```

## 호밍 절차

ASSIST 모드 진입 시 Ex.12 패턴의 호밍을 자동 수행합니다.

1. SAM10 임피던스 Kp/Kd 최대값 설정 (`XM_SendIVectorKpKdMax`)
2. 강한 임피던스 설정 (Kp=80, Kd=1)
3. 현재 위치 → 0도로 P-Vector 궤적 전송 (속도 150 deg/s)
4. 양쪽 P-Vector 완료 대기
5. 50ms 안정화 후 임피던스 해제 (Kp=0, Kd=0)
6. 평형점 캡처 + Kalman 재초기화 → 제어 시작

## 시연 순서

1. H10 전원 ON → CM 연결 대기 (LED1 heartbeat)
2. CM 연결 → STANDBY 상태
3. H10에서 ASSIST 모드 진입 → 자동 호밍 → Zero-Impedance 모드
4. **BTN1**: Zero-Impedance (자유), **BTN1 길게**: DOB 토글
5. **BTN2**: Virtual Wall (현재 위치 고정)
6. **BTN3**: Bilateral Coupling (양쪽 다리 연동)

## LED 표시

| 모드 | LED1 | LED2 | LED3 |
|------|------|------|------|
| Zero-Impedance | Heartbeat | OFF | DOB 상태 (ON/OFF) |
| Virtual Wall | ON (solid) | ON | OFF |
| Bilateral Coupling | 빠른 점멸 (200ms) | ON | ON |

## USB 스트리밍 (Meta ID: 0xF0)

7채널 실시간 스트리밍:

| 채널 | 이름 | 단위 |
|------|------|------|
| 0 | Gravity | Nm |
| 1 | DOB | Nm |
| 2 | Impedance | Nm |
| 3 | Total | Nm |
| 4 | Angle | deg |
| 5 | Velocity | deg/s |
| 6 | Mode | - |

## 디버그

STM32CubeIDE **Live Expression**에서 `g_ml_dbg` 구조체를 관찰합니다.

| 필드 | 설명 |
|------|------|
| `tsm_state` | 0=OFF, 1=STANDBY, 2=ACTIVE |
| `homing_state` | HomingState_t (0~6) |
| `homing_done` | 호밍 완료 여부 (0/1) |
| `ctrl_mode` | 0=Zero-Impedance, 1=Virtual Wall, 2=Bilateral |
| `dob_active` | DOB 활성 여부 (0/1) |
| `ang_r` / `ang_l` | 좌우 모터 각도 (deg) |
| `tau_total_r` / `tau_total_l` | 좌우 총 토크 (Nm) |
| `loop_cnt` | 루프 카운터 (증가 확인) |
| `ctrl_step` | _RunControl 체크포인트 (1~7) |

## 주의사항

* **USB CDC 디버그 메시지 비활성** — 활성 시 크래시 발생 가능 (코드에서 의도적 제거)
* **마찰보상은 SAM10이 수행** — XM10에서 이중보상 절대 금지
* **토크 포화**: ±8 Nm (정격 10 Nm 대비 보수적 제한)
* **DOB는 Zero-Impedance 모드에서만 동작** — 다른 모드 전환 시 자동 해제 + 추정치 리셋

## 참고 문헌

* Ohnishi, K. (1996) — Disturbance Observer
* Hogan, N. (1985) — Impedance Control
* Slotine, J.-J. E. & Li, W. (1991) — Applied Nonlinear Control
* Ex.20 `impedance_control.c`, Ex.21 `gravity_compensation.c`, Ex.31 `friction_comp_dob.c`
