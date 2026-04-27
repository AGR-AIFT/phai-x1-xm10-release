# 예제 22: CPG 적응 주파수 진동자 (CPG Adaptive Frequency Oscillator)

본 예제는 **CPG(Central Pattern Generator) 적응 주파수 진동자(AFO)**를 구현합니다. 착용자의 보행 주기를 실시간으로 학습하여 동기화된 리드믹 토크를 생성합니다.

> 📖 API 레퍼런스: [H10 Control & Data](../../docs/api-reference/02-h10-control-n-data.md) · [Task State Machine](../../docs/api-reference/01-task-state-machine.md)
>
> 📄 논문 레퍼런스: Ronsse, R., et al. (2011). *Oscillator-based assistance of cyclical movements: Model-based and model-free approaches.* Medical & Biological Engineering & Computing, 49(10), 1173–1185.

## 🎯 학습 목표 (Objective)

* **적응 주파수 진동자(AFO)** 원리: 위상 `φ`와 각주파수 `ω`를 동시에 적응시켜 외부 신호에 동기화하는 메커니즘을 이해합니다.
* 이산 업데이트 방정식을 학습합니다:
  - `φ[k+1] = φ[k] + dt·(ω + ε·F·cos φ)`
  - `ω[k+1] = ω[k] − dt·ε·F·sin φ`
  - `F = x_feedback − A·sin(φ)` (피드백 오차)
* **컴파일 타임 피드백 소스 전환**: `gaitCycle` 또는 `motorAngle` 모드를 선택합니다.

---

## ⚙️ 동작 원리 (How it Works)

### 피드백 소스 선택 (컴파일 타임)

```c
#define USE_GAIT_CYCLE_FEEDBACK   // Body Data 필요 — H10 CM 보행 분석 사용
// #define USE_MOTOR_ANGLE_FEEDBACK  // Body Data 불필요 — 관절각 직접 피드백
```

### AFO 루프 (ACTIVE, 1kHz)

1. 피드백 신호 `x` 읽기 (gaitCycle 또는 motorAngle)
2. 피드백 오차 `F = x − A·sin(φ)` 계산
3. 위상 및 각주파수 업데이트
4. `τ = A·sin(φ)` → `XM_SetAssistTorque` 전송

### 버튼 조작

| 버튼 | 동작 |
|------|------|
| BTN1 클릭 | 진폭 순환 (0.5 → 1.0 → 2.0 → 0.0 Nm) |
| BTN2 클릭 | 결합 강도 ε 순환 (0.5 → 1.0 → 2.0 → 5.0) |
| BTN3 클릭 | 위상 리셋 (φ=0, ω=초기값) |

---

## 🚀 실행 방법 (How to Use)

1. 피드백 소스 매크로를 선택 후 빌드하여 XM10에 플래시합니다.
2. `USE_GAIT_CYCLE_FEEDBACK` 사용 시 → [Body Data 설정](../../docs/api-reference/README.md#body-data-전제조건--반드시-읽으세요) 필수.
3. 걷기 시작하면 진동자가 자동으로 보행 주기에 동기화됩니다.
4. USB CDC에서 `CPG | φ:... ω:... F:... τ:...` 출력 확인.

---

## 💡 직접 해보기 (Things to Try)

* **ε 값 변화**: ε이 클수록 빠르게 동기화되지만 불안정해집니다. 적절한 ε을 찾아보세요.
* **진폭 조절**: 보조 토크가 너무 크면 걸음걸이가 강제됩니다. 자연스러운 진폭을 실험하세요.
* **피드백 소스 비교**: gaitCycle vs motorAngle 모드에서 동기화 속도와 안정성을 비교합니다.

---

## ⚠️ 주의사항

`USE_GAIT_CYCLE_FEEDBACK` 모드는 **Body Data 설정이 필수**입니다. 설정 없이 사용하면 `gaitCycle`이 부정확하여 진동자가 올바른 주기로 동기화되지 않습니다.
