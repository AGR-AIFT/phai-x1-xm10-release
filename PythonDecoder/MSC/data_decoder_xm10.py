"""
XM10 USB MSC Binary Data Decoder
=================================
USB 메모리에 저장된 바이너리 데이터를 CSV로 변환합니다.

사용법:
    python data_decoder_xm10.py <session_folder>
    python data_decoder_xm10.py /LOGS/BasicTest
    python data_decoder_xm10.py .                  # 현재 폴더

세션 폴더 구조:
    /LOGS/BasicTest/
      ├── metadata.txt            ← System이 자동 생성 (파싱 대상)
      ├── summary.txt             ← 세션 통계 (선택적 출력)
      ├── data_000_part_000.bin   ← 바이너리 데이터
      ├── data_000_part_001.bin   ← (파일 롤링 시 추가 파트)
      └── ...

바이너리 레코드 포맷:
    [LogPacketHeader_t:4 bytes (uint32_t payload_size)]
    [payload: auto_timestamp(4 bytes, optional) + user_data(N bytes)]

metadata.txt 포맷:
    <user metadata line>        ← 필드 설명 (예: "hip_L(float), hip_R(float)")
    
    === System Info ===
    auto_timestamp=1
    timestamp_bytes=4
    user_payload_bytes=24
    record_header_bytes=4
    record_total_bytes=32
    rolling_size_mb=10
    ...
"""

import struct
import os
import sys
import glob
import re

# ============================================================================
# Type mapping: C type string → Python struct format character
# ============================================================================
TYPE_MAP = {
    'uint32_t': 'I',
    'int32_t':  'i',
    'uint16_t': 'H',
    'int16_t':  'h',
    'uint8_t':  'B',
    'int8_t':   'b',
    'float':    'f',
    'double':   'd',
    'bool':     '?',
}

TYPE_SIZE = {
    'I': 4, 'i': 4, 'H': 2, 'h': 2, 'B': 1, 'b': 1,
    'f': 4, 'd': 8, '?': 1,
}


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
    → format: '<ff B 3x'
    → headers: ['hip_L', 'hip_R', 'gait_phase']
    """
    fmt_chars = ['<']  # Little-endian
    headers = []
    
    fields = [f.strip() for f in user_meta_line.split(',') if f.strip()]

    for field in fields:
        match = re.match(r'(\w+)\((\w+)\)', field)
        if not match:
            pad_match = re.match(r'_\w+\((\d+)bytes?\)', field)
            if pad_match:
                fmt_chars.append(f"{pad_match.group(1)}x")
            continue

        name, ctype = match.group(1), match.group(2)
        if ctype in TYPE_MAP:
            fmt_chars.append(TYPE_MAP[ctype])
            headers.append(name)

    return ''.join(fmt_chars), headers


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
        print("  → STRUCT_FMT를 수동으로 지정하세요 (하단 Manual Mode 참조)")
        return

    user_meta, sys_info = parse_metadata(meta_path)
    print(f"[INFO] User metadata: {user_meta}")
    print(f"[INFO] System info: {sys_info}")

    auto_ts = sys_info.get('auto_timestamp', '0') == '1'
    ts_bytes = int(sys_info.get('timestamp_bytes', '4')) if auto_ts else 0
    user_payload = int(sys_info.get('user_payload_bytes', '0'))

    # 2. struct format 생성
    user_fmt, user_headers = build_struct_format(user_meta)
    user_struct_size = struct.calcsize(user_fmt)

    if user_payload > 0 and user_struct_size != user_payload:
        print(f"[WARN] Struct size mismatch: parsed={user_struct_size}, metadata={user_payload}")
        print(f"  → padding이 있을 수 있습니다. 수동 확인 필요")

    # 3. CSV 헤더 구성
    csv_headers = []
    if auto_ts:
        csv_headers.append("tick_ms")
    csv_headers.extend(user_headers)

    print(f"[INFO] Auto timestamp: {'ON' if auto_ts else 'OFF'}")
    print(f"[INFO] User struct: {user_fmt} ({user_struct_size} bytes)")
    print(f"[INFO] CSV columns: {csv_headers}")

    # 4. 바이너리 파일 찾기
    bin_files = find_bin_files(session_dir)
    if not bin_files:
        print(f"[ERROR] No .bin files found in {session_dir}")
        return

    print(f"[INFO] Found {len(bin_files)} bin file(s)")

    # 5. 디코딩
    output_csv = os.path.join(session_dir, "decoded_output.csv")
    total_records = 0
    error_count = 0

    with open(output_csv, 'w', encoding='utf-8') as fout:
        fout.write(','.join(csv_headers) + '\n')

        for bin_path in bin_files:
            print(f"  Processing: {os.path.basename(bin_path)}")
            file_records = 0

            with open(bin_path, 'rb') as fin:
                while True:
                    # 5a. LogPacketHeader_t 읽기 (4 bytes)
                    hdr = fin.read(4)
                    if len(hdr) < 4:
                        break

                    payload_size = struct.unpack('<I', hdr)[0]

                    # 5b. Payload 읽기
                    payload = fin.read(payload_size)
                    if len(payload) != payload_size:
                        print(f"    [WARN] Truncated record at offset {fin.tell()}")
                        break

                    # 5c. 타임스탬프 추출 (auto_timestamp ON인 경우)
                    offset = 0
                    csv_values = []

                    if auto_ts:
                        if len(payload) < ts_bytes:
                            error_count += 1
                            continue
                        tick = struct.unpack('<I', payload[:ts_bytes])[0]
                        csv_values.append(str(tick))
                        offset = ts_bytes

                    # 5d. User 데이터 언패킹
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

                        fout.write(','.join(csv_values) + '\n')
                        file_records += 1

                    except struct.error as e:
                        error_count += 1
                        continue

            total_records += file_records
            print(f"    → {file_records} records decoded")

    # 6. summary.txt 출력
    summary_path = os.path.join(session_dir, "summary.txt")
    if os.path.exists(summary_path):
        print(f"\n--- Session Summary ---")
        with open(summary_path, 'r') as f:
            for line in f:
                line = line.strip()
                if '=' in line:
                    print(f"  {line}")

    print(f"\n[DONE] Total {total_records} records → {output_csv}")
    if error_count > 0:
        print(f"[WARN] {error_count} records skipped (parse errors)")


# ============================================================================
# Manual Mode: metadata.txt가 없는 경우 (리팩토링 이전 데이터 호환)
# ============================================================================
def decode_legacy(bin_path, struct_fmt, csv_header, output_path, has_auto_ts=False):
    """
    metadata.txt 없이 직접 포맷을 지정하여 디코딩합니다.
    
    예: decode_legacy(
            'data_010.bin',
            '<IBB8f??BB21f',
            'LoopCnt,Mode,Level,Angle1,...',
            'result.csv'
        )
    """
    struct_size = struct.calcsize(struct_fmt)
    print(f"[Legacy] Struct size: {struct_size} bytes, auto_ts: {has_auto_ts}")

    count = 0
    with open(bin_path, 'rb') as fin, open(output_path, 'w') as fout:
        header_line = "tick_ms," + csv_header if has_auto_ts else csv_header
        fout.write(header_line + '\n')

        while True:
            hdr = fin.read(4)
            if len(hdr) < 4:
                break

            payload_size = struct.unpack('<I', hdr)[0]
            payload = fin.read(payload_size)
            if len(payload) != payload_size:
                break

            offset = 0
            line_parts = []

            if has_auto_ts:
                tick = struct.unpack('<I', payload[:4])[0]
                line_parts.append(str(tick))
                offset = 4

            user_data = payload[offset:]
            if len(user_data) < struct_size:
                continue

            try:
                values = struct.unpack(struct_fmt, user_data[:struct_size])
                for v in values:
                    if isinstance(v, float):
                        line_parts.append(f"{v:.4f}")
                    elif isinstance(v, bool):
                        line_parts.append("1" if v else "0")
                    else:
                        line_parts.append(str(v))

                fout.write(','.join(line_parts) + '\n')
                count += 1
            except struct.error:
                continue

    print(f"[Legacy] Done: {count} records → {output_path}")


# ============================================================================
# Entry Point
# ============================================================================
if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python data_decoder_xm10.py <session_folder>")
        print("  예: python data_decoder_xm10.py /LOGS/BasicTest")
        print("  예: python data_decoder_xm10.py .  (현재 폴더)")
        sys.exit(1)

    session_path = sys.argv[1]

    if not os.path.isdir(session_path):
        print(f"[ERROR] '{session_path}' is not a directory")
        sys.exit(1)

    decode_session(session_path)
