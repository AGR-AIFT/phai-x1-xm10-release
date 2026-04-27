# 예제 20: 임피던스 제어 (Impedance Control)

본 예제는 **임피던스 제어(Impedance Control)**를 구현하여 H10 고관절 외골격과 착용자 사이의 **상호작용 역학(interaction dynamics)**을 가상의 스프링-댐퍼로 정의합니다.

> 📖 API 레퍼런스: [H10 Control & Data](../../docs/api-reference/02-h10-control-n-data.md) · [Task State Machine](../../docs/api-reference/01-task-state-machine.md)
>
> 📄 논문 레퍼런스: Hogan, N. (1985). *Impedance control: An approach to manipulation.* ASME Journal of Dynamic Systems, Measurement, and Control, 107(1), 1–24.

## 🎯 학습 목표 (Objective)

* **임피던스 제어 vs PD 제어**의 철학적 차이를 이해합니다.
  PD 제어는 외란(사용자 힘)을 "억제할 오차"로, 임피던스 제어는 "정의할 상호작용"으로 취급합니다.
* 가상 스프링(`K`)·댐퍼(`B`)·평형점(`θ_d`)으로 로봇의 **기계적 임피던스**를 정의하는 방법을 학습합니다.
* 제어 법칙: `τ = K·(θ_d − θ) + B·(0 − θ̇)`

---

## ⚙️ 동작 원리 (How it Works)

OFF → STANDBY → **ACTIVE**(임피던스 제어)의 TSM 3단계로 동작합니다.

### 제어 루프 (ACTIVE, 1kHz)

1. 현재 각도 `θ` 및 각속도 `θ̇` (후향 차분) 읽기
2. `τ = K·(θ_d − θ) + B·(−θ̇)` 계산
3. `XM_SetAssistTorque(τ, τ)` 로 좌우 동일 적용
4. 500ms마다 USB CDC 디버그 출력

### 버튼 조작

| 버튼 | 동작 |
|------|------|
| BTN1 클릭 | 강성 프리셋 순환 (0.1 → 0.3 → 0.8 Nm/deg) |
| BTN2 클릭 | 감쇠 프리셋 순환 (0.5 → 2.0 → 5.0 Nm·s/rad) |
| BTN3 클릭 | 평형점 이동 (−20° → −10° → 0° → 10° → 20°) |

---

## 🚀 실행 방법 (How to Use)

1. `impedance_control.c`를 `user_app.c`로 복사 후 빌드하여 XM10에 플래시합니다.
2. KIT H10 전원 ON → ASSIST MODE로 전환합니다.
3. LED1이 빠르게 깜빡이면 임피던스 제어가 시작됩니다.
4. BTN1/2/3으로 강성·감쇠·평형점을 실시간 조절하며 상호작용 특성 변화를 체감합니다.
5. USB CDC에서 `IMP | K:... B:... Eq:... θ:... τ:...` 출력을 확인합니다.

---

## 💡 직접 해보기 (Things to Try)

* **K만 조절**: B=0, K를 0.1→0.8 변경. K가 클수록 평형점으로 복원되는 "뻣뻣함"이 증가합니다.
* **B만 조절**: K=0, B를 증가. 움직임이 느릿느릿해지는 댐핑 효과를 체감합니다.
* **PD 제어와 비교**: 같은 K, B 값으로 `14_PD_Realtime_Control` 예제와 응답을 비교해보세요.
* **평형점 이동**: 보행 중 평형점을 바꾸면 보조 방향이 달라집니다. 굴곡/신전 보조의 차이를 관찰하세요.

---

## ⚠️ 주의사항

Body Data 설정은 이 예제에서 **불필요**합니다. 관절각·각속도 직접 피드백만 사용합니다.
