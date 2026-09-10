# 골든 벡터 — USB-CDC 스키마·로그 계약

이 디렉토리의 바이트는 **계약**이다. FW·Python·TypeScript 구현이 각자
같은 바이트를 만들어내고 같은 값으로 풀어야 한다.

생성: `python Extension_Module/tools/spec/gen_golden_vectors.py`
검사: `python Extension_Module/tools/spec/gen_golden_vectors.py --check`

`.hex` 는 디스크/와이어에 그대로 나가는 바이트, `.json` 은 그것을 풀었을 때
나와야 하는 값이다.

| 벡터 | 크기 | 내용 |
|---|---|---|
| `0xEE_max_single_fragment` | 1016 B | 프래그먼트 하나에 들어가는 최대 필드 수 31개. 총 24 + 31*32 = 1016 B (payload cap 1020 이하). 11개 타입 전부와 배열이 섞여 있다. |
| `0xEE_minimal` | 56 B | 가장 작은 유효 프래그먼트. 24 + 32 = 56 B. |
| `0xEE_multi_fragment` | 3048 B | 설계 상한인 93 필드 = 31 x 3 프래그먼트. `.hex` 는 세 프래그먼트를 이어붙인 것이고 `frame_lengths` 로 잘라야 한다. schema_crc32 는 **세 프래그먼트의 FieldRecord 를 순서대로 이어붙인 것** 의 CRC 다 (헤더 제외). |
| `xmlog_v1_minimal_file` | 260 B | 완전한 최소 파일 하나. 마지막 DATA 는 activation_id=0 — 스키마보다 먼저 온 데이터를 그대로 기록하는 경우다(PLAN §4.1 data-before-schema). rec_crc32 는 자기 자신 4바이트만 건너뛴 skip 방식이다. |

## 왜 이게 필요한가

구현이 하나뿐이면 그 구현이 곧 정답이 되어, 틀려도 알 수 없다.
벡터를 파일로 동결해 두면 **두 구현이 서로를 검사**할 수 있다.

설계 문서(`docs/plans/PLAN-20260908-usb-cdc-schema-logging.md` §4.0)가
*"이 벡터가 동결되기 전에는 어떤 구현도 착수하지 않는다"* 고 적어 둔 것이 이것이다.

## 대조하는 쪽

- **생성기** (여기, `gen_golden_vectors.py`) — PLAN 의 표를 보고 `struct` 로 직접 조립.
  릴리스 레포 코드를 import 하지 않는다.
- **호스트 파서** — `phai-x1-xm10-release/PythonDecoder/CDC/`
  (`schema_0xee.py`, `xmlog.py`). `python PythonDecoder/run_tests.py` 로 검증.
- **FW** — 아직 없다. `0xEE` 송신 구현 시 이 벡터를 재현해야 한다.

## 주의

`0xEE_multi_fragment.hex` 는 프래그먼트 3개를 **이어붙인** 것이다.
`frame_lengths` 로 잘라서 하나씩 먹여야 한다 — 통째로 파싱하면 안 된다.
