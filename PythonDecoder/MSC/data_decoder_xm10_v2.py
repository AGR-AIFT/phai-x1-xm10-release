"""
XM10 USB MSC Binary Data Decoder V2
=====================================
Self-describing binary format (Header/Footer/Block CRC) 지원 디코더.
기존 raw format (V1)도 하위 호환으로 지원합니다.

사용법:
    python data_decoder_xm10_v2.py <session_folder>
    python data_decoder_xm10_v2.py /LOGS/B005_003
    python data_decoder_xm10_v2.py .

세션 폴더 구조:
    /LOGS/B005_003/
      ├── metadata.txt
      ├── summary.txt
      ├── data_000_part_000.bin   ← V2 format: [Header 32B][Data+CRC blocks][Footer 12B]
      ├── data_000_part_001.bin
      └── ...

V2 Binary Format (.bin):
    [DataLogFileHeader_t: 32 bytes]
        magic=0xA14C4F47, format_version, flags, header_size,
        record_total_bytes, user_payload_bytes, creation_fattime, reserved
    [Data Block 0: <= 4096 bytes]
    [CRC32: 4 bytes]
    [Data Block 1: <= 4096 bytes]
    [CRC32: 4 bytes]
    ...
    [DataLogFileFooter_t: 12 bytes]
        record_count, data_bytes, footer_magic=0x474F4CA1

Data Record Format (within blocks):
    [LogPacketHeader_t: 4 bytes (uint32_t packetSize)]
    [payload: auto_timestamp(4B, optional) + user_data(NB)]

Event Marker (within blocks):
    [LogPacketHeader_t: 4 bytes (packetSize = 0xFFFFFF00 | marker_type)]
    [LogMarkerRecord_t: 12 bytes]
"""

import struct
import os
import sys
import glob
import re

# ============================================================================
# Constants
# ============================================================================
LOG_FILE_MAGIC = 0xA14C4F47
LOG_FILE_FOOTER_MAGIC = 0x474F4CA1
LOG_MARKER_MAGIC_MASK = 0xFFFFFF00

FLAG_AUTO_TIMESTAMP = 0x01
FLAG_BLOCK_CRC = 0x02

CRC_BLOCK_SIZE = 4096
CRC_SIZE = 4

HEADER_SIZE = 28
FOOTER_SIZE = 12
PACKET_HEADER_SIZE = 4

# ============================================================================
# Type mapping: C type string -> Python struct format character
# ============================================================================
TYPE_MAP = {
    'uint32_t': 'I', 'int32_t': 'i',
    'uint16_t': 'H', 'int16_t': 'h',
    'uint8_t': 'B',  'int8_t': 'b',
    'float': 'f',    'double': 'd',
    'bool': '?',
}


# ============================================================================
# Metadata / Format Parsing
# ============================================================================
def parse_metadata(meta_path):
    """metadata.txt에서 User 메타데이터와 System Info를 파싱합니다."""
    system_info = {}
    user_meta_line = ""

    with open(meta_path, 'r', encoding='utf-8') as f:
        in_system = False
        for line in f:
            line = line.strip()
            if line == "=== System Info ===":
                in_system = True
                continue
            if in_system and '=' in line:
                key, val = line.split('=', 1)
                system_info[key.strip()] = val.strip()
            elif not in_system and line:
                user_meta_line = line

    return user_meta_line, system_info


def build_struct_format(user_meta_line):
    """
    User 메타데이터 문자열에서 struct format과 CSV 헤더를 생성합니다.

    예: "hip_L(float), hip_R(float), gait_phase(uint8_t), _pad(3bytes)"
    -> format: '<ffB3x', headers: ['hip_L', 'hip_R', 'gait_phase']
    """
    fmt_chars = ['<']
    headers = []

    fields = [f.strip() for f in user_meta_line.split(',') if f.strip()]

    for field in fields:
        # Padding 필드 우선 체크: _pad(3bytes) 등
        pad_match = re.match(r'_\w+\((\d+)bytes?\)', field)
        if pad_match:
            fmt_chars.append(f"{pad_match.group(1)}x")
            continue

        match = re.match(r'(\w+)\((\w+)\)', field)
        if not match:
            continue

        name, ctype = match.group(1), match.group(2)
        if ctype in TYPE_MAP:
            fmt_chars.append(TYPE_MAP[ctype])
            headers.append(name)

    return ''.join(fmt_chars), headers


# ============================================================================
# File Header / Footer
# ============================================================================
def read_file_header(fin):
    """
    28B 파일 헤더를 읽고 파싱합니다.
    V2 파일이면 header dict를, V1(legacy)이면 None을 반환합니다.
    """
    pos = fin.tell()
    data = fin.read(HEADER_SIZE)
    if len(data) < HEADER_SIZE:
        fin.seek(pos)
        return None

    magic = struct.unpack('<I', data[:4])[0]
    if magic != LOG_FILE_MAGIC:
        # V1 legacy format — 헤더 없음, 파일 시작으로 되돌림
        fin.seek(pos)
        return None

    hdr = struct.unpack('<IBBHIIIII', data)
    return {
        'magic': hdr[0],
        'format_version': hdr[1],
        'flags': hdr[2],
        'header_size': hdr[3],
        'record_total_bytes': hdr[4],
        'user_payload_bytes': hdr[5],
        'creation_fattime': hdr[6],
        'reserved0': hdr[7],
        'reserved1': hdr[8],
        'auto_timestamp': bool(hdr[2] & FLAG_AUTO_TIMESTAMP),
        'has_block_crc': bool(hdr[2] & FLAG_BLOCK_CRC),
    }


def check_footer(fin, file_size):
    """파일 끝에서 Footer를 읽어 정상 종료 여부를 확인합니다."""
    if file_size < FOOTER_SIZE:
        return None

    fin.seek(file_size - FOOTER_SIZE)
    data = fin.read(FOOTER_SIZE)
    if len(data) < FOOTER_SIZE:
        return None

    record_count, data_bytes, footer_magic = struct.unpack('<III', data)
    if footer_magic != LOG_FILE_FOOTER_MAGIC:
        return None

    return {
        'record_count': record_count,
        'data_bytes': data_bytes,
        'footer_magic': footer_magic,
    }


# ============================================================================
# CRC Block Stream Reader
# ============================================================================
class BlockCRCReader:
    """
    4KB 블록 + CRC32 구조에서 순수 데이터만 추출하는 스트림 리더.

    파일 포맷:
        [Header 32B]
        [Block0 <=4096B][CRC 4B][Block1 <=4096B][CRC 4B]...
        [Footer 12B]

    이 리더는 Header 이후 ~ Footer 이전의 영역에서 CRC를 건너뛰고
    순수 데이터만 반환합니다.
    """
    def __init__(self, fin, data_start, data_end, has_crc):
        self.fin = fin
        self.data_start = data_start
        self.data_end = data_end  # Footer 시작 위치 (또는 파일 끝)
        self.has_crc = has_crc
        self.block_offset = 0  # 현재 블록 내 오프셋
        self.buf = b''
        self.buf_pos = 0
        self.crc_errors = 0
        self.crc_verified = 0

        fin.seek(data_start)
        self._fill_buffer()

    def _fill_buffer(self):
        """다음 블록의 데이터를 버퍼에 채웁니다 (CRC 건너뛰기)."""
        self.buf = b''
        self.buf_pos = 0

        remaining_file = self.data_end - self.fin.tell()
        if remaining_file <= 0:
            return

        if not self.has_crc:
            # CRC 없음: 남은 데이터를 적절한 크기로 읽기
            read_size = min(remaining_file, CRC_BLOCK_SIZE)
            self.buf = self.fin.read(read_size)
            return

        # CRC 있음: 블록 데이터 + CRC 4B 읽기
        # 마지막 블록은 4KB보다 작을 수 있음 (partial block)
        if remaining_file <= CRC_SIZE:
            # CRC만 남음 — 건너뛰기
            self.fin.read(remaining_file)
            return

        # 데이터 크기 계산: 남은 바이트 - CRC 4B, 최대 4KB
        available_data = remaining_file - CRC_SIZE
        block_data_size = min(available_data, CRC_BLOCK_SIZE)

        self.buf = self.fin.read(block_data_size)

        # CRC 읽기 (검증 또는 건너뛰기)
        crc_bytes = self.fin.read(CRC_SIZE)
        if len(crc_bytes) == CRC_SIZE:
            self.crc_verified += 1
            # CRC 검증은 선택적 — 여기서는 카운트만
            # (Python에서 CRC-32 Ethernet 검증하려면 별도 라이브러리 필요)

    def read(self, size):
        """순수 데이터를 size 바이트만큼 읽습니다 (블록/CRC 자동 처리)."""
        result = b''
        while len(result) < size:
            available = len(self.buf) - self.buf_pos
            if available <= 0:
                self._fill_buffer()
                if not self.buf:
                    break  # EOF
                continue

            need = size - len(result)
            take = min(need, available)
            result += self.buf[self.buf_pos:self.buf_pos + take]
            self.buf_pos += take

        return result

    def is_eof(self):
        """더 읽을 데이터가 있는지 확인합니다."""
        if self.buf_pos < len(self.buf):
            return False
        return self.fin.tell() >= self.data_end


class RawReader:
    """CRC 없는 raw 파일용 심플 리더 (V1 호환)."""
    def __init__(self, fin):
        self.fin = fin

    def read(self, size):
        return self.fin.read(size)

    def is_eof(self):
        pos = self.fin.tell()
        data = self.fin.read(1)
        if data:
            self.fin.seek(pos)
            return False
        return True


# ============================================================================
# Record Decoder
# ============================================================================
def decode_records(reader, user_fmt, user_struct_size, auto_ts, ts_bytes):
    """
    스트림 리더에서 레코드를 하나씩 읽어 디코딩합니다.

    Returns:
        data_rows: list of CSV value lists (데이터 레코드)
        marker_rows: list of marker dicts (이벤트 마커)
        error_count: 파싱 에러 수
    """
    data_rows = []
    marker_rows = []
    error_count = 0

    while not reader.is_eof():
        # 1. LogPacketHeader_t 읽기 (4 bytes)
        hdr_data = reader.read(PACKET_HEADER_SIZE)
        if len(hdr_data) < PACKET_HEADER_SIZE:
            break

        packet_size = struct.unpack('<I', hdr_data)[0]

        # 2. 마커 체크: 상위 24비트가 0xFFFFFF이면 마커
        if (packet_size & LOG_MARKER_MAGIC_MASK) == LOG_MARKER_MAGIC_MASK:
            marker_data = reader.read(12)  # LogMarkerRecord_t 고정 12B
            if len(marker_data) < 12:
                error_count += 1
                break

            m_header, m_tick, m_data, m_reserved = struct.unpack('<IIHH', marker_data)
            marker_type = m_header & 0xFF
            marker_rows.append({
                'type': marker_type,
                'tick_ms': m_tick,
                'data': m_data,
            })
            continue

        # 3. 일반 데이터 레코드
        payload = reader.read(packet_size)
        if len(payload) < packet_size:
            error_count += 1
            break

        offset = 0
        csv_values = []

        # 타임스탬프
        if auto_ts:
            if len(payload) < ts_bytes:
                error_count += 1
                continue
            tick = struct.unpack('<I', payload[:ts_bytes])[0]
            csv_values.append(str(tick))
            offset = ts_bytes

        # User 데이터
        user_data = payload[offset:]
        if len(user_data) < user_struct_size:
            error_count += 1
            continue

        try:
            values = struct.unpack(user_fmt, user_data[:user_struct_size])
            for v in values:
                if isinstance(v, float):
                    csv_values.append(f"{v:.6f}")
                elif isinstance(v, bool):
                    csv_values.append("1" if v else "0")
                else:
                    csv_values.append(str(v))
            data_rows.append(csv_values)
        except struct.error:
            error_count += 1
            continue

    return data_rows, marker_rows, error_count


# ============================================================================
# File Processing
# ============================================================================
def process_bin_file(bin_path, user_fmt, user_struct_size, auto_ts, ts_bytes):
    """
    단일 .bin 파일을 처리합니다.
    V2(Header/CRC/Footer)와 V1(raw) 자동 감지.
    """
    file_size = os.path.getsize(bin_path)
    if file_size == 0:
        return [], [], 0, None, None

    with open(bin_path, 'rb') as fin:
        # Header 감지
        header = read_file_header(fin)
        footer = None

        if header:
            # V2 format
            print(f"    Format: V2 (version={header['format_version']}, "
                  f"crc={'ON' if header['has_block_crc'] else 'OFF'})")

            # Header에서 auto_ts/payload 정보 가져오기 (metadata보다 header 우선)
            auto_ts_from_header = header['auto_timestamp']
            if auto_ts_from_header != auto_ts:
                print(f"    [INFO] Header auto_ts={auto_ts_from_header} "
                      f"(metadata={auto_ts}), using header value")
                auto_ts = auto_ts_from_header
                ts_bytes = 4 if auto_ts else 0

            # Footer 확인
            footer = check_footer(fin, file_size)
            if footer:
                data_end = file_size - FOOTER_SIZE
                print(f"    Footer: OK (records={footer['record_count']}, "
                      f"data_bytes={footer['data_bytes']})")
            else:
                data_end = file_size
                print(f"    Footer: MISSING (비정상 종료 가능)")

            # 데이터 영역 시작
            data_start = header['header_size']
            fin.seek(data_start)

            reader = BlockCRCReader(fin, data_start, data_end, header['has_block_crc'])
        else:
            # V1 legacy format
            print(f"    Format: V1 (legacy raw)")
            reader = RawReader(fin)

        data_rows, marker_rows, errors = decode_records(
            reader, user_fmt, user_struct_size, auto_ts, ts_bytes)

        crc_info = None
        if header and header['has_block_crc'] and isinstance(reader, BlockCRCReader):
            crc_info = {
                'verified': reader.crc_verified,
                'errors': reader.crc_errors,
            }

    return data_rows, marker_rows, errors, header, footer


# ============================================================================
# Session Decoder (Main Entry)
# ============================================================================
def find_bin_files(session_dir):
    """세션 디렉터리에서 data_*_part_*.bin 파일들을 정렬하여 반환합니다."""
    pattern = os.path.join(session_dir, "data_*_part_*.bin")
    files = sorted(glob.glob(pattern))
    if not files:
        pattern = os.path.join(session_dir, "*.bin")
        files = sorted(glob.glob(pattern))
    return files


def decode_session(session_dir):
    """세션 폴더의 바이너리 파일을 CSV로 변환합니다."""

    # 1. metadata.txt 읽기
    meta_path = os.path.join(session_dir, "metadata.txt")
    if not os.path.exists(meta_path):
        print(f"[ERROR] metadata.txt not found in {session_dir}")
        return

    user_meta, sys_info = parse_metadata(meta_path)
    print(f"[INFO] User metadata: {user_meta}")
    print(f"[INFO] System info keys: {list(sys_info.keys())}")

    auto_ts = sys_info.get('auto_timestamp', '0') == '1'
    ts_bytes = int(sys_info.get('timestamp_bytes', '4')) if auto_ts else 0
    user_payload = int(sys_info.get('user_payload_bytes', '0'))
    format_version = int(sys_info.get('file_format_version', '0'))

    # 2. struct format 생성
    user_fmt, user_headers = build_struct_format(user_meta)
    user_struct_size = struct.calcsize(user_fmt)

    if user_payload > 0 and user_struct_size != user_payload:
        print(f"[WARN] Struct size mismatch: parsed={user_struct_size}, "
              f"metadata={user_payload}")

    # 3. CSV 헤더
    csv_headers = []
    if auto_ts:
        csv_headers.append("tick_ms")
    csv_headers.extend(user_headers)

    print(f"[INFO] Auto timestamp: {'ON' if auto_ts else 'OFF'}")
    print(f"[INFO] File format version: {format_version}")
    print(f"[INFO] User struct: {user_fmt} ({user_struct_size} bytes)")
    print(f"[INFO] CSV columns: {csv_headers}")

    # 4. .bin 파일 찾기
    bin_files = find_bin_files(session_dir)
    if not bin_files:
        print(f"[ERROR] No .bin files found in {session_dir}")
        return

    print(f"[INFO] Found {len(bin_files)} bin file(s)")

    # 5. 디코딩
    output_csv = os.path.join(session_dir, "decoded_output.csv")
    output_markers = os.path.join(session_dir, "events.csv")
    total_records = 0
    total_markers = 0
    total_errors = 0
    all_markers = []
    total_crc_blocks = 0

    with open(output_csv, 'w', encoding='utf-8') as fout:
        fout.write(','.join(csv_headers) + '\n')

        for bin_path in bin_files:
            print(f"\n  Processing: {os.path.basename(bin_path)}")

            data_rows, marker_rows, errors, header, footer = process_bin_file(
                bin_path, user_fmt, user_struct_size, auto_ts, ts_bytes)

            for row in data_rows:
                fout.write(','.join(row) + '\n')

            total_records += len(data_rows)
            total_markers += len(marker_rows)
            total_errors += errors
            all_markers.extend(marker_rows)

            print(f"    -> {len(data_rows)} records, "
                  f"{len(marker_rows)} markers, "
                  f"{errors} errors")

            if footer:
                expected = footer['record_count']
                actual = len(data_rows)
                if expected != actual:
                    print(f"    [WARN] Footer record_count={expected}, "
                          f"decoded={actual}")

    # 6. 마커 파일 출력
    if all_markers:
        with open(output_markers, 'w', encoding='utf-8') as fout:
            fout.write("tick_ms,marker_type,marker_data\n")
            for m in all_markers:
                fout.write(f"{m['tick_ms']},{m['type']},{m['data']}\n")
        print(f"\n[INFO] {total_markers} event markers -> {output_markers}")

    # 7. summary.txt 출력
    summary_path = os.path.join(session_dir, "summary.txt")
    if os.path.exists(summary_path):
        print(f"\n--- Session Summary ---")
        with open(summary_path, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if line and '=' in line:
                    print(f"  {line}")

    # 8. 최종 결과
    print(f"\n{'='*50}")
    print(f"[DONE] {total_records} records -> {output_csv}")
    if total_markers > 0:
        print(f"       {total_markers} markers -> {output_markers}")
    if total_errors > 0:
        print(f"[WARN] {total_errors} records skipped (parse errors)")
    print(f"{'='*50}")


# ============================================================================
# Entry Point
# ============================================================================
if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("XM10 USB MSC Binary Data Decoder V2")
        print("=" * 40)
        print("Usage: python data_decoder_xm10_v2.py <session_folder>")
        print()
        print("Examples:")
        print("  python data_decoder_xm10_v2.py /LOGS/B005_003")
        print("  python data_decoder_xm10_v2.py .  (current folder)")
        print()
        print("Supports:")
        print("  - V2 format: Header(32B) + Block CRC(4KB) + Footer(12B)")
        print("  - V1 format: Legacy raw binary (auto-detected)")
        print("  - Event markers: auto-separated to events.csv")
        sys.exit(1)

    session_path = sys.argv[1]

    if not os.path.isdir(session_path):
        print(f"[ERROR] '{session_path}' is not a directory")
        sys.exit(1)

    decode_session(session_path)
