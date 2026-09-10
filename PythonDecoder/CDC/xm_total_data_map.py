# AUTO-GENERATED from xm_total_data.yaml v2.8 — DO NOT EDIT MANUALLY
# Layout fingerprint: ce1c6e3a5f3e1ba6 (sha256 of the canonical descriptor list)
# 생성기: Extension_Module/tools/codegen/data_map/generate_data_map.py --py-out
"""
XM10 Total Data Packet (PhAI module_id 0x20) — 디코드 맵.

PhAI V2.2 프레임의 payload 를 이름 붙은 스칼라로 푸는 데 필요한 표다.
표준 라이브러리만 쓴다 — 어디서든 import 된다.

    import xm_total_data_map as M
    raw = M.PACKET_STRUCT.unpack(payload)      # 197 개 스칼라
    for name, v, mul, div in zip(M.SCALAR_NAMES, raw, M.SCALAR_MUL, M.SCALAR_DIV):
        phys = v * mul if mul is not None else (v / div if div is not None else v)

스케일 공식(YAML `scale_formula`)은 아래 두 배열로 미리 접어 두었다:
  none            -> mul=None, div=None      (raw 그대로)
  divide          -> div=scale               (phys = raw / scale)
  multiply_divide -> mul=scale/32768         (phys = raw * scale / 32768)

`multiply_divide` 를 곱셈 하나로 접어도 값이 달라지지 않는다 — 분모 32768 이
2의 거듭제곱이라 scale/32768 이 이진 부동소수점에서 **정확**하고, 반올림이
한 번뿐인 것도 그대로다. `divide` 는 그렇지 않아서(1/100 은 부정확) 나눗셈을
그대로 남겼다 — 참조 구현(TypeScript / PhAI Studio)과 같은 값을 내기 위해서다.
"""
from __future__ import annotations

import struct
from typing import NamedTuple, Optional, Tuple

DATA_MAP_VERSION = '2.8'
DATA_MAP_FINGERPRINT = 'ce1c6e3a5f3e1ba6'
TOTAL_PACKET_SIZE = 365
PACKET_ENDIAN = 'little'
NUM_DESCRIPTORS = 111
NUM_SCALARS = 197

# `multiply_divide` 의 분모. YAML 주석(scale_formula 설명)이 정의하는 상수다 —
# YAML 데이터에는 없으므로 생성기가 알고 있어야 한다.
MULTIPLY_DIVIDE_DENOM = 32768

MODULE_ID_TOTAL = 0x20
MODULE_ID_LINK_HEALTH = 0xED
MODULE_ID_SCHEMA_DESC = 0xEE
MODULE_ID_USER_META = 0xEF
MODULE_ID_USER_CUSTOM_START = 0xF0
MODULE_ID_USER_CUSTOM_END = 0xFE

TYPE_SIZE = {
    'uint8': 1,
    'int8': 1,
    'uint16': 2,
    'int16': 2,
    'uint32': 4,
    'int32': 4,
    'float32': 4,
}


class ChannelDef(NamedTuple):
    """YAML 필드 하나. 배열은 펼치지 않고 count 로 남는다 — C 구조체와 1:1."""
    offset: int
    name: str
    type: str
    count: int
    scale: float
    scale_formula: str
    unit: str
    group: str


class ScalarDef(NamedTuple):
    """펼친 스칼라 하나. PACKET_STRUCT.unpack() 결과와 순서가 1:1 이다."""
    name: str
    group: str
    unit: str
    type: str
    mul: Optional[float]
    div: Optional[float]


# --- descriptors (111) — C 구조체 필드와 1:1 ---
TOTAL_DATA_MAP: Tuple[ChannelDef, ...] = (
    ChannelDef(  0, 'xm_loop_count',             'uint32',   1, 1,         'none',            'count',     'Header'),
    ChannelDef(  4, 'device_online_mask',        'uint16',   1, 1,         'none',            'flags',     'Header'),
    ChannelDef(  6, 'phai_x1_status',            'uint8',    1, 1,         'none',            'flags',     'Header'),
    ChannelDef(  7, 'leftHipAngle',              'int16',    1, 720,       'multiply_divide', 'deg',       'H10_Joint'),
    ChannelDef(  9, 'rightHipAngle',             'int16',    1, 720,       'multiply_divide', 'deg',       'H10_Joint'),
    ChannelDef( 11, 'leftKneeAngle',             'int16',    1, 720,       'multiply_divide', 'deg',       'H10_Joint'),
    ChannelDef( 13, 'rightKneeAngle',            'int16',    1, 720,       'multiply_divide', 'deg',       'H10_Joint'),
    ChannelDef( 15, 'leftHipTorque',             'int16',    1, 60,        'multiply_divide', 'A',         'H10_Joint'),
    ChannelDef( 17, 'rightHipTorque',            'int16',    1, 60,        'multiply_divide', 'A',         'H10_Joint'),
    ChannelDef( 19, 'leftHipMotorAngle',         'int16',    1, 720,       'multiply_divide', 'deg',       'H10_Joint'),
    ChannelDef( 21, 'rightHipMotorAngle',        'int16',    1, 720,       'multiply_divide', 'deg',       'H10_Joint'),
    ChannelDef( 23, 'leftThighAngle',            'int16',    1, 720,       'multiply_divide', 'deg',       'H10_Segment'),
    ChannelDef( 25, 'rightThighAngle',           'int16',    1, 720,       'multiply_divide', 'deg',       'H10_Segment'),
    ChannelDef( 27, 'pelvicAngle',               'int16',    1, 720,       'multiply_divide', 'deg',       'H10_Segment'),
    ChannelDef( 29, 'isLeftFootContact',         'uint8',    1, 1,         'none',            'bool',      'H10_Gait'),
    ChannelDef( 30, 'isRightFootContact',        'uint8',    1, 1,         'none',            'bool',      'H10_Gait'),
    ChannelDef( 31, 'forwardVelocity',           'int16',    1, 6000,      'multiply_divide', 'm/s',       'H10_Gait'),
    ChannelDef( 33, 'leftHipImuFrontalRoll',     'int16',    1, 720,       'multiply_divide', 'deg',       'H10_IMU'),
    ChannelDef( 35, 'leftHipImuSagittalPitch',   'int16',    1, 720,       'multiply_divide', 'deg',       'H10_IMU'),
    ChannelDef( 37, 'leftHipImuTransverseYaw',   'int16',    1, 720,       'multiply_divide', 'deg',       'H10_IMU'),
    ChannelDef( 39, 'rightHipImuFrontalRoll',    'int16',    1, 720,       'multiply_divide', 'deg',       'H10_IMU'),
    ChannelDef( 41, 'rightHipImuSagittalPitch',  'int16',    1, 720,       'multiply_divide', 'deg',       'H10_IMU'),
    ChannelDef( 43, 'rightHipImuTransverseYaw',  'int16',    1, 720,       'multiply_divide', 'deg',       'H10_IMU'),
    ChannelDef( 45, 'leftHipImuGlobalAccX',      'int16',    1, 78.4532,   'multiply_divide', 'm/s2',      'H10_IMU'),
    ChannelDef( 47, 'leftHipImuGlobalAccY',      'int16',    1, 78.4532,   'multiply_divide', 'm/s2',      'H10_IMU'),
    ChannelDef( 49, 'leftHipImuGlobalAccZ',      'int16',    1, 78.4532,   'multiply_divide', 'm/s2',      'H10_IMU'),
    ChannelDef( 51, 'rightHipImuGlobalAccX',     'int16',    1, 78.4532,   'multiply_divide', 'm/s2',      'H10_IMU'),
    ChannelDef( 53, 'rightHipImuGlobalAccY',     'int16',    1, 78.4532,   'multiply_divide', 'm/s2',      'H10_IMU'),
    ChannelDef( 55, 'rightHipImuGlobalAccZ',     'int16',    1, 78.4532,   'multiply_divide', 'm/s2',      'H10_IMU'),
    ChannelDef( 57, 'leftHipImuGlobalGyrX',      'int16',    1, 1000,      'multiply_divide', 'deg/s',     'H10_IMU'),
    ChannelDef( 59, 'leftHipImuGlobalGyrY',      'int16',    1, 1000,      'multiply_divide', 'deg/s',     'H10_IMU'),
    ChannelDef( 61, 'leftHipImuGlobalGyrZ',      'int16',    1, 1000,      'multiply_divide', 'deg/s',     'H10_IMU'),
    ChannelDef( 63, 'rightHipImuGlobalGyrX',     'int16',    1, 1000,      'multiply_divide', 'deg/s',     'H10_IMU'),
    ChannelDef( 65, 'rightHipImuGlobalGyrY',     'int16',    1, 1000,      'multiply_divide', 'deg/s',     'H10_IMU'),
    ChannelDef( 67, 'rightHipImuGlobalGyrZ',     'int16',    1, 1000,      'multiply_divide', 'deg/s',     'H10_IMU'),
    ChannelDef( 69, 'h10AssistModeLoopCnt',      'uint32',   1, 1,         'none',            'count',     'H10_Count'),
    ChannelDef( 73, 'postProcessingCnt',         'uint32',   1, 1,         'none',            'count',     'H10_Count'),
    ChannelDef( 77, 'h10FSMcurrentState',        'uint8',    1, 1,         'none',            'enum',      'H10_State'),
    ChannelDef( 78, 'h10Mode',                   'uint8',    1, 1,         'none',            'enum',      'H10_State'),
    ChannelDef( 79, 'h10AssistLevel',            'uint8',    1, 1,         'none',            'level',     'H10_State'),
    ChannelDef( 80, 'isPVectorRHDone',           'uint8',    1, 1,         'none',            'bool',      'H10_State'),
    ChannelDef( 81, 'isPVectorLHDone',           'uint8',    1, 1,         'none',            'bool',      'H10_State'),
    ChannelDef( 82, 'h10NeutralPosSet',          'uint8',    1, 1,         'none',            'bool',      'H10_State'),
    ChannelDef( 83, 'grf_left_sensor_data',      'uint8',   14, 1,         'none',            'raw',       'GRF_Left'),
    ChannelDef( 97, 'grf_left_battery',          'uint8',    1, 1,         'none',            '%',         'GRF_Left'),
    ChannelDef( 98, 'grf_left_status',           'uint8',    1, 1,         'none',            'flags',     'GRF_Left'),
    ChannelDef( 99, 'grf_left_rolling_idx',      'uint8',    1, 1,         'none',            'count',     'GRF_Left'),
    ChannelDef(100, 'grf_right_sensor_data',     'uint8',   14, 1,         'none',            'raw',       'GRF_Right'),
    ChannelDef(114, 'grf_right_battery',         'uint8',    1, 1,         'none',            '%',         'GRF_Right'),
    ChannelDef(115, 'grf_right_status',          'uint8',    1, 1,         'none',            'flags',     'GRF_Right'),
    ChannelDef(116, 'grf_right_rolling_idx',     'uint8',    1, 1,         'none',            'count',     'GRF_Right'),
    ChannelDef(117, 'ext_imu_q_w',               'float32',  1, 1,         'none',            'normalized', 'Ext_IMU'),
    ChannelDef(121, 'ext_imu_q_x',               'float32',  1, 1,         'none',            'normalized', 'Ext_IMU'),
    ChannelDef(125, 'ext_imu_q_y',               'float32',  1, 1,         'none',            'normalized', 'Ext_IMU'),
    ChannelDef(129, 'ext_imu_q_z',               'float32',  1, 1,         'none',            'normalized', 'Ext_IMU'),
    ChannelDef(133, 'ext_imu_acc_x',             'float32',  1, 1,         'none',            'm/s2',      'Ext_IMU'),
    ChannelDef(137, 'ext_imu_acc_y',             'float32',  1, 1,         'none',            'm/s2',      'Ext_IMU'),
    ChannelDef(141, 'ext_imu_acc_z',             'float32',  1, 1,         'none',            'm/s2',      'Ext_IMU'),
    ChannelDef(145, 'ext_imu_gyr_x',             'float32',  1, 1,         'none',            'deg/s',     'Ext_IMU'),
    ChannelDef(149, 'ext_imu_gyr_y',             'float32',  1, 1,         'none',            'deg/s',     'Ext_IMU'),
    ChannelDef(153, 'ext_imu_gyr_z',             'float32',  1, 1,         'none',            'deg/s',     'Ext_IMU'),
    ChannelDef(157, 'imu_hub_timestamp',         'uint32',   1, 1,         'none',            'ms',        'IMU_Hub'),
    ChannelDef(161, 'imu_hub_connected_mask',    'uint8',    1, 1,         'none',            'flags',     'IMU_Hub'),
    ChannelDef(162, 'imu_hub_sensor[0].q',       'int16',    4, 10000,     'divide',          'normalized', 'IMU_Hub'),
    ChannelDef(170, 'imu_hub_sensor[0].a',       'int16',    3, 100,       'divide',          'g',         'IMU_Hub'),
    ChannelDef(176, 'imu_hub_sensor[0].g',       'int16',    3, 10,        'divide',          'deg/s',     'IMU_Hub'),
    ChannelDef(182, 'imu_hub_sensor[1].q',       'int16',    4, 10000,     'divide',          'normalized', 'IMU_Hub'),
    ChannelDef(190, 'imu_hub_sensor[1].a',       'int16',    3, 100,       'divide',          'g',         'IMU_Hub'),
    ChannelDef(196, 'imu_hub_sensor[1].g',       'int16',    3, 10,        'divide',          'deg/s',     'IMU_Hub'),
    ChannelDef(202, 'imu_hub_sensor[2].q',       'int16',    4, 10000,     'divide',          'normalized', 'IMU_Hub'),
    ChannelDef(210, 'imu_hub_sensor[2].a',       'int16',    3, 100,       'divide',          'g',         'IMU_Hub'),
    ChannelDef(216, 'imu_hub_sensor[2].g',       'int16',    3, 10,        'divide',          'deg/s',     'IMU_Hub'),
    ChannelDef(222, 'imu_hub_sensor[3].q',       'int16',    4, 10000,     'divide',          'normalized', 'IMU_Hub'),
    ChannelDef(230, 'imu_hub_sensor[3].a',       'int16',    3, 100,       'divide',          'g',         'IMU_Hub'),
    ChannelDef(236, 'imu_hub_sensor[3].g',       'int16',    3, 10,        'divide',          'deg/s',     'IMU_Hub'),
    ChannelDef(242, 'imu_hub_sensor[4].q',       'int16',    4, 10000,     'divide',          'normalized', 'IMU_Hub'),
    ChannelDef(250, 'imu_hub_sensor[4].a',       'int16',    3, 100,       'divide',          'g',         'IMU_Hub'),
    ChannelDef(256, 'imu_hub_sensor[4].g',       'int16',    3, 10,        'divide',          'deg/s',     'IMU_Hub'),
    ChannelDef(262, 'imu_hub_sensor[5].q',       'int16',    4, 10000,     'divide',          'normalized', 'IMU_Hub'),
    ChannelDef(270, 'imu_hub_sensor[5].a',       'int16',    3, 100,       'divide',          'g',         'IMU_Hub'),
    ChannelDef(276, 'imu_hub_sensor[5].g',       'int16',    3, 10,        'divide',          'deg/s',     'IMU_Hub'),
    ChannelDef(282, 'dio_state',                 'uint8',    1, 1,         'none',            'flags',     'Ext_IO'),
    ChannelDef(283, 'adc_active_mask',           'uint16',   1, 1,         'none',            'flags',     'Ext_IO'),
    ChannelDef(285, 'adc_channel',               'uint16',  12, 1,         'none',            'raw',       'Ext_IO'),
    ChannelDef(309, 'emg_status_flags',          'uint8',    1, 1,         'none',            'flags',     'EMG_Hub'),
    ChannelDef(310, 'emg_raw_adc',               'uint16',   1, 1,         'none',            'raw',       'EMG_Hub'),
    ChannelDef(312, 'emg_voltage_uv_x10',        'int16',    1, 10,        'divide',          'uV',        'EMG_Hub'),
    ChannelDef(314, 'emg_rms_uv_x10',            'int16',    1, 10,        'divide',          'uV',        'EMG_Hub'),
    ChannelDef(316, 'emg_envelope_uv_x10',       'int16',    1, 10,        'divide',          'uV',        'EMG_Hub'),
    ChannelDef(318, 'emg_mvc_percent',           'uint8',    1, 1,         'none',            '%',         'EMG_Hub'),
    ChannelDef(319, 'emg_is_active',             'uint8',    1, 1,         'none',            'bool',      'EMG_Hub'),
    ChannelDef(320, 'rsv_emg',                   'uint8',    1, 1,         'none',            'reserved',  'EMG_Hub'),
    ChannelDef(321, 'sync_save_active',          'uint8',    1, 1,         'none',            'bool',      'Ext_Sync'),
    ChannelDef(322, 'sync_din_level',            'uint8',    1, 1,         'none',            'bool',      'Ext_Sync'),
    ChannelDef(323, 'sync_din_edge_count',       'uint16',   1, 1,         'none',            'count',     'Ext_Sync'),
    ChannelDef(325, 'fdcan1_tec',                'uint8',    1, 1,         'none',            'count',     'FDCAN1_Diag'),
    ChannelDef(326, 'fdcan1_rec',                'uint8',    1, 1,         'none',            'count',     'FDCAN1_Diag'),
    ChannelDef(327, 'fdcan1_lec',                'uint8',    1, 1,         'none',            'enum',      'FDCAN1_Diag'),
    ChannelDef(328, 'fdcan1_bus_status',         'uint8',    1, 1,         'none',            'flags',     'FDCAN1_Diag'),
    ChannelDef(329, 'fdcan1_rx_fifo0_fill',      'uint8',    1, 1,         'none',            'count',     'FDCAN1_Diag'),
    ChannelDef(330, 'fdcan1_tx_fifo_free',       'uint8',    1, 1,         'none',            'count',     'FDCAN1_Diag'),
    ChannelDef(331, 'fdcan2_tec',                'uint8',    1, 1,         'none',            'count',     'FDCAN2_Diag'),
    ChannelDef(332, 'fdcan2_rec',                'uint8',    1, 1,         'none',            'count',     'FDCAN2_Diag'),
    ChannelDef(333, 'fdcan2_lec',                'uint8',    1, 1,         'none',            'enum',      'FDCAN2_Diag'),
    ChannelDef(334, 'fdcan2_bus_status',         'uint8',    1, 1,         'none',            'flags',     'FDCAN2_Diag'),
    ChannelDef(335, 'fdcan2_rx_fifo0_fill',      'uint8',    1, 1,         'none',            'count',     'FDCAN2_Diag'),
    ChannelDef(336, 'fdcan2_tx_fifo_free',       'uint8',    1, 1,         'none',            'count',     'FDCAN2_Diag'),
    ChannelDef(337, 'user_f',                    'float32',  4, 1,         'none',            'user',      'User_Custom'),
    ChannelDef(353, 'user_i16',                  'int16',    4, 1,         'none',            'user',      'User_Custom'),
    ChannelDef(361, 'user_flags',                'uint16',   1, 1,         'none',            'flags',     'User_Custom'),
    ChannelDef(363, 'user_u8',                   'uint8',    2, 1,         'none',            'user',      'User_Custom'),
)

# --- struct 포맷 — 그룹 경계를 살려 둔다(사람이 대조할 수 있게) ---
PACKET_STRUCT_FORMAT = (
    '<'
    'IHB'            # Header
    'hhhhhhhh'       # H10_Joint
    'hhh'            # H10_Segment
    'BBh'            # H10_Gait
    'hhhhhhhhhhhhhhhhhh'  # H10_IMU
    'II'             # H10_Count
    'BBBBBB'         # H10_State
    '14BBBB'         # GRF_Left
    '14BBBB'         # GRF_Right
    'ffffffffff'     # Ext_IMU
    'IB4h3h3h4h3h3h4h3h3h4h3h3h4h3h3h4h3h3h'  # IMU_Hub
    'BH12H'          # Ext_IO
    'BHhhhBBB'       # EMG_Hub
    'BBH'            # Ext_Sync
    'BBBBBB'         # FDCAN1_Diag
    'BBBBBB'         # FDCAN2_Diag
    '4f4hH2B'        # User_Custom
)
PACKET_STRUCT = struct.Struct(PACKET_STRUCT_FORMAT)

# --- scalars (197) — unpack() 순서 그대로 ---
SCALARS: Tuple[ScalarDef, ...] = (
    ScalarDef('xm_loop_count',                   'Header',        'count',     'uint32',  None,                    None),
    ScalarDef('device_online_mask',              'Header',        'flags',     'uint16',  None,                    None),
    ScalarDef('phai_x1_status',                  'Header',        'flags',     'uint8',   None,                    None),
    ScalarDef('leftHipAngle',                    'H10_Joint',     'deg',       'int16',   0.02197265625,           None),
    ScalarDef('rightHipAngle',                   'H10_Joint',     'deg',       'int16',   0.02197265625,           None),
    ScalarDef('leftKneeAngle',                   'H10_Joint',     'deg',       'int16',   0.02197265625,           None),
    ScalarDef('rightKneeAngle',                  'H10_Joint',     'deg',       'int16',   0.02197265625,           None),
    ScalarDef('leftHipTorque',                   'H10_Joint',     'A',         'int16',   0.0018310546875,         None),
    ScalarDef('rightHipTorque',                  'H10_Joint',     'A',         'int16',   0.0018310546875,         None),
    ScalarDef('leftHipMotorAngle',               'H10_Joint',     'deg',       'int16',   0.02197265625,           None),
    ScalarDef('rightHipMotorAngle',              'H10_Joint',     'deg',       'int16',   0.02197265625,           None),
    ScalarDef('leftThighAngle',                  'H10_Segment',   'deg',       'int16',   0.02197265625,           None),
    ScalarDef('rightThighAngle',                 'H10_Segment',   'deg',       'int16',   0.02197265625,           None),
    ScalarDef('pelvicAngle',                     'H10_Segment',   'deg',       'int16',   0.02197265625,           None),
    ScalarDef('isLeftFootContact',               'H10_Gait',      'bool',      'uint8',   None,                    None),
    ScalarDef('isRightFootContact',              'H10_Gait',      'bool',      'uint8',   None,                    None),
    ScalarDef('forwardVelocity',                 'H10_Gait',      'm/s',       'int16',   0.18310546875,           None),
    ScalarDef('leftHipImuFrontalRoll',           'H10_IMU',       'deg',       'int16',   0.02197265625,           None),
    ScalarDef('leftHipImuSagittalPitch',         'H10_IMU',       'deg',       'int16',   0.02197265625,           None),
    ScalarDef('leftHipImuTransverseYaw',         'H10_IMU',       'deg',       'int16',   0.02197265625,           None),
    ScalarDef('rightHipImuFrontalRoll',          'H10_IMU',       'deg',       'int16',   0.02197265625,           None),
    ScalarDef('rightHipImuSagittalPitch',        'H10_IMU',       'deg',       'int16',   0.02197265625,           None),
    ScalarDef('rightHipImuTransverseYaw',        'H10_IMU',       'deg',       'int16',   0.02197265625,           None),
    ScalarDef('leftHipImuGlobalAccX',            'H10_IMU',       'm/s2',      'int16',   0.00239420166015625,     None),
    ScalarDef('leftHipImuGlobalAccY',            'H10_IMU',       'm/s2',      'int16',   0.00239420166015625,     None),
    ScalarDef('leftHipImuGlobalAccZ',            'H10_IMU',       'm/s2',      'int16',   0.00239420166015625,     None),
    ScalarDef('rightHipImuGlobalAccX',           'H10_IMU',       'm/s2',      'int16',   0.00239420166015625,     None),
    ScalarDef('rightHipImuGlobalAccY',           'H10_IMU',       'm/s2',      'int16',   0.00239420166015625,     None),
    ScalarDef('rightHipImuGlobalAccZ',           'H10_IMU',       'm/s2',      'int16',   0.00239420166015625,     None),
    ScalarDef('leftHipImuGlobalGyrX',            'H10_IMU',       'deg/s',     'int16',   0.030517578125,          None),
    ScalarDef('leftHipImuGlobalGyrY',            'H10_IMU',       'deg/s',     'int16',   0.030517578125,          None),
    ScalarDef('leftHipImuGlobalGyrZ',            'H10_IMU',       'deg/s',     'int16',   0.030517578125,          None),
    ScalarDef('rightHipImuGlobalGyrX',           'H10_IMU',       'deg/s',     'int16',   0.030517578125,          None),
    ScalarDef('rightHipImuGlobalGyrY',           'H10_IMU',       'deg/s',     'int16',   0.030517578125,          None),
    ScalarDef('rightHipImuGlobalGyrZ',           'H10_IMU',       'deg/s',     'int16',   0.030517578125,          None),
    ScalarDef('h10AssistModeLoopCnt',            'H10_Count',     'count',     'uint32',  None,                    None),
    ScalarDef('postProcessingCnt',               'H10_Count',     'count',     'uint32',  None,                    None),
    ScalarDef('h10FSMcurrentState',              'H10_State',     'enum',      'uint8',   None,                    None),
    ScalarDef('h10Mode',                         'H10_State',     'enum',      'uint8',   None,                    None),
    ScalarDef('h10AssistLevel',                  'H10_State',     'level',     'uint8',   None,                    None),
    ScalarDef('isPVectorRHDone',                 'H10_State',     'bool',      'uint8',   None,                    None),
    ScalarDef('isPVectorLHDone',                 'H10_State',     'bool',      'uint8',   None,                    None),
    ScalarDef('h10NeutralPosSet',                'H10_State',     'bool',      'uint8',   None,                    None),
    ScalarDef('grf_left_sensor_data[0]',         'GRF_Left',      'raw',       'uint8',   None,                    None),
    ScalarDef('grf_left_sensor_data[1]',         'GRF_Left',      'raw',       'uint8',   None,                    None),
    ScalarDef('grf_left_sensor_data[2]',         'GRF_Left',      'raw',       'uint8',   None,                    None),
    ScalarDef('grf_left_sensor_data[3]',         'GRF_Left',      'raw',       'uint8',   None,                    None),
    ScalarDef('grf_left_sensor_data[4]',         'GRF_Left',      'raw',       'uint8',   None,                    None),
    ScalarDef('grf_left_sensor_data[5]',         'GRF_Left',      'raw',       'uint8',   None,                    None),
    ScalarDef('grf_left_sensor_data[6]',         'GRF_Left',      'raw',       'uint8',   None,                    None),
    ScalarDef('grf_left_sensor_data[7]',         'GRF_Left',      'raw',       'uint8',   None,                    None),
    ScalarDef('grf_left_sensor_data[8]',         'GRF_Left',      'raw',       'uint8',   None,                    None),
    ScalarDef('grf_left_sensor_data[9]',         'GRF_Left',      'raw',       'uint8',   None,                    None),
    ScalarDef('grf_left_sensor_data[10]',        'GRF_Left',      'raw',       'uint8',   None,                    None),
    ScalarDef('grf_left_sensor_data[11]',        'GRF_Left',      'raw',       'uint8',   None,                    None),
    ScalarDef('grf_left_sensor_data[12]',        'GRF_Left',      'raw',       'uint8',   None,                    None),
    ScalarDef('grf_left_sensor_data[13]',        'GRF_Left',      'raw',       'uint8',   None,                    None),
    ScalarDef('grf_left_battery',                'GRF_Left',      '%',         'uint8',   None,                    None),
    ScalarDef('grf_left_status',                 'GRF_Left',      'flags',     'uint8',   None,                    None),
    ScalarDef('grf_left_rolling_idx',            'GRF_Left',      'count',     'uint8',   None,                    None),
    ScalarDef('grf_right_sensor_data[0]',        'GRF_Right',     'raw',       'uint8',   None,                    None),
    ScalarDef('grf_right_sensor_data[1]',        'GRF_Right',     'raw',       'uint8',   None,                    None),
    ScalarDef('grf_right_sensor_data[2]',        'GRF_Right',     'raw',       'uint8',   None,                    None),
    ScalarDef('grf_right_sensor_data[3]',        'GRF_Right',     'raw',       'uint8',   None,                    None),
    ScalarDef('grf_right_sensor_data[4]',        'GRF_Right',     'raw',       'uint8',   None,                    None),
    ScalarDef('grf_right_sensor_data[5]',        'GRF_Right',     'raw',       'uint8',   None,                    None),
    ScalarDef('grf_right_sensor_data[6]',        'GRF_Right',     'raw',       'uint8',   None,                    None),
    ScalarDef('grf_right_sensor_data[7]',        'GRF_Right',     'raw',       'uint8',   None,                    None),
    ScalarDef('grf_right_sensor_data[8]',        'GRF_Right',     'raw',       'uint8',   None,                    None),
    ScalarDef('grf_right_sensor_data[9]',        'GRF_Right',     'raw',       'uint8',   None,                    None),
    ScalarDef('grf_right_sensor_data[10]',       'GRF_Right',     'raw',       'uint8',   None,                    None),
    ScalarDef('grf_right_sensor_data[11]',       'GRF_Right',     'raw',       'uint8',   None,                    None),
    ScalarDef('grf_right_sensor_data[12]',       'GRF_Right',     'raw',       'uint8',   None,                    None),
    ScalarDef('grf_right_sensor_data[13]',       'GRF_Right',     'raw',       'uint8',   None,                    None),
    ScalarDef('grf_right_battery',               'GRF_Right',     '%',         'uint8',   None,                    None),
    ScalarDef('grf_right_status',                'GRF_Right',     'flags',     'uint8',   None,                    None),
    ScalarDef('grf_right_rolling_idx',           'GRF_Right',     'count',     'uint8',   None,                    None),
    ScalarDef('ext_imu_q_w',                     'Ext_IMU',       'normalized', 'float32', None,                    None),
    ScalarDef('ext_imu_q_x',                     'Ext_IMU',       'normalized', 'float32', None,                    None),
    ScalarDef('ext_imu_q_y',                     'Ext_IMU',       'normalized', 'float32', None,                    None),
    ScalarDef('ext_imu_q_z',                     'Ext_IMU',       'normalized', 'float32', None,                    None),
    ScalarDef('ext_imu_acc_x',                   'Ext_IMU',       'm/s2',      'float32', None,                    None),
    ScalarDef('ext_imu_acc_y',                   'Ext_IMU',       'm/s2',      'float32', None,                    None),
    ScalarDef('ext_imu_acc_z',                   'Ext_IMU',       'm/s2',      'float32', None,                    None),
    ScalarDef('ext_imu_gyr_x',                   'Ext_IMU',       'deg/s',     'float32', None,                    None),
    ScalarDef('ext_imu_gyr_y',                   'Ext_IMU',       'deg/s',     'float32', None,                    None),
    ScalarDef('ext_imu_gyr_z',                   'Ext_IMU',       'deg/s',     'float32', None,                    None),
    ScalarDef('imu_hub_timestamp',               'IMU_Hub',       'ms',        'uint32',  None,                    None),
    ScalarDef('imu_hub_connected_mask',          'IMU_Hub',       'flags',     'uint8',   None,                    None),
    ScalarDef('imu_hub_sensor[0].q[0]',          'IMU_Hub',       'normalized', 'int16',   None,                    10000.0),
    ScalarDef('imu_hub_sensor[0].q[1]',          'IMU_Hub',       'normalized', 'int16',   None,                    10000.0),
    ScalarDef('imu_hub_sensor[0].q[2]',          'IMU_Hub',       'normalized', 'int16',   None,                    10000.0),
    ScalarDef('imu_hub_sensor[0].q[3]',          'IMU_Hub',       'normalized', 'int16',   None,                    10000.0),
    ScalarDef('imu_hub_sensor[0].a[0]',          'IMU_Hub',       'g',         'int16',   None,                    100.0),
    ScalarDef('imu_hub_sensor[0].a[1]',          'IMU_Hub',       'g',         'int16',   None,                    100.0),
    ScalarDef('imu_hub_sensor[0].a[2]',          'IMU_Hub',       'g',         'int16',   None,                    100.0),
    ScalarDef('imu_hub_sensor[0].g[0]',          'IMU_Hub',       'deg/s',     'int16',   None,                    10.0),
    ScalarDef('imu_hub_sensor[0].g[1]',          'IMU_Hub',       'deg/s',     'int16',   None,                    10.0),
    ScalarDef('imu_hub_sensor[0].g[2]',          'IMU_Hub',       'deg/s',     'int16',   None,                    10.0),
    ScalarDef('imu_hub_sensor[1].q[0]',          'IMU_Hub',       'normalized', 'int16',   None,                    10000.0),
    ScalarDef('imu_hub_sensor[1].q[1]',          'IMU_Hub',       'normalized', 'int16',   None,                    10000.0),
    ScalarDef('imu_hub_sensor[1].q[2]',          'IMU_Hub',       'normalized', 'int16',   None,                    10000.0),
    ScalarDef('imu_hub_sensor[1].q[3]',          'IMU_Hub',       'normalized', 'int16',   None,                    10000.0),
    ScalarDef('imu_hub_sensor[1].a[0]',          'IMU_Hub',       'g',         'int16',   None,                    100.0),
    ScalarDef('imu_hub_sensor[1].a[1]',          'IMU_Hub',       'g',         'int16',   None,                    100.0),
    ScalarDef('imu_hub_sensor[1].a[2]',          'IMU_Hub',       'g',         'int16',   None,                    100.0),
    ScalarDef('imu_hub_sensor[1].g[0]',          'IMU_Hub',       'deg/s',     'int16',   None,                    10.0),
    ScalarDef('imu_hub_sensor[1].g[1]',          'IMU_Hub',       'deg/s',     'int16',   None,                    10.0),
    ScalarDef('imu_hub_sensor[1].g[2]',          'IMU_Hub',       'deg/s',     'int16',   None,                    10.0),
    ScalarDef('imu_hub_sensor[2].q[0]',          'IMU_Hub',       'normalized', 'int16',   None,                    10000.0),
    ScalarDef('imu_hub_sensor[2].q[1]',          'IMU_Hub',       'normalized', 'int16',   None,                    10000.0),
    ScalarDef('imu_hub_sensor[2].q[2]',          'IMU_Hub',       'normalized', 'int16',   None,                    10000.0),
    ScalarDef('imu_hub_sensor[2].q[3]',          'IMU_Hub',       'normalized', 'int16',   None,                    10000.0),
    ScalarDef('imu_hub_sensor[2].a[0]',          'IMU_Hub',       'g',         'int16',   None,                    100.0),
    ScalarDef('imu_hub_sensor[2].a[1]',          'IMU_Hub',       'g',         'int16',   None,                    100.0),
    ScalarDef('imu_hub_sensor[2].a[2]',          'IMU_Hub',       'g',         'int16',   None,                    100.0),
    ScalarDef('imu_hub_sensor[2].g[0]',          'IMU_Hub',       'deg/s',     'int16',   None,                    10.0),
    ScalarDef('imu_hub_sensor[2].g[1]',          'IMU_Hub',       'deg/s',     'int16',   None,                    10.0),
    ScalarDef('imu_hub_sensor[2].g[2]',          'IMU_Hub',       'deg/s',     'int16',   None,                    10.0),
    ScalarDef('imu_hub_sensor[3].q[0]',          'IMU_Hub',       'normalized', 'int16',   None,                    10000.0),
    ScalarDef('imu_hub_sensor[3].q[1]',          'IMU_Hub',       'normalized', 'int16',   None,                    10000.0),
    ScalarDef('imu_hub_sensor[3].q[2]',          'IMU_Hub',       'normalized', 'int16',   None,                    10000.0),
    ScalarDef('imu_hub_sensor[3].q[3]',          'IMU_Hub',       'normalized', 'int16',   None,                    10000.0),
    ScalarDef('imu_hub_sensor[3].a[0]',          'IMU_Hub',       'g',         'int16',   None,                    100.0),
    ScalarDef('imu_hub_sensor[3].a[1]',          'IMU_Hub',       'g',         'int16',   None,                    100.0),
    ScalarDef('imu_hub_sensor[3].a[2]',          'IMU_Hub',       'g',         'int16',   None,                    100.0),
    ScalarDef('imu_hub_sensor[3].g[0]',          'IMU_Hub',       'deg/s',     'int16',   None,                    10.0),
    ScalarDef('imu_hub_sensor[3].g[1]',          'IMU_Hub',       'deg/s',     'int16',   None,                    10.0),
    ScalarDef('imu_hub_sensor[3].g[2]',          'IMU_Hub',       'deg/s',     'int16',   None,                    10.0),
    ScalarDef('imu_hub_sensor[4].q[0]',          'IMU_Hub',       'normalized', 'int16',   None,                    10000.0),
    ScalarDef('imu_hub_sensor[4].q[1]',          'IMU_Hub',       'normalized', 'int16',   None,                    10000.0),
    ScalarDef('imu_hub_sensor[4].q[2]',          'IMU_Hub',       'normalized', 'int16',   None,                    10000.0),
    ScalarDef('imu_hub_sensor[4].q[3]',          'IMU_Hub',       'normalized', 'int16',   None,                    10000.0),
    ScalarDef('imu_hub_sensor[4].a[0]',          'IMU_Hub',       'g',         'int16',   None,                    100.0),
    ScalarDef('imu_hub_sensor[4].a[1]',          'IMU_Hub',       'g',         'int16',   None,                    100.0),
    ScalarDef('imu_hub_sensor[4].a[2]',          'IMU_Hub',       'g',         'int16',   None,                    100.0),
    ScalarDef('imu_hub_sensor[4].g[0]',          'IMU_Hub',       'deg/s',     'int16',   None,                    10.0),
    ScalarDef('imu_hub_sensor[4].g[1]',          'IMU_Hub',       'deg/s',     'int16',   None,                    10.0),
    ScalarDef('imu_hub_sensor[4].g[2]',          'IMU_Hub',       'deg/s',     'int16',   None,                    10.0),
    ScalarDef('imu_hub_sensor[5].q[0]',          'IMU_Hub',       'normalized', 'int16',   None,                    10000.0),
    ScalarDef('imu_hub_sensor[5].q[1]',          'IMU_Hub',       'normalized', 'int16',   None,                    10000.0),
    ScalarDef('imu_hub_sensor[5].q[2]',          'IMU_Hub',       'normalized', 'int16',   None,                    10000.0),
    ScalarDef('imu_hub_sensor[5].q[3]',          'IMU_Hub',       'normalized', 'int16',   None,                    10000.0),
    ScalarDef('imu_hub_sensor[5].a[0]',          'IMU_Hub',       'g',         'int16',   None,                    100.0),
    ScalarDef('imu_hub_sensor[5].a[1]',          'IMU_Hub',       'g',         'int16',   None,                    100.0),
    ScalarDef('imu_hub_sensor[5].a[2]',          'IMU_Hub',       'g',         'int16',   None,                    100.0),
    ScalarDef('imu_hub_sensor[5].g[0]',          'IMU_Hub',       'deg/s',     'int16',   None,                    10.0),
    ScalarDef('imu_hub_sensor[5].g[1]',          'IMU_Hub',       'deg/s',     'int16',   None,                    10.0),
    ScalarDef('imu_hub_sensor[5].g[2]',          'IMU_Hub',       'deg/s',     'int16',   None,                    10.0),
    ScalarDef('dio_state',                       'Ext_IO',        'flags',     'uint8',   None,                    None),
    ScalarDef('adc_active_mask',                 'Ext_IO',        'flags',     'uint16',  None,                    None),
    ScalarDef('adc_channel[0]',                  'Ext_IO',        'raw',       'uint16',  None,                    None),
    ScalarDef('adc_channel[1]',                  'Ext_IO',        'raw',       'uint16',  None,                    None),
    ScalarDef('adc_channel[2]',                  'Ext_IO',        'raw',       'uint16',  None,                    None),
    ScalarDef('adc_channel[3]',                  'Ext_IO',        'raw',       'uint16',  None,                    None),
    ScalarDef('adc_channel[4]',                  'Ext_IO',        'raw',       'uint16',  None,                    None),
    ScalarDef('adc_channel[5]',                  'Ext_IO',        'raw',       'uint16',  None,                    None),
    ScalarDef('adc_channel[6]',                  'Ext_IO',        'raw',       'uint16',  None,                    None),
    ScalarDef('adc_channel[7]',                  'Ext_IO',        'raw',       'uint16',  None,                    None),
    ScalarDef('adc_channel[8]',                  'Ext_IO',        'raw',       'uint16',  None,                    None),
    ScalarDef('adc_channel[9]',                  'Ext_IO',        'raw',       'uint16',  None,                    None),
    ScalarDef('adc_channel[10]',                 'Ext_IO',        'raw',       'uint16',  None,                    None),
    ScalarDef('adc_channel[11]',                 'Ext_IO',        'raw',       'uint16',  None,                    None),
    ScalarDef('emg_status_flags',                'EMG_Hub',       'flags',     'uint8',   None,                    None),
    ScalarDef('emg_raw_adc',                     'EMG_Hub',       'raw',       'uint16',  None,                    None),
    ScalarDef('emg_voltage_uv_x10',              'EMG_Hub',       'uV',        'int16',   None,                    10.0),
    ScalarDef('emg_rms_uv_x10',                  'EMG_Hub',       'uV',        'int16',   None,                    10.0),
    ScalarDef('emg_envelope_uv_x10',             'EMG_Hub',       'uV',        'int16',   None,                    10.0),
    ScalarDef('emg_mvc_percent',                 'EMG_Hub',       '%',         'uint8',   None,                    None),
    ScalarDef('emg_is_active',                   'EMG_Hub',       'bool',      'uint8',   None,                    None),
    ScalarDef('rsv_emg',                         'EMG_Hub',       'reserved',  'uint8',   None,                    None),
    ScalarDef('sync_save_active',                'Ext_Sync',      'bool',      'uint8',   None,                    None),
    ScalarDef('sync_din_level',                  'Ext_Sync',      'bool',      'uint8',   None,                    None),
    ScalarDef('sync_din_edge_count',             'Ext_Sync',      'count',     'uint16',  None,                    None),
    ScalarDef('fdcan1_tec',                      'FDCAN1_Diag',   'count',     'uint8',   None,                    None),
    ScalarDef('fdcan1_rec',                      'FDCAN1_Diag',   'count',     'uint8',   None,                    None),
    ScalarDef('fdcan1_lec',                      'FDCAN1_Diag',   'enum',      'uint8',   None,                    None),
    ScalarDef('fdcan1_bus_status',               'FDCAN1_Diag',   'flags',     'uint8',   None,                    None),
    ScalarDef('fdcan1_rx_fifo0_fill',            'FDCAN1_Diag',   'count',     'uint8',   None,                    None),
    ScalarDef('fdcan1_tx_fifo_free',             'FDCAN1_Diag',   'count',     'uint8',   None,                    None),
    ScalarDef('fdcan2_tec',                      'FDCAN2_Diag',   'count',     'uint8',   None,                    None),
    ScalarDef('fdcan2_rec',                      'FDCAN2_Diag',   'count',     'uint8',   None,                    None),
    ScalarDef('fdcan2_lec',                      'FDCAN2_Diag',   'enum',      'uint8',   None,                    None),
    ScalarDef('fdcan2_bus_status',               'FDCAN2_Diag',   'flags',     'uint8',   None,                    None),
    ScalarDef('fdcan2_rx_fifo0_fill',            'FDCAN2_Diag',   'count',     'uint8',   None,                    None),
    ScalarDef('fdcan2_tx_fifo_free',             'FDCAN2_Diag',   'count',     'uint8',   None,                    None),
    ScalarDef('user_f[0]',                       'User_Custom',   'user',      'float32', None,                    None),
    ScalarDef('user_f[1]',                       'User_Custom',   'user',      'float32', None,                    None),
    ScalarDef('user_f[2]',                       'User_Custom',   'user',      'float32', None,                    None),
    ScalarDef('user_f[3]',                       'User_Custom',   'user',      'float32', None,                    None),
    ScalarDef('user_i16[0]',                     'User_Custom',   'user',      'int16',   None,                    None),
    ScalarDef('user_i16[1]',                     'User_Custom',   'user',      'int16',   None,                    None),
    ScalarDef('user_i16[2]',                     'User_Custom',   'user',      'int16',   None,                    None),
    ScalarDef('user_i16[3]',                     'User_Custom',   'user',      'int16',   None,                    None),
    ScalarDef('user_flags',                      'User_Custom',   'flags',     'uint16',  None,                    None),
    ScalarDef('user_u8[0]',                      'User_Custom',   'user',      'uint8',   None,                    None),
    ScalarDef('user_u8[1]',                      'User_Custom',   'user',      'uint8',   None,                    None),
)

SCALAR_NAMES: Tuple[str, ...] = tuple(s.name for s in SCALARS)
SCALAR_GROUPS: Tuple[str, ...] = tuple(s.group for s in SCALARS)
SCALAR_UNITS: Tuple[str, ...] = tuple(s.unit for s in SCALARS)
SCALAR_MUL: Tuple[Optional[float], ...] = tuple(s.mul for s in SCALARS)
SCALAR_DIV: Tuple[Optional[float], ...] = tuple(s.div for s in SCALARS)


# 생성물이 스스로 어긋났는지 import 시점에 잡는다. assert 가 아닌 이유는
# `python -O` 가 assert 를 지우기 때문 — 이 검사는 지워지면 안 된다.
if PACKET_STRUCT.size != TOTAL_PACKET_SIZE:
    raise RuntimeError(
        'struct size %d != TOTAL_PACKET_SIZE %d' % (PACKET_STRUCT.size, TOTAL_PACKET_SIZE))
if len(SCALARS) != NUM_SCALARS or len(TOTAL_DATA_MAP) != NUM_DESCRIPTORS:
    raise RuntimeError(
        'table length mismatch: %d scalars / %d descriptors'
        % (len(SCALARS), len(TOTAL_DATA_MAP)))
