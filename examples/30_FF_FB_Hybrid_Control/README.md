# 예제 30: FF+FB 혼합 제어 (Feedforward + Feedback Hybrid Control)

본 예제는 **피드포워드(FF) + 피드백(FB) 혼합 제어**를 구현합니다. 물리 모델 기반 피드포워드로 명목 토크를 생성하고, PD 피드백으로 모델 오차와 외란을 보상합니다.

> 📖 API 레퍼런스: [H10 Control & Data](../../docs/api-reference/02-h10-control-n-data.md) · [Task State Machine](../../docs/api-reference/01-task-state-machine.md)
>
> 📄 논문 레퍼런스: Slotine, J.-J. E., & Li, W. (1991). *Applied Nonlinear Control.* Prentice Hall, Chapter 6 (Computed Torque Control & Feedback Linearization).

## 🎯 학습 목표 (Objective)

* **FF+FB 분리 설계**: 피드포워드는 예측 가능한 동역학을 처리하고, 피드백은 불확실성을 처리합니다.
* **중력 + 마찰 FF 모델**: `τ_ff = M·g·l·cos(θ) + B_f·θ̇`
* **BTN3으로 FF 토글**: 순수 PD와 FF+FB를 직접 비교하여 피드포워드의 실질적 이점을 체감합니다.

### 제어 법칙

```
τ_ff = MGL_EFF·cos(θ_rad) + B_FRICTION·θ̇   (모델 기반 피드포워드)
τ_fb = Kp·(θ_d − θ) + Kd·(0 − θ̇)           (PD 피드백)
τ   = τ_ff + τ_fb                            (혼합 출력)
```

---

## ⚙️ 동작 원리 (How it Works)

### 제어 루프 (ACTIVE, 1kHz)

1. 현재 각도 `θ` 및 각속도 읽기
2. `τ_ff = M·g·l·cos(θ_rad) + B_f·θ̇` 계산 (FF 활성 시)
3. `τ_fb = Kp·e + Kd·ė` 계산
4. `τ = τ_ff + τ_fb` → `XM_SetAssistTorque` 전송

### 버튼 조작

| 버튼 | 동작 |
|------|------|
| BTN1 클릭 | 목표 각도 θ_d 순환 (−20° → −10° → 0° → 10° → 20°) |
| BTN2 클릭 | Kp 순환 (0.3 → 0.5 → 1.0 Nm/deg) |
| BTN3 클릭 | **피드포워드 ON/OFF 토글** (FF+FB ↔ 순수 FB) |

---

## 🚀 실행 방법 (How to Use)

1. 빌드 후 XM10에 플래시합니다. (Body Data 불필요)
2. KIT H10 전원 ON → ASSIST MODE.
3. BTN3으로 FF를 ON 상태로 시작합니다.
4. `MGL_EFF` 매크로 값을 착용자 신체 조건에 맞게 조정합니다.
5. USB CDC에서 `FF_FB | θd:... θ:... τff:... τfb:... τ:...` 출력 확인.
6. **BTN3으로 FF 토글**: 동일 Kp에서 FF 유무에 따른 정상 상태 오차 차이를 비교합니다.

---

## 💡 직접 해보기 (Things to Try)

* **FF ON vs OFF 비교 (핵심 실험)**: BTN3으로 피드포워드를 껐을 때 정상 상태 오차와 Kp가 얼마나 높아야 동일 성능을 내는지 비교합니다.
* **MGL_EFF 오조정**: 실제 체중보다 2배 크게 설정하면 과보상, 절반으로 줄이면 저보상이 발생합니다.
* **마찰 보상만 제거**: `B_FRICTION=0`으로 설정하여 점성 마찰 보상의 역할을 확인합니다.
* **Kp 최소화**: FF가 완벽할수록 FB Kp를 낮게 유지해도 성능이 유지됩니다. FF 품질과 필요 Kp의 관계를 체감합니다.

---

## ⚠️ 주의사항

Body Data 설정은 이 예제에서 **불필요**합니다. 모델 파라미터는 `MGL_EFF`, `B_FRICTION` 매크로로 직접 설정합니다.
`MGL_EFF` 기본값은 표준 체형 근사치입니다. 실제 착용자 신체 치수로 조정하면 FF 정확도가 향상됩니다.
