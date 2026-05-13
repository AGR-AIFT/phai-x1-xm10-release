# Advanced Topics — 심화 주제

자기주도 학습을 위한 출발점이에요. 관심 분야별로 어떤 예제부터 시작해야 할지, 무엇이 아직 로드맵 상태인지를 한눈에 보여줍니다. examples 16, 18, 19 와 API Reference 를 한 번 훑은 뒤 들어오면 흐름이 잡혀요.

---

## 예제로 바로 시작하기

아래 심화 주제는 이미 `examples/` 폴더에 실습 가능한 예제가 있습니다.

| 주제 | 예제 | 설명 |
| :--- | :--- | :--- |
| **TinyML on STM32** | [Ex.16 — Tiny AI 센서 퓨전](../../examples/16_TinyAI_Sensor_Fusion/) | 상보 필터 + 3-layer NN MCU 추론 |
| **역진자 모델 보행 제어** | [Ex.15 — Inverted Pendulum](../../examples/15_Inverted_Pendulum_Control/) | MgL·sin(θ) 중력 보상 + PD 안정화 |
| **FSM 보행 의도 인식** | [Ex.17 — FSM Gait Intent](../../examples/17_FSM_Gait_Intent/) | 7-phase 보행 FSM + 단계별 보조 토크 |
| **PD 제어 이론** | [Ex.14 — PD Realtime Control](../../examples/14_PD_Realtime_Control/) | 이산 PD 제어 수식 직접 구현 |
| **시스템 디버깅** | [Ex.18 — Debug Monitor](../../examples/18_Debug_Monitor/) | 루프 프로파일링 + Health 대시보드 |
| **메모리 패턴** | [Ex.19 — Memory Aware Design](../../examples/19_Memory_Aware_Design/) | 링 버퍼, 풀 할당자, 이동 평균 |

---

## 로드맵

### 센서 허브 연동

| 주제 | 설명 |
| :--- | :--- |
| DIO ↔ ADC 동적 전환 심화 | 런타임 핀 모드 전환의 내부 동작 원리 및 고급 패턴 |
| 커스텀 센서 허브 개발 | CAN-FD 기반 사용자 정의 센서 허브 모듈 설계 및 연동 |
| IMU Hub Module 활용 | IMU Hub Module의 데이터 구조, 캘리브레이션, 필터링 |

### 통신 프로토콜 상세

| 주제 | 설명 |
| :--- | :--- |
| USB 시리얼 프로토콜 명세 | 패킷 헤더 (SOF, CRC8, STATUS) 와 커스텀 페이로드 구조 |
| CAN-FD 데이터 객체 | KIT H10 ↔ XM10 사이 표준 데이터 교환 규칙 |
| 모듈 자동 검색 | 센서 허브 / 모듈 자동 인식과 구성 흐름 |

### 심화 프로젝트

| 주제 | 설명 | 상태 |
| :--- | :--- | :---: |
| TinyML on STM32 | STM32H7에서 경량 AI 모델 학습/추론 파이프라인 | ✅ [Ex.16](../../examples/16_TinyAI_Sensor_Fusion/) |
| 논문 기반 보행 제어 | 역진자 모델 보행 보조 알고리즘 | ✅ [Ex.15](../../examples/15_Inverted_Pendulum_Control/) |
| 강화학습 기반 제어 | Jetson 시리즈 + XM10 연동 RL 파이프라인 | 로드맵 |
| AI 학습 데이터 파이프라인 | 보드 데이터 수집 → 변환 → PyTorch 학습 → 배포 | ✅ [ai-data-pipeline.md](ai-data-pipeline.md) |

### Python 도구 심화

| 주제 | 설명 |
| :--- | :--- |
| CDC Receiver 커스터마이징 | 사용자 정의 센서 데이터 시각화 설정 |
| MSC 바이너리 포맷 설계 | 효율적인 사용자 정의 데이터 구조 설계 |
| 데이터 후처리 파이프라인 | Python + MATLAB을 활용한 분석 워크플로우 |

---

콘텐츠 요청이나 기여는 [GitHub Issues](https://github.com/AGR-EXO/Extension_Module/issues)에서 받고 있습니다.

---

## 자기주도 학습 권장 경로

| 관심 분야 | 추천 출발점 | 다음 |
|----------|-----------|------|
| **AI / ML on MCU** | [Ex.16 TinyAI Sensor Fusion](../../examples/16_TinyAI_Sensor_Fusion/) | [Ex.36 On-Device Kinesthetic Learning](../../examples/36_OnDevice_Kinesthetic_Learning/) → [AI 학습 데이터 파이프라인](ai-data-pipeline.md) |
| **보행 제어 알고리즘** | [Ex.15 Inverted Pendulum](../../examples/15_Inverted_Pendulum_Control/) → [Ex.17 FSM Gait](../../examples/17_FSM_Gait_Intent/) | [Ex.20~31 알고리즘 시리즈](../../examples/20_Impedance_Control/) |
| **외란 관측 + 투명 모드** | [Ex.21 Gravity Comp](../../examples/21_Gravity_Compensation/) | [Ex.31 DOB Stage 1](../../examples/31_Friction_Comp_DOB/) → [Ex.35 MultiLayer](../../examples/35_MultiLayer_Transparent_Control/) |
| **학습 / 적응 제어** | [Ex.26 ILC](../../examples/26_Iterative_Learning_Control/) | [Ex.27 MRAC](../../examples/27_MRAC/) → [Ex.33 Kinesthetic Teaching](../../examples/33_Kinesthetic_Teaching/) |
| **데이터 수집 / 분석** | [Ex.10c MSC Advanced Log](../../examples/10c_MSC_Advanced_Log/) | [Ex.34 GaitAnalysis Log](../../examples/34_MSC_GaitAnalysis_Log/) → [AI 학습 데이터 파이프라인](ai-data-pipeline.md) |
| **시스템 진단 / 디버깅** | [Ex.18 Debug Monitor](../../examples/18_Debug_Monitor/) | [Ex.19 Memory Aware Design](../../examples/19_Memory_Aware_Design/) |

막혔다면 [docs/troubleshooting.md](../troubleshooting.md) 또는 Claude Code 에 "Ex.XX 막혔어" 라고 말해보세요.
