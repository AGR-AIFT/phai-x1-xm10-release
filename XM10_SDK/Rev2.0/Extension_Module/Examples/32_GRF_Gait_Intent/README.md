# 예제 32: GRF 기반 보행 의도 감지 (Stage 2 — Intent Sensing)

본 예제는 **발 접촉 센서(GRF — Ground Reaction Force)** 이벤트를 이용하여
보행 위상(Gait Phase)을 실시간 추정하고, 이에 동기화된 **보조 토크**를 생성합니다.
원시(raw) 센서 신호가 어떻게 "**인간 의도(intent)**"로 변환되는지를 보여주는 Stage 2의 핵심 예제입니다.

> 📖 API 레퍼런스: [H10 Control & Data](../../docs/api-reference/02-h10-control-n-data.md)
>
> 📄 전제 예제: [Ex.31 DOB 투명 모드](../31_Friction_Comp_DOB/) (Stage 1 완성)

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

## 제어 모드 이해 (초급 가이드)

> 이 섹션은 "의도 감지"가 제어공학에서 왜 중요한지 설명합니다.

### 보조 로봇의 딜레마: "언제 도울 것인가?"

외골격이 사람을 도울 때 가장 어려운 문제는 **타이밍**입니다.

```
너무 이르게 보조: 사람이 원하지 않는 방향으로 밀어 → 불편·위험
너무 늦게 보조: 이미 근육이 힘을 다 썼을 때 도달 → 효과 없음
딱 맞게 보조: 근육이 힘을 낼 직전 → 에너지 절감 최대화
```

이 예제는 **보행 위상(gait phase)**을 추정하여 "지금이 몇 % 보행 진행 중인지"를 알고,
특정 위상에서 정확한 타이밍에 보조 토크를 인가합니다.

### PhAI X1에서 접근 가능한 GRF 신호 두 종류

```
신호 1: XM.status.h10.isRightFootContact  (이 예제 사용)
  → H10 Body Data 패킷에 포함된 바이너리 접촉 신호 (0 or 1)
  → 장점: 추가 HW 불필요   단점: 힘의 크기 정보 없음

신호 2: XM.status.grf.rightSensorData[14] (고급 확장)
  → 별도 GRF 신발 센서(MarvelDex FSR)에서 수신하는 14채널 아날로그 족압
  → 0~255 raw value, 발바닥 각 영역의 실제 압력 분포
  → 장점: 연속 신호, 힘의 크기·분포 파악 가능
  → Stage 2 고도화에 활용 가능
```

이 예제는 신호 1(바이너리)로 보행 위상 개념을 학습합니다.
신호 2(아날로그 14채널)는 더 정밀한 의도 감지의 출발점입니다.

### 왜 sin 프로파일인가?

보행 보조 토크는 **계단 함수(on/off)보다 sin 반파**가 왜 더 좋은가?

```
계단 함수:  0 → 2Nm (갑자기)  → 0 (갑자기)  → 충격, 불편
sin 반파:   0 → 서서히 → 최대 → 서서히 → 0    → 부드러움
```

```
τ = A · sin(phase · π)
  → phase=0 (발뒤꿈치 착지): τ=0 (충격 없이 시작)
  → phase=0.5 (입각기 중간): τ=A (최대 보조 — 지면 반발력 최대 구간)
  → phase=1.0 (다음 착지 직전): τ=0 (자연스럽게 종료)
```

Quinlivan 2017(Science Robotics)에서 이 방식으로 보행 에너지를 23% 절감함을 실증.

---

## 학습 목표

- **보행 이벤트(Gait Event)** — Heel Strike(HS)와 Toe Off(TO) — 을 센서 신호에서 추출하는 방법을 학습합니다.
- **보행 위상 추정기(Phase Estimator)**: `phase += dt / T_period` 의 동작 원리를 이해합니다.
- **위상 동기화 보조 토크** 생성 원리를 학습합니다.
- H10 Body Data 의존성과 설정 방법을 이해합니다.
- `XM.status.grf` (아날로그 GRF)로의 고도화 방향을 이해합니다.

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
   ↓ 위상 기반 sin 프로파일
보조 토크 생성 (PUSH: 입각기 / PULL: 유각기)
```

### 보행 위상 추정기

```c
// Heel Strike 감지: false → true 전이
if (!was_contact && is_contact) {
    period_s = elapsed_ms * 0.001f;  // HS-to-HS 시간 갱신
    phase = 0.0f;                    // 위상 리셋
}

// 매 1ms 위상 증분 (선형 보간)
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

## 제어공학적 제약사항과 튜닝 가이드

### ① isFootContact = 바이너리, 힘의 크기 정보 없음

이 예제의 `isRightFootContact`는 **0 또는 1**입니다. 실제 GRF는 다음과 같습니다:

```
진짜 발뒤꿈치 착지 시 GRF 곡선:
    │      ┌──────┐
    │   ┌──┘      └──┐
    │___┘             └___
     HS              TO
```

바이너리 신호는 착지 순간과 이탈 순간만 포착합니다. 착지 강도, 발바닥 중심 이동 등은 `XM.status.grf`의 아날로그 신호로만 파악 가능합니다.

**고도화 방향**: `grf.rightSensorData[0~13]`의 14채널 족압을 활용하면 연속 GRF 신호로 더 정밀한 위상 추정이 가능합니다.

### ② Body Data 통신 지연

```
H10 CM 내부에서 보행 분석 → DOP V1 PDO로 XM10 전송 (~1ms 주기)
```

이 지연으로 인해 실제 Heel Strike 발생과 XM10이 감지하는 시점 사이에 **1~2ms 지연**이 있습니다. 보행 속도가 빠를수록 위상 오차로 이어집니다.

### ③ 보행 위상 추정기의 가정과 한계

이 추정기는 다음 가정에 의존합니다:

| 가정 | 실제 상황 | 영향 |
|------|----------|------|
| 일정한 보행 속도 | 가속/감속 구간 존재 | 위상 오차 누적 |
| 좌우 대칭 보행 | 편마비, 절뚝거림 | 비대칭 위상 추정 |
| 연속 보행 | 멈춤, 방향 전환 | 위상 발산 → BTN3 리셋 필요 |
| 주기 0.4~3.0s 범위 | 달리기, 매우 느린 보행 | 클램핑으로 방어됨 |

보행이 불규칙하거나 멈출 때는 반드시 BTN3로 추정기를 리셋하세요.

### ④ 보조 토크 진폭 선택 기준

| 진폭 | 적합한 대상 | 기준 |
|------|-----------|------|
| 0.5 Nm | 처음 사용자, 감각 확인 | 착용자가 보조를 거의 느끼지 못하는 수준 |
| 1.0 Nm | 기본 보행 보조 | 일반 성인 보행 최소 유효 보조량 |
| 2.0 Nm | 노약자, 재활 환자 | 임상 연구에서 사용되는 일반적 범위 |
| 3.0 Nm | 높은 보조 요구 | Quinlivan 2017에서 최대 효과 구간 |

### ⑤ 디버그 확인 포인트

PhAI Studio에서 다음을 확인하세요:

```
정상: Phase RH/LH가 규칙적인 톱니파 (0→1→0→1...)
문제1: Phase가 변하지 않음 → H10 Body Data 전송 비활성화
문제2: Phase가 갑자기 리셋 없이 계속 증가 → is_contact 항상 false
문제3: Phase가 아주 빠르게 진동 → 노이즈에 의한 잘못된 HS 감지
```

---

## 실행 방법

1. **H10 설정 확인 필수**: H10 소프트웨어에서 **Body Data 전송 활성화**. 비활성화 시 `isRightFootContact`/`isLeftFootContact`가 항상 `false`.
2. `grf_gait_intent.c`를 `user_app.c`로 복사 후 빌드하여 XM10에 플래시합니다.
3. H10 전원 ON → ASSIST MODE 전환.
4. USB CDC 터미널에서 `[GRF] ACTIVE 진입` 메시지 확인.
5. 걷기 시작 → Heel Strike 이벤트 감지 후 보행 위상 추정 시작.
6. PhAI Studio에서 Module ID `0xF2` 채널로 위상 곡선과 토크 파형을 실시간 확인합니다.

---

## 직접 해보기

- **PUSH vs PULL 비교**: BTN2로 모드를 전환하며 체감하는 보조 패턴의 차이를 느껴보세요.
- **위상 시각화**: PhAI Studio에서 Phase RH/LH가 규칙적인 톱니파를 그리는지 확인하세요.
- **토크 진폭 실험**: BTN1로 0.5Nm에서 시작하여 착용자 피드백에 따라 점진적으로 증가합니다.
- **GRF 고도화 실험**: `XM.status.grf.rightSensorData`의 14채널 값을 USB에 출력하여 아날로그 족압 분포를 관찰해보세요. 연속 GRF 신호로 위상 추정기를 개선할 수 있습니다.

---

## 핵심 레퍼런스

제어공학 배경 없이도 이해할 수 있는 순서로 정렬했습니다.

**[입문] 보행의 생체역학 기초**
- **Winter, D. A. (2009).** *Biomechanics and Motor Control of Human Movement* (4th ed.). Wiley. (~5000 citations)
  → 보행 위상(Gait Phase), Heel Strike, Toe Off의 정의 기준. 입문자 필독.
- **Ferris, D. P., Sawicki, G. S., & Daley, M. A. (2007).** "A physiologist's perspective on robotic exoskeletons for human locomotion." *The International Journal of HR, 4*(3), 507–528. (~400 citations)
  → 외골격 보조가 인체 근육에 미치는 영향. "왜 이 타이밍인가"의 생리학적 근거.

**[핵심] 보행 위상 동기화 보조**
- **Quinlivan, B. T. et al. (2017).** "Assistance magnitude versus metabolic cost reductions for a tethered multiarticular soft exosuit." *Science Robotics, 2*(2), eaah4416. (~600 citations)
  → **Ex.32의 sin 프로파일 보조의 직접 근거 논문.** 보행 위상 동기화로 23% 에너지 절감 실증.
- **Kim, J. et al. (2019).** "Reducing the metabolic rate of walking and running with a versatile, portable exoskeleton." *Science, 365*(6454), 668–672. (~600 citations)
  → 실제 포터블 외골격으로 보행 에너지 절감 실증. 이 예제가 지향하는 최종 목표.

**[심화] 보행 의도 감지와 제어 전략**
- **Tucker, M. R. et al. (2015).** "Control strategies for active lower extremity prosthetics and orthotics: a review." *Journal of NeuroEngineering and Rehabilitation, 12*(1), 1. (~700 citations)
  → 하지 외골격·의지 제어 전략의 종합 리뷰. 위상 감지 방법론 비교 포함. **가장 포괄적인 리뷰**.
- **Gervasi, A. et al. (2020).** "Exoskeleton gait assistance based on continuous gait phase estimation." *IROS 2020*.
  → 이 예제와 가장 유사한 연속 보행 위상 추정 알고리즘.

**[확장] 심층 학습 기반 의도 감지 (Stage 2 → Stage 4 연결)**
- **Molinaro, D. D. et al. (2020).** "Biological-hip torque estimation using a robotic hip exoskeleton." *IROS 2020*.
  → 이 예제의 GRF 이벤트 감지를 딥러닝 토크 추정으로 고도화하는 방향.

---

## 주의사항

> **Body Data 의존성**: `isRightFootContact`/`isLeftFootContact`는 H10 Body Data 패킷에서 수신됩니다.
> H10 소프트웨어 설정에서 Body Data 전송이 활성화되어 있어야 합니다.
> 비활성화 시 Heel Strike 이벤트가 발생하지 않고 보조 토크가 전혀 출력되지 않습니다.

> **보행 위상 초기화**: 첫 Heel Strike 전까지는 보조 토크를 인가하지 않습니다.
> 걷다 멈추거나 추정기가 올바르지 않을 때는 BTN3로 리셋하세요.

> **바이너리 한계**: `isFootContact`는 있다/없다의 2값입니다. 실제 지면 반발력의 크기나 분포는 `XM.status.grf` 모듈(별도 연결 필요)로만 파악 가능합니다.
