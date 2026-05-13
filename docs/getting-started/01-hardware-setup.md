# 01 — 하드웨어 연결

XM10 보드, KIT H10 로봇, ST-Link 디버거, USB 케이블 — 이 네 가지를 올바른 순서로 연결합니다. 15 분이면 충분합니다.

XM10 은 KIT H10 본체에서 24V 전원과 통신을 받습니다. 한 가닥이라도 잘못 연결되면 보드가 안 켜지거나, 로봇과 통신이 안 되거나, 펌웨어를 못 쓰는 상황이 생깁니다. 실제로 펌웨어 작성보다 연결 문제로 시간을 더 잃는 경우가 많아요. 처음에 신중하게.

---

## 준비물

| 카테고리 | 항목 |
|---------|------|
| 로봇 | KIT H10 본체 |
| 보드 | XM10 보드, Sensor Hub Module 보드 |
| 케이블 | XM10 ↔ KIT H10 연결 케이블, XM10 ↔ Sensor Hub 케이블 |
| 디버깅 | ST-Link V2 디버거 + 20-to-4 핀 변환 보드 + 4-pin SWD 케이블 |
| 액세서리 | USB-C 케이블 (데이터 모니터링용), SanDisk Ultra Dual Drive Type-C 32 GB (MSC 로깅용) |
| PC | STM32CubeIDE 가 설치된 PC, (선택) Jetson Orin NX 등 AM |

### H10 펌웨어 버전 확인

XM10 과 KIT H10 은 통신으로 묶여 있어서 양쪽 펌웨어 버전이 맞아야 합니다.

| XM FW | 필요한 KIT H10 FW |
|:---:|:---:|
| v2.1.1 (최신) | CM v2.3.0 / ESP32 v2.3.0 / SAM10 v2.3.0 |
| v2.0.x | CM v2.3.0 / ESP32 v2.3.0 / SAM10 v2.3.0 |
| v1.0.x | 출하 시 버전 그대로 |

버전이 안 맞으면 → [KIT H10 Firmware 가이드](../kit-h10-firmware/) 에서 업데이트.

---

## 연결 순서

### 1. KIT H10 ↔ XM10

KIT H10 의 좌측 구동기 어패럴 안쪽에 숨겨진 **확장 케이블** 을 XM10 의 메인 커넥터에 꽂습니다. 이 한 가닥에 전원 + CAN-FD 통신이 모두 들어있습니다.

<div align="center">
    <img src="https://github.com/user-attachments/assets/cac2643d-532b-41a6-a680-7fb57d69d2af" width="90%" />
    <p><b>Figure 1. KIT H10 ↔ XM10 연결</b></p>
</div>

**커넥터 핀맵 (Molex 1053081206):**

| 핀 | 기능 |
|---|---|
| 1 | NC |
| 2 | 24 V |
| 3 | GND |
| 4 | GND |
| 5 | CAN HIGH |
| 6 | CAN LOW |

✅ 체크: 케이블 끝까지 깊게 삽입했는지 (덜 꽂히면 통신만 끊김)

### 2. ST-Link 디버거 (PC ↔ XM10)

펌웨어 업로드 + 실시간 디버깅용. 처음 1회만 SWD 로 플래시하고, 이후는 PhAI Studio USB FTP 로도 업로드 가능 ([bootloader 가이드](../bootloader/)).

1. ST-Link 디버거 ↔ PC USB 연결
2. ST-Link 의 SWD 출력 ↔ XM10 의 4-pin SWD 포트 (변환 보드 + SWD 케이블)

<div align="center">
    <img src="https://github.com/user-attachments/assets/a0fccf85-d6af-4efe-b6ab-615710f34cec" width="60%" />
    <p><b>Figure 2. ST-Link SWD 핀맵</b></p>
</div>

✅ 체크: 장치 관리자에서 `STMicroelectronics STLink` 가 인식되는지

### 3. 센서 허브 (선택)

EMG, 발 접지 (GRF), FSR 같은 센서를 쓰려면 센서 허브 보드를 XM10 의 확장 포트에 추가로 연결합니다. Ex.07 이전까지는 없어도 됩니다.

### 4. USB-C 데이터 케이블 (선택)

PhAI Studio 실시간 모니터링이나 USB 메모리 로깅에 사용합니다. Ex.07 ~ Ex.10c 단계에서 필요해요.

---

## 자주 막히는 부분

- **보드 전원 LED 가 안 켜집니다** — KIT H10 본체 전원 ON 확인 → 메인 커넥터 끝까지 삽입 → 케이블 단선 점검 (멀티미터로 24V 측정)
- **ST-Link 가 인식 안 됩니다** — 다른 USB 포트 시도. 가능하면 PC 후면 USB-A 직결 (USB 허브는 피하세요)
- **"Target no device found"** — ST-Link 는 인식되는데 MCU 가 응답 안 함. 보드 전원 + SWD 4 핀 정렬 (1 번 핀 마커) 확인
- **KIT H10 은 잘 동작하는데 XM10 만 무반응** — CAN 의 HIGH/LOW 가 거꾸로 꽂혔을 가능성. 핀맵 (Figure 1) 다시 확인
- **부팅 직후 LED 도 안 보이는데 ST-Link 는 잡힘** — 부트로더가 안 깔려 있을 수 있어요. [bootloader 가이드](../bootloader/) 확인
- **센서 허브가 동작 안 합니다** — 허브 보드 자체 펌웨어 버전 확인

---

## 다음으로

모든 연결이 끝나고 보드 전원 LED 가 들어왔다면 → [02 개발 환경 구축](02-software-setup.md)

AI 와 함께 한꺼번에 진행하고 싶다면 → [Claude Code 와 함께 시작](00-claude-code-quickstart.md)
