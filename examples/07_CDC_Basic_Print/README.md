# 예제 07: USB 통신 기초 (CDC Basic Print)

본 예제는 XM10과 PC 간의 시리얼 통신(Serial Communication)을 다룹니다. 펌웨어 개발의 필수 도구인 `printf` 스타일의 디버깅 환경을 구축하는 첫 단계입니다.

## 🎯 학습 목표 (Objective)

* USB CDC(Communication Device Class)의 개념을 이해합니다.
* `XM_SendUsbDebugMessage()` 함수를 사용하여 텍스트 데이터를 PC로 전송합니다.
* PC에서 터미널 프로그램(TeraTerm 등)을 사용하여 보드의 상태를 모니터링합니다.

## ⚙️ 동작 원리 (How it Works)

* **Virtual COM Port:** USB 케이블을 연결하면 XM10은 PC에게 가상의 시리얼 포트(COM Port)로 인식됩니다.
* **비동기 전송:** 사용자가 메시지 전송을 요청하면, 시스템은 이를 내부 버퍼에 담아두었다가 백그라운드(`core_process`)에서 PC로 전송합니다. 덕분에 사용자의 제어 루프는 멈추지 않고 계속 돌아갑니다.

## 🚀 실행 방법 (How to Use)

1.  XM10의 USB 포트를 PC에 연결합니다.
2.  PC의 장치 관리자에서 `STMicroelectronics Virtual COM Port (COMx)` 번호를 확인합니다.
3.  **TeraTerm**이나 **PuTTY**를 열고 해당 포트에 접속합니다. (Baudrate 설정은 무시해도 됩니다.)
4.  보드의 **버튼 1**을 누릅니다.
5.  터미널 화면에 `"Hello! Button 1 was clicked."` 메시지가 출력되는지 확인합니다.

## 💡 직접 해보기 (Things to Try)

* **부팅 메시지:** `User_Setup()` 함수 마지막에 `"System Ready..."` 메시지를 출력하여, 보드가 리셋될 때마다 터미널에 표시되도록 해보세요.
* **키보드 입력:** (심화) PC 키보드에서 문자를 입력했을 때 XM10이 반응하도록 코드를 확장해보세요. (`XM_GetUsbData` 함수 활용)
