#!/usr/bin/env python3
"""`.xmlog` v1 회귀 시험 — 보드 없이 돈다.

    python test_xmlog.py

오라클이 왜 손으로 적은 바이트인가
----------------------------------
writer 가 만든 것을 reader 로 읽어 같으면 통과 — 이건 시험이 아니다. 필드 순서를
뒤집어도, 엔디안을 반대로 해도, 폭을 잘못 잡아도 **둘이 사이좋게 같이 틀린다.**
그래서 설계 문서(PLAN 4.6)의 표를 보고 **바이트를 직접 타이핑**했다. 구현은 이걸
재현해야 하고, 못 하면 구현이 틀린 것이다.

CRC 만은 손으로 못 적는다. 대신 **구현의 헬퍼를 부르지 않고** 시험 안에서
`zlib.crc32(buf[:12] + buf[16:])` 를 직접 계산한다 — 범위 규약(자기 자신만 skip)이
코드와 문서에서 각각 독립으로 표현되도록.
"""
import os
import struct
import sys
import tempfile
import zlib

import xmlog as X


# =============================================================================
# 손으로 적은 골든 바이트 — PLAN 4.6 표에서 직접 옮김
# =============================================================================

# create_unix_ns = 0x0123456789ABCDEF
GOLDEN_CREATE_NS = 0x0123456789ABCDEF

GOLDEN_FILE_HEADER_NO_CRC = (
    b"XMLOG\x00\x00\x00"          # off  0..7   magic (디스크 바이트 그대로)
    b"\x01\x00"                    # off  8..9   format_version = 1
    b"\x20\x00"                    # off 10..11  header_size = 32
    b"\x00\x00\x00\x00"            # off 12..15  header_crc32  (여기만 자리 비움)
    b"\xEF\xCD\xAB\x89\x67\x45\x23\x01"   # off 16..23  create_unix_ns LE
    b"\x00\x00\x00\x00"            # off 24..27  reserved0
    b"\x00\x00\x00\x00"            # off 28..31  reserved1
)

GOLDEN_GAP_NO_CRC = (
    b"XMR1"                        # off  0..3   rec_magic — 정수 0x584D5231 아님
    b"\x04"                        # off  4      rec_type = 4 (GAP)
    b"\x00"                        # off  5      flags
    b"\x10\x00"                    # off  6..7   header_rest_len = 16
    b"\x00\x00\x00\x00"            # off  8..11  payload_len = 0
    b"\x00\x00\x00\x00"            # off 12..15  rec_crc32
    b"\x01"                        # off 16      reason = 1 (seq gap)
    b"\x00"                        # off 17      reserved
    b"\x34\x12"                    # off 18..19  from_seq = 0x1234
    b"\x40\x12"                    # off 20..21  to_seq   = 0x1240
    b"\x00\x00"                    # off 22..23  reserved2
    b"\x0C\x00\x00\x00"            # off 24..27  lost_count = 12
    b"\x00\x00\x00\x00"            # off 28..31  reserved3
)

GOLDEN_DATA_NO_CRC = (
    b"XMR1"                        # off  0..3
    b"\x03"                        # off  4      rec_type = 3 (DATA)
    b"\x00"                        # off  5      flags
    b"\x10\x00"                    # off  6..7   header_rest_len = 16
    b"\x04\x00\x00\x00"            # off  8..11  payload_len = 4
    b"\x00\x00\x00\x00"            # off 12..15  rec_crc32
    b"\x00\x00"                    # off 16..17  activation_id = 0 (스키마 미상)
    b"\xF0"                        # off 18      module_id = 0xF0
    b"\x00"                        # off 19      reserved
    b"\x07\x00"                    # off 20..21  seq_id = 7
    b"\x00\x00"                    # off 22..23  reserved2
    b"\x88\x77\x66\x55\x44\x33\x22\x11"   # off 24..31  pc_time_us LE
    b"\xDE\xAD\xBE\xEF"            # off 32..35  payload
)

GOLDEN_ACTIVATION_NO_CRC = (
    b"XMR1"                        # off  0..3
    b"\x02"                        # off  4      rec_type = 2 (SCHEMA_ACTIVATION)
    b"\x00"                        # off  5      flags
    b"\x0C\x00"                    # off  6..7   header_rest_len = 12
    b"\x03\x00\x00\x00"            # off  8..11  payload_len = 3
    b"\x00\x00\x00\x00"            # off 12..15  rec_crc32
    b"\x01\x00"                    # off 16..17  activation_id = 1
    b"\xF1"                        # off 18      module_id = 0xF1
    b"\x01"                        # off 19      schema_proto_ver = 1
    b"\x10\x00"                    # off 20..21  struct_size = 16
    b"\x00\x00"                    # off 22..23  reserved
    b"\x78\x56\x34\x12"            # off 24..27  schema_crc32 = 0x12345678
    b"\xAA\xBB\xCC"                # off 28..30  payload (0xEE canonical 바이트 자리)
)


def _crc_of(no_crc_bytes: bytes) -> int:
    """시험이 스스로 계산하는 CRC. 구현의 _crc_skip_own 을 부르지 않는다."""
    return zlib.crc32(no_crc_bytes[:12] + no_crc_bytes[16:]) & 0xFFFFFFFF


def _with_crc(no_crc_bytes: bytes) -> bytes:
    b = bytearray(no_crc_bytes)
    struct.pack_into("<I", b, 12, _crc_of(no_crc_bytes))
    return bytes(b)


# =============================================================================
# 시험
# =============================================================================

def test_golden_file_header():
    got = X.encode_file_header(GOLDEN_CREATE_NS)
    want = _with_crc(GOLDEN_FILE_HEADER_NO_CRC)
    assert len(got) == 32, "FileHeader %d B != 32" % len(got)
    assert got == want, "FileHeader 바이트 불일치\n got %s\nwant %s" % (got.hex(), want.hex())


def test_golden_gap():
    got = X.encode_gap(reason=1, from_seq=0x1234, to_seq=0x1240, lost_count=12)
    want = _with_crc(GOLDEN_GAP_NO_CRC)
    assert got == want, "GAP 바이트 불일치\n got %s\nwant %s" % (got.hex(), want.hex())


def test_golden_data():
    got = X.encode_data(activation_id=0, module_id=0xF0, seq_id=7,
                        pc_time_us=0x1122334455667788, payload=b"\xDE\xAD\xBE\xEF")
    want = _with_crc(GOLDEN_DATA_NO_CRC)
    assert got == want, "DATA 바이트 불일치\n got %s\nwant %s" % (got.hex(), want.hex())


def test_golden_activation():
    got = X.encode_schema_activation(activation_id=1, module_id=0xF1,
                                     schema_proto_ver=1, struct_size=16,
                                     schema_crc32=0x12345678,
                                     schema_payload=b"\xAA\xBB\xCC")
    want = _with_crc(GOLDEN_ACTIVATION_NO_CRC)
    assert got == want, "ACTIVATION 바이트 불일치\n got %s\nwant %s" % (got.hex(), want.hex())


# SESSION — 필드가 가장 많고 고정폭 문자열 인코딩이 관여한다. 골든 바이트가 없으면
# _fixed() 의 NUL 패딩/절단 규칙이 표와 어긋나도 round-trip 시험은 통과한다 (감사 #9).
GOLDEN_SESSION_NO_CRC = (
    b"XMR1"                        # off  0..3
    b"\x01"                        # off  4      rec_type = 1 (SESSION)
    b"\x00"                        # off  5      flags
    b"\x48\x00"                    # off  6..7   header_rest_len = 72
    b"\x00\x00\x00\x00"            # off  8..11  payload_len = 0
    b"\x00\x00\x00\x00"            # off 12..15  rec_crc32
    # --- SESSION 헤더 72 B ---
    b"SN-GOLDEN" + b"\x00" * 15    # off 16..39  device_usb_serial[24]
    + b"fw-2.8" + b"\x00" * 18     # off 40..63  fw_build_id[24]
    + b"2.8" + b"\x00" * 5         # off 64..71  total_data_map_version[8]
    + b"\x07\x00\x00\x00"          # off 72..75  boot_epoch = 7
    b"\x03\x00\x00\x00"            # off 76..79  link_epoch = 3
    b"\x88\x77\x66\x55\x44\x33\x22\x11"   # off 80..87  host_unix_ns LE
)


def test_golden_session():
    got = X.encode_session(device_usb_serial="SN-GOLDEN", fw_build_id="fw-2.8",
                           total_data_map_version="2.8", boot_epoch=7,
                           link_epoch=3, host_unix_ns=0x1122334455667788)
    want = _with_crc(GOLDEN_SESSION_NO_CRC)
    assert len(got) == 88, "16 + 72 = 88 이어야: %d" % len(got)
    assert got == want, "SESSION 바이트 불일치\n got %s\nwant %s" % (got.hex(), want.hex())


def test_session_first_is_enforced():
    """PLAN 4.6 SESSION-first — reader 는 거부하고 writer 는 못 쓰게 막아야 한다."""
    buf = bytearray(X.encode_file_header(0))
    buf += X.encode_data(0, 0x20, 1, 1, b"\x01\x02\x03\x04")
    try:
        X.scan(bytes(buf))
    except X.LogError as e:
        assert not e.recoverable, "SESSION-first 위반은 복구 불가여야 한다"
    else:
        raise AssertionError("SESSION 없이 시작하는 파일을 받아들였다")

    import tempfile
    import os as _os
    d = tempfile.mkdtemp(prefix="xmlog_sf_")
    try:
        w = X.XmLogWriter(_os.path.join(d, "a.xmlog"))
        try:
            w.data(0x20, 1, 1, b"\x00\x00\x00\x00")
        except ValueError:
            pass
        else:
            raise AssertionError("writer 가 SESSION 없이 DATA 를 썼다")
        finally:
            w.close()
    finally:
        import shutil
        shutil.rmtree(d, ignore_errors=True)


def test_schema_activation_dedup():
    """PLAN 4.6 중복 억제 — 같은 (module, crc) 는 레코드를 다시 쓰지 않는다."""
    import tempfile
    import os as _os
    import shutil
    d = tempfile.mkdtemp(prefix="xmlog_dedup_")
    try:
        p = _os.path.join(d, "a.xmlog")
        w = X.XmLogWriter(p, flush_every=1)
        w.session(fw_build_id="fw")
        ids = [w.schema_activation(0xF0, 1, 8, 0xDEADBEEF, b"\x01" * 8)
               for _ in range(10)]
        other = w.schema_activation(0xF1, 1, 8, 0xDEADBEEF, b"\x02" * 8)
        w.close()

        assert set(ids) == {1}, "같은 스키마 10회에 id 가 %r — 재사용되지 않았다" % (sorted(set(ids)),)
        assert other == 2, "다른 모듈은 새 id 여야 한다: %r" % other

        res = X.read_file(p)
        n_act = sum(1 for r in res.records if r.rec_type == X.REC_SCHEMA_ACTIVATION)
        assert n_act == 2, "SCHEMA_ACTIVATION 레코드 %d개 (2개여야 — 10회는 억제)" % n_act
    finally:
        shutil.rmtree(d, ignore_errors=True)


def test_magic_is_bytes_not_int():
    """rev2 가 틀렸던 자리 — 0x584D5231 을 LE 로 쓰면 디스크엔 '1RMX' 가 찍힌다."""
    rec = X.encode_gap(1, 0, 0, 0)
    assert rec[:4] == b"XMR1", "디스크 바이트가 %r" % (rec[:4],)
    assert rec[:4] != struct.pack("<I", 0x584D5231), "정수+LE 로 쓰면 이렇게 뒤집힌다"
    hdr = X.encode_file_header(0)
    assert hdr[:8] == b"XMLOG\x00\x00\x00"


def test_crc_range_is_skip_not_zerofill():
    """CRC 는 자기 자신 4바이트만 건너뛴다. 그 규약을 바이트로 확인한다."""
    rec = X.encode_gap(1, 0x1234, 0x1240, 12)
    stored = struct.unpack_from("<I", rec, 12)[0]

    # 1) 시험이 직접 계산한 값과 같아야 한다
    assert stored == zlib.crc32(rec[:12] + rec[16:]) & 0xFFFFFFFF, "CRC 범위 규약 불일치"

    # 2) zero-fill 방식이었다면 이 값이 나왔을 것 — 달라야 한다
    zf = bytearray(rec)
    struct.pack_into("<I", zf, 12, 0)
    assert stored != zlib.crc32(bytes(zf)) & 0xFFFFFFFF, "skip 이 아니라 zero-fill 로 계산됐다"

    # 3) 커버 범위 안의 어느 바이트를 바꿔도 검증이 깨져야 한다
    for i in list(range(0, 12)) + list(range(16, len(rec))):
        bad = bytearray(rec)
        bad[i] ^= 0xFF
        try:
            X.decode_record(bytes(bad), 0)
        except X.LogError:
            continue
        raise AssertionError("offset %d 를 바꿨는데 통과했다" % i)


def test_fixed_string_nul_termination():
    """PLAN 4.0: NUL 이 없으면 무효. 잘릴 때도 종료를 보장해야 한다."""
    rec = X.encode_session(device_usb_serial="A" * 40, fw_build_id="build-xyz",
                           total_data_map_version="2.8")
    got = X.decode_record(rec, 0)
    assert got.fields["device_usb_serial"] == "A" * 23, \
        "24 B 칸에 23 자 + NUL 이어야 한다: %r" % got.fields["device_usb_serial"]
    assert got.fields["fw_build_id"] == "build-xyz"
    assert got.fields["total_data_map_version"] == "2.8"


def test_roundtrip_all_types():
    buf = bytearray(X.encode_file_header(GOLDEN_CREATE_NS))
    buf += X.encode_session(device_usb_serial="SN123", fw_build_id="fw-2.8",
                            total_data_map_version="2.8", boot_epoch=3,
                            link_epoch=9, host_unix_ns=42)
    buf += X.encode_schema_activation(1, 0xF0, 1, 16, 0xDEADBEEF, b"\x01\x02\x03\x04")
    buf += X.encode_data(1, 0xF0, 100, 1234567, b"\x00" * 16)
    buf += X.encode_gap(X.GAP_QUEUE_OVERFLOW, 100, 105, 4)
    buf += X.encode_data(0, 0x20, 106, 1234999, b"\xFF" * 8)

    res = X.scan(bytes(buf))
    assert res.stopped_reason is None, res.stopped_reason
    assert res.trailing_bytes == 0
    assert len(res.records) == 5, len(res.records)
    assert res.header.create_unix_ns == GOLDEN_CREATE_NS

    kinds = [r.rec_type for r in res.records]
    assert kinds == [X.REC_SESSION, X.REC_SCHEMA_ACTIVATION, X.REC_DATA,
                     X.REC_GAP, X.REC_DATA], kinds

    act = res.records[1].fields
    assert (act["module_id"], act["struct_size"], act["schema_crc32"]) == (0xF0, 16, 0xDEADBEEF)
    assert res.records[1].payload == b"\x01\x02\x03\x04"

    gap = res.records[3].fields
    assert (gap["reason"], gap["from_seq"], gap["to_seq"], gap["lost_count"]) == (2, 100, 105, 4)

    # 스키마 없이 들어온 데이터도 그대로 남아 있어야 한다
    assert res.records[4].fields["activation_id"] == 0
    assert res.records[4].payload == b"\xFF" * 8


def test_truncation_sweep():
    """마지막 레코드를 **바이트 단위로** 잘라 가며 — 완전한 것은 전부 살고,
    부분 레코드 하나만 거부되어야 한다. 크래시 복구의 핵심 계약이다."""
    buf = bytearray(X.encode_file_header(0))
    buf += X.encode_session(fw_build_id="fw")      # SESSION-first (PLAN 4.6)
    complete = []
    for i in range(4):
        rec = X.encode_data(0, 0x20, i, i * 1000, bytes([i]) * 8)
        complete.append(len(rec))
        buf += rec
    full = bytes(buf)
    last_len = complete[-1]

    for cut in range(1, last_len + 1):
        truncated = full[:len(full) - cut]
        res = X.scan(truncated)
        assert len(res.records) == 4, \
            "%d B 잘랐을 때 완전 레코드 %d 개 (SESSION+DATA 3 = 4 여야)" % (cut, len(res.records))
        assert res.trailing_bytes == last_len - cut, \
            "%d B 잘랐을 때 trailing %d" % (cut, res.trailing_bytes)
        if cut < last_len:
            assert res.stopped_reason is not None, "%d B 잘렸는데 멈추지 않았다" % cut

    # 정확히 레코드 경계에서 끝나면 아무 문제 없어야 한다
    res = X.scan(full[:len(full) - last_len])
    assert res.stopped_reason is None and res.trailing_bytes == 0


def test_zero_tail_is_rejected():
    """rev1 의 사전할당이 왜 안 되는지 — zero tail 이 정상 레코드로 보이면 안 된다."""
    buf = bytearray(X.encode_file_header(0))
    buf += X.encode_session(fw_build_id="fw")      # SESSION-first (PLAN 4.6)
    buf += X.encode_data(0, 0x20, 1, 1, b"\x01\x02\x03\x04")
    good = len(buf)
    buf += b"\x00" * 4096                     # 사전할당이 남길 미기록 영역
    res = X.scan(bytes(buf))
    assert len(res.records) == 2, len(res.records)
    assert res.valid_bytes == good, "%d != %d" % (res.valid_bytes, good)
    assert res.trailing_bytes == 4096
    assert res.stopped_reason is not None


def test_writer_and_crash_recovery():
    tmp = tempfile.mkdtemp(prefix="xmlog_test_")
    path = os.path.join(tmp, "a.xmlog")
    try:
        w = X.XmLogWriter(path, create_unix_ns=GOLDEN_CREATE_NS, flush_every=1)
        w.session(device_usb_serial="SN", fw_build_id="fw", total_data_map_version="2.8")
        a1 = w.schema_activation(0xF0, 1, 8, 0x11111111, b"\x01" * 8)
        a2 = w.schema_activation(0xF1, 1, 8, 0x22222222, b"\x02" * 8)
        assert (a1, a2) == (1, 2), "activation_id 는 파일이 1부터 발급한다: %r" % ((a1, a2),)
        for i in range(10):
            w.data(0xF0, i, i * 1000, b"\xAB" * 8, activation_id=a1)
        w.flush()
        clean_size = os.path.getsize(path)
        w.close()

        res = X.read_file(path)
        assert len(res.records) == 13, len(res.records)
        assert res.stopped_reason is None

        # 크래시 흉내: 파일 끝에 반쪽 레코드가 남은 상태
        with open(path, "ab") as f:
            f.write(X.encode_data(1, 0xF0, 99, 99, b"\xCD" * 8)[:20])
        res2 = X.read_file(path)
        assert len(res2.records) == 13, "부분 레코드가 섞여 들어왔다: %d" % len(res2.records)
        assert res2.valid_bytes == clean_size
        assert res2.stopped_reason is not None
    finally:
        try:
            for f in os.listdir(tmp):
                os.remove(os.path.join(tmp, f))
            os.rmdir(tmp)
        except OSError:
            pass


def test_record_size_budget():
    """PLAN Phase D 수용 기준: Combined 10ch DATA 레코드 <= 72 B."""
    rec = X.encode_data(1, 0x10, 1, 1, b"\x00" * 40)   # 10ch × float32
    assert len(rec) == 72, "10ch DATA 레코드 %d B (16 + 16 + 40)" % len(rec)
    assert len(rec) <= 72


def test_bad_inputs_rejected():
    try:
        X.encode_record(99, b"")
    except ValueError:
        pass
    else:
        raise AssertionError("unknown rec_type 를 받아들였다")

    try:
        X.encode_record(X.REC_GAP, b"\x00" * 15)
    except ValueError:
        pass
    else:
        raise AssertionError("header_rest_len 이 틀린 것을 받아들였다")

    # 파일 헤더가 아닌 것
    try:
        X.scan(b"NOTXMLOG" + b"\x00" * 40)
    except X.LogError as e:
        assert not e.recoverable
    else:
        raise AssertionError("magic 이 틀린 파일을 열었다")


# =============================================================================
# Runner
# =============================================================================

def main():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")

    tests = [
        ("golden FileHeader bytes", test_golden_file_header),
        ("golden SESSION bytes", test_golden_session),
        ("SESSION-first enforced", test_session_first_is_enforced),
        ("SCHEMA_ACTIVATION dedup", test_schema_activation_dedup),
        ("golden GAP bytes", test_golden_gap),
        ("golden DATA bytes", test_golden_data),
        ("golden SCHEMA_ACTIVATION bytes", test_golden_activation),
        ("magic is bytes, not int", test_magic_is_bytes_not_int),
        ("CRC range = skip own 4 bytes", test_crc_range_is_skip_not_zerofill),
        ("fixed strings stay NUL-terminated", test_fixed_string_nul_termination),
        ("roundtrip all record types", test_roundtrip_all_types),
        ("byte-wise truncation sweep", test_truncation_sweep),
        ("zero tail is rejected", test_zero_tail_is_rejected),
        ("writer + crash recovery", test_writer_and_crash_recovery),
        ("10ch DATA record <= 72 B", test_record_size_budget),
        ("bad inputs rejected", test_bad_inputs_rejected),
    ]

    failed = 0
    for name, fn in tests:
        try:
            fn()
            print("  PASS  " + name)
        except AssertionError as e:
            failed += 1
            print("  FAIL  " + name)
            print("        " + str(e))
        except Exception as e:  # noqa: BLE001
            failed += 1
            print("  ERROR " + name)
            print("        %s: %s" % (type(e).__name__, e))

    print("")
    if failed:
        print("%d/%d FAILED" % (failed, len(tests)))
        return 1
    print("%d/%d passed — .xmlog v1 바이트 ABI 가 PLAN 4.6 표와 일치" % (len(tests), len(tests)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
