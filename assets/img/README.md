# XM10 이미지 자료 — 보드 사진·다이어그램 가이드

문서·예제 곳곳에 `placeholder` 라고 표시된 자리가 사용자 (각 채널 관리자) 가 직접 채울 이미지 자리예요. 이 폴더에 같은 파일명으로 이미지를 넣으면 자동으로 렌더링됩니다.

이미지 파일이 없어도 문서는 정상 동작해요 (markdown 텍스트 placeholder 만 보임). 우선순위 순서로 채워가면 됩니다.

---

## 우선순위 분류

### Tier 1 — 학생 첫 30 분 동선 (가장 먼저)

처음 보드를 받은 학생이 "내 보드가 작동한다" 를 시각적으로 확인하는 핵심 자리예요. 사진/GIF 가 없으면 학생이 "이게 정상인가?" 에서 막혀요.

| 파일명 | 사용처 | 권장 형식 | 상태 |
|--------|--------|----------|------|
| `board-photo.png` | docs/hardware/README.md 보드 평면도 | 정면 사진 + 인터페이스 라벨 오버레이 | ✅ 등록됨 |
| `rev1.1-photo.png` | docs/hardware/external-gpio-rev1.1.md | Rev 1.1 보드 정면 + 외부 GPIO 헤더 위치 | ✅ 등록됨 |
| `rev2.0-photo.png` | docs/hardware/external-gpio-rev2.0.md | Rev 2.0 보드 정면 + 외부 GPIO 헤더 위치 | ✅ 등록됨 |
| `00_boot_sequence.gif` | Ex.00 Quick Start | 보드 부팅 → 3 LED 시퀀스 (3~5 초) | — |
| `01_btn_led_basic.gif` | Ex.01 Button & LED Basic | 버튼 누르면 LED 점등 (3 초) |
| `02_button_event.gif` | Ex.02 Button & LED Event | BTN1 토글 + BTN2 원샷 효과 (5 초) |
| `03_fsm_transition.gif` | Ex.03 Button & LED FSM | 4 모드 전환 (10 초) |

### Tier 2 — 외부 입출력 + USB 통신 (수업 1 주차~2 주차)

학생이 외부 부품을 연결하기 시작하는 단계. 회로 연결도가 없으면 학생이 "어디에 꽂아야 하지?" 에서 막혀요.

| 파일명 | 사용처 | 권장 형식 |
|--------|--------|----------|
| `04_ext_io_basic.png` | Ex.04 외부 스위치 + LED | 회로 연결도 (실물 사진 + Fritzing 류) |
| `05_pot_to_adc.png` | Ex.05 가변저항 ADC | 가변저항 → ADC 핀 결선 |
| `05a_fsr_voltage_divider.png` | Ex.05a DIO→ADC 전환 | FSR 분압 회로도 |
| `05b_fsr_8ch.png` | Ex.05b FSR 8 ch | FSR 8 채널 배열 사진 |
| `05c_mixed_adc.png` | Ex.05c Mixed ADC | 12 채널 ADC 시스템 |
| `05d_hybrid.png` | Ex.05d DIO+ADC Hybrid | 혼합 모드 회로 |
| `06_safety_fsm.png` | Ex.06 Safety Switch | 3 상태 FSM 다이어그램 |
| `07_cdc_terminal.png` | Ex.07 USB Print | PuTTY/PhAI Studio 콘솔 캡처 |
| `08_sensor_print.png` | Ex.08 Sensor Print | 센서 모니터링 출력 |
| `09_phai_stream.png` | Ex.09 PhAI Stream | PhAI Studio 4 채널 실시간 그래프 |
| `10a_usb_log.png` | Ex.10a USB 메모리 로깅 | USB 메모리 자동 마운트 + 파일 생성 |

### Tier 3 — 외골격 제어 (수업 3 주차~)

KIT H10 외골격을 실제 작동시키는 단계. 동영상이 있으면 학생이 "내 코드로 로봇이 움직였다" 의 임팩트를 느껴요.

| 파일명 | 사용처 | 권장 형식 |
|--------|--------|----------|
| `11_passive_motion.gif` | Ex.11 패시브 모드 | H10 좌·우 왕복 운동 (5~10 초) |
| `12_active_assist.gif` | Ex.12 액티브 어시스트 | 사용자 동작 감지 + 보조 (10 초) |
| `13_resistive.gif` | Ex.13 저항 모드 | 사용자 움직임에 저항 (5 초) |

### Tier 4 — 제어 알고리즘 시각화 (자기주도 학습)

이론 + 시뮬레이션 결과 그래프. 학생이 "이 알고리즘이 뭘 만들어내는지" 한눈에 보는 자료. 영상보다 정적 이미지 (matplotlib / PhAI Studio 캡처) 가 더 적합.

| 파일명 | 사용처 | 권장 형식 |
|--------|--------|----------|
| `14_pd_step.png` | Ex.14 PD 제어 | 스텝 응답 곡선 + Kp/Kd 비교 |
| `15_inverted_pendulum.png` | Ex.15 역진자 | 모델 다이어그램 + 토크 분해 |
| `16_tinyai_posture.png` | Ex.16 TinyAI | NN 자세 분류 실시간 출력 |
| `17_gait_phases.png` | Ex.17 보행 FSM | 7-phase 보행 사이클 다이어그램 |
| `18_health_dashboard.png` | Ex.18 Debug Monitor | Health Dashboard 콘솔 출력 |
| `19_filter_compare.png` | Ex.19 Memory Design | Raw vs Filtered 비교 그래프 |

### Tier 5 — Physical AI 응용 (연구·학기 프로젝트)

Stage 1~5 학습 흐름의 시각 자료. 학회 발표/논문급 자료. 우선순위는 가장 낮지만, 있으면 학생이 "내가 만든 게 이런 수준까지 갈 수 있구나" 동기 부여.

| 파일명 | 사용처 | 권장 형식 |
|--------|--------|----------|
| `20_impedance_phase.png` | Ex.20 임피던스 | 가상 스프링-댐퍼 위상도 |
| `21_transparency.png` | Ex.21 중력 보상 | α 점진 활성화 → 토크 추종 |
| `22_afo_locking.png` | Ex.22 CPG | AFO 위상 락온 곡선 |
| `23_gait_phase_profile.png` | Ex.23 보행 위상 | 위상별 토크 프로파일 |
| `24_bezier_tracking.png` | Ex.24 Virtual Constraint | Bézier 곡선 + PD 추종 |
| `25_stance_blending.png` | Ex.25 입각기 강성 | 입각/유각 LPF 전환 |
| `26_ilc_convergence.png` | Ex.26 ILC | 반복 수렴 곡선 |
| `27_mrac_evolution.png` | Ex.27 MRAC | 게인 적응 궤적 |
| `28_admittance_block.png` | Ex.28 Admittance | 임피던스 vs 어드미턴스 블록도 |
| `29_bilateral_phase.png` | Ex.29 Bilateral | 좌·우 역위상 vs 비대칭 |
| `30_ff_fb_comparison.png` | Ex.30 FF+FB | FF ON/OFF 추종 비교 |

### Tier 6 — 신규 Physical AI 예제 (Ex.31~33, 추가 예제)

가장 최신 연구 시리즈. Ex.31~33 의 README 가 이미 자세히 설명하고 있어서 이미지 없이도 학습 가능하지만, 있으면 좋은 자료.

| 파일명 | 사용처 |
|--------|--------|
| `31_dob_estimate.png` | Ex.31 DOB | 외란 추정 신호 |
| `32_grf_phase.png` | Ex.32 GRF 위상 | 발 접지 + 연속 위상 추정 |
| `33_kinesthetic_pipeline.png` | Ex.33 Kinesthetic | 교시 → 기록 → 재생 파이프라인 |

---

## 권장 제작 도구

| 자료 유형 | 추천 도구 |
|----------|----------|
| 보드 정면 사진 (배경 흰색) | 스마트폰 + 흰 종이 / DSLR |
| 인터페이스 라벨 오버레이 | [draw.io](https://app.diagrams.net/) / Figma / Inkscape |
| 회로 연결도 | [Fritzing](https://fritzing.org/) |
| 동작 GIF | [ScreenToGif](https://www.screentogif.com/) / OBS + ffmpeg |
| 그래프 / 시뮬레이션 결과 | matplotlib (Python) / PhAI Studio 스크린샷 |
| FSM 다이어그램 | draw.io / [PlantUML](https://plantuml.com/) |

---

## 파일 네이밍 규칙

- 소문자 + `_` 구분
- 예제 이미지: `<번호>_<짧은_이름>.<확장자>` — 예제 README 의 placeholder 경로와 일치
- 보드 사진: `<리비전 또는 용도>-photo.png` (예: `board-photo.png`, `rev2.0-photo.png`)
- 권장 해상도: 가로 800~1200 px (GitHub 렌더링 최적)
- GIF 파일은 5 MB 이하 권장 (긴 영상은 mp4 + 외부 호스팅 고려)

---

## 빠진 자료 표시

각 README 의 placeholder 줄은 다음과 같은 형태로 들어있어요.

```markdown
> 📸 `![BTN1 누르면 LED1 점등](../assets/img/01_btn_led_basic.gif)` placeholder
```

이미지가 채워지면 `>` 인용 줄을 일반 markdown 이미지로 바꾸세요.

```markdown
![BTN1 누르면 LED1 점등](../assets/img/01_btn_led_basic.gif)
```
