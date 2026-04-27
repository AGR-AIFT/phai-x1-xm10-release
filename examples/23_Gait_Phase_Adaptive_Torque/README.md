# 예제 23: 보행 위상 적응 토크 (Gait-Phase Adaptive Torque)

본 예제는 **보행 위상(gait phase)에 따라 토크 프로파일을 실시간으로 적응**시키는 기법을 구현합니다. H10 CM의 `gaitCycle`(0~100%)을 위상 소스로 사용하여 정현파 기반 구간별 보조 토크를 인가합니다.

> 📖 API 레퍼런스: [H10 Control & Data](../../docs/api-reference/02-h10-control-n-data.md) · [Task State Machine](../../docs/api-reference/01-task-state-machine.md)
>
> 📄 논문 레퍼런스: Quinlivan, B. T., et al. (2017). *Assistance magnitude versus metabolic cost reductions for a tethered multiarticular soft exosuit.* Science Robotics, 2(2), eaah4416.

## 🎯 학습 목표 (Objective)

* **보행 주기 기반 제어**를 구현합니다: `gaitCycle` 0~100%를 위상으로 사용합니다.
* 입각기(0~60%)·유각기(60~100%) 구간별로 토크 방향과 크기를 다르게 설정합니다.
* `XM_SendUserBodyData()` 필요성과 `gaitCycle` 정확도의 관계를 체험합니다.

### 토크 프로파일 (기본)

| 위상 범위 | 구간 | 보조 방향 |
|-----------|------|-----------|
| 0~30% | 입각 초기 | 0 (대기) |
| 30~60% | 입각 후기 (신전 보조) | +A·sin(π·(φ−0.3)/0.3) |
| 60~80% | 유각 초기 (굴곡 보조) | −A·sin(π·(φ−0.6)/0.2) |
| 80~100% | 유각 후기 | 0 (대기) |

---

## ⚙️ 동작 원리 (How it Works)

### 제어 루프 (ACTIVE, 1kHz)

1. `forwardVelocity < 0.1 m/s` 시 토크=0 (정지 안전 처리)
2. `gaitCycle`(0~100%) → `phi`(0~1.0) 변환
3. 구간별 정현파 토크 계산
4. `XM_SetAssistTorque` 전송

### 버튼 조작

| 버튼 | 동작 |
|------|------|
| BTN1 클릭 | 진폭 순환 (0.0 → 1.0 → 2.0 → 3.0 Nm) |
| BTN2 클릭 | 프로파일 프리셋 (균형/신전 강조/굴곡 강조) |
| BTN3 클릭 | 좌우 위상 오프셋 토글 (동위상 ↔ 역위상) |

---

## 🚀 실행 방법 (How to Use)

1. **Body Data 설정 필수**: `User_Setup()`의 `XM_SendUserBodyData()` 체중·신장·다리 길이를 실측값으로 수정합니다.
2. 빌드하여 XM10에 플래시합니다.
3. KIT H10 전원 ON → ASSIST MODE → 보행 시작.
4. BTN1로 진폭을 점진적으로 증가시킵니다 (갑작스러운 고진폭 금지).
5. USB CDC에서 `GAIT | φ:... A:... τR:... τL:...` 출력 확인.

---

## 💡 직접 해보기 (Things to Try)

* **Body Data 미설정 vs 설정**: Body Data 없이 실행 시 `gaitCycle`이 부정확하여 보조 타이밍이 어긋나는 것을 확인합니다.
* **프로파일 비교**: 신전 강조 / 굴곡 강조 프리셋에서 에너지 소모 차이를 체감합니다.
* **위상 오프셋**: 좌/우 역위상(50% 오프셋)으로 전환 시 좌우 교대 보조 느낌을 체감합니다.

---

## ⚠️ 주의사항

**Body Data 설정이 필수**입니다. `XM_SendUserBodyData()` 없이는 `gaitCycle`이 부정확하여 보조 타이밍이 잘못 계산됩니다. 설정 방법은 [API Reference README](../../docs/api-reference/README.md)를 참조하세요.
