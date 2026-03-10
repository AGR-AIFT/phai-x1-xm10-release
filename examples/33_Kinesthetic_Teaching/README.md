# 예제 33: 운동감각 교시 + 재생 (Kinesthetic Teaching — Stage 5)

본 예제는 **운동감각 교시(Kinesthetic Teaching, KT)**를 구현합니다.
전문가가 로봇을 직접 손으로 이끌어 동작을 교시하고, 로봇이 그 궤적을 그대로 재생합니다.
이것이 **장인 스킬 데이터 캡처(Expert Skill Capture)**의 핵심 원시(primitive)이며,
Physical AI 데이터 파이프라인의 출발점입니다.

> 📖 API 레퍼런스: [H10 Control & Data](../../docs/api-reference/02-h10-control-n-data.md) · [Task State Machine](../../docs/api-reference/01-task-state-machine.md)
>
> 📄 전제 예제: [Ex.21 중력+마찰 보상](../21_Gravity_Compensation/) (교시 중 투명 모드) · [Ex.10b SD카드 로깅](../10b_MSC_Custom_Struct/) (데이터 저장)
>
> 📄 논문 레퍼런스: Billard, A. et al. (2008). *Robot Programming by Demonstration.* Springer Handbook of Robotics, 1371–1394. · Chi, C. et al. (2023). *Diffusion Policy: Visuomotor Policy Learning via Action Diffusion.* RSS 2023.

---

## Physical AI 5단계 학습 여정에서의 위치

```
Stage 1    Physical Transparency  ← Ex.31 (로봇을 투명하게)
Stage 2    Intent Sensing         ← Ex.32 (인간 의도 감지)
Stage 3    Adaptive Assistance    ← Ex.12, 14, 23, 25, 28
Stage 4    Machine Learning       ← Ex.15, 24, 26, 27
Stage 5 ★  Shared Autonomy        ← 지금 여기 (장인 스킬 캡처 → AI 학습 데이터)
```

> **Stage 5 핵심 인사이트**: Polanyi(1966)의 "암묵지(Tacit Knowledge)" — 전문가가
> 말로 표현하기 어려운 장인의 솜씨는, 로봇을 착용하고 직접 시연함으로써 데이터로 포착됩니다.
> 이 데이터가 π0 스타일 VLA(Vision-Language-Action) 모델의 학습 입력이 됩니다.

---

## 학습 목표

- **운동감각 교시(KT)의 원리**를 이해합니다:
  투명 모드(중력+마찰 보상) → 인간이 컨트롤러 → 궤적 기록 → AI 학습 데이터
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

  BTN3: 어떤 상태에서도 IDLE로 리셋
```

### 버튼 조작

| 버튼 | 동작 |
|------|------|
| BTN1 클릭 (IDLE) | 교시 시작 — 투명 모드 활성화, 100Hz 기록 시작 |
| BTN1 클릭 (RECORD) | 교시 중단 — 버퍼 보존, RECORDED 상태 전환 |
| BTN2 클릭 (RECORDED) | 재생 시작 — PD 추적으로 기록된 궤적 재연 |
| BTN3 클릭 (언제든) | 완전 리셋 — 버퍼 클리어, IDLE 복귀 |

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

수집된 궤적 데이터를 AI 학습에 활용하는 단계별 방법:

```
[이 예제] 교시 → 메모리 버퍼
      ↓ XM_SetUsbLogSource (Ex.10b 참조)
[SD카드] CSV/Binary 저장
      ↓ PhAI Studio 연동
[PhAI Studio] 데이터 라벨링 · 품질 검증
      ↓ Cloud GPU
[AI 학습] π0 스타일 VLA(Vision-Language-Action) 모델
      ↓ 모델 배포
[로봇] 전문가 수준의 동작 재현 또는 인간 보조
```

---

## 직접 해보기

- **교시 정밀도 비교**: 투명 모드 없이(중력 보상 OFF 상태에서) 교시와 비교하면 어떻게 다른가요? 투명 모드가 교시 정확도에 미치는 영향을 체험하세요.
- **재생 PD 게인 튜닝**: `KP_REPLAY`(1.5 Nm/deg)와 `KD_REPLAY`(0.05)를 조정하며 추적 오차를 관찰하세요. PhAI Studio의 `Theta Target` vs `Theta Actual` 채널로 확인합니다.
- **SD카드 저장 연동**: Ex.10b의 `XM_SetUsbLogSource` 패턴을 참고하여 교시 데이터를 SD카드에 저장하세요.
- **Stage 1 연결**: `_ComputeTransparentTorque()`를 Ex.31의 DOB 버전으로 교체하면 더 정밀한 투명 모드 기반 교시가 가능합니다.

---

## 주의사항

> **메모리**: `s_teach_buf[2000]` = 16KB SRAM 정적 할당. 더 큰 버퍼가 필요하면 XM10의 PSRAM(8MB)을 활용하세요 (Ex.10b PSRAM 패턴 참조).

> **재생 안전**: 재생 중 외골격이 예상치 못한 방향으로 움직일 수 있습니다. BTN3는 언제든 비상 정지로 사용 가능합니다. 처음 재생 시 낮은 KP_REPLAY 게인으로 시작하세요.

> **교시 시작 전**: H10이 ASSIST MODE에 있고 착용자가 안전한 자세인지 확인하세요. 투명 모드에서 중력만 상쇄되므로 갑작스러운 움직임에 주의합니다.
