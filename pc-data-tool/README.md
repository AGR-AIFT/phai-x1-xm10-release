# XM10 USB 데이터 다루기 — `xm10` 도구 (pc-data-tool 폴더)

이 폴더가 `xm10` 도구입니다 (사용법: [04. PC 에서 데이터 받기](../docs/getting-started/04-pc-data-tool.md)).
XM10 보드가 USB-CDC 로 내보내는 데이터를 PC 에서 받아 그래프로 보고 저장하는 파이썬 도구이며,
아래는 코드를 직접 고치거나 다른 언어로 옮기려는 분(자체 GUI 제작, 커스텀 후처리, 포팅 등)을 위한
안쪽 구조 설명입니다. XM10 의 PhAI 와이어 프로토콜(SOF/LEN/SEQ_ID/MODULE_ID/CRC16 + COBS)을
직접 파싱하는 실제 동작 코드입니다.

> **USB 메모리(MSC) 파일 로깅 기능은 XM10 v2.5.0 에서 제거되었습니다.** 데이터 수집은 이제
> USB-CDC 실시간 스트리밍으로 합니다. PhAI Studio 는 아직 개발 중이라, 직접 정의한 데이터
> 구조체(커스텀 구조체)는 우선 `xm10` 도구로 보고 저장하세요. 온보드 저장(SD카드)은 현재
> 지원하지 않습니다.

---

## 여기서 시작 — `xm10.py`

목적별 스크립트를 하나씩 찾아 열 필요 없이, 진입점 하나로 다 됩니다.

```bash
python xm10.py demo                  # 보드 없이 전 구간 시연 (처음이면 이것부터)
python xm10.py ports                 # 연결된 포트 나열 + XM10 후보 표시
python xm10.py recv                  # 실시간 수신 (그래프 GUI)
python xm10.py recv --cli --log      # 실시간 수신 (콘솔) + .xmlog 저장
python xm10.py soak --minutes 30     # 보드 실측 — 손실 0 인지 판정
python xm10.py export FILE --csv DIR # .xmlog 를 요약하거나 CSV 로
python xm10.py selftest              # 보드 없이 도는 자체 검증 (창을 띄우는 시험은 python run_tests.py)
```

### `demo` 가 무엇을 보여주나

보드가 없어도 **앱 전체**가 한 번 돕니다. 가짜인 것은 바이트를 만드는 곳
(`CDC/demo_stream.py`) 하나뿐이고, 그 뒤로는 보드를 꽂았을 때와 같은 코드입니다.

```
합성 와이어 -> COBS/CRC 파싱 -> 실시간 타입 디코딩 -> .xmlog -> 되읽기 -> CSV -> 판정
```

일부러 어긋나기 쉬운 자리를 전부 밟습니다 — 0x20 의 365→368 B 패딩, 패딩 구멍이
둘 있는 사용자 구조체, **데이터가 스키마보다 먼저 도착하는** 실제 FW 순서, seq 구멍,
CRC 가 깨진 프레임, 그리고 시리얼이 프레임 경계를 지켜 주지 않는다는 사실(스트림을
일부러 어중간한 크기로 잘라 먹입니다). 17개 항목을 판정해 PASS/FAIL 을 냅니다.

### 실행파일로 배포하기

```bash
python build_exe.py            # GUI 포함 (약 56 MB)
python build_exe.py --no-gui   # 콘솔 전용 (약 23 MB)
```

만든 뒤 **그 exe 를 실제로 실행해** `selftest` · `demo` · `recv -h` 까지 돌려 봅니다. 이
프로젝트에서 "빌드가 됐다"는 "동작한다"가 아닙니다 — PyInstaller 는 동적 import 를 못
따라가서, 모듈이 번들에서 빠져도 빌드는 성공하기 때문입니다.

개발 PC 에 torch·scipy·pandas 같은 무거운 패키지가 깔려 있어도 상관없습니다 — 앱이 안 쓰는
것은 스크립트가 제외합니다.

> `--no-gui` 빌드에서는 **실시간 디코딩 경로가 검사되지 않습니다**(수신기 모듈이
> PyQt5 를 필요로 함). `selftest` 가 그 사실을 출력에 적습니다. 전체 검사는 소스에서
> `python run_tests.py`.

---

## ⚠️ 다루는 범위 (중요)

이 샘플이 의미 있게 디코딩하는 채널입니다.

- 예전 방식의 소형 센서 모듈 채널 (Module ID `0x01~0x07`, `0x10`, `0xFF`)
- 사용자 정의 커스텀 채널 (Module ID `0xF0~0xFE`) — [Ex.09 CDC Stream](../examples/09_CDC_Stream/) 방식
- **Total Data Packet (Module ID `0x20`)** — 197채널, 365 B. `total_data_decoder.py` 담당

0x20 은 원래 이 샘플이 못 풀던 패킷입니다. uint32/uint16/uint8/float 가 섞인 구조체라
"payload 를 float 배열로 해석"하는 방식으로는 값이 깨졌고, 예전 README 는 구조체 레이아웃에
맞춰 직접 `struct.unpack` 을 확장하라고 안내했습니다. 그 손작업은 필드가 하나 바뀔 때마다
사람이 따라가야 해서, 실제로는 아무도 최신 상태로 유지하지 못하는 종류의 일이었습니다.

이제는 레이아웃을 펌웨어와 같은 정의에서 생성한 표(`xm_total_data_map.py`)가 갖고 있습니다.

디코딩하지 않는다는 것이 무시한다는 뜻은 아닙니다. `frame_router.py` 가 시스템 채널
(`0x20`/`0xEF`/`0xED`/`0xEE`)을 사용자 채널과 **분리해서 따로 셉니다** — 그래프와 CSV 에는
들어가지 않지만 몇 프레임이 왔는지는 화면 위쪽 통계줄의 `Sys[...]` 에 그대로 표시됩니다
(프레임 수만 나옵니다. 바이트 수는 `--cli` 로 받다가 Ctrl+C 로 끝낼 때의 요약에 있습니다).
예전 버전은 이 패킷을 사용자 채널에 섞어 넣어 CSV 컬럼 수가 헤더와 어긋나곤 했습니다.

## `CDC/cdc_phai_receiver.py` — 실시간 모니터링 GUI

PhAI V2.2 프로토콜을 실시간 수신하여 그래프 표시 + CSV 저장합니다.

```bash
python CDC/cdc_phai_receiver.py                 # GUI 모드
python CDC/cdc_phai_receiver.py --cli --port COM6   # CLI 모드 (CSV만 저장)
```

주요 기능: OpenGL 6-plot 실시간 그래프, 센서 도메인별 자동 그룹핑, 채널 visibility 토글,
처리량(pkt/s, KB/s) 표시, Freeze/Screenshot/테마 토글, Auto-reconnect, CSV 자동 저장.

의존성:
```bash
pip install pyserial pyqt5 pyqtgraph numpy
```

## `CDC/frame_router.py` — 프레임 파싱·라우팅 (GUI/CLI 공용)

와이어 프로토콜을 다루는 부분만 따로 뺀 모듈입니다. Qt 도 pyserial 도 쓰지 않아서
단독으로 import 해 쓸 수 있습니다 — 자체 도구를 만든다면 이 파일만 가져가면 됩니다.
필요한 외부 패키지는 **numpy 하나**입니다(`PhAIFrame.as_float32()` 가 씁니다).

- `parse_phai_frame()` — COBS 를 푼 프레임 하나를 검증해 `PhAIFrame` 으로
- `FrameRouter` — 프레임이 사용자 채널인지 시스템 채널인지 판정. 사용자 module_id
  (0xF0~0xFE) 는 **전부** `'user'` 로 라우팅되고, `router.user_modules` 에 처음 본
  순서대로 module별 관측 상태가 쌓입니다 — GUI 는 그 순서로 탭을 만듭니다
  (다중 module 실시간 뷰. 예전엔 처음 본 module 하나만
  화면·CSV 에 반영하고 나머지는 세기만 했습니다). 프레임을 받는 스레드와 화면을 그리는
  스레드가 다르면 `router.user_modules_snapshot()` 으로 읽으세요 — `user_modules` 를 직접
  순회하면 새 module 이 처음 들어오는 순간 오류가 납니다.
- `GlobalSequenceLedger` — 패킷 손실 집계

GUI 는 module_id 별로 탭 하나(사이드바 + 6-plot + CSV)를 만듭니다. 탭 바로 위에 있는
통계줄에 나오는 값들:

| 표시 | 뜻 |
|---|---|
| `Good` | CRC 까지 통과한 **전체** 프레임 수 (시스템 채널 포함) |
| `SEQ↓` | 와이어에서 실제로 잃은 프레임 수 |
| `QOvf` | PC 가 못 따라가 큐에서 버린 수 — 케이블 문제가 아니라 **로컬 지연**입니다 |
| `Sys[...]` | 시스템 채널별 수신 프레임 수 |
| `Resync` | seq 가 뒤로 간 횟수 — 손실이 아니라 기준을 다시 잡은 것 (있을 때만) |

통계줄 위 작은 패널에는 module 별 행(frames / lost(전역 원장) / schema 출처
`[0xEE]`·`[0xEF]`·`[float32 가정]`)이 나옵니다.

`seq_id` 는 module 과 무관하게 펌웨어 카운터 하나가 발급하므로, 손실 계산은 **module 을
나누기 전에** 전체 프레임을 대상으로 해야 합니다. 특정 module 의 프레임끼리만 비교하면
그 사이에 낀 다른 module 프레임이 전부 손실로 잡힙니다. `FrameRouter.route()` 가 이 순서를 강제합니다.

### 검증

보드 없이 돌아갑니다. `data/` 의 실제 캡처를 와이어로 되돌려 재생하는 회귀 시험까지 포함합니다.

```bash
python CDC/test_frame_router.py
```

## 사용자 채널에 이름이 붙습니다

FW 는 `XM_SetUsbCustomMeta()` 로 등록한 채널 이름·단위를 **이미 보내고 있습니다**
(Module ID `0xEF`). 예전 버전은 그걸 받고도 세기만 하고 버려서, 사용자 채널이 늘
`ch0, ch1...` 로만 나왔습니다. 이제 파싱해서 씁니다.

```c
/* FW 쪽 (Ex.09 와 동일) */
XM_SetUsbCustomMeta(0xF0, "[{\"name\":\"Left Hip Angle\",\"unit\":\"deg\"}, ...]");
XM_SendUsbDataWithId(&s_debug, sizeof(s_debug), 0xF0);
```

→ 그래프·CSV 에 `Left Hip Angle` 로 나옵니다. 메타가 데이터보다 늦게 와도
`.xmlog` 로 저장했다면 **사후 내보내기에서는 전부 이름이 붙습니다.**

> 실시간 CSV 는 module 마다 **첫 프레임**을 기준으로 열을 정합니다. 그 뒤에 타입을 알려 주는
> 정보(`0xEE`)가 도착해서 채널 수나 값을 읽는 방식이 달라지면, 그 뒤 프레임은 열이 밀리거나
> 값의 뜻이 섞이지 않도록 값 대신 받은 바이트를 hex 로 적습니다(화면 위쪽 상태 패널에
> `CSV hex행 N` 으로 표시). Review CSV 뷰어는 그런 행의 값은 그리지 않고(그 구간은 빈 채로
> 남습니다) 시간·seq·Tx drop 은 그대로 세므로, 패킷 손실 통계는 어긋나지 않습니다. 이름과 값이
> 모두 맞는 CSV 가 필요하면 `.xmlog` 에서 다시 뽑으세요.

> ⚠ **`0xEF` 는 이름과 단위만 알려줍니다 — 타입은 모릅니다.** 그래서 값은 float32 로
> **가정**해서 풉니다. 정수나 혼합 타입 struct 를 보내면 값이 깨집니다. 도구가 그럴 때
> `⚠ 타입 미상 — float32 가정` 이라고 표시합니다.
>
> PC 도구는 타입 정보가 오면 그대로 씁니다. 지금 보드는 이름만 보냅니다.

## 무손실 저장 — `.xmlog`

**해석하기 전에 먼저 눕힌다.** CSV 는 해석의 결과물이라 스키마를 모르면 아무것도 못 적는데,
스키마는 데이터보다 늦게 올 수도 아예 안 올 수도 있습니다. 그래서 받은 바이트를 그대로
저장하고, CSV 는 나중에 뽑습니다 — 디코더를 고치면 예전 로그도 같이 고쳐집니다.

```bash
# 받아서 그대로 저장 (스키마를 몰라도 적는다)
python CDC/cdc_phai_receiver.py --cli --port COM6 --log

# 나중에 뽑기
python CDC/xmlog_export.py data/cdc_20260910_120000.xmlog             # 요약
python CDC/xmlog_export.py data/cdc_20260910_120000.xmlog --csv out/  # 모듈별 CSV
python CDC/xmlog_export.py data/cdc_20260910_120000.xmlog --dump 20   # 레코드 훑어보기
```

저장되는 것:

| 레코드 | 내용 |
|---|---|
| SESSION | 언제·어떤 FW·어떤 데이터맵으로 받았는지. 파일 맨 앞에 있습니다. 스크립트가 재연결 뒤에도 같은 파일에 이어 적으면 그때마다 하나씩 더 생깁니다 |
| DATA | CRC 통과한 프레임의 payload **원본 바이트**. 시스템 채널(0x20)도 포함 |
| GAP | 잃은 구간 — seq 갭 / 큐 오버플로 / 링크 리셋 |
| SCHEMA_ACTIVATION | 보드가 보낸 이진 스키마(0xEE)를 받는 순간 그대로 기록. 펌웨어가 아직 안 보내는 동안은 없습니다 |

**중간에 끊겨도 앞부분은 삽니다.** 파일을 앞에서부터 검증하다 첫 실패에서 멈추는 구조라,
크래시로 마지막 레코드가 잘리면 **그 하나만** 버려집니다. 바이트 단위 truncation 을 전부
시험합니다(`test_xmlog.py`).

어디까지 믿을 수 있나: 프로그램이 죽어도 디스크로 내보낸(flush) 레코드는 남습니다. 아직 내보내지
않은 마지막 몇 백 개(기본 256개 단위)는 프로그램과 함께 사라질 수 있습니다. SESSION 과
SCHEMA_ACTIVATION 은 적는 즉시 내보내므로, 스키마 번호(`activation_id`)를 단 데이터가 남았는데 그
스키마 기록이 없는 파일은 생기지 않습니다. 스키마가 오기 전에 받은 행(`activation_id` 0)은 원본
바이트로만 남습니다. **전원이 갑자기 꺼지는 경우까지는 보장하지 않습니다.**

CSV 로 뽑을 때 채널 이름의 출처는 이 순서입니다:

1. **0x20** → 펌웨어와 같은 정의에서 생성된 맵 (197채널 이름·단위·스케일)
2. **SCHEMA_ACTIVATION** → 보드가 보낸 이진 스키마 *(펌웨어가 보내기 시작하면)*
3. 없으면 → float32 배열로 **가정**하고 `ch0..chN`

> 3번은 가정입니다. FW 는 임의 바이트를 보낼 수 있으므로(정수·혼합 struct) 값이 깨질 수
> 있습니다. 그럴 땐 `--raw-hex` 로 원본 바이트를 그대로 뽑으세요 — **해석이 틀려도 데이터는
> 잃지 않습니다.** 그게 `.xmlog` 를 먼저 쓰는 이유입니다.

### 스키마가 늦게 오거나 도중에 바뀌어도 — `activation_id`

DATA 마다 `activation_id` 가 붙습니다. **이 행을 어느 스키마로 풀어야 하는지**의 번호이고,
CSV 의 세 번째 열로도 나옵니다.

| 값 | 뜻 |
|---|---|
| `1, 2, …` | 그 행을 받을 때 유효했던 스키마의 번호. 같은 스키마가 다시 오면(재연결 등) 같은 번호를 씁니다 |
| `0` | 스키마 없이 받은 행. 스키마보다 먼저 왔거나, 재연결 직후 새 스키마가 오기 전입니다. 바이트는 그대로 저장됩니다 |

내보낼 때는 파일 끝의 스키마 하나로 전부 푸는 것이 아니라 **행마다 그 시점의 스키마**로 풉니다.
`0` 인 행은 같은 연결 안에서 그 뒤에 처음 오는 스키마로 풀고, 끝까지 스키마가 없으면
float32 가정으로 내려갑니다.

- **한 파일 안에서 스키마가 바뀌었다면** (예: 스크립트가 재연결 뒤에도 같은 파일에 이어 적은 경우 —
  그래프 창은 연결마다 새 파일을 엽니다) 같은 모듈의 CSV 가 스키마마다 나뉩니다 — 처음 쓰인 스키마가
  `..._user_0xF0.csv`, 그다음이 `..._user_0xF0_schema2.csv`, `_schema3` … 입니다. 열 이름이 같아도
  값의 뜻이 다르기 때문입니다: 같은 4바이트 `00 00 80 3f` 가 float32 스키마로는 `1.0`, uint32
  스키마로는 `1065353216` 입니다. 앞 구간이 뒤 스키마로 잘못 풀리지 않습니다. 스키마로 끝내 풀지
  못한 행(재연결 뒤 스키마가 오기 전 등)이 같은 모듈에 섞여 있으면, 어느 쪽이 먼저 나왔든 그
  행들은 `..._user_0xF0_unresolved.csv` 로 따로 나옵니다(스키마가 하나도 없는 모듈은 파일 하나,
  기본 이름입니다). 요약에는 `스키마가 2종류 쓰였다` 는 경고가 붙습니다.
- **연결을 끊었다 다시 붙으면** 그래프 창은 새 `.xmlog` 를 엽니다. 다시 붙은 보드가 아까 그 보드인지
  알 방법이 없으므로 앞 파일의 스키마를 새 파일로 넘기지 않습니다 — 새 파일은 그 연결에서 보드가 보낸
  채널 정보만 압니다. PC 프로그램은 한 번에 하나만 연결하세요. 채널 이름은 연결할 때 한 번
  전달됩니다. 이름이 안 보이면 USB 케이블을 다시 꽂고 다시 연결하세요. (이름 없이 받은 `.xmlog` 는
  `ch0, ch1, …` 이고 값은 float32 로 가정합니다.) 타입까지 알려 주는 이진 스키마(`0xEE`)는 아직
  보내지 않습니다.

  코드로 한 파일에 이어 쓸 때는 `XmLogCapture.on_reconnect()` 가 같은 일을 합니다(새 SESSION 레코드,
  그때 유효하던 스키마 무효화 — 새 스키마가 오기 전의 데이터는 `activation_id=0` 으로 남습니다).
- 펌웨어 빌드(`fw_build_id`)는 지금은 보드에서 알아낼 방법이 없어서 `unknown` 이라고 적힙니다.
- `activation_id` 가 엉뚱한 모듈의 스키마를 가리키는 손상된 파일은, 그 행을 버리지도 틀린
  스키마로 풀지도 않고 `activation_id=0` 으로 내려 원본 바이트를 남깁니다. 이 검사는 내보내기 도구가
  합니다 — `xmlog.py` 로 레코드를 직접 읽으면 파일에 적힌 번호가 그대로 보입니다.

## `CDC/total_data_decoder.py` — Total Data(0x20) 디코더

생성된 표(`CDC/xm_total_data_map.py`)를 읽어 365 B 구조체를 이름 붙은 197개 값으로 풉니다.

```python
from total_data_decoder import TotalDataDecoder
dec = TotalDataDecoder()
named = dec.named(frame.payload)        # {'leftHipAngle': 12.3, ...}
take  = dec.selector(['leftHipAngle'])  # 몇 개만 뽑을 때 (197개 전부 계산하지 않음)
```

CLI 로 바로 CSV 를 뽑을 수도 있습니다 — 사용자 채널 CSV 와 **별도 파일**로 나갑니다
(컬럼 의미가 다르고, 197채널을 사용자 CSV 에 섞으면 헤더가 어긋납니다).

```bash
python CDC/cdc_phai_receiver.py --cli --port COM6 --total-data
#  data/cdc_phai_<시각>_user_0xF0.csv   사용자 채널 (module 하나당 파일 하나, 0xF0 예시)
#  data/cdc_total_<시각>.csv            0x20 197채널
#  data/cdc_total_<시각>.csv.meta.txt   어느 맵으로 풀었는지
```

> 사용자 채널을 Module ID 여러 개(0xF0~0xFE)로 보내면 Module ID 마다 파일이 하나씩 생깁니다
> (`..._user_0xF0.csv`, `..._user_0xF1.csv`, ...). `.xmlog` 에서 CSV 를 뽑을 때
> (`python xm10.py export FILE --csv DIR`)도 같은 이름 규칙입니다.

> **알아 둘 한계** — 디코더는 자기가 **어떤 맵을 쓰는지**는 말할 수 있지만, 보드가
> **어떤 맵으로 보내는지**는 알 수 없습니다. 0x20 패킷에 버전·지문 필드가 없기 때문입니다.
> 그래서 FW 와 PC 의 맵이 어긋나면 **조용히 잘못된 값**이 나옵니다. `.meta.txt` 에 남는
> fingerprint 는 "PC 가 푼 맵"이지 "보드가 보낸 맵"이 아닙니다 — 나중에 값이 이상할 때
> 되짚기 위한 기록입니다.

> GUI(`cdc_phai_receiver.py` 그래프 화면)는 0x20 을 그래프로 그리지는 않습니다. 대신 "Show 0x20 tab"
> 체크박스를 켜면 표(채널 이름 + 최신값, 초당 5번 갱신)로 볼 수 있습니다. 받은 크기가 PC 가 가진
> 맵과 다르면 값 대신 `map mismatch` 경고를 띄웁니다 — 틀린 값을 그럴듯하게 보여주지 않습니다.
> (크기는 같은데 배치만 다른 경우는 알아낼 수 없습니다. 위 '알아 둘 한계' 그대로입니다.)

### 검증

```bash
python run_tests.py                      # 전부 한 번에 (권장)
```

개별로 돌리려면:

```bash
python CDC/total_data_decoder.py         # 디코더 단독 자기검사
python CDC/test_total_data_decoder.py    # 와이어 -> 0x20 디코드
python CDC/test_xmlog.py                 # .xmlog 바이트 ABI (손으로 적은 골든 바이트)
python CDC/test_xmlog_chain.py           # 와이어 -> .xmlog -> CSV 전 구간
python CDC/test_xmlog_activation.py      # 스키마(activation)의 수명 — 발급·재연결·도중 교체·시점별 디코딩
python CDC/test_frame_router.py          # 와이어 파싱·시퀀스 회계
python CDC/test_schema.py                # 0xEE/0xEF 스키마 + 레지스트리
python CDC/test_golden_vectors.py        # 독립 구현이 만든 계약 바이트와 대조
python CDC/test_gui_tabs.py              # 실제 창(오프스크린)에 합성 스트림 — 탭·CSV·0x20 표 (PyQt5 필요)
python CDC/soak.py --selftest            # soak 회계·판정 로직
```

> `test_gui_tabs.py` 는 PyQt5 가 없으면 `SKIP` 으로 끝납니다. `run_tests.py` 도 이걸 통과로
> 세지 않고 `SKIP` 으로 따로 적습니다 — 창을 검사하지 못했다는 뜻입니다.

> `run_tests.py` 에는 시험 말고 **undefined-name 게이트**(pyflakes)가 하나 더 있습니다.
> 단위 시험이 지나가지 않는 코드 경로에서 이름이 빠지는 결함을 잡습니다.

## `CDC/cdc_csv_reviewer.py` — 후처리 분석 뷰어

`cdc_phai_receiver.py`가 저장한 CSV를 로드하여 전체 세션을 분석합니다.

```bash
python CDC/cdc_csv_reviewer.py                                  # 파일 선택 다이얼로그
python CDC/cdc_csv_reviewer.py data/cdc_phai_20260224_120000_user_0xF0.csv
```

주요 기능: 센서 도메인별 그래프 그룹, **Sequence Gap(ΔSeq) 분석**(패킷 누락 시점),
**Tx Drop 누적 그래프**, 이상치 자동 필터, X축 연동, 드래그 앤 드롭.
값 대신 받은 바이트(hex)를 적은 행은 이상치로 세지 않습니다. 그 행의 값은 그래프에서 비워 두고
(몇 행인지 알려 줍니다), 시간·seq·Tx drop 은 그대로 세어 손실 통계에 넣습니다.

## 무손실 측정 — `CDC/soak.py`

보드를 꽂고 한 명령으로 "정말 하나도 안 잃었는가" 를 판정합니다.

```bash
python CDC/soak.py --minutes 30          # 포트 자동 탐색
python CDC/soak.py --port COM6 --minutes 10
python CDC/soak.py --list-ports
```

무엇을 보는가:

| 조건 | 왜 |
|---|---|
| FW Tx 드롭 == 0 | 드롭은 **이미 와이어에 있습니다** — 모든 프레임의 STATUS 바이트. 디버거 불필요 |
| **초당 프레임 ≥ 950** | ⚠ 이게 없으면 **거짓 통과**합니다 — 스트림이 죽으면 "드롭 0" 이 자동 성립 |
| CRC · 프레이밍 오류 == 0 | |
| 와이어 시퀀스 손실 == 0 | |
| 받은 수 == 디스크에 남은 수 | "받았다" 와 "저장됐다" 는 다른 주장입니다 |

판정은 스크립트가 합니다 — 30분을 눈으로 세지 않도록. 회계 로직은 보드 없이
`--selftest` 로 검증됩니다(깨끗=PASS / 드롭=FAIL / 죽은 스트림=FAIL / 손실=FAIL).

---

## 파일 한눈에

보통은 `xm10.py` 만 쓰면 됩니다. 안쪽을 고치거나 읽고 싶을 때 참고하세요.

| 파일 | 하는 일 |
| :--- | :--- |
| `xm10.py` | 진입점 — demo / ports / recv / soak / export / selftest |
| `build_exe.py` | PyInstaller 로 `dist/xm10.exe` 만들고 **그 exe 를 실제로 돌려** 검증 |
| `run_tests.py` | 개발용 검증 전부 (pyflakes 게이트 포함) |
| `CDC/frame_router.py` | 바이트 → 프레임 (COBS·CRC·순번 회계). GUI/CLI 공용 |
| `CDC/cdc_phai_receiver.py` | 실시간 수신 — 그래프 창(GUI) 과 콘솔(`--cli`) |
| `CDC/schema_0xee.py` | 보드가 보내는 구조체 설명(0xEE) 파서·재조립 |
| `CDC/schema_registry.py` | 채널 이름·타입의 출처를 하나로 (0xEE > 0xEF > 생성 맵 > float32 가정) |
| `CDC/total_data_decoder.py` | Total Data(0x20) 197채널 디코더 |
| `CDC/xm_total_data_map.py` | 0x20 레이아웃 표 — **생성 파일**, 손으로 고치지 않음 |
| `CDC/xmlog.py` | `.xmlog` 파일 형식 — 쓰기·읽기·잘린 파일 복구 |
| `CDC/xmlog_capture.py` | 받은 프레임을 `.xmlog` 로 눕히는 다리 (수신 루프가 이걸 부른다). 스키마가 오면 번호(activation)를 발급해 뒤따르는 DATA 에 단다 |
| `CDC/xmlog_export.py` | `.xmlog` → 요약 / CSV. 행마다 그 시점의 스키마로 푼다 |
| `CDC/soak.py` | 보드 실측 — 손실 0 판정 |
| `CDC/demo_stream.py` | 보드 없이 쓰는 합성 스트림 (데모·시험 공용) |
| `CDC/demo_run.py` | 데모 전 구간 실행 + 17항목 판정 |
| `CDC/cdc_csv_reviewer.py` | 저장한 CSV 후처리 뷰어 |
| `CDC/test_*.py` | 각 조각의 시험. `test_golden_vectors` 는 다른 구현이 만든 바이트와 대조, `test_demo_stream` 은 두 COBS 구현을 맞댐, `test_gui_tabs` 는 실제 창을 오프스크린으로 띄워 확인 |
| `spec/golden/` | 와이어 계약 골든 벡터 — 독립 구현이 만든 것 |

---

## CSV 포맷 참고 (CDC, PhAI V2.2)

```
time_s,pc_time_s,seq_id,module_id,tx_drops,AccX,AccY,AccZ,GyrX,GyrY,GyrZ,MotorAngle_L,MotorAngle_R,MotorTorque_L,MotorTorque_R
0.001234,0.001501,0,16,0,0.012,-9.781,0.234,0.001,-0.002,0.003,15.2,14.8,1.23,1.15
```

- `time_s` — 장치 시각. `seq_id` 로 재구성해 매끄럽습니다(손실 구간도 시간이 흐릅니다)
- `pc_time_s` — PC 가 받은 시각. 다른 장비와 맞출 때 씁니다
