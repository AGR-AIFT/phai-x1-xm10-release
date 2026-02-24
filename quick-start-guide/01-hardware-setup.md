# 퀵 스타트 가이드: 1. 하드웨어 연결

성공적인 개발의 첫걸음은 올바른 하드웨어 연결입니다. 아래 순서에 따라 `XM10`과 `KIT H10`, `Sensor Hub Modules` 그리고 개발용 PC를 연결하세요.

## ✅ 준비물 (Package Contents)

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
* **악세사리 (Accessories):**
    * USB C cable for Data Monitoring
    * Sandisk Ultra Dual Drive Type C(32GB) for Data Save
* **PC**
    * STM32CUBEIDE가 설치된 모든 PC
    * Application Module(Jetson Orin NX, 선택 사항)

<div style="page-break-after: always;"></div>
---

## 🔌 1단계: KIT H10과 XM10 연결

`KIT H10`의 확장 케이블(왼쪽 구동기쪽 어패럴에 숨겨져 있음)을 `XM10`의 메인 커넥터에 연결합니다. 이 연결을 통해 `전원`과 `CAN-FD` 통신 라인이 활성화됩니다.
<div align="center">
    <img src="https://github.com/user-attachments/assets/cac2643d-532b-41a6-a680-7fb57d69d2af" width="90%" />
    <p><b>▲ Figure 1. KIT H10 & XM10 Connection</b></p>
</div>

**커넥터 핀맵 (Molex 1053081206):**
| 핀 번호 | 기능 |
| :--- | :--- |
| 1 | NC (No Connect)|
| 2 | 24V |
| 3 | GND |
| 4 | GND |
| 5 | CAN HIGH |
| 6 | CAN LOW |

<div style="page-break-after: always;"></div>
---

## 💻 2단계: 디버거 연결(PC)

실시간 디버깅과 펌웨어 업로드를 위해 ST-Link 디버거를 연결합니다.

1.  PC의 USB 포트와 ST-Link 디버거를 연결합니다.
2.  ST-Link 디버거의 SWD 출력 단자와 `XM10` 보드의 `4-pin SWD` 포트를 SWD 케이블로 연결합니다.
<div align="center">
<img src="https://github.com/user-attachments/assets/a0fccf85-d6af-4efe-b6ab-615710f34cec" width="60%" />
    <p><b>▲ Figure 2. Connector Pinout</b></p>
</div>

<!-- 
## 💻 3단계: Sensor Hub Module 연결 (작성 예정)
## 💻 4단계: Application Module 연결 (작성 예정) -->

이제 하드웨어 준비가 완료되었습니다. 다음 소프트웨어 설정을 진행하세요.
<div style="page-break-after: always;"></div>

# 퀵 스타트 가이드: 2. 개발 환경 구축

하드웨어 연결이 완료되었다면, 이제 `XM10`의 펌웨어를 빌드하고 업로드하기 위한 소프트웨어 개발 환경을 구축할 차례입니다.

## ✅ 준비물 (Prerequisites)

* GitHub 계정
* angel'a DEV 계정
* 인터넷에 연결된 PC

<div style="page-break-after: always;"></div>
---

## 💻 1단계: STM32CubeIDE 설치

`XM10`은 `STM32CubeIDE`를 공식 개발 환경으로 사용합니다.

1.  **다운로드:** [STMicroelectronics 공식 웹사이트](https://www.st.com/en/development-tools/stm32cubeide.html)에 접속하여, **XM10 릴리즈 노트에 명시된 버전(v2.0.0)을 다운로드**합니다.

2.  **설치:** 다운로드한 설치 파일을 실행하여 설치를 진행합니다.

    > **⚠️ 중요 경고**
    > 설치 파일이 있는 경로와 STM32CubeIDE가 설치될 경로에 **한글이나 공백이 포함되어 있으면 빌드 시 예기치 않은 오류가 발생**할 수 있습니다. 모든 경로는 영문으로만 지정하는 것을 강력히 권장합니다.
    > * **좋은 예:** `C:\dev\STM32CubeIDE`
    > * **나쁜 예:** `C:\내 문서\개발 툴`

---

## 🖥️ 2단계: GitHub Desktop 설치

`XM10` 프로젝트는 `GitHub Desktop`을 사용하여 소스 코드를 편리하게 관리하는 것을 권장합니다.

1.  **다운로드:** [GitHub Desktop 공식 웹사이트](https://desktop.github.com/)에 접속하여 프로그램을 다운로드합니다.
2.  **설치 및 로그인:** 설치 파일을 실행하고, 안내에 따라 GitHub 계정으로 로그인해주세요.

---

## 📂 3단계: 예제 다운로드 (Clone)

이제 `GitHub Desktop`을 사용하여 공식 예제 레포지토리를 PC로 복제(Clone)합니다.

1.  **레포지토리 방문:** 웹 브라우저에서 [XM10 공식 GitHub 레포지토리](https://github.com/angel-robotics/Extension_Module)로 이동합니다.

2.  **코드를 PC로 복제:**
    * 녹색 **<> Code** 버튼을 클릭합니다.
    * **Open with GitHub Desktop** 탭을 선택하고 버튼을 클릭합니다.

3.  **저장 경로 선택 및 복제:**
    * `GitHub Desktop`이 자동으로 실행되며 복제(Clone) 창이 나타납니다.
    * **Local Path** 항목에서 프로젝트를 저장할 로컬 경로를 선택합니다.
    * **Clone** 버튼을 클릭합니다. 잠시 후 선택한 경로에 `Extension_Module` 폴더가 생성됩니다.
    * 'Extension_Module' 프로젝트 내 폴더 중 `Examples`폴더에 예제 소스파일이 담겨 있습니다.
    * 예제 소스파일의 내용을 그대로 `User_Algorithm`에 옮겨 사용하시거나(`user_app.c`는 삭제해야 함) `user_app.c`에 코드 내용을 옮겨서 사용하시면 됩니다.

    > **⚠️ 중요 경고**
    > 이 예제 프로젝트가 저장되는 `Local Path` 경로에도 **한글이나 공백이 포함되지 않도록** 주의해주세요.


이제 개발에 필요한 모든 소프트웨어와 예제 코드가 준비되었습니다. 첫 번째 코드를 XM10에서 실행해보세요.
<div style="page-break-after: always;"></div>

# 퀵 스타트 가이드: 3. 예제 불러오기 및 실행

모든 준비가 끝났습니다. 이제 `STM32CubeIDE`에서 예제 프로젝트를 불러와 빌드하고, `XM10` 보드에 첫 펌웨어를 업로드해 보겠습니다.

## ✅ 준비물 (Prerequisites)

* 이전 가이드 `[1. 하드웨어 연결]`과 `[2. 개발 환경 구축]` 완료
* `XM10` 보드에 전원 및 ST-Link 디버거 연결

---

## 📂 1단계: 예제 프로젝트 불러오기 (Import)

`GitHub`에서 다운로드한 프로젝트를 `STM32CubeIDE`의 작업 공간(Workspace)으로 가져옵니다.

1.  `STM32CubeIDE`를 실행합니다.
2.  상단 메뉴에서 `File > Import...` 를 선택합니다.
3.  `General` 폴더를 열고 `Existing Projects into Workspace`를 선택한 후 `Next`를 클릭합니다.
4.  `Select root directory` 항목 옆의 **`Browse...`** 버튼을 눌러 이전 단계에서 `GitHub`로부터 다운로드한 `Extension_Module` 폴더를 선택합니다.
5.  `Projects:` 목록에 `Extension_Module` 프로젝트가 나타나면 **`Finish`** 버튼을 클릭합니다.
6.  `Examples`폴더의 원하는 예제 소스파일을 `User_Algorithm`폴더로 파일 또는 코드를 복사 붙여 넣기 합니다.

---

## 🛠️ 2단계: 프로젝트 빌드하기 (Build)

소스 코드를 `XM10`이 이해할 수 있는 기계어로 변환(컴파일 및 빌드)하는 과정입니다.

1.  `STM32CubeIDE` 왼쪽의 `Project Explorer` 창에서 `Extension_Module` 프로젝트가 성공적으로 불러와졌는지 확인합니다.
2.  상단 툴바에서 **망치 모양의 'Build' 아이콘**을 클릭합니다.
3.  하단의 `Console` 탭에 빌드 과정이 표시되며, 잠시 후 **`Build Finished. [cite_start]0 errors, 0 warnings.`** 메시지가 나타나면 성공입니다.

<div style="page-break-after: always;"></div>
---

## ⚡ 3단계: 펌웨어 업로드 및 실행

빌드된 펌웨어를 ST-Link 디버거를 통해 `XM10` 보드에 업로드합니다.

### **방법 A: 디버깅 모드로 실행 (추천)**

코드가 한 줄씩 어떻게 실행되는지 추적하고 변수 값을 실시간으로 확인할 수 있습니다.

1.  상단 툴바에서 **벌레 모양의 'Debug' 아이콘**을 클릭합니다.
2.  ST-Link가 정상적으로 연결되어 있다면 별도의 설정 없이 바로 디버깅 세션이 시작됩니다.
3.  `STM32CubeIDE`가 `Debug Perspective` 화면으로 전환되면, **재생(F8) 아이콘**을 눌러 코드를 실행합니다.



### **방법 B: 일반 모드로 실행**

디버깅 없이 펌웨어만 업로드하고 바로 실행합니다.

1.  상단 툴바에서 **재생(Run) 아이콘**을 클릭합니다.
2.  펌웨어 업로드가 완료되면 코드가 자동으로 실행됩니다.



🎉 **축하합니다!** 이제 `XM10` 보드에서 첫 번째 예제 코드가 동작하고 있습니다.

각 예제 폴더 안에 있는 `README.md` 파일을 참고하여, 현재 실행 중인 예제가 어떤 동작을 하는지 확인해보세요.
