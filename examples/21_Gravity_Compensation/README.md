# 예제 21: 중력 보상 / 투명 모드 (Gravity Compensation & Transparent Mode)

본 예제는 **중력 보상(Gravity Compensation)**을 구현하여 고관절 외골격이 착용자에게 "마치 없는 것처럼" 느껴지는 **투명 모드(Transparent Mode)**를 실현합니다.

> 📖 API 레퍼런스: [H10 Control & Data](../../docs/api-reference/02-h10-control-n-data.md) · [Task State Machine](../../docs/api-reference/01-task-state-machine.md)
>
> 📄 논문 레퍼런스: Just, F., et al. (2018). *Exoskeleton transparency: Feed-forward compensation vs. disturbance observer.* Journal of NeuroEngineering and Rehabilitation, 15(1), 1–12.

## 🎯 학습 목표 (Objective)

* **중력 보상 모델**: `τ_grav = Mgl·sin(θ)` (단순 역진자 모델)을 구현합니다.
* **점진적 활성화**: `α` 비율(0→1)로 보상량을 서서히 증가시켜 착용자의 안전을 확보하는 패턴을 학습합니다.
* 제어 법칙: `τ = α·(Mgl·sin θ + B_f·sign(θ̇) + B_v·θ̇)`

---

## ⚙️ 동작 원리 (How it Works)

OFF → STANDBY → **ACTIVE**(중력 보상)의 TSM 3단계로 동작합니다.

### 제어 루프 (ACTIVE, 1kHz)

1. 현재 각도 `θ` (deg→rad 변환) 및 각속도 읽기
2. 중력 보상 토크 = `Mgl·sin(θ_rad)` 계산
3. 마찰 보상 토크 추가 (정지 마찰 + 점성 마찰, 토글 가능)
4. α 스케일 적용 → `XM_SetAssistTorque` 전송

### 버튼 조작

| 버튼 | 동작 |
|------|------|
| BTN1 클릭 | α 비율 순환 (0.0 → 0.25 → 0.5 → 0.75 → 1.0) |
| BTN2 클릭 | 체중 프리셋 순환 (50/70/90 kg — Mgl 재계산) |
| BTN3 클릭 | 마찰 보상 ON/OFF 토글 |

---

## 🚀 실행 방법 (How to Use)

1. `gravity_compensation.c`를 `user_app.c`로 복사 후 빌드하여 XM10에 플래시합니다.
2. KIT H10 전원 ON → ASSIST MODE 전환.
3. **BTN1을 여러 번 눌러 α를 0 → 1로 점진적으로 증가**시킵니다 (갑작스러운 α=1 적용 금지).
4. USB CDC에서 `GC | α:... Mgl:... τ_grav:... τ_cmd:...` 출력을 확인합니다.

---

## 💡 직접 해보기 (Things to Try)

* **α=0 → 1 점진 증가**: 각 단계에서 외골격의 "무게감"이 사라지는 과정을 체감합니다.
* **BTN3 마찰 보상 토글**: 마찰 보상 OFF 시 모터 마찰로 인해 착용자가 추가 힘을 써야 함을 느껴보세요.
* **체중 프리셋 변경**: 체중이 다를 때 동일 α에서 보상 토크가 얼마나 달라지는지 비교합니다.
* **sin(θ) 모델의 한계**: θ=0°(수직)에서 보상토크가 0이 되는 특성을 확인합니다.

---

## ⚠️ 주의사항

Body Data 설정은 이 예제에서 **불필요**합니다. 중력 보상은 `BODY_MASS_KG` 매크로 값만 사용합니다.
α를 1.0으로 급격히 올리지 마십시오. 착용자 안전을 위해 BTN1 순차 증가 방식을 사용하십시오.
