# 예제 24: 가상 구속/HZD 제어 (Virtual Constraint Control)

본 예제는 **가상 구속(Virtual Constraint)** 기법을 구현합니다. 보행 위상 `s`를 독립 변수로 하는 **5차 Bézier 다항식**으로 고관절 목표 궤도를 생성하여 착용자가 자연스러운 보행 패턴을 따르도록 유도합니다.

> 📖 API 레퍼런스: [H10 Control & Data](../../docs/api-reference/02-h10-control-n-data.md) · [Task State Machine](../../docs/api-reference/01-task-state-machine.md)
>
> 📄 논문 레퍼런스: Westervelt, E. R., et al. (2003). *Hybrid zero dynamics of planar biped walkers.* IEEE Transactions on Automatic Control, 48(1), 42–56.

## 🎯 학습 목표 (Objective)

* **HZD(Hybrid Zero Dynamics)** 개념: 보행 위상 `s`(=gaitCycle/100)를 독립 변수로 설정하여 시간 기반 제어의 한계를 극복합니다.
* **5차 Bézier 다항식** `θ_d(s) = Σ C(5,k)·s^k·(1−s)^(5−k)·α_k` 를 구현합니다.
* 위상-각도 구속 관계(virtual constraint)가 왜 보행 안정성에 유리한지 이해합니다.

### 제어 법칙

```
s = gaitCycle / 100
θ_d(s) = 5차 Bézier 다항식 (제어점 6개)
τ = Kp·(θ_d − θ) + Kd·(0 − θ̇)
```

---

## ⚙️ 동작 원리 (How it Works)

### 제어 루프 (ACTIVE, 1kHz)

1. `gaitCycle`로 위상 `s` 계산 (stall 감지: 2초 이상 정체 시 안전 복귀)
2. Bézier 다항식으로 `θ_d(s)` 계산
3. PD 추종 토크 계산 → `XM_SetAssistTorque` 전송

### 버튼 조작

| 버튼 | 동작 |
|------|------|
| BTN1 클릭 | Bézier 제어점 프리셋 순환 (자연 보행 / 신전 강조 / 굴곡 강조) |
| BTN2 클릭 | Kp 순환 (1.0 → 2.0 → 3.0 Nm/deg) |
| BTN3 클릭 | 제어 강도 스케일 순환 (0.25 → 0.5 → 1.0) |

---

## 🚀 실행 방법 (How to Use)

1. **Body Data 설정 필수** — `gaitCycle`이 위상 변수 `s`의 유일한 소스입니다.
2. 빌드하여 XM10에 플래시합니다.
3. KIT H10 전원 ON → ASSIST MODE → 보행 시작.
4. USB CDC에서 `VC | s:... θd:... θ:... τ:...` 출력 확인.

---

## 💡 직접 해보기 (Things to Try)

* **프리셋 비교**: 자연 보행 프리셋에서 시작하여 신전/굴곡 강조 프리셋으로 전환하며 보행 패턴 변화를 관찰합니다.
* **Bézier 제어점 직접 수정**: `VC_BEZIER_NATURAL` 배열의 중간 제어점을 조절하여 맞춤형 보행 패턴을 만들어보세요.
* **Kp 튜닝**: Kp가 너무 크면 보조가 착용자의 움직임을 강제합니다. 적절한 순응 Kp를 찾아보세요.

---

## ⚠️ 주의사항

**Body Data 설정이 필수**입니다. `s = gaitCycle/100`이 Bézier 입력이므로 `gaitCycle`이 부정확하면 잘못된 궤도가 생성됩니다.
