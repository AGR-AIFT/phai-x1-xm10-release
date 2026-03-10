# 예제 25: 입각기 가변 강성 변조 (Stance Stiffness Modulation)

본 예제는 **족저 접촉(foot contact) 상태에 따라 강성을 동적으로 전환**하는 기법을 구현합니다. 입각기(stance)에는 지지 강성을, 유각기(swing)에는 유연한 감쇠를 제공합니다.

> 📖 API 레퍼런스: [H10 Control & Data](../../docs/api-reference/02-h10-control-n-data.md) · [Task State Machine](../../docs/api-reference/01-task-state-machine.md)
>
> 📄 논문 레퍼런스: Collins, S. H., et al. (2015). *Reducing the energy cost of human walking using an unpowered exoskeleton.* Nature, 522(7555), 212–215.

## 🎯 학습 목표 (Objective)

* **족저 접촉 기반 위상 감지**: `isRightFootContact` / `isLeftFootContact`로 입각·유각 전환을 검출합니다.
* **LPF 블렌딩**으로 강성 전환 시 불연속 토크 충격을 완화하는 방법을 학습합니다:
  `K_active = α·K_target + (1−α)·K_active`
* 좌/우 **독립 제어**: 각 다리의 위상이 독립적으로 관리됩니다.

### 제어 법칙

```
입각기: τ = K_stance·(θ_eq − θ)
유각기: τ = −B_swing·θ̇
LPF: K_active[k] = 0.05·K_target + 0.95·K_active[k-1]
```

---

## ⚙️ 동작 원리 (How it Works)

### 제어 루프 (ACTIVE, 1kHz)

1. `isFootContact` → 입각/유각 판단 (좌/우 독립)
2. 목표 강성/감쇠 결정 → LPF로 현재 값 갱신
3. 토크 계산 → `XM_SetAssistTorqueRH/LH` 전송
4. 500ms마다 USB CDC 디버그 출력

### 버튼 조작

| 버튼 | 동작 |
|------|------|
| BTN1 클릭 | 입각 강성 K_stance 순환 (0.5 → 1.0 → 2.0 → 3.0 Nm/deg) |
| BTN2 클릭 | 유각 감쇠 B_swing 순환 (0.5 → 1.5 → 3.0 Nm·s/rad) |
| BTN3 클릭 | 평형 각도 θ_eq 순환 (0° → 5° → 10° → −5°) |

---

## 🚀 실행 방법 (How to Use)

1. **Body Data 설정 필수** — `footContact`가 이 예제의 핵심 위상 소스입니다.
2. 빌드 후 XM10에 플래시합니다.
3. KIT H10 전원 ON → ASSIST MODE → 보행 시작.
4. LED 2/3으로 우/좌 입각 상태를 시각적으로 확인합니다.
5. USB CDC에서 `STSF | R:[ST/SW] K:... τR:... L:[ST/SW] τL:...` 출력 확인.

---

## 💡 직접 해보기 (Things to Try)

* **K_stance 조절**: 큰 K로 입각기 지지력을 높이면 무릎 관절의 부하가 줄어드는 느낌을 체감합니다 (Nature 2015 논문의 핵심 아이디어).
* **LPF 계수 변경**: `LPF_ALPHA`를 0.1로 높이면 전환이 빠르지만 충격이 커집니다. 0.01로 낮추면 부드럽지만 반응이 느립니다.
* **Body Data 미설정 시**: `footContact`가 검출되지 않아 항상 유각 모드로 동작합니다. 비교 실험으로 Body Data의 중요성을 확인하세요.

---

## ⚠️ 주의사항

**Body Data 설정이 필수**입니다. `footContact` 없이는 입각/유각 구분이 불가능합니다.
