# Phase 4 — 보드 플래시 (펌웨어 다운로드)

> 📌 **이 Phase 가 끝나면**: `.elf` 가 보드 MCU 의 flash 메모리에 쓰여 부팅됩니다.
> ⏱️ 예상 시간: 5 분
> 🧰 사전 조건: Phase 3 통과 (Extension_Module.elf 생성), XM10 보드 + ST-Link 디버거 + USB 케이블

## 💡 WHY — 왜 플래시가 필요한가

`.elf` 는 학생 PC 의 파일일 뿐, 보드의 MCU 는 모릅니다. **플래시** 는 그 `.elf` 를 보드 내부의 flash 메모리 (전원 꺼도 유지되는 ROM 영역) 에 복사하는 작업입니다. 이후 보드가 부팅하면 flash 의 코드를 실행합니다.

> 🧒 비유: 책 (.elf) 을 도서관 책장 (flash) 에 꽂는 일. 꽂혀 있어야 다음 사람이 와서 읽을 수 있음.

## 📖 WHAT — 필요한 도구

- **XM10 보드** — 본인 리비전 (Rev1.1 또는 Rev2.0)
- **ST-Link 디버거** (V2 또는 V3) — MCU 와 PC 잇는 USB 장치. CubeIDE 설치 시 드라이버 자동 설치됨.
- **USB 케이블** — ST-Link 와 PC 연결용 (Mini USB 또는 Micro USB, ST-Link 모델에 따라)
- **20-pin (또는 SWD 4-pin) cable** — ST-Link 와 XM10 보드 잇는 케이블

## 🔧 HOW — 단계별 진행

### 1. HW 연결

1. XM10 보드의 **SWD 커넥터** 위치 확인 ([docs/getting-started/01-hardware-setup.md](../../../docs/getting-started/01-hardware-setup.md) 참조)
2. ST-Link 디버거 ↔ XM10 SWD 커넥터 연결
3. ST-Link ↔ PC USB 연결
4. XM10 보드에 별도 전원 공급 (배터리 또는 외부 전원 — ST-Link 만으로는 부족할 수 있음)

### 2. ST-Link 인식 확인

PowerShell:
```powershell
# 장치 관리자에 STMicroelectronics STLink 가 보이는지
Get-PnpDevice -FriendlyName "*STLink*" -Status OK
```

✅ 한 줄 이상 출력되면 인식 성공. 안 나오면 USB 케이블/포트 교체.

### 3. CubeIDE 에서 플래시

CubeIDE 메뉴:
- `Run` → `Debug As` → `STM32 C/C++ Application`
- 또는 툴바의 **벌레 아이콘** (Debug) 클릭

자동 진행 순서:
1. ST-Link 가 MCU 와 통신
2. flash erase
3. `.elf` 전송 (수 초)
4. flash 검증
5. MCU halt (정지) + Debug perspective 자동 전환

✅ CubeIDE 가 **Debug perspective** 로 전환되고, main() 함수 진입 위치에서 멈춰 있으면 성공.

### 4. 보드 단독 부팅으로 전환

학생 코드를 보드에서 자율 실행하려면:
- CubeIDE 의 빨간 정사각형 (Terminate) 클릭 또는 `Ctrl + F2`
- 또는 Debug perspective 의 `Resume` (`F8`) 클릭 후 보드가 정상 동작하는지 확인

이후 보드 전원만으로 펌웨어 실행됨 (PC 연결 불필요).

## ⚠️ 흔한 실수 / 막혔다면

- **"No ST-Link detected"** → USB 케이블 불량 또는 포트 문제. 다른 USB 포트 시도. 가능하면 USB-A 직결 (허브 X).
- **"Target no device found"** → ST-Link 는 인식되지만 MCU 응답 X. 보드 전원 공급 확인. SWD 케이블 핀 정렬 확인.
- **"Old firmware on ST-Link, please update"** → CubeIDE 가 ST-Link 펌웨어 업데이트 권유. `Yes` → 잠시 대기.
- **플래시 도중 멈춤 → "Connection error"** → ST-Link 의 reset 설정 문제. `Run` → `Debug Configurations` → `Debugger` 탭 → `Reset Behaviour: Connect under reset` 로 변경 후 재시도.
- **부트로더 영역까지 지워졌나 걱정** → 본 SDK 는 application 영역만 쓰므로 부트로더에 영향 없음. 만약 부트로더 자체를 잘못 건드린 경우 [docs/bootloader/](../../../docs/bootloader/) 참조.
- **보드에 전원이 안 들어옴** → 전원 LED 가 안 켜진 경우. USB 가 아닌 별도 전원 공급 필요할 수 있음. 보드 라벨 또는 hardware-setup.md 참조.

## ➡️ 다음 단계

✅ Phase 4 완료. Phase 5 (LED 점등 확인) 으로 진행:
→ [05-verify-leds.md](05-verify-leds.md)
