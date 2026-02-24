# 2. 개발 환경 구축

하드웨어 연결이 완료되었다면, 이제 `XM10`의 펌웨어를 빌드하고 업로드하기 위한 소프트웨어 개발 환경을 구축할 차례입니다.

---

## 준비물 (Prerequisites)

* GitHub 계정
* 인터넷에 연결된 PC

---

## 1단계: STM32CubeIDE 설치

`XM10`은 `STM32CubeIDE`를 공식 개발 환경으로 사용합니다.

1. **다운로드:** [STMicroelectronics 공식 웹사이트](https://www.st.com/en/development-tools/stm32cubeide.html)에 접속하여, **v2.0.0 이상** 버전을 다운로드합니다.

2. **설치:** 다운로드한 설치 파일을 실행하여 설치를 진행합니다.

> **주의:** 설치 파일이 있는 경로와 STM32CubeIDE가 설치될 경로에 **한글이나 공백이 포함되어 있으면 빌드 시 예기치 않은 오류가 발생**할 수 있습니다. 모든 경로는 영문으로만 지정하는 것을 강력히 권장합니다.
> * 좋은 예: `C:\dev\STM32CubeIDE`
> * 나쁜 예: `C:\내 문서\개발 툴`

---

## 2단계: GitHub Desktop 설치

`XM10` 프로젝트는 `GitHub Desktop`을 사용하여 소스 코드를 편리하게 관리하는 것을 권장합니다.

1. **다운로드:** [GitHub Desktop 공식 웹사이트](https://desktop.github.com/)에 접속하여 프로그램을 다운로드합니다.
2. **설치 및 로그인:** 설치 파일을 실행하고, 안내에 따라 GitHub 계정으로 로그인해주세요.

---

## 3단계: 예제 다운로드 (Clone)

`GitHub Desktop`을 사용하여 공식 예제 레포지토리를 PC로 복제(Clone)합니다.

1. **레포지토리 방문:** 웹 브라우저에서 [XM10 공식 GitHub 레포지토리](https://github.com/angel-robotics/Extension_Module)로 이동합니다.

2. **코드를 PC로 복제:**
    * 녹색 **<> Code** 버튼을 클릭합니다.
    * **Open with GitHub Desktop** 탭을 선택하고 버튼을 클릭합니다.

3. **저장 경로 선택 및 복제:**
    * `GitHub Desktop`이 자동으로 실행되며 복제(Clone) 창이 나타납니다.
    * **Local Path** 항목에서 프로젝트를 저장할 로컬 경로를 선택합니다.
    * **Clone** 버튼을 클릭합니다. 잠시 후 선택한 경로에 `Extension_Module` 폴더가 생성됩니다.

> **경로 관련 필수 안내**
>
> Windows의 기본 경로 길이 제한(260자)으로 인해, 깊은 경로에 Clone하면 빌드 오류가 발생할 수 있습니다.
>
> | 구분 | 경로 예시 | 상태 |
> | :--- | :--- | :---: |
> | **권장** | `C:\XM_SDK\` | OK |
> | **권장** | `D:\Projects\Extension_Module\` | OK |
> | **비권장** | `C:\Users\UserName\Documents\GitHub\Extension_Module\` | 주의 |
> | **위험** | `C:\Users\...\OneDrive - Company\Documents\...\Extension_Module\` | 오류 가능 |
>
> **경로 규칙:**
> * 가능한 한 **드라이브 루트에 가까운 짧은 경로**에 Clone
> * 경로에 **한글, 공백, 특수문자**가 포함되지 않도록 주의
> * Windows Long Path 지원 활성화로 근본 해결 가능 → [Troubleshooting](../troubleshooting.md#경로-길이-문제-windows-max_path) 참조

---

이제 개발에 필요한 모든 소프트웨어와 예제 코드가 준비되었습니다.

**다음 단계:** [3. 첫 빌드 & 실행](03-first-build.md)
