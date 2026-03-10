# 예제 27: 모델 참조 적응 제어 MRAC (Model Reference Adaptive Control)

본 예제는 **MRAC(Model Reference Adaptive Control)**를 구현합니다. 참조 모델(이상적인 응답)을 정의하고 MIT Rule로 적응 게인을 온라인 조정하여 실제 시스템이 참조 모델을 추종하도록 합니다.

> 📖 API 레퍼런스: [H10 Control & Data](../../docs/api-reference/02-h10-control-n-data.md) · [Task State Machine](../../docs/api-reference/01-task-state-machine.md)
>
> 📄 논문 레퍼런스: Slotine, J.-J. E., & Li, W. (1991). *Applied Nonlinear Control.* Prentice Hall, Chapter 8 (Model Reference Adaptive Control).

## 🎯 학습 목표 (Objective)

* **참조 모델**: 원하는 시스템 동특성을 1차 LPF로 정의합니다: `x_m[k+1] = a_m·x_m + b_m·r`
* **MIT Rule** 적응 법칙을 구현합니다:
  - `θ̂₁ += γ·e·x` (상태 피드백 게인 적응)
  - `θ̂₂ += γ·e·r` (입력 피드포워드 게인 적응)
* 고정 게인 PD 대비 **적응 제어의 장점**을 이해합니다.

### 제어 법칙

```
e = x_m − x          (추종 오차)
u = θ̂₁·x + θ̂₂·r    (적응 제어 입력)
τ = u·τ_scale
```

---

## ⚙️ 동작 원리 (How it Works)

### 제어 루프 (ACTIVE, 1kHz)

1. 참조 모델 업데이트: `x_m[k+1] = 0.99·x_m + 0.01·r`
2. 추종 오차 `e = x_m − θ_current` 계산
3. MIT Rule로 `θ̂₁`, `θ̂₂` 업데이트 (범위 제한)
4. 제어 입력 `u = θ̂₁·x + θ̂₂·r` → τ 산출 → 전송

### 버튼 조작

| 버튼 | 동작 |
|------|------|
| BTN1 클릭 | 참조 입력 `r` 순환 (0° → 10° → 20° → −10°) |
| BTN2 클릭 | 적응 이득 γ 순환 (0.001 → 0.01 → 0.1) |
| BTN3 클릭 | 적응 게인 리셋 (θ̂₁=0, θ̂₂=1) |

---

## 🚀 실행 방법 (How to Use)

1. 빌드 후 XM10에 플래시합니다. (Body Data 불필요)
2. KIT H10 전원 ON → ASSIST MODE.
3. BTN1로 참조 입력 `r`(목표 각도)을 설정합니다.
4. 시스템이 자동으로 게인을 적응시켜 실제 각도가 참조 모델을 추종하는 것을 관찰합니다.
5. USB CDC에서 `MRAC | r:... xm:... θ:... e:... θ̂1:... θ̂2:...` 출력 확인.

---

## 💡 직접 해보기 (Things to Try)

* **γ 비교**: γ=0.001(느린 적응) vs γ=0.1(빠른 적응). 빠를수록 수렴이 빠르지만 불안정해질 수 있습니다.
* **BTN3 리셋 후 재적응**: 게인을 초기화하고 수렴 과정을 다시 관찰합니다.
* **고정 게인 PD와 비교**: `14_PD_Realtime_Control` 예제와 동일 조건에서 응답을 비교합니다.
* **파라미터 변화 시험**: 착용자가 갑자기 움직임을 바꿀 때 MRAC가 어떻게 적응하는지 관찰합니다.

---

## ⚠️ 주의사항

Body Data 설정은 이 예제에서 **불필요**합니다. 관절각 직접 피드백만 사용합니다.
MIT Rule은 Lyapunov 안정성을 엄밀히 보장하지 않습니다. `THETA_MAX` 범위 제한이 드리프트 방지를 위해 반드시 유지되어야 합니다.
