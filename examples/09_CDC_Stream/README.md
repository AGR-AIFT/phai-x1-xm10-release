# 예제 09: 고속 데이터 스트리밍 (CDC High Speed Stream)

본 예제는 텍스트가 아닌 **바이너리(Binary) 데이터**를 초고속(500Hz)으로 전송하여, PC에서 **실시간 그래프**를 그리는 고급 기법을 다룹니다.

## 🎯 학습 목표 (Objective)

* **바이너리 통신**의 효율성을 텍스트 통신과 비교하여 이해합니다.
* `XM_SetUsbStreamSource` API를 사용하여 사용자 정의 구조체를 자동으로 전송하는 방법을 학습합니다.
* **Arduino Serial Plotter**나 Python 등을 이용해 데이터를 시각화합니다.

## ⚙️ 동작 원리 (How it Works)

* **자동 스트리밍:** 사용자가 `MyGraphData` 구조체를 등록(`XM_SetUsbStreamSource`)해두면, `core_process`가 2ms 제어 주기마다 구조체 내용을 그대로 USB로 쏩니다.
* **Zero Overhead:** `sprintf`와 같은 무거운 문자열 변환 과정이 없으므로 CPU 부하가 거의 없이 대용량 데이터를 실시간으로 보낼 수 있습니다.
* **시각화:** PC 소프트웨어는 들어오는 바이너리 데이터를 파싱하여 그래프로 그립니다.

## 🚀 실행 방법 (How to Use)

1.  **툴 준비:** Arduino IDE의 **Serial Plotter** 또는 바이너리 그래프 툴(Better Serial Plotter 등)을 준비합니다.
2.  코드를 업로드합니다.
3.  PC 툴에서 포트를 열면, 사인파(Sine Wave)와 실제 로봇 각도가 실시간 그래프로 그려지는 것을 확인합니다.
4.  (참고: 이 예제는 Arduino Serial Plotter 호환을 위해 텍스트 포맷으로 변환하여 보낼 수도 있고, 전용 툴을 위해 바이너리로 보낼 수도 있습니다. 코드 내 주석을 확인하세요.)

## 💡 직접 해보기 (Things to Try)

* **데이터 추가:** 그래프에 `Target Torque`와 `Actual Torque`를 추가하여 제어가 잘 추종하는지 눈으로 확인해보세요.
* **수학 함수:** `sin` 함수 대신 `cos`이나 `sawtooth` 파형을 만들어 그래프 모양이 바뀌는지 테스트해보세요.
