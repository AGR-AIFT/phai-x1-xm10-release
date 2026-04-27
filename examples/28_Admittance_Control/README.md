# 예제 28: 어드미턴스 제어 (Admittance Control)

본 예제는 **어드미턴스 제어(Admittance Control)**를 구현합니다. 측정된 외부 토크를 입력으로 받아 가상 질량-스프링-댐퍼 동역학을 통해 위치 참조를 생성하고, 내부 PD 제어기로 추종합니다.

> 📖 API 레퍼런스: [H10 Control & Data](../../docs/api-reference/02-h10-control-n-data.md) · [Task State Machine](../../docs/api-reference/01-task-state-machine.md)
>
> 📄 논문 레퍼런스: Keemink, A. Q. L., et al. (2018). *Admittance control for physical human-robot interaction.* International Journal of Robotics Research, 37(11), 1421–1444.

## 🎯 학습 목표 (Objective)

* **임피던스 vs 어드미턴스 쌍대성**: 임피던스는 위치→힘, 어드미턴스는 힘→위치 변환임을 이해합니다.
* **가상 질량-스프링-댐퍼** 동역학으로 외력에 대한 "순응적 위치 반응"을 구현합니다:
  `M_v·θ̈_ref = τ_ext − K_v·θ_ref − B_v·θ̇_ref`
* 내부 PD 위치 제어기가 가상 동역학으로 생성된 참조를 추종합니다.

### 구조

```
τ_ext (측정) → 가상 M-K-B 동역학 → θ_ref(t)
                                       ↓
              θ_current → PD 위치 제어기 → τ_cmd
```

---

## ⚙️ 동작 원리 (How it Works)

### 제어 루프 (ACTIVE, 1kHz — Forward Euler)

1. 외부 토크 `τ_ext = XM.status.h10.rightHipTorque` 읽기
2. 가상 동역학: `vel += dt·(τ_ext − K_v·pos − B_v·vel)/M_v`
3. 위치 업데이트: `pos += dt·vel`
4. 내부 PD: `τ = Kp_inner·(θ_ref − θ) + Kd_inner·(θ̇_ref − θ̇)`
5. `XM_SetAssistTorque` 전송

### 버튼 조작

| 버튼 | 동작 |
|------|------|
| BTN1 클릭 | 가상 질량 M_v 순환 (0.5 → 1.0 → 2.0 → 5.0 kg·m²) |
| BTN2 클릭 | 가상 강성 K_v 순환 (0.0 → 0.5 → 2.0 Nm/deg) |
| BTN3 클릭 | 참조 위치 리셋 (θ_ref = 현재 각도로 리셋) |

---

## 🚀 실행 방법 (How to Use)

1. 빌드 후 XM10에 플래시합니다. (Body Data 불필요)
2. KIT H10 전원 ON → ASSIST MODE.
3. 착용자가 관절을 밀면 가상 질량이 힘에 반응하여 위치가 이동하는 것을 체감합니다.
4. BTN1로 M_v를 조절하여 무겁고 가벼운 느낌 차이를 비교합니다.
5. USB CDC에서 `ADM | τext:... θref:... θ:... τcmd:...` 출력 확인.

---

## 💡 직접 해보기 (Things to Try)

* **M_v 비교**: M_v=0.5(가벼운 가상 물체) vs M_v=5.0(무거운 가상 물체). 같은 힘으로 밀었을 때 이동 거리 차이를 비교합니다.
* **K_v=0 (순수 어드미턴스)**: 복원력 없이 힘만 적분. 착용자가 힘을 가하면 위치가 계속 이동합니다.
* **임피던스 제어(Ex.20)와 비교**: 동일한 `K`, `B` 값으로 두 제어기의 상호작용 특성을 비교합니다.

---

## ⚠️ 주의사항

Body Data 설정은 이 예제에서 **불필요**합니다. 측정 토크 피드백만 사용합니다.
가상 질량 M_v가 너무 작으면 수치 적분이 불안정해질 수 있습니다. `MIN_VIRTUAL_MASS = 0.1f` 이하로 내리지 마세요.
