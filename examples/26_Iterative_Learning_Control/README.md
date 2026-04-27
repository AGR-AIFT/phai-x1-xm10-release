# 예제 26: 반복 학습 제어 ILC (Iterative Learning Control)

본 예제는 **반복 학습 제어(ILC)**를 구현합니다. 매 보행 주기의 오차 이력을 학습하여 다음 주기에서 피드포워드 토크를 개선합니다. 주기를 반복할수록 오차가 줄어드는 "학습"을 체험합니다.

> 📖 API 레퍼런스: [H10 Control & Data](../../docs/api-reference/02-h10-control-n-data.md) · [Task State Machine](../../docs/api-reference/01-task-state-machine.md)
>
> 📄 논문 레퍼런스: Emken, J. L., et al. (2007). *Robot-enhanced motor learning: Accelerating internal model formation during locomotion by transient dynamic amplification.* IEEE Transactions on Neural Systems and Rehabilitation Engineering, 15(4), 505–515.

## 🎯 학습 목표 (Objective)

* **P-type ILC**: `τ_{k+1}(i) = τ_k(i) + L·e_k(i)` 업데이트 법칙을 구현합니다.
* `gaitCycle` 0~100%를 배열 인덱스 0~99로 매핑하는 이산 위상 인덱싱을 학습합니다.
* **주기 완료 감지**: `prev_gait > 80% && current < 20%` 패턴으로 주기 전환을 검출합니다.

### 업데이트 법칙

```c
// 매 주기 완료 시
for (i = 0; i < 100; i++) {
    ilc_torque[i] += L * ilc_error[i];
    ilc_torque[i] = clamp(ilc_torque[i], -MAX, +MAX);
}
```

---

## ⚙️ 동작 원리 (How it Works)

### 제어 루프 (ACTIVE, 1kHz)

1. `gaitCycle` → 인덱스 `i` (0~99) 계산
2. `error[i] = target_torque_profile[i] − current_torque` 기록
3. 현재 `ilc_torque[i]` 적용
4. 주기 완료 감지 → 학습 업데이트 실행

### 버튼 조작

| 버튼 | 동작 |
|------|------|
| BTN1 클릭 | 학습 이득 L 순환 (0.05 → 0.1 → 0.2 → 0.5) |
| BTN2 클릭 | 학습 배열 리셋 (모든 ilc_torque = 0) |
| BTN3 클릭 | 학습 동결 토글 (현재 프로파일 고정) |

---

## 🚀 실행 방법 (How to Use)

1. **Body Data 설정 필수** — `gaitCycle`이 학습 인덱스 소스입니다.
2. 빌드 후 XM10에 플래시합니다.
3. KIT H10 전원 ON → ASSIST MODE → 보행 시작.
4. 처음 몇 주기는 학습량이 적어 보조가 약합니다. 주기를 반복하면서 토크가 점점 개선됩니다.
5. USB CDC에서 `ILC | cyc:... i:... τ_ff:... τ_cmd:...` 출력 확인.
6. **BTN3으로 학습 동결** 후 현재 학습된 프로파일 성능을 평가합니다.

---

## 💡 직접 해보기 (Things to Try)

* **수렴 과정 관찰**: 5~10 주기 후 USB CDC에서 토크 프로파일이 점점 강해지는 것을 확인합니다.
* **학습 이득 L 비교**: L=0.05(느린 학습) vs L=0.5(빠른 학습)에서 수렴 속도와 안정성을 비교합니다.
* **BTN2 리셋 후 재학습**: 리셋 후 다시 주기를 반복하며 학습이 재시작되는지 확인합니다.
* **BTN3 동결**: 10주기 학습 후 동결하면 이후 동일한 피드포워드가 반복 적용됩니다.

---

## ⚠️ 주의사항

**Body Data 설정이 필수**입니다. `gaitCycle`이 부정확하면 학습 인덱스 매핑이 잘못되어 의미 없는 프로파일이 생성됩니다.
학습 이득 L이 너무 크면 발산할 수 있습니다. 초기에는 L=0.05~0.1에서 시작하세요.
