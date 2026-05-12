# Phase 5 — LED 점등 시각 확인

> 📌 **이 Phase 가 끝나면**: 보드에서 전원 LED + Status LED 가 켜져있음을 확인합니다.
> ⏱️ 예상 시간: 2~5 분
> 🧰 사전 조건: Phase 4 통과 (플래시 완료, 보드 단독 부팅 가능)

## 💡 WHY — 왜 LED 점등 확인이 마지막 관문인가

빌드도 됐고 플래시도 됐다고 해서 보드가 진짜로 동작한다는 보장이 없습니다. 보드의 **전원 LED** 와 펌웨어가 켜는 **Status LED** 가 켜져있어야 시스템이 "심장이 뛰고 있다" 는 신호입니다.

> 🧒 비유: 자동차 시동을 걸었다고 = 계기판 점등 + 엔진 회전 소리. 둘 중 하나라도 없으면 다시 봐야 함.

## 📖 WHAT — 확인할 LED 들

| LED | 의미 | 정상 동작 |
|-----|------|-----------|
| **PWR** (전원 LED) | 보드에 전원 공급 OK | 상시 ON (점등 유지) |
| **STATUS** / **HB** (Heartbeat LED) | 펌웨어가 정상 실행 중 | 일정 주기로 깜빡임 (1 Hz 등) |
| **DEBUG / USR** | 사용자 정의 LED (Ex.00 코드가 사용) | 펌웨어 로직에 따라 변화 |

위치는 보드 silk screen 의 LED 라벨 + [docs/getting-started/01-hardware-setup.md](../../../docs/getting-started/01-hardware-setup.md) 의 사진 참조.

## 🔧 HOW — 확인 단계

### 1. 전원 LED 확인

- 보드를 책상 위에 평평하게 두고 LED 들을 살펴보세요.
- **`PWR`** 라벨이 있는 LED 가 **상시 켜져있어야** 합니다.
- 보드 전원 단자 (USB-C 또는 별도 전원 입력) 가 제대로 꽂혀있는지 재확인.

✅ PWR 점등 확인 시 학생에게 메시지:
```
✅ 전원 LED 점등 확인. 다음으로 Status LED 가 깜빡이는지 확인하세요.
```

### 2. Status / Heartbeat LED 확인

- 펌웨어가 정상이라면 보드에 **`STATUS`** 또는 **`HB`** 라벨 LED 가 깜빡입니다 (보통 0.5~1 Hz).
- 깜빡임이 안 보이면 = 펌웨어가 부팅 직후 hang 또는 main loop 진입 실패.

### 3. 학생에게 시각 확인 질문

AI 가 학생에게 다음 질문을 합니다:

> "보드에서 PWR LED 가 상시 켜져있고, STATUS (또는 HB) LED 가 일정한 주기로 깜빡이고 있나요?
> (a) 둘 다 YES → 다음 단계로
> (b) PWR 만 켜짐, STATUS 안 깜빡임 → 펌웨어 실행 안 됨 (아래 트러블슈팅)
> (c) 둘 다 안 켜짐 → 전원 문제 (아래 트러블슈팅)"

### 4. 통과 시 sentinel 생성

```powershell
New-Item -ItemType File $HOME\.xm10-onboard-done -Force
```

이후 student-onboard skill 은 자동 트리거되지 않습니다.

## ⚠️ 흔한 실수 / 막혔다면

### Case (c) — 둘 다 안 켜짐 (전원 문제)
1. USB 케이블 교체
2. 다른 USB 포트 시도 (PC 후면 직결 권장)
3. 전원 어댑터 사양 확인 (5V/2A 이상 권장)
4. 보드 입력 전압 멀티미터 측정

### Case (b) — PWR 만 켜짐 (펌웨어 hang)
1. CubeIDE Debug perspective 에서 `Restart` (`Ctrl + Shift + B`) → 다시 `Resume` (`F8`)
2. 그래도 hang → main() 진입 후 어디서 멈췄는지 stepping 으로 추적
3. 부트로더 영역 손상 의심 시 [docs/bootloader/](../../../docs/bootloader/) 의 부트로더 재설치 절차
4. Reset 버튼 (보드의 RST/RESET) 한 번 눌러보기

### Case 추가 — STATUS LED 가 너무 빨리 깜빡임
- 펌웨어가 fault handler 에 빠진 경우 (HardFault, error blink pattern)
- Debug perspective 에서 `Suspend` 누르고 PC 가 가리키는 함수 확인

## ➡️ 다음 단계

✅ Phase 5 통과. Phase 6 (학습 시작 핸드오프) 로 진행:
→ [06-handoff.md](06-handoff.md)

🎉 **환경 구축 완료! 이제 진짜 코드를 작성할 시간입니다.**
