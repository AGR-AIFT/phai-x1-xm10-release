# 예제 36: On-Device Kinesthetic Learning

본 예제는 사람이 로봇을 잡고 움직인 궤적을 **MCU 위에서 Tiny Neural Network로 실시간 학습**한 뒤, 학습된 NN + Multi-Layer 제어로 자율 재현하는 방법을 보여줍니다.

## 학습 목표 (Objective)

* `XM_BgTask_Create()` API를 사용하여 **백그라운드 태스크에서 NN 학습**을 수행하는 방법을 학습합니다.
* 실시간 제어 루프(1kHz)와 무거운 연산을 **RTOS 태스크 분리**로 공존시키는 아키텍처를 이해합니다.
* **Phase Encoding** (sin/cos)을 사용한 Tiny NN으로 주기적 궤적을 효과적으로 학습하는 방법을 학습합니다.
* **LQR 기반 최적 PD 게인**을 물리 모델로부터 유도하는 방법을 이해합니다.
* **Alpha-Beta Tracker**로 모델프리 속도 추정을 수행하는 방법을 학습합니다.
* `XM_UserNV_Write/Read` API로 학습된 가중치를 **비휘발성 메모리에 저장/복원**하는 방법을 학습합니다.

---

## 동작 원리 (How it Works)

### 전체 흐름

```
IDLE → [BTN1] → TEACH (투명모드 + 궤적기록, 최대 20초)
     → [BTN1] → LEARN (백그라운드 NN 학습, ~수초)
     → 자동   → REPLAY (NN 추론 + 다층 제어로 재현)
     → [BTN1] → IDLE
```

### 1. TEACH — 투명 모드 + 궤적 기록

사용자가 로봇을 잡고 원하는 궤적으로 움직입니다. **적응 중력보상**으로 로봇의 자중이 상쇄되어 투명한 느낌이 제공됩니다.

* 양측 고관절 각도를 100Hz로 다운샘플 기록 (최대 2000 샘플 = 20초)
* TEACH 중 실측 토크로 MGL_EFF(중력 파라미터)를 온라인 추정

### 2. LEARN — 백그라운드 NN 학습

`XM_BgTask_Create()`로 별도 RTOS 태스크를 생성하여 학습을 수행합니다. **1kHz 제어 루프와 독립적**으로 실행되므로 PnP 통신에 영향을 주지 않습니다.

* **NN 구조**: 3 input (phase, sin(2πφ), cos(2πφ)) → 16 hidden (ReLU) → 2 output (θ_R, θ_L)
* **학습**: SGD with LR decay (0.001→0.0001), 5000 epoch, batch 20
* 학습 중에도 투명 모드 유지 (잡고 있어도 됨)

### 3. REPLAY — NN + Multi-Layer 제어

학습 완료 후 자동으로 REPLAY에 진입합니다. **Ping-pong 방식**으로 궤적을 왕복 재현합니다.

| 레이어 | 역할 |
|--------|------|
| **Gravity Comp** | 적응 추정된 MGL로 중력 상쇄 (feedforward) |
| **LQR-PD** | NN 목표 각도 추종 (J_total 기반 최적 게인) |

### 4. NV Flash 저장/복원

* **BTN1 길게 (REPLAY 중)**: 학습된 NN 가중치 + MGL을 Flash에 저장
* **부팅 시**: Flash에서 자동 복원 → BTN2로 즉시 REPLAY 가능

---

## 버튼 조작

| 버튼 | 동작 | 상태 전환 |
|------|------|----------|
| **BTN1 클릭** | IDLE→TEACH→LEARN→(자동)→REPLAY→IDLE | 순차 전환 |
| **BTN1 길게** | REPLAY 중 NN 가중치를 Flash에 저장 | LED2 한 번 깜빡 |
| **BTN2 클릭** | IDLE에서 마지막 학습 결과로 즉시 REPLAY | Flash 또는 RAM |
| **BTN3 클릭** | 전체 리셋 (버퍼 + NN 초기화) | → IDLE |

---

## LED 표시

| 상태 | LED1 | LED2 | LED3 |
|------|------|------|------|
| IDLE | 심장박동 | OFF | OFF |
| TEACH | 빠른 점멸 (200ms) | OFF | OFF |
| LEARN | 매우 빠른 점멸 (100ms) | ON | OFF |
| REPLAY | ON (고정) | OFF | ON |

---

## 실행 방법 (How to Use)

1. `STM32CubeIDE`에서 본 예제 소스파일을 빌드하고 `XM10`에 업로드합니다.
2. `KIT H10`의 전원을 켜고 `XM10`과 연결합니다.
3. **ASSIST 모드**로 진입합니다 (angel'a DEV 또는 전원 버튼 더블 클릭).
4. **BTN1 클릭** → TEACH 시작. 로봇 다리를 잡고 원하는 궤적으로 3~10초간 움직입니다.
5. **BTN1 클릭** → LEARN 시작. LED1이 매우 빠르게 깜빡이며 수 초 내 학습 완료.
6. 자동으로 **REPLAY** 진입. 로봇이 교시된 궤적을 왕복 재현합니다.
7. **BTN1 클릭**으로 정지 (IDLE 복귀).

---

## 직접 해보기 (Things to Try)

* 다양한 궤적을 교시해보세요 — 단순 왕복, 원호, 좌우 비대칭 등
* `NN_EPOCHS`와 `NN_BATCH_SIZE`를 조절하여 학습 시간과 정확도의 트레이드오프를 관찰해보세요
* `LQR_WN`을 10~25 범위로 변경하여 추종 응답 속도를 조절해보세요
* **BTN1 길게** 눌러 Flash에 저장 후, 전원을 껐다 켜고 **BTN2**로 즉시 재현되는지 확인해보세요
* `KP_LQR`, `KD_LQR` 주석의 유도 과정을 따라 자신의 링크 물성치로 게인을 재계산해보세요
