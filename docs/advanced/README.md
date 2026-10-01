# Advanced Topics — 심화 주제

자기주도 학습을 위한 출발점입니다. 관심 분야별로 어떤 예제부터 시작하면 좋은지 한눈에 보여줍니다. examples 16, 18, 19 와 API Reference 를 한 번 훑은 뒤 들어오면 흐름이 잡힙니다.

---

## 예제로 바로 시작하기

아래 심화 주제는 `examples/` 폴더에 실습 가능한 예제가 있습니다.

| 주제 | 예제 | 설명 |
| :--- | :--- | :--- |
| **TinyML on STM32** | [Ex.16 — Tiny AI 센서 퓨전](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/16_TinyAI_Sensor_Fusion/) | 상보 필터 + 3-layer NN MCU 추론 |
| **역진자 모델 보행 제어** | [Ex.15 — Inverted Pendulum](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/15_Inverted_Pendulum_Control/) | MgL·sin(θ) 중력 보상 + PD 안정화 |
| **FSM 보행 의도 인식** | [Ex.17 — FSM Gait Intent](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/17_FSM_Gait_Intent/) | 7-phase 보행 FSM + 단계별 보조 토크 |
| **PD 제어 이론** | [Ex.14 — PD Realtime Control](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/14_PD_Realtime_Control/) | 이산 PD 제어 수식 직접 구현 |
| **시스템 디버깅** | [Ex.18 — Debug Monitor](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/18_Debug_Monitor/) | 루프 프로파일링 + Health 대시보드 |
| **메모리 패턴** | [Ex.19 — Memory Aware Design](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/19_Memory_Aware_Design/) | 링 버퍼, 풀 할당자, 이동 평균 |

---

콘텐츠 요청이나 기여는 [GitHub Issues](https://github.com/AGR-AIFT/phai-x1-xm10-release/issues)에서 받고 있습니다.

---

## 자기주도 학습 권장 경로

| 관심 분야 | 추천 출발점 | 다음 |
|----------|-----------|------|
| **AI / ML on MCU** | [Ex.16 TinyAI Sensor Fusion](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/16_TinyAI_Sensor_Fusion/) | [Ex.36 On-Device Kinesthetic Learning](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/36_OnDevice_Kinesthetic_Learning/) → [AI 학습 데이터 파이프라인](ai-data-pipeline.md) |
| **보행 제어 알고리즘** | [Ex.15 Inverted Pendulum](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/15_Inverted_Pendulum_Control/) → [Ex.17 FSM Gait](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/17_FSM_Gait_Intent/) | [Ex.20~31 알고리즘 시리즈](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/20_Impedance_Control/) |
| **외란 관측 + 투명 모드** | [Ex.21 Gravity Comp](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/21_Gravity_Compensation/) | [Ex.31 DOB Stage 1](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/31_Friction_Comp_DOB/) → [Ex.35 MultiLayer](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/35_MultiLayer_Transparent_Control/) |
| **학습 / 적응 제어** | [Ex.26 ILC](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/26_Iterative_Learning_Control/) | [Ex.27 MRAC](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/27_MRAC/) → [Ex.33 Kinesthetic Teaching](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/33_Kinesthetic_Teaching/) |
| **데이터 수집 / 분석** | [Ex.09 CDC Stream](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/09_CDC_Stream/) | → [AI 학습 데이터 파이프라인](ai-data-pipeline.md) |
| **시스템 진단 / 디버깅** | [Ex.18 Debug Monitor](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/18_Debug_Monitor/) | [Ex.19 Memory Aware Design](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/19_Memory_Aware_Design/) |

막혔다면 [docs/troubleshooting.md](../troubleshooting.md) 또는 Claude Code 에 "Ex.XX 막혔어" 라고 말해보세요.
