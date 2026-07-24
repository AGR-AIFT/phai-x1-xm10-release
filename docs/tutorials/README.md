# Tutorials — 45 개 예제로 배우기

XM10 기능을 단계적으로 익힐 수 있도록 45 개 예제를 준비했습니다. 각 폴더에 소스 코드와 README 가 함께 있고, README 는 모두 같은 형식 (목표 → 사전 지식 → 핵심 코드 → 실험 → 다음 단계 + 흔한 실수) 으로 정돈되어 있습니다. 기초 예제 (Ex.09 이하)는 30 분 안에 끝나도록 설계했습니다. 다만 제어·고급 예제 (Ex.11 이상)는 난이도에 따라 45 분에서 수 주가 걸릴 수 있습니다.

예제 전체 카탈로그는 [examples/README.md](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/README.md) 에 있고, 막히면 각 README 맨 아래의 "흔한 실수" 섹션을 먼저 보세요. Claude Code 사용자라면 `"Ex.XX 가 안 돼"` 라고만 말해도 `example-helper` 가 해당 예제의 흔한 실수 + 트러블슈팅 문서를 인용해 답해줍니다.

---

## 추천 순서

자기 수준에 맞는 경로 하나를 골라 따라가면 됩니다.

| 경로 | 대상 | 순서 | 예상 시간 |
|------|------|------|-----------|
| 입문 | 임베디드 처음 — 일단 동작 확인까지 가보기 | Ex.00 → 01 → 04 → 07 → 09 → 11 | 3 시간 정도 |
| 중급 | 임베디드 기초 있음 — 통신·실시간 제어 | Ex.02 → 05b → 08 → 09 → 12 → 14 → 18 | 1 주 정도 (자율 학습 기준 하루 1~2 시간) |
| 고급 | 제어 알고리즘 + AI 직접 설계 | Ex.03 → 09 → 15 → 16 → 17 → 19 → 20+ | 한 학기 정도 (Ex.20+ 부터 한 예제당 1~2 주) |
| Physical AI 응용 | 재활·학습 제어 등 | Ex.21 → 31 → 32 → 33 → 36 | 자기주도 학습 (난이도 별로 ⭐⭐⭐) |

예상 시간은 일반 사용자 기준입니다. "고급" 의 한 학기는 **개별 예제마다 1~2 주씩** 들여 깊이 학습한다고 가정한 것입니다. 짧게 훑기만 하면 며칠로도 가능하지만, 변형 실험과 논문 출처까지 따라가면 그만큼 시간이 필요합니다.

---

## 한 학기 (16 주) 수업 진도표 예시

대학 수업으로 운영할 경우 한 학기를 분할한 참고 진도표입니다. 사용자 수준과 보드 수령 시점에 따라 자유롭게 조정하세요.

| 주차 | 주제 | 예제 | 과제 / 산출물 |
|:---:|------|------|--------------|
| 1 | 환경 구축 + 첫 빌드 | Ex.00 | 보드 LED 점등 시연 영상 |
| 2 | 버튼 + LED + 상태 머신 | Ex.01 → 02 → 03 | 자기만의 LED 모드 4 가지 추가 |
| 3 | 외부 GPIO + ADC | Ex.04 → 05 → 05a | 외부 스위치로 LED 4 개 제어 |
| 4 | 다채널 ADC + 안전 스위치 | Ex.05b → 05c → 06 | FSR 8 채널 동시 측정 시연 |
| 5 | USB 시리얼 통신 | Ex.07 → 08 | PC 터미널로 센서값 실시간 모니터링 |
| 6 | 바이너리 스트리밍 + PhAI Studio | Ex.09 | 4 채널 그래프 캡처 + 분석 보고서 |
| 7 | USB-CDC 데이터 수집 + 저장 | Ex.09 + PythonDecoder/CDC | 10 분 데이터 스트리밍 → CSV 저장·분석 |
| 8 | **중간고사 / 프로젝트 1** | (자유) | "내 보드, 내 데이터" 미니 프로젝트 발표 |
| 9 | KIT H10 외골격 기본 모드 | Ex.11 → 12 → 13 | 3 가지 모드 비교 영상 |
| 10 | PD 실시간 제어 | Ex.14 | 자기 PD 게인 튜닝 + 응답 비교 |
| 11 | 디버깅 + 메모리 패턴 | Ex.18 → 19 | Health Dashboard 결과 분석 |
| 12 | 보행 의도 인식 + Tiny AI | Ex.16 → 17 | 7-phase FSM 시연 + 의도 신호 그래프 |
| 13 | 임피던스 + 투명 모드 | Ex.20 → 21 | 가상 스프링 강성 변화 비교 |
| 14 | 보행 위상 적응 토크 | Ex.23 → 25 | 보행 위상 추정 정확도 측정 |
| 15 | 학습 / 적응 제어 | Ex.26 또는 27 | ILC 수렴 곡선 또는 MRAC 적응 궤적 |
| 16 | **기말 프로젝트** | (자유 응용) | 팀 프로젝트 발표 + GitHub 공유 |

수업 운영 팁:
- 첫 2 주는 사용자가 환경 구축에서 가장 많이 막힙니다. TA / 강사가 직접 점검 가능한 시간 배정을 권장합니다.
- 8 주차 미니 프로젝트는 사용자의 동기 유지에 효과적입니다. "Ex.05~09 까지 익힌 걸로 무엇이든" 같은 열린 주제로 운영하세요.
- 16 주차 기말은 팀 단위 (3~4 명) 권장합니다. Physical AI 응용 트랙 (Ex.21, 31, 32, 33) 을 마지막에 깊이 다루는 팀이 보통 나옵니다.

진도표에 빠진 예제 (Ex.05d, 15, 22, 24, 28~42) 는 자기주도 학습 또는 다음 학기 후속 과목용으로 남겨두는 것이 분량 면에서 자연스럽습니다.

---

## Part 0 — 동작 확인

| 예제 | 제목 | 난이도 | 학습 내용 |
| :---: | :--- | :---: | :--- |
| [00](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/00_Quick_Start/) | 보드 동작 확인 | ⭐ | 상태 머신 + LED + USB 시리얼 — 외부 하드웨어 없이 |

---

## Part 1 — XM10 기본 I/O 제어

기초 입출력부터 상태 기반 프로그래밍 (FSM) 까지, 임베디드 제어의 핵심.

### Button & LED (Ex.01 ~ 03)

| 예제 | 제목 | 난이도 | 학습 내용 |
| :---: | :--- | :---: | :--- |
| [01](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/01_Button_LED_Basic/) | 버튼 + LED 기초 | ⭐ | 버튼 상태를 매번 확인해서 LED 제어 |
| [02](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/02_Button_LED_Event/) | 이벤트 + 특수 효과 | ⭐⭐ | 클릭 이벤트 감지, 한 번만 깜빡이는 LED |
| [03](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/03_Button_LED_FSM/) | 상태 머신 (FSM) | ⭐⭐ | 길게 누르기로 모드 전환, 상태별 동작 분리 |

### External I/O — GPIO & ADC (Ex.04 ~ 06)

| 예제 | 제목 | 난이도 | 학습 내용 |
| :---: | :--- | :---: | :--- |
| [04](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/04_Ext_IO_Basic/) | 외부 디지털 제어 | ⭐⭐ | 확장 포트 외부 스위치/LED |
| [05](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/05_Ext_IO_analog/) | 아날로그 센서 입문 | ⭐⭐ | 고정 ADC 핀 전압 측정 |
| [05a](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/05a_Ext_IO_DIO_to_ADC/) | DIO→ADC 동적 전환 | ⭐⭐ | DIO 핀을 ADC 모드로 FSR 읽기 |
| [05b](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/05b_Ext_IO_FSR_8ch/) | FSR 8채널 일괄 | ⭐⭐ | 8 채널 + Resolution 설정 |
| [05c](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/05c_Ext_IO_Mixed_ADC/) | 혼합 ADC 12채널 | ⭐⭐⭐ | 고정 4 + DIO→ADC 8 |
| [05d](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/05d_Ext_IO_DIO_ADC_Hybrid/) | DIO/ADC 하이브리드 | ⭐⭐⭐ | 디지털 + 아날로그 동시 |
| [06](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/06_Ext_IO_Safety_Switch/) | 안전 상태 머신 | ⭐⭐ | 리미트 스위치 기반 FSM |

> **ADC 시리즈 권장 순서:** Ex.05 → 05a → 05b → 05c → 05d (점진 난이도)

---

## Part 2 — USB 통신

PC 와 실시간으로 메시지를 주고받고 데이터를 스트리밍합니다. 디버깅과 데이터 수집의 핵심.

> **주의**: USB 시리얼 (CDC) 포트는 한 번에 한 프로그램만 점유 가능. Ex.07~09 는 시리얼 터미널 또는 PhAI Studio 중 하나만 열어둔 상태에서 실행하세요. 동시에 켜면 충돌합니다.

### USB 시리얼 통신 — Ex.07 ~ 09

| 예제 | 제목 | 난이도 | 학습 내용 |
| :---: | :--- | :---: | :--- |
| [07](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/07_CDC_Basic_Print/) | USB 시리얼 기초 | ⭐⭐ | PC 터미널로 텍스트 메시지 |
| [08](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/08_CDC_Sensor_Print/) | 센서 데이터 모니터링 | ⭐⭐ | 실시간 데이터 sprintf 출력 |
| [09](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/09_CDC_Stream/) | 고속 바이너리 스트리밍 | ⭐⭐⭐ | PhAI Studio 호환 프로토콜, 500 Hz 전송 |

> 수집한 스트림을 PC 에 저장·분석하려면 PhAI Studio 녹화 또는 레포 내 `PythonDecoder/CDC` 파이썬 샘플을 사용하세요. (온보드 파일 저장은 v2.5.0 에서 제거 — 향후 HW 리비전에서 SD카드로 지원 예정)

---

## Part 3 — KIT H10 외골격 기본 모드

실제 외골격 로봇 (KIT H10) 의 세 가지 기본 동작 모드.

| 예제 | 제목 | 난이도 | 학습 내용 |
| :---: | :--- | :---: | :--- |
| [11](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/11_Passive_Mode/) | Passive (수동 모드) | ⭐⭐⭐ | 사전 정의 움직임 명령으로 자동 왕복 |
| [12](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/12_Active_Assist_Mode/) | Active Assist (능동 보조) | ⭐⭐⭐ | 사용자 의도 감지 후 보조 토크 |
| [13](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/13_Resistive_Mode/) | Resistive (저항 모드) | ⭐⭐ | H10 내장 모드 — 물속 걷기 같은 저항감 |

---

## Part 4 — 제어 알고리즘 기초

직접 작성하는 첫 제어 알고리즘들.

| 예제 | 제목 | 난이도 | 학습 내용 |
| :---: | :--- | :---: | :--- |
| [14](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/14_PD_Realtime_Control/) | PD 실시간 토크 제어 | ⭐⭐⭐ | PD 수식, 이산 미분, 토크 포화 |
| [15](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/15_Inverted_Pendulum_Control/) | 역진자 모델 보행 보조 | ⭐⭐⭐ | 중력 보상 MgL·sin(θ) + Lyapunov 안정성 |
| [16](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/16_TinyAI_Sensor_Fusion/) | Tiny AI 센서 퓨전 | ⭐⭐⭐ | 보드 안에서 3-layer 신경망 추론 |
| [17](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/17_FSM_Gait_Intent/) | 보행 의도 인식 (FSM) | ⭐⭐⭐ | 보행 7 단계 상태 머신 + 단계별 토크 |
| [18](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/18_Debug_Monitor/) | 시스템 디버깅 모니터 | ⭐⭐ | 루프 실행 시간 측정, 상태 대시보드 |
| [19](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/19_Memory_Aware_Design/) | 메모리 인식 설계 | ⭐⭐⭐ | 링 버퍼, 풀 할당자, malloc 없이 구현 |

---

## Part 5 — 제어 알고리즘 심화

상호작용 역학, 투명 모드, 보행 위상 기반 제어 — 외골격 제어의 정통 기법들.

| 예제 | 제목 | 난이도 | 학습 내용 |
| :---: | :--- | :---: | :--- |
| [20](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/20_Impedance_Control/) | Hogan 임피던스 제어 | ⭐⭐⭐ | 가상 스프링-댐퍼 상호작용 (Hogan 1985) |
| [21](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/21_Gravity_Compensation/) | 중력 보상 (투명 모드) | ⭐⭐⭐ | Mgl·sin(θ) + 마찰 보상 + 점진 활성화 |
| [22](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/22_CPG_Oscillator/) | CPG 적응 진동자 | ⭐⭐⭐ | 보행 리듬 자동 동기화 (Ronsse 2011) |
| [23](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/23_Gait_Phase_Adaptive_Torque/) | 보행 위상 적응 토크 | ⭐⭐⭐ | 4 구간 정현파 토크 (Quinlivan 2017) |
| [24](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/24_Virtual_Constraint/) | 가상 구속 (HZD) | ⭐⭐⭐ | 5 차 Bézier 궤도 (Westervelt 2003) |
| [25](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/25_Stance_Stiffness_Modulation/) | 입각기 가변 강성 | ⭐⭐⭐ | 입각/유각 부드러운 전환 (Collins 2015) |

---

## Part 6 — 학습 + 적응 제어

매 보행 주기마다 스스로 학습하거나, 사용자 변화에 자동으로 적응하는 제어.

| 예제 | 제목 | 난이도 | 학습 내용 |
| :---: | :--- | :---: | :--- |
| [26](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/26_Iterative_Learning_Control/) | 반복 학습 제어 (ILC) | ⭐⭐⭐ | 주기마다 토크 프로파일 학습 (Emken 2007) |
| [27](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/27_MRAC/) | 모델 참조 적응 제어 | ⭐⭐⭐ | MIT Rule 로 게인 온라인 적응 (Slotine 1991) |
| [28](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/28_Admittance_Control/) | 어드미턴스 제어 | ⭐⭐⭐ | 힘 입력 → 위치 출력 (임피던스의 짝, Keemink 2018) |
| [29](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/29_Bilateral_Coordination/) | 좌·우 협응 제어 | ⭐⭐⭐ | 역위상 대칭 + 약측 강화 (재활용) |
| [30](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/30_FF_FB_Hybrid_Control/) | FF + FB 혼합 제어 | ⭐⭐⭐ | 모델 기반 사전 보상 + PD 피드백 |

---

## Part 7 — Physical AI 응용

투명성 → 의도 감지 → 학습 → 자율 재생으로 이어지는 Physical AI 의 핵심 단계들.

| 예제 | 제목 | 난이도 | 학습 내용 |
| :---: | :--- | :---: | :--- |
| [31](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/31_Friction_Comp_DOB/) | 외란 관측기 (DOB) | ⭐⭐⭐ | 남은 외란까지 추정해서 진정한 투명 모드 |
| [32](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/32_GRF_Gait_Intent/) | 발 접지로 의도 감지 | ⭐⭐⭐ | Heel Strike 이벤트로 보행 위상 추정 |
| [33](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/33_Kinesthetic_Teaching/) | 직접 가르치기 + 재생 | ⭐⭐⭐ | 사람이 손으로 시연 → 보드가 그대로 재생 |
| [35](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/35_MultiLayer_Transparent_Control/) | 다층 투명 제어 | ⭐⭐⭐ | 투명/벽/좌우 커플링 세 모드 실시간 전환 |
| [36](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/36_OnDevice_Kinesthetic_Learning/) | 보드 안에서 직접 학습 🛑 **Rev 2.0 전용** | ⭐⭐⭐ | 작은 신경망을 보드 위에서 학습 → LQR 재생 — Internal Flash UserNV API 가 Rev 2.0 만 지원 |
| [37](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/37_FES_Hub_Module_Ctrl/) | FES Hub 모듈 제어 | ⭐⭐⭐ | CAN-FD 로 FES Hub 연결, 채널별 전기 자극 파라미터 제어 |
| [38](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/38_Periodic_Background_Task/) | 주기적 백그라운드 태스크 | ⭐⭐ | 저주기 보조 작업 분리, 제어 루프 지터 최소화 |
| [39](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/39_Task_Lifecycle/) | 태스크 생명주기 관리 | ⭐⭐⭐ | 태스크 생성·일시정지·종료, RTOS 태스크 상태 머신 패턴 |
| [40](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/40_EMG_Proportional_Assist/) | EMG 비례 보조 토크 🛑 **Rev 2.0 전용** | ⭐⭐⭐⭐ | 외부 ADC 4채널 EMG → envelope → 비례 토크, BTN 캘리브 + PhAI Studio 0xF0 스트리밍 (EMG 경진대회 토대) |
| [41](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/41_IMU_Hub_Dashboard/) | IMU Hub 자세 대시보드 🛑 **Rev 2.0 전용** | ⭐⭐⭐ | 최대 6개 IMU 쿼터니언→오일러(r/p/y) 변환, 연결 자동감지 + PhAI Studio 0xF0 18채널(50Hz) 스트리밍 (FDCAN2 센서허브) |
| [42](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/42_EMG_Hub_Biofeedback/) | EMG Hub 바이오피드백 🛑 **Rev 2.0 전용** | ⭐⭐⭐ | 허브 처리 근활성도(envelope/MVC%) 수신, BTN 캘리브 + LED/PhAI Studio 0xF0 4채널(50Hz) 실시간 피드백 (FDCAN2 센서허브, 모터 미구동) |

---

## 예제 시험해보기

1. `examples/` 에서 원하는 예제의 `.c` 파일을 엽니다.
2. 내용을 통째로 `XM_Apps/Control_Task/control_task.c` 에 복사.
3. STM32CubeIDE 에서 빌드 → XM10 에 업로드.
4. 해당 예제의 README 를 따라가며 동작을 확인합니다.

빌드/플래시가 처음이라면: [첫 빌드 & 실행](../getting-started/03-first-build.md) · AI 자동 안내가 편하다면: [Claude Code 와 함께 시작](../getting-started/00-claude-code-quickstart.md)
