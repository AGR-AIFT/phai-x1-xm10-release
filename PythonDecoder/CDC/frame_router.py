"""
PhAI V2.2 CDC 프레임 파싱 / 라우팅 — GUI 워커와 CLI 가 함께 쓰는 단일 소스.

cdc_phai_receiver.py 의 PhAISerialWorker / MainWindow / run_cli() 가 모두 이 모듈만
참조한다. 예전에는 GUI 와 CLI 가 프레임 검증·시퀀스 계산을 각자 복사해 갖고 있었고,
그래서 같은 버그를 두 곳에서 따로 고쳐야 했다.

이 파일은 Qt 도 pyserial 도 쓰지 않는다 — 단독으로 import 해서 테스트할 수 있다.
"""
from __future__ import annotations

import numpy as np

# ============================================================================
# Protocol Constants — PhAI V2.2
# ============================================================================

PHAI_SOF = 0xAA
PHAI_HEADER_SIZE = 6   # SOF(1) + LEN(1) + SEQ_ID(2) + MODULE_ID(1) + STATUS(1)
PHAI_CRC_SIZE = 2      # CRC16-CCITT (2 bytes LE)
PHAI_MAX_LEN_UNITS = 255

DEVICE_PERIOD_MS = 1   # XM 제어 주기 1ms

# ----------------------------------------------------------------------------
# 시스템 module_id — XM 펌웨어가 스스로 만들어 보내는 프레임이다.
# 사용자 채널 그래프·CSV 에 섞이면 안 되지만, 버리지도 않는다(개수·바이트를 센다).
# ----------------------------------------------------------------------------
PHAI_MODULE_TOTAL_DATA  = 0x20   # Total Data 자동 스트림 (기본 ON)
PHAI_MODULE_USER_META   = 0xEF   # 사용자 메타(JSON) — float 가 아니다
PHAI_MODULE_LINK_HEALTH = 0xED   # 아직 FW 가 보내지 않는다. 자리만 예약
PHAI_MODULE_SCHEMA_DESC = 0xEE   # 〃

SYSTEM_MODULE_IDS = frozenset({
    PHAI_MODULE_TOTAL_DATA,
    PHAI_MODULE_USER_META,
    PHAI_MODULE_LINK_HEALTH,
    PHAI_MODULE_SCHEMA_DESC,
})

CRC16_TABLE = [
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0,
]


# ============================================================================
# Helpers
# ============================================================================

def crc16_ccitt(data: bytes) -> int:
    """CRC16-CCITT (poly 0x1021, init 0xFFFF)."""
    crc = 0xFFFF
    for b in data:
        crc = ((crc << 8) & 0xFFFF) ^ CRC16_TABLE[((crc >> 8) ^ b) & 0xFF]
    return crc


def cobs_decode(encoded: bytes) -> bytes:
    """COBS-decode a frame (without trailing 0x00 delimiter)."""
    out = bytearray()
    i = 0
    while i < len(encoded):
        code = encoded[i]
        i += 1
        if code == 0:
            break
        for _ in range(1, code):
            if i >= len(encoded):
                break
            out.append(encoded[i])
            i += 1
        if code < 0xFF and i < len(encoded):
            out.append(0x00)
    return bytes(out)


# ============================================================================
# Parsed Frame
# ============================================================================

class PhAIFrame:
    """payload 를 float 로 미리 바꾸지 않는다.

    예전 PhAIPacket 은 파싱하는 순간 모든 payload 를 float32 로 해석해서 들고 있었다.
    그래서 0xEF(JSON 문자열) 같은 비-float 프레임까지 float 로 뭉개졌다.
    실제로 사용자 채널로 쓸 때만 as_float32() 를 부른다.
    """
    __slots__ = ('seq_id', 'module_id', 'status', 'tx_drops',
                 'payload', 'wire_len', 'recv_t', 'device_time_s')

    def __init__(self, seq_id, module_id, status, payload: bytes,
                 wire_len: int, recv_t: float):
        self.seq_id = seq_id
        self.module_id = module_id
        self.status = status
        self.tx_drops = status & 0x7F
        self.payload = payload
        self.wire_len = wire_len   # COBS 풀린 raw 프레임 바이트 수 (헤더+패딩 payload+CRC)
        self.recv_t = recv_t
        # 이 프레임이 장치 시각 몇 초에 해당하는지. FrameRouter.route() 가 채운다.
        # 프레임이 직접 들고 있어야 나중에 큐에서 꺼낼 때도 값이 맞다.
        self.device_time_s = 0.0

    def as_float32(self) -> np.ndarray:
        return np.frombuffer(self.payload, dtype='<f4').copy()


def parse_phai_frame(frame: bytes, recv_t: float):
    """COBS 를 푼 raw 프레임 하나를 검증한다.

    반환: (PhAIFrame, None) 성공 / (None, 'sync') 또는 (None, 'crc') 실패.
    검증 순서는 예전 PhAISerialWorker._parse_frame 과 같다 — 위치만 옮겼다.
    """
    min_size = PHAI_HEADER_SIZE + PHAI_CRC_SIZE
    if len(frame) < min_size or frame[0] != PHAI_SOF:
        return None, 'sync'

    len_units = frame[1]
    if len_units == 0 or len_units > PHAI_MAX_LEN_UNITS:
        return None, 'sync'

    expected_size = PHAI_HEADER_SIZE + (len_units * 4) + PHAI_CRC_SIZE
    if len(frame) < expected_size:
        return None, 'sync'

    crc_recv = frame[expected_size - 2] | (frame[expected_size - 1] << 8)
    crc_calc = crc16_ccitt(frame[:expected_size - PHAI_CRC_SIZE])
    if crc_calc != crc_recv:
        return None, 'crc'

    seq_id = frame[2] | (frame[3] << 8)
    module_id = frame[4]
    status_byte = frame[5]
    payload = bytes(frame[PHAI_HEADER_SIZE:PHAI_HEADER_SIZE + len_units * 4])

    return PhAIFrame(seq_id, module_id, status_byte, payload,
                     expected_size, recv_t), None


# ============================================================================
# Global Sequence Ledger
# ============================================================================

class GlobalSequenceLedger:
    """공유 16-bit seq_id 전용 회계.

    seq_id 는 module_id 와 무관하게 펌웨어의 카운터 하나가 발급한다.
    그래서 특정 module 의 프레임끼리만 seq 를 비교하면, 그 사이에 낀 다른 module
    프레임 수만큼 매번 "손실"로 잡힌다 — 실제로는 아무것도 잃지 않았는데도.

    그래서 CRC 를 통과한 모든 프레임을 도착 순서대로, module 을 나누기 전에
    여기에 먹여야 한다. FrameRouter.route() 가 그 순서를 강제한다.

    seq 가 뒤로 가면(펌웨어 재시작, 중복 프레임 등) 손실로 세지 않고 resync_count 로
    따로 센 뒤 그 자리에서 기준을 다시 잡는다. 16비트 카운터라 '앞으로 많이'와
    '뒤로 조금'이 같은 값으로 보이기 때문에, 절반(32768)을 넘으면 뒤로 간 것으로 읽는다.
    """
    __slots__ = ('_last_seq', '_started', 'device_time_s',
                 'lost_count', 'zero_delta_count', 'resync_count', 'frame_count')

    def __init__(self):
        self._last_seq = -1
        self._started = False
        self.device_time_s = 0.0
        self.lost_count = 0        # 누적 손실 프레임 수 (전역). 클램프 없음
        self.zero_delta_count = 0  # delta==0 (65536 배수 wrap 또는 중복 seq). 손실 아님
        self.resync_count = 0      # seq 가 뒤로 간 횟수. 손실 아님 — 기준을 다시 잡는다
        self.frame_count = 0

    def observe(self, seq_id: int) -> int:
        self.frame_count += 1

        if not self._started:
            self._started = True
            self._last_seq = seq_id
            return 1

        delta = (seq_id - self._last_seq) & 0xFFFF
        self._last_seq = seq_id

        if delta == 0:
            self.zero_delta_count += 1
            return 0

        if delta >= 0x8000:
            # 앞으로 32768틱(32초) 이상 건너뛴 것보다 seq 가 뒤로 간 쪽이 훨씬 흔하다.
            # 손실로 세면 한 프레임이 통계와 시간축을 세션 내내 오염시킨다.
            # 예전 코드의 클램프가 (의도치 않게) 막아주던 경우이기도 하다.
            self.resync_count += 1
            return 0

        if delta > 1:
            # 예전 코드에는 여기 'delta > 1000 이면 1로 친다' 는 클램프가 있었다.
            # 1초를 넘는 단선은 손실이 아예 0건으로 보고돼 문제를 숨겼다
            # (delta 를 1 로 덮어쓰면 뒤따르는 `if delta != 1` 이 False 가 된다).
            self.lost_count += delta - 1

        self.device_time_s += delta * (DEVICE_PERIOD_MS / 1000.0)
        return delta


# ============================================================================
# System Frame Tap
# ============================================================================

class SystemFrameTap:
    """시스템 module 프레임을 버리지 않았다는 증거.

    "사용자 버퍼에 안 섞였다" 와 "정확히 보존됐다" 는 다른 주장이다.
    개수와 바이트를 세어 두 번째를 검증할 수 있게 만든다.
    """
    __slots__ = ('module_id', 'frame_count', 'byte_count')

    def __init__(self, module_id: int):
        self.module_id = module_id
        self.frame_count = 0
        self.byte_count = 0

    def push(self, frame: PhAIFrame):
        self.frame_count += 1
        self.byte_count += frame.wire_len


# ============================================================================
# Frame Router
# ============================================================================

class FrameRouter:
    """프레임 하나가 갈 곳을 정한다.

    순서가 핵심이다 — ledger 를 먼저 갱신하고 그 다음에 module 을 나눈다.
    거꾸로 하면(시스템 프레임을 먼저 걸러내면) 걸러낸 프레임의 seq 만큼
    정상 프레임마다 거짓 손실이 잡힌다.
    """

    def __init__(self):
        self.ledger = GlobalSequenceLedger()
        self.system_taps = {mid: SystemFrameTap(mid) for mid in SYSTEM_MODULE_IDS}
        self.other_user_frames = 0     # primary 아닌 사용자 module (드묾) — 세기만 한다
        self.primary_user_module = -1  # 첫 사용자 프레임의 module_id. GUI/CSV 가 이걸 잠근다

    def route(self, frame: PhAIFrame):
        """반환: ('system' | 'user_primary' | 'user_other', seq_delta)"""
        delta = self.ledger.observe(frame.seq_id)   # 반드시 module 분기보다 먼저
        frame.device_time_s = self.ledger.device_time_s

        if frame.module_id in SYSTEM_MODULE_IDS:
            self.system_taps[frame.module_id].push(frame)
            return 'system', delta

        if self.primary_user_module < 0:
            self.primary_user_module = frame.module_id

        if frame.module_id != self.primary_user_module:
            self.other_user_frames += 1
            return 'user_other', delta

        return 'user_primary', delta
