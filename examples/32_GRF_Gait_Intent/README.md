# 예제 32: GRF 기반 보행 의도 감지 (Stage 2 — Intent Sensing)

본 예제는 **발 접촉 센서(GRF — Ground Reaction Force)** 이벤트를 이용하여
보행 위상(Gait Phase)을 실시간 추정하고, 이에 동기화된 **보조 토크**를 생성합니다.
원시(raw) 센서 신호가 어떻게 "**인간 의도(intent)**"로 변환되는지를 보여주는 Stage 2의 핵심 예제입니다.

> 📖 API 레퍼런스: [H10 Control & Data](../../docs/api-reference/02-h10-control-n-data.md)
>
> 📄 전제 예제: [Ex.31 DOB 투명 모드](../31_Friction_Comp_DOB/) (Stage 1 완성)
>
> 📄 논문 레퍼런스: Winter, D.A. (2009). *Biomechanics and Motor Control of Human Movement*, 4th ed. (보행 위상 정의) · Gervasi, A. et al. (2020). *Exoskeleton gait assistance based on continuous gait phase estimation.* IROS 2020.

---

## Physical AI 5단계 학습 여정에서의 위치

```
Stage 1    Physical Transparency  ← Ex.31 (DOB로 완성)
Stage 2 ★  Intent Sensing         ← 지금 여기 (GRF → 보행 의도)
Stage 3    Adaptive Assistance
Stage 4    Machine Learning
Stage 5    Shared Autonomy
```

> **Stage 2 핵심 질문**: 로봇이 인간의 다음 동작 의도를 어떻게 감지하는가?
> 이 예제의 답: **물리 이벤트(발 착지/이탈)** → 보행 위상 추정 → 위상 동기화 보조.
> Stage 3 이상에서는 이 신호를 AI 추론(System 2)으로 고도화합니다.

---

## 학습 목표

- **보행 이벤트(Gait Event)** — Heel Strike(HS)와 Toe Off(TO) — 을 센서 신호에서 추출하는 방법을 학습합니다.
- **보행 위상 추정기(Phase Estimator)**: `phase += dt / T_period` 를 이해합니다.
- **위상 동기화 보조 토크** 생성 원리를 학습합니다:
  - PUSH(입각기 보조): `τ = A · sin(phase · π)`
  - PULL(유각기 보조): `τ = A · sin((phase - 0.5) · 2π)` [phase > 0.5]
- H10 Body Data 의존성과 설정 방법을 이해합니다.

---

## 동작 원리

### 신호 흐름

```
발 접촉 센서 (isRightFootContact / isLeftFootContact)
   ↓ 상태 전이 감지
보행 이벤트 추출 (Heel Strike / Toe Off)
   ↓ HS-to-HS 시간 측정
보행 주기 추정 (T_period, 클램핑: 0.4 ~ 3.0초)
   ↓ phase += dt / T_period
보행 위상 추정 (0.0 = HS, 1.0 = 다음 HS 직전)
   ↓ 위상 기반 프로파일
보조 토크 생성 (sin 반파 형태)
```

### 보행 위상 추정기

```c
// Heel Strike 감지: false → true 전이
if (!was_contact && is_contact) {
    period_s = elapsed_ms * 0.001f;  // HS-to-HS 시간 갱신
    phase = 0.0f;                    // 위상 리셋
}

// 매 1ms 위상 증분
phase += CONTROL_DT / period_s;
if (phase >= 1.0f) phase -= 1.0f;   // 래핑
```

### 상태 전이 (TSM)

`OFF → STANDBY → ACTIVE(보행 의도 감지 + 보조)`

### 버튼 조작

| 버튼 | 동작 |
|------|------|
| BTN1 클릭 | 토크 진폭 순환 (0.5 → 1.0 → 2.0 → 3.0 Nm) |
| BTN2 클릭 | 보조 모드 전환 (PUSH ↔ PULL) |
| BTN3 클릭 | 보행 위상 추정기 리셋 |

### USB 스트리밍 (Module ID 0xF2)

| 채널 | 이름 | 단위 | 설명 |
|------|------|------|------|
| 0 | Phase RH | - | 우측 보행 위상 (0.0 ~ 1.0) |
| 1 | Phase LH | - | 좌측 보행 위상 (0.0 ~ 1.0) |
| 2 | Torque RH | Nm | 우측 보조 토크 |
| 3 | Torque LH | Nm | 좌측 보조 토크 |

---

## 실행 방법

1. **H10 설정 확인 필수**: H10 소프트웨어에서 **Body Data 전송 활성화**. 비활성화 시 `isRightFootContact`/`isLeftFootContact`가 항상 `false`로 수신되어 보행 의도 감지가 불가능합니다.
2. `grf_gait_intent.c`를 `user_app.c`로 복사 후 빌드하여 XM10에 플래시합니다.
3. H10 전원 ON → ASSIST MODE 전환.
4. USB CDC 터미널에서 `[GRF] ACTIVE 진입` 메시지 확인.
5. 걷기 시작 → Heel Strike 이벤트 감지 후 보행 위상 추정 시작.
6. PhAI Studio에서 Module ID `0xF2` 채널로 위상 곡선과 토크 파형을 실시간 확인합니다.

---

## 직접 해보기

- **PUSH vs PULL 비교**: BTN2로 모드를 전환하며 체감하는 보조 패턴의 차이를 느껴보세요.
  PUSH: 입각기(발이 땅에 닿는 구간) 보조 / PULL: 유각기(발을 드는 구간) 보조.
- **위상 시각화**: PhAI Studio에서 Phase RH/LH가 규칙적인 톱니파(sawtooth)를 그리는지 확인하세요. 불규칙하면 Body Data 설정을 점검하세요.
- **토크 진폭 실험**: BTN1로 0.5Nm에서 시작하여 착용자 피드백에 따라 점진적으로 증가합니다.
- **Stage 3 연결**: 이 예제의 위상 추정기를 Ex.23(Gait Phase Adaptive Torque)과 비교하세요. Stage 3에서는 위상 추정 결과를 적응 학습에 활용합니다.

---

## 주의사항

> **Body Data 의존성**: `isRightFootContact`/`isLeftFootContact`는 H10 Body Data 패킷에서 수신됩니다.
> H10 소프트웨어 설정에서 Body Data 전송이 활성화되어 있어야 합니다.
> 비활성화 시 Heel Strike 이벤트가 발생하지 않고 보조 토크가 출력되지 않습니다.

> **보행 위상 초기화**: 첫 Heel Strike 전까지는 보조 토크를 인가하지 않습니다.
> 걷다 멈추거나 추정기가 올바르지 않을 때는 BTN3로 리셋하세요.
