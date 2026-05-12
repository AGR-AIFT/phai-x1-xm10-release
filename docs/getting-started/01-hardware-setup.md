# 01 — 하드웨어 연결

> 📌 **이 페이지를 읽고 나면**: XM10 보드 + KIT H10 + ST-Link 디버거 + USB 케이블을 올바르게 연결할 수 있습니다.
> ⏱️ 예상 학습 시간: 15분
> 🧰 사전 지식: 없음

---

## 💡 WHY — 왜 정확한 연결이 중요한가

XM10 은 `KIT H10` 본체에서 24 V 전원과 CAN-FD 통신을 받습니다. 한 가닥이라도 잘못 연결되면 보드가 부팅하지 않거나 (전원), 로봇과 통신이 안 되거나 (CAN-FD), 펌웨어를 못 쓰는 (ST-Link) 문제가 발생합니다. 펌웨어 작성보다 **연결 문제로 시간을 가장 많이 잃기 쉽습니다.**

> 🧒 비유: 배선이 잘못된 자동차는 시동 자체가 안 걸립니다. 코드는 그 뒤 문제.

---

## 📖 WHAT — 무엇을 준비하나

### 준비물 체크리스트

| 카테고리 | 항목 |
|---------|------|
| 로봇 | KIT H10 본체 |
| 보드 | XM10 보드, Sensor Hub Module 보드 |
| 케이블 | XM10 ↔ KIT H10 연결 케이블, XM10 ↔ Sensor Hub 케이블 |
| 디버깅 | ST-Link V2 디버거 + 20-to-4 핀 변환 보드 + 4-pin SWD 케이블 |
| 액세서리 | USB-C 케이블 (데이터 모니터링용), SanDisk Ultra Dual Drive Type-C 32 GB (MSC 로깅용) |
| PC | STM32CubeIDE 가 설치된 PC, (선택) Jetson Orin NX 등 AM |

### 사전 호환성 확인 — H10 펌웨어 버전

XM10 은 KIT H10 과 CAN-FD 로 통신하므로 **양쪽 펌웨어 버전이 맞아야** 합니다.

| XM FW | 필요한 KIT H10 FW |
|:---:|:---:|
| **v2.0.0 이상** | **CM v2.3.0 / ESP32 v2.3.0 / SAM10 v2.3.0** |
| v1.0.x | 출하 버전 유지 |

> 버전 불일치 시 → [KIT H10 Firmware 가이드](../kit-h10-firmware/) 의 업데이트 절차

---

## 🔧 HOW — 단계별 연결

### 1단계 — KIT H10 ↔ XM10

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

### 2단계 — ST-Link 디버거 (PC ↔ XM10)

펌웨어 업로드 + 실시간 디버깅용. 처음 1회만 SWD 로 플래시하고, 이후는 PhAI Studio USB FTP 로도 업로드 가능 ([bootloader 가이드](../bootloader/)).

1. ST-Link 디버거 ↔ PC USB 연결
2. ST-Link 의 SWD 출력 ↔ XM10 의 4-pin SWD 포트 (변환 보드 + SWD 케이블)

<div align="center">
    <img src="https://github.com/user-attachments/assets/a0fccf85-d6af-4efe-b6ab-615710f34cec" width="60%" />
    <p><b>Figure 2. ST-Link SWD 핀맵</b></p>
</div>

✅ 체크: 장치 관리자에서 `STMicroelectronics STLink` 가 인식되는지

### 3단계 — Sensor Hub Module (선택)

생체 신호 센서 (EMG, GRF, FSR 등) 를 사용하려면 Sensor Hub 보드를 XM10 의 CAN-FD 확장 포트에 추가 연결합니다. Ex.07 이후 단계까지는 생략 가능.

### 4단계 — USB-C 데이터 케이블 (선택)

PhAI Studio 실시간 모니터링 또는 USB MSC 로깅용. Ex.07 ~ Ex.10c 단계에서 필요.

---

## ⚠️ 흔한 실수 / 막혔다면

- **보드 전원 LED 가 안 켜진다** → ① KIT H10 본체 전원 ON 확인 ② 메인 커넥터 깊게 삽입 ③ 케이블 단선 (육안 + 멀티미터 24 V 측정)
- **ST-Link 인식 안 됨** → 다른 USB 포트 시도. 가능하면 PC 후면 USB-A 직결 (허브 금지)
- **"Target no device found"** → ST-Link 는 인식되는데 MCU 응답 X. 보드 전원 + SWD 4 핀 정렬 (1번 핀 마커 확인)
- **KIT H10 동작하지만 XM10 무반응** → CAN-FD HIGH/LOW 가 거꾸로. 핀맵 (Figure 1) 다시 확인
- **부팅 직후 LED 도 안 보이는데 ST-Link 는 인식됨** → 부트로더 미설치 가능성. [bootloader 가이드](../bootloader/) 참조
- **Sensor Hub 가 동작 안 함** → Hub 보드 자체 펌웨어 버전 확인. XM10 과의 통신은 CAN-FD 위 DOP V3 프로토콜

---

## ➡️ 다음 단계

✅ 모든 연결 + LED 점등 확인 → [02. 개발 환경 구축](02-software-setup.md) 으로 진행

💡 Claude Code 사용자라면 [00-claude-code-quickstart](00-claude-code-quickstart.md) 에서 AI 자동 안내로 한꺼번에 진행 가능.
