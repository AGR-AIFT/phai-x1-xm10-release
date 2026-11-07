# 장치 예제

XM10 예제의 **상대 장치**입니다. 직접 만든 장치나 PC 를 XM10 의 CAN-FD #2 포트에 연결해 예제를 돌려 볼 때 씁니다.

| 폴더 | 무엇 | 필요한 것 | 상대 XM10 예제 |
|------|------|------|:---:|
| [`canfd-sensor-stm32g474/`](canfd-sensor-stm32g474/) | CAN-FD 내 장치 참조 펌웨어 — 주기 데이터 · 살아있음 · LED 명령 | **NUCLEO-G474RE + CAN-FD 트랜시버 보드** | [Ex.45](../examples/45_CANFD_My_Sensor/) |
| [`canfd-sensor-pc-sim/`](canfd-sensor-pc-sim/) | 위와 같은 장치를 PC 로 흉내 | CAN-FD USB 어댑터 + Python · python-can | [Ex.45](../examples/45_CANFD_My_Sensor/) |
| [`canopen-pc-sim/`](canopen-pc-sim/) | CANopen 속도 모드 드라이브를 PC 로 흉내 | 클래식 CAN USB 어댑터 + Python · python-can | [Ex.46](../examples/46_CANopen_Drive/) · [Ex.47](../examples/47_CANopen_Drive_PDO/) |

- 모두 XM10 **Rev 2.0** 과 씁니다.
- 한 포트에 클래식 장치와 CAN-FD 장치를 함께 연결하지 마세요. Ex.46 · 47 은 포트를 클래식 CAN 으로 정합니다.
- 버스 양 끝에 120 Ω 종단 저항이 있어야 합니다. XM10 보드에는 이미 있으므로 장치 쪽 끝에 하나를 둡니다.
- 아직 보드에서 확인하지 못한 예제입니다. (보드 확인 전) <!-- board-check -->
