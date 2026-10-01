# 04. PC 에서 데이터 받기 — `xm10` 도구

보드가 USB 로 내보내는 데이터를 PC 에서 **그래프로 보고, 원본 그대로 저장하고, CSV 로 뽑는**
도구입니다. `PythonDecoder/` 폴더에 있고, 파이썬으로 바로 쓰거나 실행파일 하나로 만들어
파이썬 없는 PC 에서도 쓸 수 있습니다.

> **어디에 있나요?** SDK ZIP 을 풀면 `Extension_Module\PythonDecoder\` 가 같이 들어 있습니다.
> 받은 ZIP 에 이 폴더가 없다면(예전 버전) GitHub 레포에서 `PythonDecoder/` 폴더만 받아
> 아무 데나 두고 쓰면 됩니다 — 보드 리비전과 무관하게 같은 파일입니다.

> **PhAI Studio 와 무엇이 다른가요?** PhAI Studio 는 웹에서 바로 쓰는 공식 도구이고, 이건
> 코드가 열려 있는 로컬 도구입니다. 받은 바이트를 **해석하기 전에 그대로 저장**하기 때문에
> 나중에 채널 구성이 바뀌어도 다시 뽑을 수 있고, 파이썬을 아신다면 원하는 대로 고쳐 쓸 수
> 있습니다. 두 도구는 **같은 COM 포트를 동시에 열 수 없습니다** — PC 프로그램은 한 번에
> 하나만 연결하세요.

> **내 구조체는 이 도구로 보세요.** PhAI Studio 는 보드가 자동으로 보내는 Total Data(0x20)를
> 보여 줍니다. PhAI Studio 는 아직 개발 중이라, 직접 정의한 데이터 구조체(커스텀 구조체)는
> 우선 `xm10` 도구로 보고 저장하세요.

| 보고 싶은 것 | 쓰는 프로그램 |
| :--- | :--- |
| 보드가 자동으로 보내는 시스템 데이터 — 관절 각도·토크 등 (Total Data 0x20. 그 안의 28바이트 `User_Custom` 칸을 채우는 방법은 [`xm_api_user_custom`](../api-reference/ref/xm-api-user-custom.md)) | PhAI Studio |
| `XM_SendUsbDataWithId` 로 직접 보낸 내 구조체 (`0xF0`~`0xFE`) | `xm10` 도구 (이 문서) |
| `XM_SendUsbDebugMessage` 로 보낸 텍스트 | 시리얼 터미널 |

---

## 5분 체험 — 보드 없이

보드가 아직 없어도 전체 흐름을 한 번 볼 수 있습니다.

```bash
cd Extension_Module\PythonDecoder   # SDK ZIP 을 푼 폴더 안
pip install pyserial pyqt5 pyqtgraph numpy      # 처음 한 번
python xm10.py demo
```

가짜 보드가 보낸 것처럼 만든 데이터를 실제 수신 코드에 흘려 넣고, 저장하고, 다시 읽어
CSV 로 뽑은 뒤 17개 항목을 스스로 확인합니다. 끝에 이렇게 나오면 됩니다.

```
데모 통과 — 17/17 항목. 산출물:
   demo_out\demo.xmlog
   demo_out\csv\demo_total_0x20.csv
   demo_out\csv\demo_user_0xF0.csv
   ...
```

`demo_out\csv\demo_user_0xF0.csv` 를 열어 보세요. 앞의 세 열(`pc_time_us` 받은 시각,
`seq_id` 받은 순서 번호, `activation_id` 그 행을 받을 때 유효했던 구조체 설명의 번호 — 0 은
"아직 설명을 받기 전"이라는 뜻이고 첫 몇 행이 그렇습니다. 그런 행도 뒤에 도착한 설명으로 풀려
있습니다)은 PC 가 붙인 수신 정보이고, 그 뒤가 `state, contact, emg_rms, angle_x10,
tick, torque[0], torque[1]` — 보드 쪽에서 C 구조체로 정의한 필드 이름이 그대로 CSV 열이
된 겁니다. 이게 이 도구가 하려는 일입니다.

---

## 보드를 연결해서

### 1. 포트 찾기

```bash
python xm10.py ports
```

```
포트 3개:
  COM6     0483:5740  USB 직렬 장치(COM6)          <- XM10 후보
  COM11    -          표준 Bluetooth에서 직렬 링크(COM11)
  COM12    -          표준 Bluetooth에서 직렬 링크(COM12)
```

XM10 후보가 안 보이면 케이블과 보드 전원을 확인하세요. 블루투스 가상 포트만 보이는 경우가
흔한데, 그건 XM10 이 아닙니다.

### 2. 그래프로 보기

```bash
python xm10.py recv
```

창이 뜹니다. 위쪽 줄에서 **Port** 를 고르고 **Connect** 를 누르면 보드가 보내는 사용자
채널이 그래프로 올라옵니다. 예제 09 처럼 `XM_SetUsbCustomMeta` 로 이름을 등록했다면 그
이름이 그래프 제목에 붙습니다.

> PC 프로그램은 한 번에 하나만 연결하세요. 채널 이름은 연결할 때 한 번 전달됩니다.
> 이름이 안 보이면 USB 케이블을 다시 꽂고 다시 연결하세요.

사용자 채널을 Module ID 여러 개(`0xF0`, `0xF1`, …)로 보내면 Module ID 마다 탭이 하나씩
생깁니다. 탭마다 그래프와 저장 파일이 따로이고, 지금 보고 있지 않은 탭도 저장은 계속됩니다.

| 버튼 | 하는 일 |
| :--- | :--- |
| **Refresh** | 포트 목록 다시 읽기 — 창을 먼저 켜고 보드를 나중에 꽂았다면 눌러야 보입니다 |
| **Connect / Disconnect** | 연결 / 끊기 |
| **Freeze** | 그래프만 멈춤 (저장은 계속) |
| **Output** | 파일이 저장될 폴더 (기본 `data/`) |
| **Save .xmlog** | 받은 바이트를 그대로 저장 (기본 켜짐 — 아래 참고) |
| **Show 0x20 tab** | Total Data(0x20)를 그래프 대신 표(이름 + 최신값, 초당 5번 갱신)로 보는 탭 — 기본 꺼짐 |
| **Screenshot** | 지금 화면을 PNG 로 |

Disconnect 하면 아래 상태 표시줄에 몇 프레임을 받았고 어느 파일에 저장했는지 나옵니다.

### 3. 저장된 파일

연결할 때마다 `Output` 폴더에 두 종류가 생깁니다.

| 파일 | 무엇인가 |
| :--- | :--- |
| `cdc_phai_<시각>_user_0xF0.csv` | 화면에 그린 사용자 채널을 표로 (엑셀에서 바로 열림). Module ID 마다 파일이 하나씩 생깁니다 |
| `cdc_<시각>.xmlog` | 받은 **모든** 프레임을 바이트 그대로 (아래 설명) |

CSV 만 있으면 될 것 같은데 왜 하나 더 저장하나요 — CSV 는 받는 순간 **해석한 결과**입니다.
채널 이름이 데이터보다 늦게 도착했거나, 나중에 채널 구성을 바꿨거나, 해석에 실수가 있었다면
그 CSV 는 그걸로 끝입니다. `.xmlog` 는 해석하지 않고 그대로 눕혀 두기 때문에, 언제든 다시
뽑을 수 있습니다. 그래프로는 안 그리는 시스템 채널(0x20 Total Data 197채널)도 여기엔 다
있습니다 (**Show 0x20 tab** 을 켜면 표로는 볼 수 있습니다).

### 4. `.xmlog` 에서 CSV 뽑기

```bash
python xm10.py export data/cdc_20260910_143000.xmlog              # 무슨 일이 있었는지 요약
python xm10.py export data/cdc_20260910_143000.xmlog --csv out/   # 채널별 CSV 로
```

```
CSV:
  cdc_..._total_0x20.csv      12000 rows   [generated-map] v2.8 ...
  cdc_..._user_0xF0.csv       12000 rows   [0xEF] JSON 메타 4채널  ⚠ 타입 미상 — float32 가정
```

`total_0x20` 은 보드가 항상 보내는 197채널(관절 각도·토크·IMU·GRF 등)이고, `user_0xF0` 은
여러분이 예제 코드에서 보낸 채널입니다. 받는 동안 보드가 보낸 채널 이름이 파일 안에 같이
저장돼 있어서, 뽑을 때 따로 설정할 게 없습니다.

행마다 **그 행을 받을 때 유효했던 채널 설명**으로 풉니다(설명보다 먼저 받은 행은, 같은 연결에서 그
뒤에 처음 도착한 설명으로 풉니다). 한 파일 안에서 같은 Module ID 의 구조체 설명이 바뀌었다면(예:
재연결 뒤에도 한 파일에 이어 적는 스크립트로 받은 경우 — 그래프 창은 연결마다 새 파일을 엽니다)
그 Module ID 의 CSV 가 설명마다 나뉩니다 — `..._user_0xF0.csv`, `..._user_0xF0_schema2.csv`.
열 이름이 같아도 값의 뜻이 다를 수 있어서입니다(같은 4바이트가 `float` 로는 `1.0`, `uint32_t` 로는
`1065353216`).

**다시 연결하면** 그래프 창이 새 `.xmlog` 를 엽니다. (채널 이름 없이 받은 `.xmlog` 는 열
이름이 `ch0, ch1, …` 이고 값은 `float` 로 가정합니다. 타입은 아직 보내지 않습니다.)

---

## 콘솔로만 쓰기

그래프 창이 필요 없거나 오래 켜 둘 때는 콘솔 모드가 가볍습니다.

```bash
python xm10.py recv --cli --port COM6 --log                # CSV + .xmlog 저장, Ctrl+C 로 종료
python xm10.py recv --cli --port COM6 --log --total-data   # 0x20 197채널도 별도 CSV 로
```

---

## 손실 없이 받고 있나 확인하기 — `soak`

1 kHz 로 30분을 받으면 180만 프레임입니다. 그중 하나라도 빠졌는지 눈으로는 알 수 없어서,
자동으로 판정해 주는 명령이 있습니다.

```bash
python xm10.py soak --port COM6 --minutes 30
```

끝나면 PASS / FAIL 과 이유를 말해 줍니다 — 보드가 버린 프레임, 깨진 프레임, 빠진 번호,
초당 프레임 수, 도중에 보드가 재시작했는지까지. 처음 셋업할 때, 케이블을 바꿨을 때,
채널을 늘렸을 때 한 번씩 돌려 보세요.

---

## 내 데이터 보내기 (보드 쪽)

[Ex.09 CDC Stream](../../examples/09_CDC_Stream/) 의 `cdc_stream.c` 가 본보기입니다.
필요한 조각은 셋 — 구조체 하나, 이름 등록 한 줄, 보내기 한 줄. 아래는 그 파일에서 그대로
가져온 것입니다.

```c
/* 보낼 값을 담는 구조체 — 전부 float, 순서가 곧 채널 순서 */
typedef struct {
    float is_connected;
    float left_hip_angle;
    float right_hip_angle;
    float forward_velocity;
} UserDebugData_t;
static UserDebugData_t s_debug;

/* Control_Setup — 채널 이름 등록 (한 번). 항목 순서 = 구조체 필드 순서 */
XM_SetUsbCustomMeta(0xF0,
    "[{\"name\":\"H10 Connected\",\"unit\":\"bool\"},"
    "{\"name\":\"Left Hip Angle\",\"unit\":\"deg\"},"
    "{\"name\":\"Right Hip Angle\",\"unit\":\"deg\"},"
    "{\"name\":\"Forward Velocity\",\"unit\":\"m/s\"}]");

/* Control_Loop — 값 채우고 보내기 (매 tick) */
XM_SendUsbDataWithId(&s_debug, sizeof(s_debug), 0xF0);
```

지켜야 할 것 다섯 가지:

1. **구조체는 `float` 만** — 이름을 등록하는 방식은 타입을 같이 보내지 않아서, PC 는 값을
   전부 float 로 읽습니다. `uint32_t` 하나가 섞이면 그 열은 이상한 숫자가 됩니다.
2. **JSON 항목 수 = 구조체 필드 수, 순서도 같게** — 하나만 어긋나도 열이 통째로 밀립니다.
   값은 그럴듯한 숫자라 알아채기 어렵습니다.
3. **Module ID 는 `0xF0`~`0xFE`** — 나머지는 시스템이 씁니다.
4. **이름 등록은 연결당 하나만** — `XM_SetUsbCustomMeta` 를 두 번 부르면 마지막 것만
   남습니다. 채널 그룹이 여럿이면 하나의 Module ID 로 묶어 등록하세요 (Ex.41 은 IMU 6개를
   하나의 Module ID 로 묶어 보냅니다).
5. **JSON 은 512바이트 이하** — 넘으면 이름이 전송되지 않아 열 이름이 `ch0, ch1, …` 로
   나옵니다 (케이블을 다시 꽂아도 해결되지 않습니다).

SDK 예제 24개는 1~3 을 지키고 있습니다. 직접 만들 때도 같은 규칙을 따라 주세요.

> 데모에서 본 "구조체 필드 이름이 타입까지 그대로 CSV 열이 되는" 방식(`state` 는 정수,
> `contact` 는 bool …)은 PC 쪽에는 이미 준비돼 있지만, 지금 보드는 위처럼 **이름만** 보냅니다.

---

## 실행파일로 만들기

파이썬이 없는 PC 에 가져가려면 하나로 묶습니다.

```bash
python build_exe.py            # dist/xm10.exe — 그래프 창 포함 (약 55.6 MB)
python build_exe.py --no-gui   # 콘솔 전용 (약 23.0 MB)
```

만들고 나면 그 exe 로 `demo` 와 자체 검사를 **실제로 돌려서** 확인합니다. 명령은 파이썬
때와 같습니다 — `xm10.exe demo`, `xm10.exe recv`, `xm10.exe export ...`.

---

## 뭔가 이상할 때

| 증상 | 확인할 것 |
| :--- | :--- |
| 포트 목록에 XM10 이 없다 | 창을 먼저 켰다면 **Refresh**. `xm10.py ports` 로 다시. 케이블·전원. 장치 관리자에서 COM 포트가 잡히는지 |
| 이름을 두 개 등록했는데 하나만 보인다 | 등록은 연결당 하나만 남습니다 — 한 Module ID 로 묶으세요 |
| Connect 가 실패한다 | PhAI Studio 나 시리얼 터미널이 같은 포트를 잡고 있지 않은지 |
| 그래프 제목이 `ch0, ch1…` 이다 | `XM_SetUsbCustomMeta` 를 `Control_Setup` 에서 불렀는지. 항목 수가 구조체와 같은지. JSON 이 512바이트 이하인지. 연결할 때 이름을 못 받았을 수 있으니 USB 케이블을 다시 꽂고 다시 연결 (PC 프로그램은 한 번에 하나만) |
| 값이 이상한 숫자다 | 구조체에 `float` 아닌 멤버가 있는지 |
| 프레임이 빠지는 것 같다 | `xm10.py soak` 로 판정. 보내는 채널 수를 줄여 보기 |
| 도구가 제대로 설치됐는지 모르겠다 | `python xm10.py selftest` — 보드 없이 자체 검증 |

---

## 폴더 안에 뭐가 있나

```
PythonDecoder/
├── xm10.py          ← 여기서 시작. demo / ports / recv / soak / export / selftest
├── build_exe.py     ← 실행파일 만들기
├── run_tests.py     ← 개발용 검증 (selftest 보다 조금 더)
├── README.md        ← 안쪽 구조·파일별 설명
└── CDC/             ← 실제 코드. 직접 고치고 싶을 때
```

`CDC/` 안 파일 하나하나가 무엇을 하는지는 [PythonDecoder/README.md](../../PythonDecoder/README.md)
의 "파일 한눈에" 표에 있습니다. 보통은 `xm10.py` 만 쓰시면 됩니다.

---

**다음:** [Tutorials](../tutorials/) 의 Ex.07~09 (USB 통신) 로 넘어가세요.
