# 예제 33: 운동감각 교시 + 재생 (Kinesthetic Teaching — Stage 5)

본 예제는 **운동감각 교시(Kinesthetic Teaching, KT)**를 구현합니다.
전문가가 로봇을 직접 손으로 이끌어 동작을 교시하고, 로봇이 그 궤적을 그대로 재생합니다.
이것이 **장인 스킬 데이터 캡처(Expert Skill Capture)**의 핵심 원시(primitive)이며,
Physical AI 데이터 파이프라인의 출발점입니다.

> 📖 API 레퍼런스: [H10 Control & Data](../../docs/api-reference/02-h10-control-n-data.md) · [Task State Machine](../../docs/api-reference/01-task-state-machine.md)
>
> 📄 전제 예제: [Ex.21 중력+마찰 보상](../21_Gravity_Compensation/) · [Ex.31 DOB 투명 모드](../31_Friction_Comp_DOB/) · [Ex.10b SD카드 로깅](../10b_MSC_Custom_Struct/)

---

## Physical AI 5단계 학습 여정에서의 위치

```
Stage 1    Physical Transparency  ← Ex.31 (로봇을 투명하게 — 교시의 선행 조건)
Stage 2    Intent Sensing         ← Ex.32 (인간 의도 감지)
Stage 3    Adaptive Assistance    ← Ex.12, 14, 23, 25, 28
Stage 4    Machine Learning       ← Ex.15, 24, 26, 27
Stage 5 ★  Shared Autonomy        ← 지금 여기 (장인 스킬 캡처 → AI 학습 데이터)
```

> **Stage 5 핵심 인사이트**: Polanyi(1966)의 "암묵지(Tacit Knowledge)" — 전문가가
> 말로 표현하기 어려운 장인의 솜씨는, 로봇을 착용하고 직접 시연함으로써 데이터로 포착됩니다.
> 이 데이터가 π0 스타일 VLA(Vision-Language-Action) 모델의 학습 입력이 됩니다.

---

## 제어 모드 이해 (초급 가이드)

> 이 섹션은 "왜 운동감각 교시가 Physical AI의 핵심인가"를 설명합니다.

### 로봇 학습의 세 가지 방법

```
방법 1: 수작업 프로그래밍
  if (gait_phase > 0.3) torque = 2.0;
  → 개발자가 모든 조건을 코드로 명시
  → 한계: 전문가의 직관을 코드로 표현 불가

방법 2: 강화학습 (RL)
  시뮬레이션에서 수천 시간 trial-and-error
  → 한계: 외골격 시뮬레이션 부정확, 인체 안전

방법 3: 시연 학습 (Learning from Demonstration) ← 이 예제
  전문가가 직접 시연 → 데이터 수집 → AI 모델 학습
  → 장점: 전문가 지식을 빠르게 캡처, 안전
  → 핵심 전제 조건: 로봇이 투명(Stage 1)해야 함
```

### 왜 교시 중 "투명 모드"가 필수인가?

```
투명 모드 없음: 착용자가 로봇 무게 + 마찰을 이기며 움직임
  → 데이터에 로봇 역학이 섞임 → 데이터 품질 저하
  → 전문가가 피로 → 교시 지속 불가

투명 모드 있음 (이 예제): 로봇이 "보이지 않는 존재"
  → 인간의 순수한 동작 의도가 데이터로 기록됨
  → 데이터 품질 = 인간 전문가의 실력
  → π0 VLA 학습 시 더 깨끗한 행동 레이블
```

### PD 재생 제어가 하는 일

교시된 궤적을 재생할 때 PD 제어기는 "저장된 위치로 쫓아가는 스프링"처럼 동작합니다:

```
τ = Kp · (θ_target - θ_actual) + Kd · Δerror/dt
       ↑ 위치 오차에 비례       ↑ 오차 변화율에 비례
       (스프링)               (댐퍼)

목표: θ_actual → θ_target  (기록된 궤적 추적)
```

이것은 토크 명령으로 구현된 **위치 추적 제어**입니다.
H10의 MD는 이 토크 명령을 전류로 변환하여 모터를 구동합니다.

---

## 학습 목표

- **운동감각 교시(KT)의 원리**를 이해합니다:
  투명 모드 → 인간이 컨트롤러 → 궤적 기록 → AI 학습 데이터
- **Physical AI 데이터 파이프라인**의 첫 단계를 직접 구현합니다.
- **PD 위치 추적**으로 기록된 궤적을 재연하는 방법을 학습합니다.
- 교시 데이터를 PhAI Studio 및 π0 VLA 모델 학습에 연결하는 워크플로우를 이해합니다.

---

## 동작 원리

### 교시 사이클

```
[IDLE]
  BTN1 ↓
[RECORD — 투명 모드 + 100Hz 궤적 기록]
  → 로봇이 중력·마찰을 상쇄하여 "보이지 않는 존재"가 됨
  → 전문가가 자유롭게 동작을 표현 (Human IS the Controller)
  → 100Hz(10ms)로 좌/우 고관절 각도 기록 (최대 2000포인트 = 20초)
  BTN1 or 버퍼 만료 ↓
[RECORDED — 재생 가능 상태]
  BTN2 ↓
[REPLAY — PD 위치 추적으로 궤적 재생]
  → τ = Kp·(θ_target - θ_actual) + Kd·Δerror/dt
  → 기록과 동일한 10ms 주기로 인덱스 전진 (같은 속도로 재생)
  재생 완료 ↓
[RECORDED 복귀 (BTN2로 반복 재생 가능)]

  BTN3: 어떤 상태에서도 IDLE로 리셋 (비상 정지)
```

### 버튼 조작

| 버튼 | 동작 |
|------|------|
| BTN1 클릭 (IDLE) | 교시 시작 — 투명 모드 활성화, 100Hz 기록 시작 |
| BTN1 클릭 (RECORD) | 교시 중단 — 버퍼 보존, RECORDED 상태 전환 |
| BTN2 클릭 (RECORDED) | 재생 시작 — PD 추적으로 기록된 궤적 재연 |
| BTN3 클릭 (언제든) | 완전 리셋 — 버퍼 클리어, IDLE 복귀 (비상 정지) |

### LED 표시

| 상태 | LED1 | LED2 | LED3 |
|------|------|------|------|
| IDLE | 500ms 깜빡임 | 소등 | 소등 |
| RECORD | 100ms 빠른 깜빡임 | 점등 | 소등 |
| RECORDED | 상시 점등 | 소등 | 소등 |
| REPLAY | 200ms 깜빡임 | 소등 | 점등 |

### USB 스트리밍 (Module ID 0xF3)

| 채널 | 이름 | 단위 | 설명 |
|------|------|------|------|
| 0 | Teach Mode | - | 상태 (0=IDLE, 1=RECORD, 2=RECORDED, 3=REPLAY) |
| 1 | Teach Count | pts | 기록된 포인트 수 |
| 2 | Theta Target | deg | 재생 목표 각도 (우측, REPLAY 중) |
| 3 | Theta Actual | deg | 현재 실제 각도 (우측) |

---

## 제어공학적 제약사항과 튜닝 가이드

### ① PD 게인 분석: Kp = 1.5 Nm/deg

이 예제의 `KP_REPLAY = 1.5 Nm/deg`는 **각도 단위가 도(degree)**임을 주의하세요.

```
단위 변환: 1.5 Nm/deg × (180/π deg/rad) = 85.9 Nm/rad

비교 — 임피던스 제어(Ex.20) 강성 범위:
  낮음:  0.1 Nm/deg = 5.7 Nm/rad  (매우 부드러운 스프링)
  중간:  0.3 Nm/deg = 17.2 Nm/rad
  높음:  0.8 Nm/deg = 45.8 Nm/rad
  재생:  1.5 Nm/deg = 85.9 Nm/rad (단단한 위치 추적)
```

1.5 Nm/deg는 재생 추적에 필요한 높은 강성이지만, `REPLAY_TORQUE_MAX = 8.0 Nm` 포화로 인해 오차 5.3°를 초과하면 포화됩니다. 즉 **작은 오차에서는 강한 스프링, 큰 오차에서는 정전류 제한**.

### ② 감쇠 특성 분석: Kd = 0.05 Nm·s/deg

```
Kd = 0.05 Nm·s/deg × (180/π) = 2.86 Nm·s/rad

고관절 유효 관성 (외골격 포함): J_eff ≈ 1.0~2.0 kg·m² (추정)

임계 감쇠 조건:
  Kd_critical = 2 × √(Kp × J_eff)
              = 2 × √(85.9 × 1.5) ≈ 22.7 Nm·s/rad

실제 Kd: 2.86 Nm·s/rad << 임계감쇠 22.7 Nm·s/rad
→ 시스템은 저감쇠(underdamped) 상태
→ 재생 시 진동·오버슈트 가능
```

**진동이 발생할 때 대처 방법**:
- Kd를 0.1~0.3까지 증가 (진동 억제)
- Kp를 낮추면 추적 오차는 커지지만 더 안전 (0.5~1.0 Nm/deg 추천 시작값)
- feedforward(중력 보상) 추가로 정상 상태 오차 감소

### ③ 교시 투명성 품질 vs 데이터 품질

교시 중 투명 모드의 잔류 토크가 클수록 전문가의 동작이 왜곡됩니다:

```
투명 모드 품질 비교:
  Ex.21 (공칭 모델): 잔류 오차 ~0.5~2 Nm (마찰, 모델 오차 잔존)
  Ex.31 (DOB):       잔류 오차 ~0.1~0.5 Nm (DOB가 대부분 보상)

→ 최고 품질 데이터: Ex.33의 투명 모드 부분을 Ex.31 DOB로 교체
  _ComputeTransparentTorque() → Ex.31의 _RunDobCompensation() 호출
```

현재 예제는 Ex.21 수준의 투명성을 사용합니다. DOB 적용 시 더 자연스러운 교시 가능.

### ④ 재생 속도와 안전

- **재생 속도**: 기록된 궤적을 10ms마다 1포인트 전진 → 교시 속도와 동일하게 재생.
  빠른 교시 동작(예: 빠른 보행) 재생 시 갑작스러운 관절 이동이 발생할 수 있습니다.
- **속도 제한 없음**: 현재 코드에는 각속도 제한이 없습니다. 개선 방향: PD 이전에 목표 위치 변화율 제한.

```c
// 안전 개선 예시 (현재 미적용)
float delta = target - prev_target;
delta = CLAMP(delta, -MAX_DELTA_DEG, MAX_DELTA_DEG);  // 최대 이동 제한
target = prev_target + delta;
```

### ⑤ 메모리와 기록 주파수 트레이드오프

| 기록 설정 | 메모리 | 최대 시간 | 해상도 | 적합한 용도 |
|----------|--------|----------|--------|------------|
| 현재: 100Hz, 2000pt | 16KB | 20초 | 10ms | 보행 교시 |
| 고급: 200Hz, 4000pt | 32KB | 20초 | 5ms | 빠른 동작 |
| PSRAM: 100Hz, 30000pt | 240KB | 300초 | 10ms | 장시간 교시 |
| PSRAM: 1kHz, 60000pt | 480KB | 60초 | 1ms | 고정밀 교시 |

더 큰 버퍼는 PSRAM(8MB) 활용. Ex.10b의 PSRAM 패턴 참조.

---

## 실행 방법

1. `kinesthetic_teaching.c`를 `user_app.c`로 복사 후 빌드하여 XM10에 플래시합니다.
2. H10 전원 ON → ASSIST MODE 전환.
3. USB CDC 터미널에서 `[KT] 운동감각 교시 시스템 준비 완료` 메시지 확인.
4. **BTN1** — 교시 시작. LED2 점등과 함께 투명 모드 활성화.
5. 로봇 외골격을 자유롭게 움직여 동작을 교시합니다. (Human IS the Controller)
6. **BTN1** — 교시 완료. `[KT] N포인트 기록됨` 메시지 확인.
7. **BTN2** — 재생 시작. LED3 점등과 함께 PD 추적 재생.
8. PhAI Studio에서 Module ID `0xF3`으로 `Theta Target` vs `Theta Actual`의 추적 오차를 확인합니다.

---

## Physical AI 데이터 파이프라인 연동

```
[이 예제] 교시 → RAM 버퍼 (16KB, 최대 20초)
      ↓ XM_SetUsbLogSource (Ex.10b 참조)
[SD카드] Binary 저장 (TeachPoint_t 구조체, 8bytes × 2000 = 16KB)
      ↓ USB Mass Storage 전송
[PhAI Studio] 데이터 라벨링 · 품질 검증 · 시각화
      ↓ Cloud GPU 전송
[AI 학습] π0 스타일 VLA 모델 (State: 각도+토크, Action: 다음 각도)
      ↓ 학습된 정책 배포
[로봇] 학습된 전문가 수준의 동작 자율 실행 또는 인간 보조
```

---

## 직접 해보기

- **투명성 품질 비교**: 투명 모드 없이 교시 → 로봇 무게 저항 느낌. 투명 모드로 교시 → 차이를 직접 체감하세요.
- **재생 PD 게인 튜닝**: `KP_REPLAY = 0.5 Nm/deg`부터 시작하여 1.0, 1.5로 늘리며 추적 오차와 진동을 PhAI Studio에서 관찰하세요.
- **SD카드 저장 연동**: Ex.10b의 `XM_SetUsbLogSource` 패턴을 참고하여 교시 데이터를 저장하세요. `TeachPoint_t` 구조체가 그대로 바이너리로 기록됩니다.
- **DOB 교시 업그레이드**: `_ComputeTransparentTorque()`를 Ex.31의 DOB 로직으로 교체하면 잔류 마찰이 더 줄어 데이터 품질이 향상됩니다.

---

## 핵심 레퍼런스

제어공학 배경 없이도 이해할 수 있는 순서로 정렬했습니다.

**[입문] 시연을 통한 로봇 학습 개요**
- **Argall, B. D., Chernova, S., Veloso, M., & Browning, B. (2009).** "A survey of robot learning from demonstration." *Robotics and Autonomous Systems, 57*(5), 469–483. (~3500 citations)
  → Learning from Demonstration의 종합 입문 논문. 운동감각 교시가 왜 가장 자연스러운 방법인지 설명. **가장 먼저 읽을 논문**.

**[핵심] 현대 시연 학습 방법론**
- **Ravichandar, H., Polydoros, A. S., Chernova, S., & Billard, A. (2020).** "Recent advances in robot learning from demonstration." *Annual Review of Control, Robotics, and Autonomous Systems, 3*, 297–330. (~500 citations)
  → 2020년까지의 LfD 방법론 총정리. 운동감각 교시 → AI 모델 학습의 전체 파이프라인 포함.
- **Calinon, S. (2016).** "A tutorial on task-parameterized movement learning and retrieval." *Intelligent Service Robotics, 9*(1), 1–29. (~800 citations)
  → 교시 데이터에서 움직임 패턴을 추출하는 가우시안 혼합 모델(GMM) 기반 방법. 이 예제의 PD 재생을 넘는 다음 단계.

**[Physical AI 연결] π0와 Diffusion Policy**
- **Black, K., Brown, N., Driess, D., Esmail, A., et al. (2024).** "π0: A Vision-Language-Action Flow Model for General Robot Control." *arXiv:2410.24164*.
  → 이 예제가 수집하는 데이터의 **최종 소비자** 논문. VLA 모델 구조와 Flow Matching 학습 방식.
- **Chi, C., Xu, Z., Feng, S., Cousineau, E., Du, Y., et al. (2023).** "Diffusion policy: Visuomotor policy learning via action diffusion." *Robotics: Science and Systems (RSS) 2023*. (~1000 citations)
  → π0의 전신 개념. 교시 데이터로 확산 모델을 학습하는 방법. 이 예제의 궤적 데이터 활용 방향.

**[암묵지 이론 배경]**
- **Polanyi, M. (1966).** *The Tacit Dimension.* Doubleday & Company.
  → "장인은 자기가 아는 것보다 더 많은 것을 할 수 있다." 운동감각 교시의 철학적 배경.
- **Billard, A., Calinon, S., Dillmann, R., & Schaal, S. (2008).** "Robot programming by demonstration." *Springer Handbook of Robotics* (pp. 1371–1394). (~2000 citations)
  → 시연 학습의 체계적 프레임워크. 운동감각 교시의 분류와 구현 방법론.

---

## 주의사항

> **메모리**: `s_teach_buf[2000]` = 16KB SRAM 정적 할당. 더 큰 버퍼가 필요하면 XM10의 PSRAM(8MB)을 활용하세요.

> **재생 안전**: 재생 중 외골격이 예상치 못한 방향으로 움직일 수 있습니다. BTN3는 언제든 비상 정지로 사용 가능합니다. 처음 재생 시 `KP_REPLAY = 0.5 Nm/deg` (낮은 게인)부터 시작하세요.

> **진동 발생 시**: `KP_REPLAY`를 낮추거나 `KD_REPLAY`를 높이세요. 현재 기본 게인은 저감쇠 상태로 일부 진동 가능성이 있습니다.

> **교시 품질 ↔ 데이터 품질**: 투명 모드가 완벽할수록 수집되는 AI 학습 데이터의 품질이 높아집니다. Ex.31 DOB 적용을 권장합니다.
