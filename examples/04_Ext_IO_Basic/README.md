# 예제 04: 외부 I/O 제어 기초 (External IO Basic)

본 예제는 XM10의 확장 포트(Extension Port)를 사용하여 외부 하드웨어를 제어하는 방법을 다룹니다. 외부 스위치와 LED를 연결하고, 디지털 입력(Digital Read)과 디지털 출력(Digital Write)을 통해 회로를 제어합니다.

## 🎯 학습 목표 (Objective)

* XM10 확장 포트의 핀 번호(`XM_EXT_DIO_x`)와 물리적 위치를 매칭합니다. (아래 그림은 XM10 Overview)
<div align="center">
    <img src="https://github.com/user-attachments/assets/c2984f32-7cfb-450d-9e75-5ab19e8e0f6b" width="90%" />
    <p><b>▲ Figure 1. XM10 Board Overview</b></p>
</div>

* XM_SetExtPinMode를 사용하여 핀의 용도(입력/출력)를 설정하는 방법을 학습합니다.
* 입력 풀업(Input Pullup) 모드를 사용하여 외부 저항 없이 스위치를 연결하는 방법을 이해합니다.
* XM_DigitalRead와 XM_DigitalWrite API를 사용하여 외부 신호를 읽고 쓰는 법을 배웁니다.

---

## ⚙️ 동작 원리 (How it Works)

1.  **핀 설정:**
    * **Pin 3 (입력):** `XM_IO_INPUT_PULLUP`으로 설정합니다. 내부 저항이 전원(3.3V)에 연결되므로, 스위치가 열려있으면 `XM_HIGH`, 눌려서 GND와 닿으면 `XM_LOW`가 됩니다.
    * **Pin 4 (출력):** `XM_IO_OUTPUT`으로 설정하여 LED에 전압을 공급합니다.
2.  **로직:** `Run_Loop`에서 Pin 3의 상태를 계속 읽습니다. 값이 `XM_LOW`(눌림)이면 Pin 4에 `XM_HIGH`(3.3V)를 출력하여 LED를 켭니다.

---

## 🚀 실행 방법 (How to Use)

1.  **하드웨어 연결:** (브레드보드 권장)
    * **스위치:** 한쪽 다리는 `XM_EXT_DIO_3`에, 다른 쪽 다리는 `GND`에 연결합니다.
    * **LED:** 긴 다리(+)는 `XM_EXT_DIO_4`에, 짧은 다리(-)는 저항(220Ω~1kΩ)을 거쳐 `GND`에 연결합니다.
2.  코드를 빌드하고 업로드합니다.
3.  외부 스위치를 누르면 외부 LED가 켜지는 것을 확인합니다.

---

## 💡 직접 해보기 (Things to Try)

* **로직 반전:** 스위치를 누르면 꺼지고, 떼면 켜지도록 `if` 문 조건을 반대로(`== XM_HIGH`) 수정해보세요.
* **센서 연결:** 스위치 대신 디지털 출력을 가진 센서(예: 적외선 근접 센서)를 Pin 3에 연결하여, 물체가 감지되면 LED가 켜지게 해보세요.
