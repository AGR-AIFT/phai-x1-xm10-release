# AI 학습 데이터 파이프라인

XM10 보드에서 모은 외골격 데이터를 PyTorch / scikit-learn 같은 학습 프레임워크로 가져가는 흐름을 한 페이지에 정리했습니다. AI 모델 학습이 목표라면 이 페이지를 흐름표로 두고 작업하세요.

> USB 메모리(MSC) 파일 로깅 기능은 v2.5.0 에서 제거되었습니다. 데이터 수집은 **USB-CDC 실시간 스트리밍**으로 하며, 아래 길 A 가 표준 경로입니다. 온보드 저장(SD카드)은 향후 HW 리비전에서 지원 예정입니다.

---

## 큰 그림

```
[보드 동작]              [수집]                      [변환]                 [학습]
  ↓                        ↓                          ↓                     ↓
KIT H10 착용  →  USB-CDC 실시간 스트리밍   →   CSV / npy 출력   →  PyTorch DataLoader
                 (PhAI Studio 또는                                    또는 sklearn
                  PythonDecoder/CDC)
```

두 가지 길이 있습니다. 목적에 따라 골라가세요.

| 길 | 적합한 상황 | 데이터량 | 지연 |
|----|-----------|---------|------|
| A. USB-CDC 스트리밍 수집 | 오프라인 학습 (대부분) | 분 ~ 시간 | < 100 ms |
| B. 보드 내 추론 | 학습된 모델을 보드로 배포 | — | 1 ms |

---

## 길 A — USB-CDC 스트리밍 수집 (표준)

### 1. 보드 측 — 커스텀 채널 스트리밍

`examples/09_CDC_Stream/` 패턴을 사용합니다. 학습에 쓸 데이터를 커스텀 채널로 실시간 전송합니다.

```c
void Control_Setup(void) {
    // 채널 메타데이터(JSON): 인자는 module_id, json_str 2개
    XM_SetUsbCustomMeta(0xF0, "[{\"name\":\"hip_L\",\"unit\":\"deg\"},{\"name\":\"hip_R\",\"unit\":\"deg\"}]");
}

void Control_Loop(void) {
    float sample[2] = { hip_L, hip_R };
    XM_SendUsbDataWithId(sample, sizeof(sample), 0xF0);  // (data, len, id) 순서 — 매 루프 전송
}
```

### 2. PC 측 — 스트림 수신 + CSV 저장

두 가지 방법이 있습니다.

- **PhAI Studio** — USB 연결 → 채널 `0xF0` 선택 → 실시간 그래프 확인 + 녹화 버튼 → export 로 `.csv` 저장. 가장 간단합니다.
- **PythonDecoder/CDC** — 직접 파싱이 필요하면 레포 내 파이썬 샘플로 수신하면서 CSV 를 자동 저장합니다.

```bash
python PythonDecoder/CDC/cdc_phai_receiver.py --cli --port COM6   # CSV 자동 저장
```

### 3. 학습 프레임워크에서 로드

#### PyTorch 예시

```python
import pandas as pd
import numpy as np
import torch
from torch.utils.data import Dataset, DataLoader

class XM10Dataset(Dataset):
    def __init__(self, csv_path):
        df = pd.read_csv(csv_path)
        self.data = df.select_dtypes("number").values.astype("float32")  # (N, features)

    def __len__(self):
        return len(self.data) - 100  # 100-step window

    def __getitem__(self, idx):
        x = self.data[idx : idx + 100]
        y = self.data[idx + 100, 0]  # 다음 스텝의 첫 채널 예측
        return torch.tensor(x), torch.tensor(y, dtype=torch.float32)

ds = XM10Dataset("cdc_phai_20260224_120000.csv")
loader = DataLoader(ds, batch_size=64, shuffle=True)
```

> 큰 데이터셋은 CSV 를 한 번 `np.save()` 로 `.npy` 로 변환해두면 로드가 훨씬 빠릅니다.

#### scikit-learn 예시 (간단 분류)

```python
import pandas as pd
from sklearn.ensemble import RandomForestClassifier

df = pd.read_csv("cdc_phai_20260224_120000.csv")
X = df[["imu_ax", "imu_ay", "imu_az", "knee_angle"]].values
y = df["gait_phase"].values

clf = RandomForestClassifier(n_estimators=100)
clf.fit(X, y)
```

---

## 길 B — 보드 내 추론 (학습된 모델 배포)

길 A 로 학습한 모델을 보드에 다시 올려서 1 ms 루프 안에서 추론합니다.

### 1. 모델 경량화

PyTorch / TensorFlow 모델을 STM32 가 다룰 수 있도록 변환.

| 도구 | 변환 결과 | 적합한 모델 |
|------|----------|-----------|
| [TensorFlow Lite Micro](https://www.tensorflow.org/lite/microcontrollers) | `.tflite` | 작은 CNN, MLP |
| [STM32Cube.AI](https://www.st.com/en/embedded-software/x-cube-ai.html) | C 코드 자동 생성 | TF/PyTorch/ONNX 모두 |
| 직접 구현 | 사람이 C 로 forward 작성 | 매우 작은 NN (3-layer 이하) |

### 2. 보드에서 추론

`Ex.16 TinyAI Sensor Fusion` 이 직접 구현 (3-layer NN) 예시. `Ex.36 OnDevice Kinesthetic Learning` 은 온디바이스 학습까지 다룹니다.

핵심 패턴:

```c
void Control_Loop(void) {
    float input[3] = { imu_ax, imu_ay, imu_az };
    float output[5];

    my_nn_forward(input, output);  // 사용자 정의 추론 함수
    int gait_phase = argmax(output, 5);

    if (gait_phase == STANCE_PHASE) {
        XM_SetAssistTorqueRH(assist_table[gait_phase]);
    }
}
```

추론 시간 측정은 `Ex.18 Debug Monitor` 의 루프 프로파일링 패턴 사용 → 1 ms 안에 끝나는지 확인.

---

## 자주 막히는 부분

- **CSV 가 너무 큼/느림** — 큰 데이터셋은 `.npy` 또는 `.parquet` 로 변환해 로드. CSV 는 사람 확인용으로만.
- **스트리밍 중 패킷 누락** — `PythonDecoder/CDC/cdc_csv_reviewer.py` 의 Sequence Gap(ΔSeq) / Tx Drop 분석으로 누락 시점을 확인하고, 전송 데이터량을 줄이거나 채널을 간추리세요.
- **NaN / Inf 값** — 보드 측에서 `assert(isfinite(value))` 추가. 학습 직전에 `np.isfinite()` 로 필터링.
- **클래스 불균형** — Stance/Swing 같은 보행 phase 는 7:3 정도로 비균등. `class_weight` 옵션 또는 SMOTE 사용.
- **보드에서 추론이 1 ms 를 넘김** — 모델 양자화 (int8) 또는 layer 수 감소. STM32H7 의 FPU 활용 확인.

---

## 다음

- `Ex.09 CDC Stream` 으로 커스텀 채널 스트리밍 패턴 익히기
- `Ex.16 TinyAI Sensor Fusion` → `Ex.36 OnDevice Kinesthetic Learning` 으로 보드 내 추론 학습
- 학습 모델을 보드에 올린 뒤에는 `Ex.33 Kinesthetic Teaching` 처럼 전문가 시연 → 모델 학습 → 재생 의 전체 사이클 시도

PhAI Studio + Python 후처리에 대한 더 자세한 내용이 필요하면 GitHub Issues 에 요청해주세요.
