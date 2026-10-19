# 05 — USB 메모리에 저장하기

보드가 만든 값을 **USB 메모리에 저장하고, PC 에서 CSV 로 푸는** 과정을 처음부터 따라 합니다. 저장하는 동안에는 PC 가 필요 없습니다. [Ex.10](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/10_USB_Memory_Log/) 으로 한 번 해 보면 전체 흐름이 보입니다.

> 🟢 **Rev 2.0 전용** — Rev 2.0 보드와 Rev 2.0 SDK 에서만 쓸 수 있습니다. Rev 1.1(v2.8.1)에는 USB 메모리 저장이 없습니다. Rev 1.1 에서는 [04. PC 에서 데이터 받기](04-pc-data-tool.md)의 `xm10` 도구로 받으세요.

> ⚠️ **USB-C 포트는 하나입니다.** USB 메모리를 꽂아 두는 동안에는 PhAI Studio · `xm10` 도구 · USB 로 하는 펌웨어 업로드를 쓸 수 없습니다. 저장이 끝나면 USB 메모리를 뽑고 PC 를 연결하세요. (보드 확인 전) <!-- board-check -->

> ⚠️ USB 메모리 저장 전체(저장 · 정지 · 뽑기)와 Ex.10 · Ex.34 는 보드에서 시험하기 전입니다. (보드 확인 전) <!-- board-check -->

| 하고 싶은 것 | 쓰는 방법 |
| :--- | :--- |
| PC 를 연결한 채 실시간으로 보고 저장 | `xm10` 도구 ([04. PC 에서 데이터 받기](04-pc-data-tool.md)) |
| PC 없이 USB 메모리에 저장하고, 나중에 PC 에서 풀기 | 이 문서 |

---

## 준비물

- **Rev 2.0 보드와 KIT H10 본체.** 보드 전원은 H10 본체에서 들어오므로 H10 본체의 전원을 켜 둡니다 ([01 하드웨어 연결](01-hardware-setup.md)).
- **USB 메모리 1개.** 안의 파일이 지워지니 비워도 되는 것을 쓰세요.
- **개발 환경과 Rev 2.0 SDK.** [02](02-software-setup.md) · [03](03-first-build.md) 까지 끝낸 상태입니다.
- **디코더를 설치할 Windows PC.**

---

## 1. USB 메모리 포맷하기

USB 메모리는 **FAT32** 여야 합니다. exFAT 과 NTFS 는 인식하지 않습니다.

1. USB 메모리를 PC 에 꽂습니다. 필요한 파일은 먼저 옮기세요. 포맷하면 모두 지워집니다.
2. 탐색기에서 USB 메모리를 오른쪽 클릭하고 **포맷** 을 고릅니다.
3. **파일 시스템** 을 `FAT32` 로 바꿉니다. 기본값이 exFAT 인 경우가 있습니다.
4. **할당 단위 크기** 를 `32킬로바이트` 로 바꿉니다. 32 KB 를 권장합니다.
5. **시작** 을 누릅니다.

> 32 GB 를 넘는 USB 메모리는 Windows 기본 포맷 창에서 FAT32 를 고를 수 없습니다. 32 GB 이하 메모리를 쓰세요.

---

## 2. 예제 올리기

1. SDK 의 `Examples/10_USB_Memory_Log/usb_memory_log.c` 내용을 통째로 복사해 `XM_Apps/Control_Task/control_task.c` 에 붙여 넣습니다. [03. 첫 빌드](03-first-build.md) 의 "예제 코드 적용" 옵션 A 와 같습니다.
2. 폴더 이름을 정합니다. 시계(RTC)를 맞춰 두었다면 `LOG_SESSION_NAME` 을 `""` 그대로 둡니다. 시계로 `YYMMDD_HHMMSS` 이름이 만들어집니다.
3. 시계를 맞추지 않았다면 `LOG_SESSION_NAME` 을 `"walk01"` 처럼 직접 정합니다. 같은 이름으로 다시 저장하면 같은 폴더에 이어서 저장되니, 저장할 때마다 다른 이름을 쓰세요.
4. 빌드하고 보드에 올립니다. [03. 첫 빌드](03-first-build.md) 의 3 · 4 단계와 같습니다.

시계는 `XM_RTC_SetDateTime()` 으로 맞춥니다 ([08. 실시간 시계](../api-reference/08-rtc-clock.md)).

---

## 3. 저장하기

1. 보드의 USB-C 포트에 USB 메모리를 꽂습니다. **LED 1 이 켜지면** 저장할 수 있습니다.
2. **버튼 1** 을 짧게 누릅니다. 저장을 시작합니다.
3. 파일을 여는 동안 LED 2 가 빠르게 깜빡입니다. 이어서 **LED 2 가 천천히 깜빡이면** 저장 중입니다.
4. 저장하는 동안 **버튼 2** 를 누르면 표시(마커)가 들어갑니다. 버튼 1 · 2 · 3 의 눌림 상태도 값으로 저장됩니다.
5. 몇 초 저장한 뒤 **버튼 1** 을 다시 누릅니다. 저장을 멈춥니다. 시작하자마자 바로 멈추면 기록이 하나도 없는 빈 폴더가 남을 수 있습니다.
6. LED 2 가 빠르게 깜빡이는 동안은 파일을 닫는 중입니다. **뽑지 말고 기다리세요.**
7. LED 2 가 켜진 채로 있으면 저장이 끝난 것입니다. 이제 USB 메모리를 뽑아도 됩니다. (보드 확인 전) <!-- board-check -->

| 버튼 | 하는 일 |
| :--- | :--- |
| 버튼 1 | 저장 시작 / 정지 (짧게 누름) |
| 버튼 2 | 저장 중에 표시(마커) 넣기 |
| 버튼 3 | 마지막 저장의 통계를 PC 로 보내기 (아래 4 단계) |

| LED | 뜻 |
| :--- | :--- |
| LED 1 켜짐 | USB 메모리가 준비되었습니다 |
| LED 2 느린 깜빡임 | 저장 중입니다 |
| LED 2 빠른 깜빡임 | 파일을 열거나 닫는 중입니다. USB 메모리를 뽑지 마세요 |
| LED 2 켜짐 | 저장이 끝났습니다 |
| LED 3 켜짐 | 오류입니다. 시작하지 못했거나 저장이 스스로 끝났습니다. 버튼 1 을 다시 누르면 꺼집니다 |
| LED 3 짧게 한 번 | 표시(마커)를 넣지 못했습니다. 저장은 계속됩니다 |

저장하는 동안에는 USB 메모리가 꽂혀 있어서 PC 와 연결할 수 없습니다. 상태는 LED 로 봅니다. 저장 중에는 USB 메모리를 뽑지 마세요.

---

## 4. 통계 보기 (선택)

저장이 끝난 뒤에 USB 메모리를 뽑고 PC 를 연결해서, 마지막 저장의 통계를 볼 수 있습니다. (보드 확인 전) <!-- board-check -->

1. USB 메모리를 뽑습니다.
2. PC 와 보드를 USB-C 케이블로 연결하고, 시리얼 터미널(PuTTY · Tera Term 등)로 보드의 COM 포트를 엽니다. PC 프로그램은 한 번에 하나만 연결하세요.
3. **버튼 3** 을 누르면 터미널에 한 줄이 나옵니다. (보드 확인 전) <!-- board-check -->

```
[USB Log] records=<n> bytes=<n> duration_ms=<n> empty_slots=<n> longest_gap_ms=<n> overflow_records=<n> write_errors=<n> buffer_peak_percent=<n> start_error=<n>
```

| 항목 | 보는 것 |
| :--- | :--- |
| `empty_slots` | 빠진 표본 수. 0 이면 빠진 것이 없습니다 |
| `longest_gap_ms` | 가장 긴 공백 [ms] |
| `overflow_records` | 내부 버퍼가 넘쳐 저장하지 못한 레코드 수. 1 이상이면 그 저장은 거기서 멈춥니다 |
| `write_errors` | USB 메모리 쓰기 실패 횟수 |
| `start_error` | 마지막 시작의 결과. 0 이면 시작한 것입니다 |

이 예제는 일반 터미널용 설정이라 PhAI Studio 의 실시간 화면은 나오지 않습니다. 실시간 그래프는 [Ex.09](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/09_CDC_Stream/) 를 보세요.

---

## 5. PC 에서 풀기 — XM10 Log Decoder

저장한 폴더는 **XM10 Log Decoder** 로 풉니다. Windows 에서 쓰는 설치형 프로그램이고, 파이썬은 필요 없습니다. <!-- release-check -->

이 문서는 XM10 Log Decoder 2.2.0 이상 기준입니다. <!-- release-check -->

### 디코더 받기

1. 릴리스 페이지의 첨부 파일(Assets)에서 설치 파일(`XM10LogDecoder_Setup_` 으로 시작하는 `.exe`)을 받습니다. <!-- release-check -->
2. 브라우저가 `.exe` 다운로드를 막으면 같은 이름의 `.zip` 으로 받아 압축을 풉니다. <!-- release-check -->
3. 받은 파일이 맞는지 확인하려면 첨부된 `SHA256SUMS.txt` 와 비교합니다. PowerShell 에서 `Get-FileHash <받은 파일>` 을 실행하면 값이 나옵니다. <!-- release-check -->
4. 설치 파일을 실행합니다. 처음 실행할 때 Windows 가 "알 수 없는 게시자" 경고를 보여 줄 수 있습니다. **추가 정보** → **실행** 을 누르세요. <!-- release-check -->
5. 설치가 끝나면 시작 메뉴에서 **XM10 Log Decoder** 를 실행합니다. <!-- release-check -->

### 디코더로 풀기

1. USB 메모리의 `LOGS` 폴더를 PC 로 복사합니다. USB 메모리를 PC 에 꽂은 채로 풀어도 됩니다. <!-- release-check -->
2. 디코더에서 `USB 탐지` 를 누르면 PC 에 꽂은 USB 메모리의 `LOGS` 폴더를 찾습니다. PC 로 복사한 폴더는 `폴더 선택` 으로 고릅니다. <!-- release-check -->
3. 세션 목록에서 풀 세션을 고르고 `전체 디코딩` 이나 `선택 디코딩` 을 누릅니다. <!-- release-check -->
4. 결과는 세션 폴더에 만들어집니다. 출력 폴더를 따로 정할 수도 있습니다. <!-- release-check -->

### 결과 파일

XM10 Log Decoder 2.2.0 이상이 만드는 파일입니다. <!-- release-check -->

| 파일 | 내용 |
| :--- | :--- |
| `decoded_output.csv` | 1000 Hz 로 저장한 데이터를 푼 표 |
| `decoded_output_<주기>hz.csv` | 1000 Hz 가 아닌 주기로 저장한 데이터를 푼 표. 예: 100 Hz 는 `decoded_output_100hz.csv` |
| `events.csv` | 표시(마커)를 넣었을 때 모아 둔 표 |
| `data_quality_report.txt` | 저장 품질 보고서 — 판정과 이유 |

- 빠진 표본은 보간으로 메우고 `row_origin` 열에 `1` 로 표시합니다. 측정한 값은 `0` 입니다. 보간한 값은 측정값이 아닙니다.
- Ex.10 은 100 Hz 로 저장하므로 `loop_count` 는 보통 10 씩 늘어납니다. (보드 확인 전) <!-- board-check -->

### 판정 읽기

`data_quality_report.txt` 에서 판정을 확인하세요. 디코더의 로그 창에도 세션마다 줄 수 · 빠진 표본 · 판정이 한 줄로 나옵니다. <!-- release-check -->

아래는 XM10 Log Decoder 2.2.0 이상 보고서 기준입니다. <!-- release-check -->

| 판정 | 뜻 | 받아들이나 |
| :--- | :--- | :---: |
| `GOOD` | 빠진 표본이 없습니다 | 예 |
| `USABLE` | 작은 공백만 있습니다(가장 긴 공백 4 ms 이하). 수용합니다 | 예 |
| `WARNING` | 공백을 보간으로 메웠습니다. 수용합니다. 1000 Hz 는 가장 긴 공백 20 ms, 100 Hz 는 10 ms(한 칸)까지입니다 | 예 |
| `RE-COLLECT` | 다시 저장하기를 권합니다. 빠진 표본이 많거나 공백이 깁니다 | 아니오 |
| `INVALID` | 저장 설정이나 파일에 문제가 있습니다 | 아니오 |
| `UNVERIFIED` | 빠진 표본을 셀 수 없어 판정하지 못합니다 | 아니오 |

- 1000 Hz · 100 Hz 가 아닌 저장 주기의 허용 공백은 실제 보드 로그로 확인하지 않은 계산값입니다. (보드 확인 전) <!-- board-check -->
- 저장 주기가 다른 로그의 판정은 그대로 비교하지 마세요.
- `RE-COLLECT` 이면 USB 메모리를 FAT32(할당 단위 32 KB)로 전체 포맷한 뒤 다시 저장하거나 다른 USB 메모리로 바꿔 보세요. 저장 주기를 낮추거나 구조체를 줄여도 됩니다.
- `RE-COLLECT` 가 계속되면 Angel Robotics(엔젤로보틱스) CS팀에 문의해 주세요.

---

## 6. 다음 단계

- **내 값을 저장하기** — Ex.10 에서 `LogRecord_t` 와 `LOG_METADATA` 를 같이 고칩니다. 필드를 바꾸면 메타 줄도 바꿔야 하고, 크기의 합이 다르면 저장을 시작하지 않습니다. 규칙은 [06. USB 메모리 저장](../api-reference/06-usb-data-logging.md#meta-line) 에 있습니다.
- **H10 보행 데이터를 1 kHz 로 저장하기** — [Ex.34](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/34_USB_Memory_Gait_Log/) 입니다. 구조체 하나(96 B)를 1 ms 마다 저장합니다. 처음이면 Ex.10 을 먼저 해 보세요.
- Ex.34 는 H10 을 연결하고 착용자가 입은 뒤 버튼 1 로 시작합니다. 버튼 2 는 마커 1(걷기 시작), 버튼 3 은 마커 2(이벤트)입니다.
- Ex.34 를 빌드하기 전에 `_SetupBodyData()` 의 신체 정보(체중 · 키 · 다리 길이)를 착용자의 값으로 고치세요. 예시 값 그대로면 무릎 · 허벅지 각도가 부정확합니다.
- Ex.34 의 기본 설정(`USE_H10_ASSIST 0`)은 기록만 합니다. 토크나 제어 명령을 보내지 않습니다.
- 1000 Hz 로 저장하면 표본이 일부 빠질 수 있습니다. 빠진 위치와 길이는 디코더가 알려 줍니다.
- Ex.34 에서 H10 값이 갱신되지 않으면 디코더는 저장 시각(레코드마다 함께 저장되는 시각)으로 시간 기준을 만듭니다. (보드 확인 전) <!-- board-check -->

> ⚠️ **안전**: Ex.34 에서 `USE_H10_ASSIST` 를 1 로 바꾸면 H10 의 기존 보조 알고리즘이 켜져 착용자에게 보조 토크가 걸립니다. 착용자와 주변의 안전을 확인한 뒤에만 켜세요.

---

## 뭔가 이상할 때

| 증상 | 확인할 것 |
| :--- | :--- |
| LED 1 이 안 켜진다 | USB 메모리가 FAT32 인지 확인하세요. exFAT · NTFS 는 인식하지 않습니다. 꽂은 직후에는 잠깐 걸릴 수 있으니 다시 꽂아 보세요 |
| 버튼 1 을 눌렀더니 LED 3 이 켜졌다 | 시작하지 못한 것입니다. 디버거(Live Expressions)로 `g_last_start_error` 값을 보고 [시작 실패 이유](../api-reference/ref/xm-api-usb-msc.md#xmusblogstarterror_e) 표에서 찾으세요 |
| 저장하다가 LED 3 이 켜졌다 | 저장이 스스로 끝난 것입니다. USB 메모리가 빠졌거나 내부 버퍼가 넘친 경우 등이 있습니다 |
| 폴더 이름이 날짜가 아니거나 이전 저장과 같다 | 시계를 맞추지 않았을 수 있습니다. `XM_RTC_SetDateTime()` 으로 맞추거나 `LOG_SESSION_NAME` 을 직접 정하세요 |
| PhAI Studio · `xm10` 도구가 보드를 못 찾는다 | USB 메모리가 꽂혀 있으면 PC 와 연결되지 않습니다. 위의 "USB-C 포트는 하나입니다" 를 보세요 |
| 디코더 판정이 `RE-COLLECT` | 위의 "판정 읽기" 를 보세요 |

더 많은 해결 방법은 [Troubleshooting](../troubleshooting.md) 에 있습니다.

---

**다음:** [06. USB 메모리 저장](../api-reference/06-usb-data-logging.md) 에서 함수와 규칙을 자세히 보거나, [Tutorials](../tutorials/README.md) 로 넘어가세요.
