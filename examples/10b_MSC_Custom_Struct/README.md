# 예제 10b: 사용자 정의 구조체 + 수동 타임스탬프 (MSC Custom Struct)

본 예제는 다양한 데이터 타입을 혼합한 **사용자 정의 구조체**를 설계하고, 자동 타임스탬프를 비활성화하여 **직접 시간을 관리**하는 중급 기법을 다룹니다.

## 🎯 학습 목표 (Objective)

* `uint32_t`, `float`, `uint8_t` 등 **다양한 타입을 혼합한 구조체**를 설계하는 방법을 이해합니다.
* `XM_SetUsbLogAutoTimestamp(false)`로 자동 타임스탬프를 비활성화하고 직접 `tick_ms`를 관리합니다.
* **메타데이터 문자열을 상세하게 작성**하여 Python 디코더가 자동으로 파싱할 수 있도록 합니다.
* 4-byte 정렬과 **패딩(`_pad`)** 처리 방법을 학습합니다.

## ⚙️ 동작 원리 (How it Works)

* **구조체 설계:** `SensorSnapshot_t`는 `uint32_t tick_ms`, `float` 5개, `uint8_t` 1개, 패딩 3바이트로 구성되어 총 28바이트입니다. 4-byte 정렬을 유지하여 `__attribute__((packed))` 없이 안전합니다.
* **수동 타임스탬프:** `XM_GetTick()`으로 매 루프마다 구조체에 직접 시간을 기록합니다.
* **상세 메타데이터:** `"tick_ms(uint32_t), hip_angle_L(float), ... , _pad(3bytes)"` 형식으로 작성하면, `data_decoder_xm10.py`가 `struct.unpack` 포맷과 CSV 헤더를 자동으로 생성합니다.

## 📂 바이너리 레코드 포맷

```
auto_timestamp = OFF 이므로:
[Header:4 bytes][user_data:28 bytes] = 32 bytes/record

SensorSnapshot_t 레이아웃:
  Offset  Size  Type        Field
  0       4     uint32_t    tick_ms
  4       4     float       hip_angle_L
  8       4     float       hip_angle_R
  12      4     float       accel_x
  16      4     float       accel_y
  20      4     float       accel_z
  24      1     uint8_t     gait_phase
  25      3     uint8_t[3]  _pad (4-byte 정렬 유지)
```

## 🚀 실행 방법 (How to Use)

1.  USB 메모리를 연결하고 코드를 업로드합니다.
2.  **BTN1 클릭:** 로깅 시작 (LED1 깜빡임)
3.  **BTN2 클릭:** 로깅 정지
4.  USB 메모리에서 `/LOGS/SensorCapture/` 폴더를 확인합니다.
5.  **Python 디코더로 CSV 변환:**
    ```bash
    python data_decoder_xm10.py /LOGS/SensorCapture
    ```
    메타데이터 파싱에 의해 `tick_ms, hip_angle_L, hip_angle_R, accel_x, accel_y, accel_z, gait_phase` 컬럼이 자동 생성됩니다.

## 💡 직접 해보기 (Things to Try)

* `bool` 타입 필드(예: `is_foot_contact`)를 추가해보세요. 메타데이터에 `is_foot_contact(bool)`로 기재하면 디코더가 자동 인식합니다.
* 패딩 없이 구조체를 설계하면 어떤 문제가 생길 수 있는지 크기를 비교해보세요.
* 다음 예제 **10c**에서 상태 머신(TSM)과 에러 복구를 통합하는 고급 패턴을 학습합니다.
