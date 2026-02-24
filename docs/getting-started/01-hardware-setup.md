# 1. 하드웨어 연결

성공적인 개발의 첫걸음은 올바른 하드웨어 연결입니다. 아래 순서에 따라 `XM10`과 `KIT H10`, `Sensor Hub Modules` 그리고 개발용 PC를 연결하세요.

---

## 준비물 (Package Contents)

시작하기 전에 아래 구성품이 모두 있는지 확인해주세요.

* **로봇 (Robot):**
    * KIT H10
* **보드 (Board):**
    * XM10 Board
    * Sensor Hub Module Board
* **케이블 (Cables):**
    * XM10 ↔ KIT H10 연결 케이블
    * XM10 ↔ Sensor Hub Modules 연결 케이블
* **디버깅 도구 (Debugging Tools):**
    * ST-Link V2 디버거
    * ST-Link 20핀 to 4핀 (SWD) 변환 보드
    * 변환 보드 ↔ XM10 4핀 (SWD) 케이블
* **액세서리 (Accessories):**
    * USB C cable for Data Monitoring
    * Sandisk Ultra Dual Drive Type C(32GB) for Data Save
* **PC:**
    * STM32CubeIDE가 설치된 모든 PC
    * Application Module (Jetson Orin NX, 선택 사항)

---

## 1단계: KIT H10과 XM10 연결

`KIT H10`의 확장 케이블(왼쪽 구동기쪽 어패럴에 숨겨져 있음)을 `XM10`의 메인 커넥터에 연결합니다. 이 연결을 통해 `전원`과 `CAN-FD` 통신 라인이 활성화됩니다.

<div align="center">
    <img src="https://github.com/user-attachments/assets/cac2643d-532b-41a6-a680-7fb57d69d2af" width="90%" />
    <p><b>Figure 1. KIT H10 & XM10 Connection</b></p>
</div>

**커넥터 핀맵 (Molex 1053081206):**

| 핀 번호 | 기능 |
| :--- | :--- |
| 1 | NC (No Connect) |
| 2 | 24V |
| 3 | GND |
| 4 | GND |
| 5 | CAN HIGH |
| 6 | CAN LOW |

---

## 2단계: 디버거 연결 (PC)

실시간 디버깅과 펌웨어 업로드를 위해 ST-Link 디버거를 연결합니다.

1. PC의 USB 포트와 ST-Link 디버거를 연결합니다.
2. ST-Link 디버거의 SWD 출력 단자와 `XM10` 보드의 `4-pin SWD` 포트를 SWD 케이블로 연결합니다.

<div align="center">
    <img src="https://github.com/user-attachments/assets/a0fccf85-d6af-4efe-b6ab-615710f34cec" width="60%" />
    <p><b>Figure 2. ST-Link SWD Connector Pinout</b></p>
</div>

이제 하드웨어 준비가 완료되었습니다.

---

**다음 단계:** [2. 개발 환경 구축](02-software-setup.md)
