# AI Training Data Pipeline

This page maps the end-to-end flow from collecting exoskeleton data on the XM10 board to loading it into a training framework such as PyTorch or scikit-learn. Use it as a quick-reference map while you work.

---

## Big Picture

```
[Board operation]             [Collection]              [Conversion]              [Training]
       ↓                           ↓                          ↓                       ↓
KIT H10 worn  →  USB memory logging     →  Python decoder    →  PyTorch DataLoader
              or PhAI Studio real-time       CSV / npy output        or sklearn
                  streaming
```

There are three paths. Choose based on your goal.

| Path | Best for | Data volume | Latency |
|------|----------|-------------|---------|
| A. USB memory logging | Offline training (most cases) | Hours of data | — |
| B. PhAI Studio streaming | Real-time visualization + short sessions | Minutes of data | < 100 ms |
| C. On-device inference | Deploying a trained model back to the board | — | 1 ms |

---

## Path A — USB Memory Logging (Most Common)

### 1. Board Side — Recording Data

Use the `examples/10c_MSC_Advanced_Log/` or `examples/34_MSC_GaitAnalysis_Log/` pattern.

Three key calls:

```c
// 1. Register the log source once (in Control_Setup) — the registered struct is logged automatically
XM_SetUsbLogSource(&my_data, sizeof(my_data));

// 2. Start the session once (from a state-transition function — NOT inside the 1ms control loop)
//    Passing NULL as sessionName auto-numbers S_001, S_002 ...
XM_StartUsbDataLog(NULL, "hip_L(float), hip_R(float)");

// 3. Recording is automatic in the background every loop — there is no per-loop write call
//    File split size:  XM_SetUsbLogRollingSize(MB)  (default 10MB)
//    Status check:     XmLogStatus_e st = XM_GetUsbLogStatus();
//    Stop session:     XM_StopUsbDataLog();   // also outside the 1ms loop
```

Full API reference: [docs/api-reference/06-usb-data-logging.md](../api-reference/06-usb-data-logging.md).

### 2. Retrieving the Files

Plug the USB drive into a PC. You will see `.bin` or `.csv` files. The recommended format is `.bin` — the decoder handles the conversion.

### 3. Converting with PythonDecoder

The USB MSC decoder in the `PythonDecoder/` folder converts the session folder and produces `decoded_output.csv`.

```bash
cd PythonDecoder/MSC
python data_decoder_xm10.py /LOGS/S_001        # pass the session folder
```

Output formats:

- `.csv` — human-readable, loads directly into pandas
- `.npy` — fast loading for PyTorch / numpy
- `.mat` — for MATLAB users

### 4. Loading into a Training Framework

#### PyTorch Example

```python
import torch
from torch.utils.data import Dataset, DataLoader
import numpy as np

class XM10Dataset(Dataset):
    def __init__(self, npy_path):
        self.data = np.load(npy_path)  # shape (N, features)

    def __len__(self):
        return len(self.data) - 100  # 100-step window

    def __getitem__(self, idx):
        x = self.data[idx : idx + 100]
        y = self.data[idx + 100, 0]  # predict the first channel of the next step
        return torch.tensor(x, dtype=torch.float32), torch.tensor(y, dtype=torch.float32)

ds = XM10Dataset("my_log.npy")
loader = DataLoader(ds, batch_size=64, shuffle=True)
```

#### scikit-learn Example (Simple Classification)

```python
import pandas as pd
from sklearn.ensemble import RandomForestClassifier

df = pd.read_csv("my_log.csv")
X = df[["imu_ax", "imu_ay", "imu_az", "knee_angle"]].values
y = df["gait_phase"].values

clf = RandomForestClassifier(n_estimators=100)
clf.fit(X, y)
```

---

## Path B — PhAI Studio Real-Time Streaming

Use this path when you need short sessions with visual verification.

### 1. Board Side — Registering Custom Channels

```c
void Control_Setup(void) {
    XM_SetUsbCustomMeta(0xF0, "[{\"name\":\"my_signal\",\"unit\":\"V\"}]");  // channel 0xF0, unit V
}

void Control_Loop(void) {
    float my_value = read_my_sensor();
    XM_SendUsbDataWithId(&my_value, sizeof(my_value), 0xF0);  // arg order: (data, len, module_id)
}
```

### 2. PhAI Studio Side — Live Graph + Recording

In PhAI Studio, connect via USB → select channel `0xF0` → watch the live graph and click the record button simultaneously.

Recorded data can be exported as `.csv` using PhAI Studio's export feature. From there, follow the same PyTorch / sklearn steps described in Path A.

Refer to the separate PhAI Studio documentation for detailed usage.

---

## Path C — On-Device Inference (Deploying a Trained Model)

Take a model trained via Path A and deploy it back to the board to run inference inside the 1 ms control loop.

### 1. Model Compression

Convert a PyTorch or TensorFlow model into a format the STM32 can handle.

| Tool | Output | Suitable models |
|------|--------|-----------------|
| [TensorFlow Lite Micro](https://www.tensorflow.org/lite/microcontrollers) | `.tflite` | Small CNN, MLP |
| [STM32Cube.AI](https://www.st.com/en/embedded-software/x-cube-ai.html) | Auto-generated C code | TF / PyTorch / ONNX |
| Manual implementation | Hand-written C forward pass | Very small NN (≤ 3 layers) |

### 2. Running Inference on the Board

`Ex.16 TinyAI Sensor Fusion` demonstrates manual implementation (3-layer NN). `Ex.36 OnDevice Kinesthetic Learning` goes further and covers on-device learning as well.

Core pattern:

```c
void Control_Loop(void) {
    float input[3] = { imu_ax, imu_ay, imu_az };
    float output[5];

    my_nn_forward(input, output);  // user-defined inference function
    int gait_phase = argmax(output, 5);

    if (gait_phase == STANCE_PHASE) {
        XM_SetAssistTorqueRH(assist_table[gait_phase]);
    }
}
```

Measure inference time using the loop profiling pattern from `Ex.18 Debug Monitor` to confirm it completes within 1 ms.

---

## Common Pitfalls

- **Log files are too large** — At 1 ms × 1 hour, you get 3.6 million samples. The registered struct is logged automatically every loop, so if you don't need all of it, save only the fields you care about in a **smaller struct**, or split files with `XM_SetUsbLogRollingSize`.
- **CSV is too slow** — For large datasets, use `.npy` or `.parquet`. Keep CSV for human inspection only.
- **Timestamps are misaligned** — Use the manual timestamp pattern from `Ex.10b`. The automatic mode only records per-file timestamps.
- **NaN / Inf values** — Add `assert(isfinite(value))` on the board side. Filter with `np.isfinite()` just before training.
- **Class imbalance** — Gait phases like Stance/Swing are naturally imbalanced (roughly 7:3). Use the `class_weight` option or SMOTE.
- **On-board inference exceeds 1 ms** — Apply model quantization (int8) or reduce the number of layers. Verify that the STM32H7's FPU is being utilized.

---

## Next Steps

- Practice the logging pattern with `Ex.10c MSC Advanced Log`
- Collect gait analysis data directly using `Ex.34 MSC GaitAnalysis Log`
- Progress from `Ex.16 TinyAI Sensor Fusion` to `Ex.36 OnDevice Kinesthetic Learning` for on-device inference
- After deploying a trained model to the board, try the full expert demonstration → model training → playback cycle as shown in `Ex.33 Kinesthetic Teaching`

If you need more detail on PhAI Studio or Python post-processing, open a request on GitHub Issues.
