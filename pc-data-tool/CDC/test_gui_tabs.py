"""실시간 수신 화면(GUI)·저장(CSV) 시험 — 오프스크린 Qt 로 실제 `MainWindow` 를 띄운다.

    python test_gui_tabs.py

보드도 시리얼 포트도 필요 없다. PyQt5/pyqtgraph/pyserial 이 **없으면** SKIP 으로 끝난다(종료코드
77, automake 의 "건너뜀" 관례) — `run_tests.py` 가 이걸 PASS 와 구분해서 보여 준다. 반대로
그 패키지는 깔려 있는데 import 가 실패하면(우리 코드의 결함. Qt 네이티브 라이브러리를 못
올리는 PC 는 제외) SKIP 이 아니라 **실패**다.

왜 이 파일이 있나
------------------
실시간 화면을 모듈별 탭으로 바꾼 커밋의 메시지는 "오프스크린 Qt 로 실제 MainWindow 에 합성
스트림을 넣어 탭 2개·CSV 파일명·0x20 표까지 확인했다" 고 적었다. 그런데 그 확인을 되풀이할
코드가 저장소 어디에도 없었다 — 1000줄 남짓 바뀐 화면 코드가 자동 시험을 하나도 안 거친
셈이다. 이 파일이 그 자리를 채운다. 이어서 찾아낸 결함마다 "고치기 전에는
여기서 실패한다" 는 시험을 하나씩 붙였다.

  [1]  실제 창에 합성 스트림 — 탭 2개 · CSV 파일명 · 0x20 표 · 늦게 오는 스키마(열 정렬)
  [2]  CLI(`run_cli`) — module 별 CSV 이름 · 열 정렬 · README 예시와 실제 파일 일치
  [3]  poll 한 틱의 처리량 상한 — 밀린 프레임은 다음 틱이 잇고, 하나도 안 잃는다
  [4]  module 이 한꺼번에 나타나도 한 틱에 탭은 하나만 — 나머지 프레임은 순서대로 다음 틱
  [5]  밀려 있는 동안은 그래프를 매 틱 그리지 않는다 (그리는 시간이 큐 비우는 속도를 깎지 않게)
  [6]  연결을 끊거나 창을 닫을 때 큐·미뤄 둔 프레임까지 CSV 에 들어간다
  [7]  연결된 채로 다시 Connect 해도 이전 세션에 밀려 있던 프레임이 CSV 에 들어간다
  [8]  재연결로 탭을 버리기 전에 CSV 가 디스크에 나간다
  [9]  재연결 뒤 이전 탭은 실제로 지워진다
  [9b] 다시 연결해도 채널 이름은 남는다 — 포트가 바뀌었거나 끊겼다 되돌아온 재연결에서만 잊는다
  [9c] 탭을 지운 직후에 보류 중인 마우스 시그널이 남아 있어도 오류가 나지 않는다
  [10] 0x20 표 — 디코드는 5Hz 로만, 맵이 안 맞으면 float32 로 뭉개지 않고 "안 맞음" 표시
  [11] 스키마가 늦게 와서 채널 수는 같은데 해석이 바뀌는 경우 — CSV 에 값이 섞이지 않는다
  [11b] 스키마가 없는 module 의 채널 수만 도중에 바뀌는 경우 — CSV 열이 밀리지 않는다
  [12] 상태 패널은 `router.user_modules` 를 직접 순회하지 않는다 (워커 스레드와 겹치면 죽는다)
  [13] CSV 리뷰어 — hex 행에서 데이터가 잘리지 않고, hex 행의 seq · Tx drop 도 통계에 들어간다
  [14] 실제 스레드 · QThread · 시그널 경로 (가짜 시리얼 포트만 끼운다)
  [15] .xmlog — GUI 워커와 CLI 가 같은 파일을 만든다 · 재연결은 새 파일이고 스키마는 처음부터 ·
       같은 초에 두 번 시작해도(GUI 재연결이든 CLI 든) 앞 파일을 덮어쓰지 않는다

시리얼 포트만 가짜다. 그 뒤(읽기 루프 -> COBS 분리 -> `_parse_frame` -> 큐 -> `_on_poll`
-> 탭)는 앱이 실제로 가는 코드다.

⚠ QApplication 은 반드시 변수로 붙든다
--------------------------------------
    QtWidgets.QApplication.instance() or QtWidgets.QApplication([])     # 이렇게 쓰면 안 된다

이 한 줄은 QApplication 을 만들고 **바로 버린다** — 파이썬 래퍼의 참조가 0 이 되는 순간
C++ 객체가 파괴되고, 다음 위젯을 만드는 자리에서 Qt 가 "Must construct a QApplication
before a QWidget" 로 프로세스를 죽인다. Windows 에서는 종료코드 0xC0000409(fast-fail)라
파이썬 트레이스백도, faulthandler 출력도 없이 그냥 사라진다. 이 시험을 처음 쓸 때(2026-09-29)
그대로 밟았다. 제품 코드(`main()`)는 `app = QApplication(...)` 로 붙들고 있어 무관하다 —
이전 판(모듈별 탭 도입 커밋)의 MainWindow 도 실제 화면(기본 플랫폼)과 오프스크린에서 모두
돌려 죽지 않는 것을 확인했다.

덧붙여 — `QApplication(sys.argv)` 는 Qt 가 argv 에서 `-platform`, `-style` 같은 옵션을
스스로 읽는다(`--platform` 도 같은 옵션으로 본다). 스크립트가 자기 옵션을 그런 이름으로
지으면 Qt 가 엉뚱한 플러그인을 찾다가 같은 방식으로 죽는다. 이 시험은 `[]` 를 넘긴다.
"""
import contextlib
import csv
import io
import os
import re
import shutil
import struct
import sys
import tempfile
import time
import types

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from frame_router import PhAIFrame, cobs_decode   # noqa: E402  (Qt 없이 import 된다)

# Qt 를 import 하기 전에 정한다. 이미 정해져 있으면(개발자가 실제 창으로 보고 싶다면) 그대로 둔다.
os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

SKIP_RC = 77   # automake 의 "이 환경에서는 못 돌렸다" 종료코드. 0(성공)/1(실패)과 구분된다.
_OPTIONAL_GUI_DEPS = ("PyQt5", "pyqtgraph", "serial")

_passed = 0


def ok(msg):
    global _passed
    _passed += 1
    print("  [OK] " + msg)


# ============================================================================
# SKIP 과 실패를 가르는 기준
# ============================================================================

def _is_missing_gui_dependency(e):
    """이 ImportError 가 '선택 의존성이 이 PC 에 없다' 는 뜻인가.

    True 면 SKIP(이 환경에서는 창을 못 띄운다), False 면 **실패**다 — 패키지는 깔려 있는데
    우리 코드가 잘못된 이름을 import 하는 것 같은 진짜 결함이다. 예전엔 메시지에 "PyQt5" 가
    들어 있기만 하면 SKIP 이라, GUI 가 아예 못 뜨는 결함도 "의존성이 없어서 건너뜀" 으로 읽히고
    종료코드 0 으로 지나갔다.
    """
    top = (getattr(e, "name", None) or "").split(".")[0]
    if top not in _OPTIONAL_GUI_DEPS:
        return False
    if isinstance(e, ModuleNotFoundError):
        return True                       # 패키지 자체가 없다
    # 패키지는 있는데 네이티브 라이브러리를 못 올리는 PC (Windows DLL · Linux .so 없음)
    msg = str(e)
    return "DLL load failed" in msg or "cannot open shared object file" in msg


def _selfcheck_import_classifier():
    """위 기준 자체의 시험. Qt 가 없어도 돈다 — SKIP 판정 로직이 SKIP 뒤에 숨지 않게."""
    def missing(name):
        return ModuleNotFoundError("No module named %r" % name, name=name)

    for name in ("PyQt5", "PyQt5.QtCore", "pyqtgraph", "serial"):
        assert _is_missing_gui_dependency(missing(name)), name
    # 필수 의존성(numpy)이나 우리 모듈이 없는 건 환경 문제가 아니라 결함이다
    assert not _is_missing_gui_dependency(missing("numpy"))
    assert not _is_missing_gui_dependency(missing("cdc_phai_receiver"))
    # 패키지는 있는데 없는 이름을 가져오려는 결함 — 메시지에 PyQt5 가 들어 있어도 SKIP 이 아니다
    assert not _is_missing_gui_dependency(ImportError(
        "cannot import name 'NoSuchQtName' from 'PyQt5.QtCore'", name="PyQt5.QtCore"))
    assert not _is_missing_gui_dependency(ImportError(
        "cannot import name 'x' from 'cdc_phai_receiver'", name="cdc_phai_receiver"))
    # 패키지는 있는데 네이티브 라이브러리를 못 올리는 환경은 SKIP
    assert _is_missing_gui_dependency(ImportError(
        "DLL load failed while importing QtCore: 지정된 모듈을 찾을 수 없습니다.",
        name="PyQt5.QtCore"))
    assert _is_missing_gui_dependency(ImportError(
        "libGL.so.1: cannot open shared object file: No such file or directory",
        name="PyQt5.QtGui"))


# ============================================================================
# 도구
# ============================================================================

_windows = []      # 만든 창들 — 시험이 중간에 실패해도 main() 이 열린 CSV 를 닫고 임시 폴더를 지운다


def _make_window(RX, out_dir):
    """MainWindow 하나 + CSV 폴더 지정. 시리얼 연결은 하지 않는다."""
    win = RX.MainWindow()
    _windows.append(win)
    win._edit_folder.setText(out_dir)
    win._open_log_session()          # _csv_folder / _csv_stem 만 정한다
    return win


def _dispose(win):
    """창을 닫는다(=열린 CSV 를 닫는다). 시험 사이에 Qt 객체가 쌓이지 않게 지운다."""
    win.close()
    win.deleteLater()


@contextlib.contextmanager
def _fresh_schema_registry(RX):
    """`SCHEMA_REG` 는 모듈 전역이라 시험끼리 스키마가 새어 나간다 — 시험마다 새 것으로."""
    saved = RX.SCHEMA_REG
    RX.SCHEMA_REG = RX._schema.SchemaRegistry()
    try:
        yield RX.SCHEMA_REG
    finally:
        RX.SCHEMA_REG = saved


def _push(worker, DS, seq, module_id, payload):
    """프레임 하나를 워커의 실제 경로(`_parse_frame`)로 큐에 넣는다."""
    wire = DS.wire_frame(seq & 0xFFFF, module_id, payload)   # 끝의 0x00 딜리미터 포함
    worker._parse_frame(cobs_decode(wire[:-1]), seq * 0.001)


def _settle(win, worker):
    """큐와 미뤄 둔 프레임이 빌 때까지 poll — 틱마다 상한이 있어서 여러 번 돈다."""
    for _ in range(1000):
        if not (worker.packet_queue or win._carry):
            return
        win._on_poll()
    raise AssertionError("poll 을 1000 번 돌려도 큐가 안 빈다")


def _read_csv(path):
    with open(path, encoding="utf-8", newline="") as f:
        rows = list(csv.reader(f))
    if not rows:                     # 아무것도 못 적은 파일 — 호출부 assert 가 행 수로 잡는다
        return [], []
    return rows[0], rows[1:]


def pump_until(cond, what, timeout_s=20.0):
    """Qt 이벤트를 돌리며 조건을 기다린다 (다른 스레드가 보낸 시그널이 GUI 스레드에서 처리되도록)."""
    from PyQt5 import QtWidgets
    app = QtWidgets.QApplication.instance()
    deadline = time.perf_counter() + timeout_s
    while not cond():
        app.processEvents()
        time.sleep(0.002)
        if time.perf_counter() > deadline:
            raise AssertionError("시간 안에 안 끝났다: %s" % what)


class _FakeSerial:
    """실제 포트 대신 준비한 바이트를 청크로 내주는 가짜 `serial.Serial`.

    `PhAISerialWorker.run()` 이 이걸 진짜 포트처럼 읽는다. `on_read` 는 read() 가 불릴 때마다
    먼저 부르는 콜백(=GUI 가 그 사이사이 poll 하는 상황을 만든다), `on_exhausted` 는 준비한
    바이트를 다 내준 뒤 부르는 콜백이다.
    """
    is_open = True

    def __init__(self, chunks, on_read=None, on_exhausted=None, idle_sleep_s=0.0):
        self._chunks = list(chunks)
        self._i = 0
        self._on_read = on_read
        self._on_exhausted = on_exhausted
        self._idle_sleep_s = idle_sleep_s
        self.reads = 0                      # read() 가 불린 횟수 — 워커가 돌기 시작했는지 본다

    def read(self, n):
        self.reads += 1
        if self._on_read is not None:
            self._on_read()
        if self._i < len(self._chunks):
            chunk = self._chunks[self._i]
            self._i += 1
            return chunk
        if self._on_exhausted is not None:
            self._on_exhausted()
        if self._idle_sleep_s:
            time.sleep(self._idle_sleep_s)
        return b""

    def close(self):
        self.is_open = False


class _CliSerial(_FakeSerial):
    """`run_cli` 용: 준비한 바이트를 다 주면 Ctrl+C 를 흉내 낸다 (CLI 는 Ctrl+C 로 끝난다)."""

    def read(self, n):
        if self._i >= len(self._chunks):
            raise KeyboardInterrupt
        return super().read(n)


@contextlib.contextmanager
def _patched_serial(RX, fake):
    real = RX.serial.Serial
    RX.serial.Serial = lambda *a, **k: fake
    try:
        yield
    finally:
        RX.serial.Serial = real


def _chunks(stream, size=41):
    # 프레임 경계와 일부러 어긋나는 크기 — 시리얼 read() 는 프레임을 지켜 주지 않는다.
    return [stream[i:i + size] for i in range(0, len(stream), size)]


@contextlib.contextmanager
def _capture_message_boxes():
    """모달 QMessageBox 는 이벤트 루프를 붙잡아 시험이 멈춘다 — 띄우는 대신 내용을 모은다."""
    from PyQt5 import QtWidgets
    shown = []
    real = {n: getattr(QtWidgets.QMessageBox, n) for n in ("critical", "warning", "information")}
    for n in real:
        setattr(QtWidgets.QMessageBox, n,
                staticmethod(lambda *a, _n=n, **k: shown.append((_n, a[1:3]))))
    try:
        yield shown
    finally:
        for n, fn in real.items():
            setattr(QtWidgets.QMessageBox, n, fn)


# ============================================================================
# [1] 실제 창에 합성 스트림
# ============================================================================

def test_two_tabs_end_to_end(RX, DS, tmp):
    print("\n[1] 실제 MainWindow — 합성 스트림을 워커(run) 경로로 넣어 탭·CSV·0x20 표 확인")
    from xmlog_export import _module_label
    import xm_total_data_map as M

    stream, exp = DS.build_demo_session(cycles=14, drop_at=10, drop_len=3)

    with _fresh_schema_registry(RX):
        win = _make_window(RX, tmp)
        win._chk_total_tab.setChecked(True)      # "Show 0x20 tab" — 데이터가 오기 전에 켠다
        worker = RX.PhAISerialWorker("TEST_PORT")
        win._worker = worker

        # 워커의 read 루프가 청크를 읽을 때마다 GUI 가 한 번 poll 한다 — 스키마(0xEE)가
        # 데이터보다 늦게 도착하는 실제 순서에서, 앞 프레임이 스키마 없이 먼저 화면에 올라간다.
        fake = _FakeSerial(_chunks(stream), on_read=win._on_poll, on_exhausted=worker.stop)
        with _patched_serial(RX, fake):
            worker.run()
        _settle(win, worker)

        assert worker.good == exp.frames_ok, (worker.good, exp.frames_ok)
        assert worker.crc_err == exp.frames_crc_bad, (worker.crc_err, exp.frames_crc_bad)
        assert worker.sync_err == 0, worker.sync_err
        ok("워커 run() 이 합성 스트림을 그대로 읽었다: 정상 %d · CRC 오류 %d · 동기 오류 0"
           % (worker.good, worker.crc_err))

        # ---- 탭 2개 ----
        assert list(win._tabs) == [DS.MODULE_TYPED, DS.MODULE_META_ONLY], list(win._tabs)
        ok("사용자 module 탭 2개, 처음 본 순서대로: 0x%02X, 0x%02X"
           % (DS.MODULE_TYPED, DS.MODULE_META_ONLY))

        tab0, tab1 = win._tabs[DS.MODULE_TYPED], win._tabs[DS.MODULE_META_ONLY]
        title0 = win._tab_widget.tabText(win._tab_widget.indexOf(tab0))
        title1 = win._tab_widget.tabText(win._tab_widget.indexOf(tab1))
        assert title0 == "0xF0 · %s" % DS.DEMO_STRUCT_NAME, title0
        assert title1 == "0xF1", title1
        ok("탭 제목: %r (0xEE struct 이름) · %r (채널이 여럿인 0xEF 는 이름 대신 ID)" % (title0, title1))

        # ---- 링 버퍼는 탭당 하나 (예전엔 안 쓰는 두 번째 버퍼가 module 수만큼 곱해졌다) ----
        import numpy as np
        one_buf = tab0._window_size * (1 + RX.MAX_CHANNELS) * 4          # float32
        distinct = {id(v): v for v in vars(tab0).values() if isinstance(v, np.ndarray)}   # 별칭은 한 번만
        held = sum(v.nbytes for v in distinct.values())
        assert held == one_buf, "탭이 붙든 numpy 버퍼 %d B (링 버퍼 하나면 %d B)" % (held, one_buf)
        ok("탭 하나가 붙드는 numpy 버퍼 = %d KB (링 버퍼 하나 분량)" % (held // 1024))

        # ---- 안 보이는 탭도 저장은 계속된다 ----
        assert win._tab_widget.currentWidget() is not tab1
        assert tab1.frame_count == exp.meta_only_frames, (tab1.frame_count, exp.meta_only_frames)
        assert tab0.frame_count == exp.typed_frames, (tab0.frame_count, exp.typed_frames)
        ok("현재 탭이 아닌 0xF1 도 %d 프레임 전부 받았다 (보이지 않아도 저장은 계속)"
           % tab1.frame_count)

        # ---- CSV 파일명 (xmlog_export._module_label 규약) ----
        for mid, tab in ((DS.MODULE_TYPED, tab0), (DS.MODULE_META_ONLY, tab1)):
            want = os.path.join(tmp, "%s_%s.csv" % (win._csv_stem, _module_label(mid)))
            assert tab.csv_path == want, (tab.csv_path, want)
            assert os.path.basename(want).endswith("_user_0x%02X.csv" % mid), want
        ok("CSV 파일명: %s_user_0xF0.csv / %s_user_0xF1.csv (module 하나당 파일 하나)"
           % (win._csv_stem, win._csv_stem))

        win._close_all_csv()

        # ---- 늦게 오는 스키마: 열이 밀리지 않는다 ----
        header0, rows0 = _read_csv(tab0.csv_path)
        assert header0[5:] == ["ch%d" % i for i in range(6)], header0
        assert len(rows0) == exp.typed_frames, (len(rows0), exp.typed_frames)
        bad = [r for r in rows0 if len(r) != len(header0)]
        assert not bad, "열 수가 헤더(%d)와 다른 행 %d개" % (len(header0), len(bad))
        hex_idx = [k for k, r in enumerate(rows0) if all(c == "" for c in r[5:-1])]
        assert hex_idx, ("전제가 안 생겼다: 0xEE 가 도착한 뒤에도 채널 수가 안 바뀌었다 — "
                         "demo_stream.py 의 스키마 위치나 poll 시점이 달라졌는지 확인")
        assert hex_idx[0] >= 1 and hex_idx == list(range(hex_idx[0], exp.typed_frames)), hex_idx
        assert tab0._mismatched == len(hex_idx), (tab0._mismatched, len(hex_idx))
        ok("0xF0 CSV: 헤더 %d열 · 행 %d개 전부 같은 열 수 (스키마가 늦게 와서 채널 수가 바뀐 %d행은 hex)"
           % (len(header0), len(rows0), len(hex_idx)))

        for k in hex_idx:
            want_payload = DS.typed_payload(**exp.typed_rows[k])
            assert bytes.fromhex(rows0[k][-1]) == want_payload, k
        ok("hex 행은 보낸 payload 원본 그대로다 (%d B) — 값을 잃지 않는다" % DS.DEMO_STRUCT_SIZE)

        header1, rows1 = _read_csv(tab1.csv_path)
        assert len(header1) == 5 + 3 and len(rows1) == exp.meta_only_frames
        assert all(len(r) == len(header1) for r in rows1)
        assert tab1._mismatched == 0
        ok("0xF1 CSV: 헤더 %d열 · 행 %d개 (채널 수가 안 바뀌어 hex 행 없음)" % (len(header1), len(rows1)))

        # ---- 상태 패널 ----
        panel = win._module_status.toPlainText()
        assert title0 in panel and "0xF1" in panel and "hex행 %d" % len(hex_idx) in panel, panel
        ok("상태 패널에 module 별 행과 'CSV hex행 %d' 표시가 나온다" % len(hex_idx))

        # ---- 0x20 표 ----
        total = win._total_tab
        assert total is not None
        assert total._table.rowCount() == len(M.SCALAR_NAMES), total._table.rowCount()
        assert total._table.item(0, 0).text() == M.SCALAR_NAMES[0]
        assert total._table.item(0, 1) is not None and total._table.item(0, 1).text() != ""
        assert total._warn.isHidden()
        ok("'Show 0x20 tab' 표: %d 채널 이름 + 값이 채워졌다" % total._table.rowCount())

        # ---- 끊기 ----
        win._on_worker_finished()
        assert win._worker is None
        msg = win._status_bar.currentMessage()
        assert msg.startswith("Disconnected"), msg
        ok("Disconnect 경로: 상태 표시줄 %r" % msg[:60])
        _dispose(win)


# ============================================================================
# [2] CLI
# ============================================================================

def test_cli_files_and_alignment(RX, DS, tmp):
    print("\n[2] CLI(run_cli) — module 별 CSV · 열 정렬 · README 예시와 실제 파일")
    import glob

    stream, exp = DS.build_demo_session(cycles=14, drop_at=10, drop_len=3)
    out = os.path.join(tmp, "cli_out")

    buf = io.StringIO()
    with _fresh_schema_registry(RX):
        with _patched_serial(RX, _CliSerial(_chunks(stream))):
            with contextlib.redirect_stdout(buf):
                # README 의 CLI 예시(`--cli --port COM6 --total-data`)와 같은 옵션
                RX.run_cli("COM_TEST", 921600, out, total_data=True, log=False)
    text = buf.getvalue()

    names = sorted(os.listdir(out))
    stamp = re.search(r"\d{8}_\d{6}", names[0]).group(0)      # 실행 시각 도장 (YYYYMMDD_HHMMSS)
    got = {n.replace(stamp, "<시각>") for n in names}
    want = {
        "cdc_phai_<시각>_user_0xF0.csv",
        "cdc_phai_<시각>_user_0xF1.csv",
        "cdc_total_<시각>.csv",
        "cdc_total_<시각>.csv.meta.txt",
    }
    assert got == want, (sorted(got), sorted(want))
    ok("README 예시대로 %d 개 파일: %s" % (len(got), ", ".join(sorted(got))))

    p0 = glob.glob(os.path.join(out, "cdc_phai_*_user_0xF0.csv"))[0]
    p1 = glob.glob(os.path.join(out, "cdc_phai_*_user_0xF1.csv"))[0]
    header0, rows0 = _read_csv(p0)
    header1, rows1 = _read_csv(p1)
    assert len(rows0) == exp.typed_frames and len(rows1) == exp.meta_only_frames
    assert all(len(r) == len(header0) for r in rows0), "0xF0 열이 밀린 행이 있다"
    assert all(len(r) == len(header1) for r in rows1), "0xF1 열이 밀린 행이 있다"
    n_hex = sum(1 for r in rows0 if all(c == "" for c in r[5:-1]))
    assert n_hex > 0, "전제가 안 생겼다: CLI 경로에서도 스키마가 늦게 도착해야 한다"
    ok("CLI 0xF0: 행 %d개 모두 헤더(%d열)와 같은 열 수 — 채널 수가 바뀐 %d행은 hex"
       % (len(rows0), len(header0), n_hex))
    assert "해석이 달라" in text and str(n_hex) in text, text
    ok("종료 요약에 hex 로 적은 행 수(%d)가 경고로 나온다" % n_hex)

    header20, rows20 = _read_csv(glob.glob(os.path.join(out, "cdc_total_*.csv"))[0])
    assert len(rows20) == exp.total_frames, (len(rows20), exp.total_frames)
    ok("0x20 CSV: %d 프레임 -> %d 행 (맵이 맞으면 전부 푼다)" % (exp.total_frames, len(rows20)))


# ============================================================================
# [3] poll 한 틱의 처리량 상한
# ============================================================================

def test_poll_is_bounded(RX, DS, tmp):
    print("\n[3] poll 한 틱은 MAX_PACKETS_PER_POLL 개까지만 — 나머지는 다음 틱, 유실 0")
    cap = RX.MAX_PACKETS_PER_POLL
    n = cap * 2 + 137
    assert n < RX.PhAISerialWorker("X").packet_queue.maxlen, "큐 용량을 넘기면 시험의 전제가 깨진다"

    with _fresh_schema_registry(RX):
        win = _make_window(RX, tmp)
        worker = RX.PhAISerialWorker("TEST_PORT")
        win._worker = worker
        for i in range(n):
            _push(worker, DS, i, 0xF2, struct.pack("<4f", float(i), 0.0, 0.0, 0.0))
        assert len(worker.packet_queue) == n

        win._on_poll()
        left = len(worker.packet_queue)
        assert left == n - cap, ("첫 poll 뒤 큐에 %d 개 (기대 %d) — 한 틱이 상한 없이 큐를 통째로 비웠다"
                                 % (left, n - cap))
        ok("첫 poll 은 %d 개만 처리하고 %d 개를 큐에 남겼다 (예전엔 %d 개를 한 번에 다 비웠다)"
           % (cap, left, n))

        polls = 1
        while worker.packet_queue:
            win._on_poll()
            polls += 1
            assert polls <= 10, "큐가 안 줄어든다"
        assert polls == 3, polls
        ok("%d 틱에 나눠 다 소화했다" % polls)

        tab = win._tabs[0xF2]
        assert tab.frame_count == n, (tab.frame_count, n)
        win._close_all_csv()
        header, rows = _read_csv(tab.csv_path)
        assert len(rows) == n, (len(rows), n)
        assert [int(r[2]) for r in rows] == list(range(n)), "seq 순서·개수가 다르다"
        assert [float(r[5]) for r in rows] == [float(i) for i in range(n)]
        ok("CSV %d 행 — 빠진 프레임 · 중복 · 순서 바뀜 없음 (seq 0..%d, 값 일치)" % (len(rows), n - 1))
        _dispose(win)


# ============================================================================
# [4] 탭 만들기는 틱마다 한 개씩
# ============================================================================

def test_new_tabs_spread_across_ticks(RX, DS, tmp):
    limit = RX.MAX_NEW_TABS_PER_POLL
    print("\n[4] module 15개가 한꺼번에 나타나도 한 틱에 탭은 %d 개만 — 나머지 프레임은 순서대로 다음 틱" % limit)
    n_mod, cycles = 15, 6
    mids = [0xF0 + k for k in range(n_mod)]
    total = n_mod * cycles
    assert total < RX.MAX_PACKETS_PER_POLL, "프레임 수 상한이 아니라 탭 한도가 나누는 것을 보이려는 시험이다"

    with _fresh_schema_registry(RX):
        win = _make_window(RX, tmp)
        worker = RX.PhAISerialWorker("TEST_PORT")
        win._worker = worker
        for c in range(cycles):
            for k, m in enumerate(mids):
                _push(worker, DS, c * n_mod + k, m, struct.pack("<2f", float(c), float(m)))

        created = []
        while worker.packet_queue or win._carry:
            before = len(win._tabs)
            win._on_poll()
            created.append(len(win._tabs) - before)
            assert len(created) <= 4 * n_mod, "poll 이 끝나지 않는다: %r" % created
        assert created[0] == limit, ("첫 틱에 탭이 %d 개 생겼다 (한도 %d) — 예전엔 %d 개를 한 번에 만들어 "
                                     "그 한 틱이 창을 1초 가까이 멈춰 세웠다" % (created[0], limit, n_mod))
        assert max(created) <= limit, created
        assert len(win._tabs) == n_mod and len(created) == -(-n_mod // limit), created
        ok("첫 틱에 탭 %d 개, 이후에도 틱마다 최대 %d 개 — %d 틱에 탭 %d 개 완성"
           % (created[0], max(created), len(created), n_mod))

        assert win._total_recv == total, (win._total_recv, total)
        win._close_all_csv()
        for k, m in enumerate(mids):
            header, rows = _read_csv(win._tabs[m].csv_path)
            assert [int(r[2]) for r in rows] == [c * n_mod + k for c in range(cycles)], (hex(m), rows)
            assert [float(r[5]) for r in rows] == [float(c) for c in range(cycles)], hex(m)
        ok("module %d 개 CSV 모두 프레임 %d 개씩 — 미뤄 둔 프레임도 안 빠지고 순서 그대로" % (n_mod, cycles))
        _dispose(win)


# ============================================================================
# [5] 밀려 있는 동안의 그리기
# ============================================================================

def test_render_waits_for_backlog(RX, DS, tmp):
    print("\n[5] 밀려 있는 동안은 그래프를 매 틱 그리지 않는다 — 다 따라잡으면 바로 그린다")
    tick_s = 0.03                                # MainWindow 의 QTimer 주기와 같다
    per_tick = 100
    clock = [1000.0]
    renders = []

    real = (RX.MAX_PACKETS_PER_POLL, RX.ModuleTab.render, RX.time)
    RX.MAX_PACKETS_PER_POLL = per_tick           # 틱을 잘게 나눠, 적은 프레임으로 긴 밀림을 만든다
    RX.ModuleTab.render = lambda self: renders.append(clock[0])
    RX.time = types.SimpleNamespace(perf_counter=lambda: clock[0])
    try:
        with _fresh_schema_registry(RX):
            win = _make_window(RX, tmp)
            worker = RX.PhAISerialWorker("TEST_PORT")
            win._worker = worker
            seq = [0]

            def backlog(n_ticks):
                """n_ticks 틱 분량이 밀린 상태를 만들고 다 따라잡을 때까지 30ms 간격으로 poll 한다."""
                for _ in range(per_tick * n_ticks):
                    _push(worker, DS, seq[0], 0xF2, struct.pack("<2f", float(seq[0]), 0.0))
                    seq[0] += 1
                del renders[:]
                ticks = 0
                while worker.packet_queue or win._carry:
                    win._on_poll()
                    ticks += 1
                    last_tick_t = clock[0]
                    clock[0] += tick_s
                return ticks, last_tick_t

            # (a) 짧은 밀림: 따라잡을 때까지 한 번도 안 그리고, 따라잡은 틱에서 그린다
            ticks, last_t = backlog(6)
            assert ticks == 6, ticks
            assert renders == [last_t], ("6틱 밀림 동안 그린 시각 %r — 매 틱 그리거나 마지막에 안 그렸다"
                                         % (renders,))
            ok("6 틱(%.2f초) 밀린 동안 0번 그리고, 따라잡은 틱에서 한 번 그렸다"
               % ((ticks - 1) * tick_s))

            # (b) 긴 밀림: 그래도 화면이 굳지 않게 BACKLOG_RENDER_PERIOD_S 마다는 그린다
            ticks, last_t = backlog(40)
            elapsed = (ticks - 1) * tick_s
            allowed = int(elapsed / RX.BACKLOG_RENDER_PERIOD_S) + 1        # 마지막 한 번 포함
            assert renders[-1] == last_t, "따라잡은 마지막 틱에서 안 그렸다"
            assert 2 <= len(renders) <= allowed, (
                "%d 틱(%.2f초) 밀림에서 %d 번 그렸다 (허용 %d) — 틱마다 그리면 그리는 시간이 큐를 "
                "비우는 속도를 깎는다" % (ticks, elapsed, len(renders), allowed))
            ok("%d 틱(%.2f초) 밀림에서 %d 번만 그렸다 (매 틱이면 %d 번) — 오래 밀려도 %.1f초마다는 갱신"
               % (ticks, elapsed, len(renders), ticks, RX.BACKLOG_RENDER_PERIOD_S))

            # (c) 안 밀릴 때는 예전처럼 틱마다 그린다
            del renders[:]
            for k in range(5):
                _push(worker, DS, seq[0], 0xF2, struct.pack("<2f", 1.0, 2.0))
                seq[0] += 1
                win._on_poll()
                clock[0] += tick_s
            assert len(renders) == 5, renders
            ok("안 밀릴 때는 틱마다 그린다 (5 틱 5 번)")

            # (d) 실제 스트리밍: 틱이 처리하는 동안 워커 스레드가 새 프레임을 계속 넣는다. 그건 밀린
            #     게 아니다 — 큐가 틱 끝에 안 비어 있다는 이유로 '밀림' 으로 보면, 평소에도 그래프가
            #     0.5초에 한 번만 갱신된다 (스레드 없는 시험은 이걸 못 본다).
            real_apply = win._apply_batch

            def apply_while_frames_arrive(batch, max_new_tabs):
                result = real_apply(batch, max_new_tabs)
                for _ in range(3):
                    _push(worker, DS, seq[0], 0xF2, struct.pack("<2f", 3.0, 4.0))
                    seq[0] += 1
                return result

            win._apply_batch = apply_while_frames_arrive
            try:
                del renders[:]
                for _ in range(3):                       # 첫 틱이 꺼낼 프레임
                    _push(worker, DS, seq[0], 0xF2, struct.pack("<2f", 3.0, 4.0))
                    seq[0] += 1
                for k in range(8):
                    win._on_poll()
                    clock[0] += tick_s
                    assert worker.packet_queue, "전제가 안 생겼다: 틱이 끝났는데 큐가 비었다"
                assert len(renders) == 8, ("스트리밍 8 틱에 %d 번 그렸다 — 처리 중에 들어온 프레임을 "
                                           "'밀림' 으로 세고 있다" % len(renders))
            finally:
                win._apply_batch = real_apply
            ok("처리하는 동안 워커가 새 프레임을 넣어도 밀린 게 아니다 — 8 틱 8 번 그렸다")
            _settle(win, worker)

            # (e) Freeze: 멈춰 있는 동안은 그리지 않고(받은 프레임은 버퍼·CSV 에 계속 들어간다),
            #     풀면 다음 틱에 밀린 것까지 그린다
            frames_before = win._tabs[0xF2].frame_count
            del renders[:]
            win._btn_freeze.setChecked(True)
            for _ in range(3):
                _push(worker, DS, seq[0], 0xF2, struct.pack("<2f", 5.0, 6.0))
                seq[0] += 1
                win._on_poll()
                clock[0] += tick_s
            assert not renders, "Freeze 인데 그렸다: %r" % (renders,)
            assert win._tabs[0xF2].frame_count == frames_before + 3, "Freeze 중에 받은 프레임이 탭에 안 들어갔다"
            win._btn_freeze.setChecked(False)
            _push(worker, DS, seq[0], 0xF2, struct.pack("<2f", 5.0, 6.0))
            seq[0] += 1
            win._on_poll()
            assert len(renders) == 1, renders
            ok("Freeze 중에는 그리지 않고 프레임은 계속 받으며, 풀면 바로 그린다")

            assert not win._unrendered
            win._worker = None
            _dispose(win)
    finally:
        RX.MAX_PACKETS_PER_POLL, RX.ModuleTab.render, RX.time = real


# ============================================================================
# [6] 끊거나 닫을 때 큐에 남은 프레임
# ============================================================================

def test_backlog_is_written_on_close(RX, DS, tmp):
    print("\n[6] 연결을 끊거나 창을 닫을 때, 큐에 남은 프레임과 미뤄 둔 프레임까지 CSV 에 들어간다")
    cap = RX.MAX_PACKETS_PER_POLL
    n = cap + 300

    # --- (a) 한 module: 큐에 밀린 프레임
    for how in ("disconnect", "close"):
        renders = []
        real_render = RX.ModuleTab.render
        RX.ModuleTab.render = lambda self: renders.append(1)
        try:
            with _fresh_schema_registry(RX):
                sub = os.path.join(tmp, "backlog_" + how)
                os.makedirs(sub)
                win = _make_window(RX, sub)
                worker = RX.PhAISerialWorker("TEST_PORT")
                win._worker = worker
                for i in range(n):
                    _push(worker, DS, i, 0xF2, struct.pack("<4f", float(i), 0.0, 0.0, 0.0))
                win._on_poll()
                assert len(worker.packet_queue) == 300     # 전제: 밀린 채로 끊는다
                assert not renders, "전제가 안 생겼다: 밀려 있는데 이미 그렸다"

                tab = win._tabs[0xF2]
                path = tab.csv_path
                if how == "disconnect":
                    win._on_worker_finished()              # 워커가 끝났다는 시그널이 부르는 슬롯
                    assert len(renders) == 1, ("끊은 뒤 마지막 화면을 안 그렸다 (밀려 있는 동안 건너뛴 "
                                               "그리기가 끝내 안 돌아온다)")
                else:
                    win.close()                            # closeEvent
                assert not worker.packet_queue, "%s: 큐에 %d 개가 남은 채 닫혔다" % (how, len(worker.packet_queue))
                header, rows = _read_csv(path)
                assert len(rows) == n, "%s: CSV %d 행 (기대 %d) — 큐에 밀려 있던 프레임이 빠졌다" % (how, len(rows), n)
                assert [int(r[2]) for r in rows] == list(range(n))
                ok("%s: 큐에 남았던 300 개까지 %d 행 전부 CSV 에 있다%s"
                   % (how, len(rows), " · 마지막 화면도 그렸다" if how == "disconnect" else ""))
                win.deleteLater()
        finally:
            RX.ModuleTab.render = real_render

    # --- (b) 여러 module: 새 탭을 못 만들어 다음 틱으로 미뤄 둔 프레임 (탭도 닫을 때 만든다)
    mids = [0xF4, 0xF5, 0xF6]
    per_mod = 50
    for how in ("disconnect", "close"):
        with _fresh_schema_registry(RX):
            sub = os.path.join(tmp, "carry_" + how)
            os.makedirs(sub)
            win = _make_window(RX, sub)
            worker = RX.PhAISerialWorker("TEST_PORT")
            win._worker = worker
            for c in range(per_mod):
                for k, m in enumerate(mids):
                    _push(worker, DS, c * len(mids) + k, m, struct.pack("<2f", float(c), float(m)))
            win._on_poll()
            assert len(win._tabs) == 1 and win._carry, "전제가 안 생겼다: 미뤄 둔 프레임이 없다"

            if how == "disconnect":
                win._on_worker_finished()
            else:
                win.close()
            assert not win._carry and not worker.packet_queue
            assert sorted(win._tabs) == mids, sorted(win._tabs)
            for k, m in enumerate(mids):
                header, rows = _read_csv(win._tabs[m].csv_path)
                assert [int(r[2]) for r in rows] == [c * len(mids) + k for c in range(per_mod)], (
                    "%s: 0x%02X CSV %d 행 — 미뤄 둔 프레임이 빠졌거나 순서가 바뀌었다" % (how, m, len(rows)))
            ok("%s: 탭 만들기를 미뤄 둔 채 닫아도 module %d 개 모두 %d 행씩 CSV 에 있다"
               % (how, len(mids), per_mod))
            win.deleteLater()


# ============================================================================
# [7] 연결된 채로 다시 Connect
# ============================================================================

def test_reconnect_drains_old_session(RX, DS, tmp):
    print("\n[7] 연결된 채로 다시 Connect — 이전 세션에 밀려 있던 프레임도 CSV 에 들어간다")
    cap = RX.MAX_PACKETS_PER_POLL
    n = cap + 250

    with _fresh_schema_registry(RX):
        sub = os.path.join(tmp, "reconnect")
        os.makedirs(sub)
        win = _make_window(RX, sub)
        old = RX.PhAISerialWorker("OLD_PORT")
        win._worker = old                       # 이전 연결 (스레드 없이 워커만 붙인다)
        for i in range(n):
            _push(old, DS, i, 0xF2, struct.pack("<4f", float(i), 0.0, 0.0, 0.0))
        win._on_poll()
        assert len(old.packet_queue) == 250, len(old.packet_queue)     # 전제: 밀린 채로 다시 연결
        path = win._tabs[0xF2].csv_path

        fake = _FakeSerial([], idle_sleep_s=0.002)
        with _capture_message_boxes() as popups:
            with _patched_serial(RX, fake):
                win._start_connection("NEW_PORT")       # 이전 세션 정리(남은 프레임 반영) -> 새 세션
                assert win._worker is not old and win._worker is not None
                assert not old.packet_queue, "이전 세션 큐에 %d 개가 남았다" % len(old.packet_queue)
                # 워커가 run() 안에서 돌기 시작한 뒤에 끊는다 — run() 이 시작하면서 _running 을
                # 켜므로, 그 전에 부른 stop() 은 덮어써진다.
                pump_until(lambda: fake.reads > 0, "새 워커 시작")
                win._on_disconnect()
                pump_until(lambda: win._worker is None, "새 연결 종료")
        assert not popups, popups

        header, rows = _read_csv(path)
        assert len(rows) == n, ("이전 세션 CSV %d 행 (기대 %d) — 다시 Connect 할 때 큐에 밀려 있던 "
                                "프레임이 빠졌다" % (len(rows), n))
        assert [int(r[2]) for r in rows] == list(range(n))
        ok("이전 세션의 CSV 에 %d 행 전부 있다 (다시 Connect 할 때 큐에 남았던 250 개 포함)" % len(rows))
        _dispose(win)


# ============================================================================
# [8] 재연결로 탭을 버릴 때
# ============================================================================

def test_reset_state_flushes_csv(RX, DS, tmp):
    print("\n[8] 탭을 버리기 전에(_reset_state) 아직 디스크에 안 나간 CSV 행을 내보낸다")
    with _fresh_schema_registry(RX):
        win = _make_window(RX, tmp)
        worker = RX.PhAISerialWorker("TEST_PORT")
        win._worker = worker
        n = 200
        assert n < RX.FLUSH_EVERY          # 전제: 자동 flush 에 못 미치는 양
        for i in range(n):
            _push(worker, DS, i, 0xF3, struct.pack("<2f", float(i), 1.0))
        win._on_poll()

        tab = win._tabs[0xF3]
        path = tab.csv_path
        with open(path, encoding="utf-8") as f:
            on_disk = sum(1 for _ in f)
        assert on_disk <= 1, "전제가 안 생겼다: %d 줄이 이미 디스크에 있다" % on_disk
        ok("전제: %d 행이 아직 메모리(_pending)에만 있다 (디스크 %d 줄)" % (n, on_disk))

        win._reset_state()        # 연결된 채로 다시 Connect 하면 여기로 온다
        assert not win._tabs
        header, rows = _read_csv(path)
        assert len(rows) == n, ("탭을 버린 뒤 CSV 에 %d 행 (기대 %d) — 메모리에만 있던 행이 사라졌다"
                                % (len(rows), n))
        ok("탭을 버린 뒤 파일에 %d 행이 모두 있다 (예전엔 0 행 — 조용히 사라졌다)" % len(rows))
        win._worker = None
        _dispose(win)


# ============================================================================
# [9] 재연결 뒤 이전 탭
# ============================================================================

def test_reset_state_starts_a_new_session(RX, DS, tmp):
    print("\n[9] 재연결(_reset_state) — 이전 탭은 실제로 지워진다 (채널 이름을 언제 잊는지는 [9b])")
    from PyQt5 import QtCore

    stream, exp = DS.build_demo_session(cycles=8)
    with _fresh_schema_registry(RX):
        win = _make_window(RX, tmp)
        win._chk_total_tab.setChecked(True)
        worker = RX.PhAISerialWorker("TEST_PORT")
        win._worker = worker
        buf = bytearray(stream)
        while True:
            d = buf.find(0)
            if d < 0:
                break
            if d > 0:
                worker._parse_frame(cobs_decode(bytes(buf[:d])), 0.001)
            del buf[:d + 1]
        _settle(win, worker)

        assert len(win.findChildren(RX.ModuleTab)) == 2 and len(win.findChildren(RX.TotalDataTab)) == 1
        assert RX.SCHEMA_REG.struct_name(DS.MODULE_TYPED) == DS.DEMO_STRUCT_NAME
        assert win._tab_title(DS.MODULE_TYPED) == "0xF0 · %s" % DS.DEMO_STRUCT_NAME

        # "Show 0x20 tab" 을 껐다 켜도 꺼진 탭이 쌓이지 않는다
        win._chk_total_tab.setChecked(False)
        QtCore.QCoreApplication.sendPostedEvents(None, QtCore.QEvent.DeferredDelete)
        assert not win.findChildren(RX.TotalDataTab), "체크를 끈 0x20 탭이 지워지지 않았다"
        win._chk_total_tab.setChecked(True)
        assert len(win.findChildren(RX.TotalDataTab)) == 1
        ok("'Show 0x20 tab' 을 껐다 켜면 예전 표는 지워지고 새 표 하나만 남는다")

        win._reset_state()                    # 연결된 채로 다시 Connect 하면 여기로 온다
        QtCore.QCoreApplication.sendPostedEvents(None, QtCore.QEvent.DeferredDelete)

        left = len(win.findChildren(RX.ModuleTab)) + len(win.findChildren(RX.TotalDataTab))
        assert left == 0, ("재연결 뒤에도 이전 탭 %d 개가 살아 있다 — removeTab 은 위젯을 지우지 않아서 "
                           "연결할 때마다 탭당 OpenGL 그래프 6개가 쌓인다" % left)
        ok("이전 세션의 module 탭 2개 · 0x20 탭 1개가 실제로 지워졌다")

        # _reset_state 는 탭만 치운다 — 채널 이름(0xEE 스키마 · 0xEF)은 건드리지 않는다. 같은
        # 포트로 Disconnect -> Connect 하는 경로가 여기로 오는데, 그때 보드는 이름을 다시 보내지
        # 않는다(USB 가 새로 잡힌 게 아니므로). 언제 잊는지는 [9b].
        assert RX.SCHEMA_REG.struct_name(DS.MODULE_TYPED) == DS.DEMO_STRUCT_NAME, (
            "_reset_state 가 채널 이름까지 비웠다 — 같은 포트로 다시 Connect 하면 보드는 이름을 다시 "
            "보내지 않아서 이름이 영영 안 돌아온다")
        assert win._tab_title(DS.MODULE_TYPED) == "0xF0 · %s" % DS.DEMO_STRUCT_NAME
        ok("탭만 치운다: 0xF0 의 0xEE 스키마 이름(%s)은 그대로다" % DS.DEMO_STRUCT_NAME)
        win._worker = None
        _dispose(win)


# ============================================================================
# [9b] 다시 연결할 때 채널 이름을 남기나 잊나
# ============================================================================

def _data_only_stream(DS, cycles=6):
    """0xF0(타입 구조체) · 0xF1(float32 3개) **데이터만** — 0xEE/0xEF 가 없다.

    보드는 USB 가 새로 잡힐 때만 채널 이름을 보낸다. COM 포트만 닫았다 여는 재연결 뒤에 오는
    스트림이 정확히 이 모양이다.
    """
    out, seq = bytearray(), 0
    for i in range(cycles):
        out += DS.wire_frame(seq, DS.MODULE_TYPED,
                             DS.typed_payload(i % 5, True, 0.5 + i, 10 * i, 10000 + i, (1.0, 2.0)))
        out += DS.wire_frame(seq + 1, DS.MODULE_META_ONLY, struct.pack("<3f", 1.0 + i, 2.0, 3.0))
        seq += 2
    return bytes(out), 2 * cycles


class _LossySerial(_FakeSerial):
    """준비한 바이트를 다 주면 포트가 사라진 것처럼 `SerialException` 을 던진다 (케이블 빠짐)."""

    def __init__(self, RX, chunks, **kw):
        super().__init__(chunks, **kw)
        self._exc = RX.serial.SerialException

    def read(self, n):
        if self._i >= len(self._chunks):
            raise self._exc("device disconnected (test)")
        return super().read(n)


def test_reconnect_keeps_channel_names_unless_usb_reenumerated(RX, DS, tmp):
    print("\n[9b] 다시 연결해도 채널 이름은 남는다 — 포트가 바뀌었거나 끊겼다 되돌아온 재연결에서만 잊는다")
    full, exp = DS.build_demo_session(cycles=8)           # 0xEE · 0xEF 를 실어 오는 세션
    n_full = exp.typed_frames + exp.meta_only_frames
    bare, n_bare = _data_only_stream(DS)                  # 이름 없이 데이터만 오는 세션
    typed_size = DS.DEMO_STRUCT_SIZE

    def session(win, port, stream, n_user, serial_factory=None):
        """실제 Connect 경로(`_start_connection`)로 세션 하나를 돌리고 Disconnect 까지 마친다.

        반환: ({module_id: 탭 제목}, {module_id: CSV 머리글의 채널 이름들})
        """
        if serial_factory is None:
            fake = _FakeSerial(_chunks(stream, 300), idle_sleep_s=0.002)
        else:
            fake = serial_factory(_chunks(stream, 300))
        with _patched_serial(RX, fake):
            if port is None:                                  # 자동 재연결: 타이머가 부르는 그 함수
                win._try_reconnect()
            else:
                win._start_connection(port)
            worker = win._worker
            assert worker is not None
            pump_until(lambda: win._total_recv >= n_user and not worker.packet_queue and not win._carry,
                       "사용자 프레임 %d 개 소비 (현재 %d)" % (n_user, win._total_recv))
            if serial_factory is None:
                win._on_disconnect()
            pump_until(lambda: win._worker is None, "세션 종료")
        win._refresh_tab_titles()
        titles = {mid: win._tab_widget.tabText(win._tab_widget.indexOf(tab))
                  for mid, tab in win._tabs.items()}
        names = {mid: _read_csv(tab.csv_path)[0][5:] for mid, tab in win._tabs.items()}
        return titles, names

    with _capture_message_boxes() as popups, _fresh_schema_registry(RX):
        win = _make_window(RX, os.path.join(tmp, "names"))
        win._chk_xmlog.setChecked(False)                  # .xmlog 파일은 이 시험과 무관하다
        real_comports = RX.serial.tools.list_ports.comports
        RX.serial.tools.list_ports.comports = lambda: [types.SimpleNamespace(device="COM_C")]
        try:
            # (1) 첫 세션: 이름이 온다
            titles, _names = session(win, "COM_A", full, n_full)
            assert titles[DS.MODULE_TYPED] == "0xF0 · %s" % DS.DEMO_STRUCT_NAME, titles
            typed_names = list(RX.SCHEMA_REG.get(DS.MODULE_TYPED, typed_size).names)
            meta_names = ["Battery", "Load Cell", "Ambient"]
            assert RX.SCHEMA_REG.get(DS.MODULE_META_ONLY, 12).names == meta_names
            ok("(1) 첫 연결(COM_A): 0xEE 로 0xF0 = '%s'(%d채널), 0xEF 로 0xF1 = %s"
               % (DS.DEMO_STRUCT_NAME, len(typed_names), meta_names))

            # (2) 같은 포트로 Disconnect -> Connect: 보드는 이름을 다시 보내지 않는다 — 이름이 남아야 한다
            titles, names = session(win, "COM_A", bare, n_bare)
            assert titles[DS.MODULE_TYPED] == "0xF0 · %s" % DS.DEMO_STRUCT_NAME, titles
            assert names[DS.MODULE_TYPED] == typed_names, names[DS.MODULE_TYPED]
            assert names[DS.MODULE_META_ONLY] == meta_names, names[DS.MODULE_META_ONLY]
            header, rows = _read_csv(win._tabs[DS.MODULE_TYPED].csv_path)
            ticks = [int(float(r[header.index("tick")])) for r in rows]
            assert ticks == [10000 + i for i in range(6)], (
                "이름만 남는 게 아니라 값도 0xEE 타입대로 풀려야 한다 (float32 로 뭉개지면 안 된다): %r" % (ticks,))
            ok("(2) 같은 포트로 다시 Connect(이름 재전송 없음): 탭 제목 · CSV 열 이름 · 타입 디코드가 그대로")

            # (3) 다른 포트: 다른 보드일 수 있다 — 옛 이름을 물려받지 않는다
            titles, names = session(win, "COM_B", bare, n_bare)
            assert titles == {DS.MODULE_TYPED: "0xF0", DS.MODULE_META_ONLY: "0xF1"}, titles
            assert names[DS.MODULE_TYPED] == ["ch%d" % i for i in range(6)], names[DS.MODULE_TYPED]
            assert names[DS.MODULE_META_ONLY] == ["ch0", "ch1", "ch2"], names[DS.MODULE_META_ONLY]
            ok("(3) 다른 포트(COM_B)로 연결: 이전 보드의 이름을 안 물려받는다 (0xF0 -> ch0..ch5, 탭 제목 '0xF0')")

            # (4) 포트가 끊겼다 되돌아온 자동 재연결: USB 가 다시 잡혔다 — 옛 이름은 버린다
            titles, _names = session(win, "COM_C", full, n_full,
                                     serial_factory=lambda ch: _LossySerial(RX, ch, idle_sleep_s=0.002))
            assert titles[DS.MODULE_TYPED] == "0xF0 · %s" % DS.DEMO_STRUCT_NAME, titles      # 전제: 이름을 배웠다
            win._stop_reconnect()                                                            # 2초 타이머는 쓰지 않는다
            titles, names = session(win, None, bare, n_bare)                                 # _try_reconnect
            assert titles == {DS.MODULE_TYPED: "0xF0", DS.MODULE_META_ONLY: "0xF1"}, (
                "포트가 끊겼다 되돌아온 재연결인데 옛 이름이 남았다: %r" % (titles,))
            assert names[DS.MODULE_TYPED] == ["ch%d" % i for i in range(6)], names[DS.MODULE_TYPED]
            ok("(4) 포트가 끊겼다 되돌아온 자동 재연결(COM_C): 옛 이름을 버리고 새로 시작한다")

            # (5) 그 뒤의 수동 재연결은 다시 이름을 남긴다 (끊김 표시가 계속 남아 있으면 안 된다)
            session(win, "COM_C", full, n_full)
            titles, names = session(win, "COM_C", bare, n_bare)
            assert titles[DS.MODULE_TYPED] == "0xF0 · %s" % DS.DEMO_STRUCT_NAME, titles
            assert names[DS.MODULE_META_ONLY] == meta_names, names
            ok("(5) 자동 재연결이 끝난 뒤 같은 포트 수동 재연결은 다시 이름을 남긴다")
        finally:
            RX.serial.tools.list_ports.comports = real_comports
        assert not popups, popups
        _dispose(win)


# ============================================================================
# [9c] 탭을 지운 직후의 마우스 시그널
# ============================================================================

def test_pending_mouse_signal_after_reset(RX, DS, tmp):
    print("\n[9c] 탭을 지운 직후 보류 중인 마우스 시그널이 지워진 그래프를 건드리지 않는다")
    from PyQt5 import QtCore
    app = QtCore.QCoreApplication.instance()

    with _fresh_schema_registry(RX):
        win = _make_window(RX, tmp)
        worker = RX.PhAISerialWorker("TEST_PORT")
        win._worker = worker
        for seq, mid in enumerate([0xF0, 0xF1, 0xF2]):
            _push(worker, DS, seq, mid, struct.pack("<3f", 1.0, 2.0, 3.0))
        _settle(win, worker)
        assert len(win._tabs) == 3

        # 마우스가 그래프 위를 지나가는 중이다: 신호가 막 들어와 SignalProxy 의 30Hz 타이머가
        # 아직 안 울렸다. 그 타이머가 울리기 전에 재연결로 탭이 지워진다.
        keep = list(win._tabs.values())          # 파이썬 쪽 참조를 붙들어, 프록시가 GC 로 먼저 사라지지 않게
        for tab in keep:
            for pw in tab._plot_widgets:
                pw.scene().sigMouseMoved.emit(QtCore.QPointF(20.0, 20.0))
        pending = sum(1 for tab in keep for p in tab._mouse_proxies if p.args is not None)
        assert pending == 3 * 6, "전제가 안 생겼다: 보류 중인 프록시 %d 개 (18 개여야)" % pending

        errors = []
        real_hook = sys.excepthook
        sys.excepthook = lambda t, v, tb: errors.append(v)      # Qt 슬롯 안의 예외는 여기로 온다
        try:
            win._reset_state()
            QtCore.QCoreApplication.sendPostedEvents(None, QtCore.QEvent.DeferredDelete)
            assert not win.findChildren(RX.ModuleTab), "전제가 안 생겼다: 탭이 안 지워졌다"
            deadline = time.perf_counter() + 0.6                 # 타이머가 울리고도 남을 시간
            while time.perf_counter() < deadline:
                app.processEvents()
                time.sleep(0.005)
        finally:
            sys.excepthook = real_hook
        assert not errors, "지워진 탭의 마우스 시그널이 오류를 냈다: %r" % (errors[0],)
        ok("탭 3개(그래프 18개)에 마우스 신호가 보류된 채 지워도 0.6초 뒤까지 오류 없음")
        win._worker = None
        _dispose(win)


# ============================================================================
# [10] 0x20 표
# ============================================================================

def test_total_tab_throttle_and_mismatch(RX, DS, tmp):
    print("\n[10] 0x20 표 — 디코드는 5Hz 로만 · 맵이 안 맞으면 '안 맞음' 을 보여 준다")
    import xm_total_data_map as M

    valid = DS.total_payload({"xm_loop_count": 1234})
    assert len(valid) == M.TOTAL_PACKET_SIZE

    # --- 단위: 이 크기는 못 풀고, float32 로 새지도 않는다 ---
    def frame(n):
        return PhAIFrame(0, 0x20, 0, bytes(n), 12, 0.0)

    with _fresh_schema_registry(RX):
        got = RX.decode_system_values(frame(124))
        assert got is None, "맵에 없는 크기(124 B)를 %d 채널로 풀었다" % len(got[0])
        ok("크기가 안 맞는 payload(124 B) -> decode_system_values() 는 None (float32 로 안 푼다)")

        assert RX.decode_system_values(frame(M.TOTAL_PACKET_SIZE)) is not None
        names, vals = RX.decode_system_values(frame(368))
        assert len(names) == len(vals) == len(M.SCALAR_NAMES)
        ok("맞는 크기(365/368 B)는 %d 채널로 풀린다" % len(names))

    # 맵 자체가 없는 환경(레지스트리가 float32 폴백을 내주는 경우)도 새면 안 된다.
    # 앞 블록이 채운 캐시를 안 쓰도록 새 레지스트리에서 한다.
    with _fresh_schema_registry(RX) as reg:
        reg._total = None
        cs = reg.get(0x20, 368)
        assert cs is not None and cs.source == "fallback", cs      # 전제: 레지스트리는 폴백을 준다
        got = RX.decode_system_values(frame(368))
        assert got is None, "맵이 없는데 float32 폴백으로 %d 채널을 풀었다" % len(got[0])
        ok("맵이 없어 레지스트리가 float32 폴백을 내주는 경우에도 None (표에 그럴듯한 틀린 숫자가 안 뜬다)")

    # --- 통합: 실제 창에서 poll 마다 0x20 이 와도 디코드는 5Hz ---
    calls = []
    real_decode = RX.decode_system_values

    def counting_decode(pkt):
        calls.append(1)
        return real_decode(pkt)

    clock = [100.0]
    fake_time = types.SimpleNamespace(perf_counter=lambda: clock[0])
    real_time = RX.time

    with _fresh_schema_registry(RX):
        win = _make_window(RX, tmp)
        worker = RX.PhAISerialWorker("TEST_PORT")
        win._worker = worker
        win._chk_total_tab.setChecked(True)
        tab = win._total_tab
        RX.decode_system_values = counting_decode
        RX.time = fake_time
        try:
            n_polls = 34                       # 0.03 s 간격 · 약 1 초
            for k in range(n_polls):
                _push(worker, DS, k, 0x20, valid)
                win._on_poll()
                clock[0] += 0.03
            assert len(calls) == 5, ("poll %d 번에 0x20 디코드 %d 번 (5Hz 면 5번) — 디코드가 스로틀 앞에서 돈다"
                                     % (n_polls, len(calls)))
            ok("poll %d 번(약 1 초)에 0x20 이 매번 와도 디코드는 %d 번 (5Hz) — 예전엔 %d 번"
               % (n_polls, len(calls), n_polls))

            row = [i for i in range(tab._table.rowCount())
                   if tab._table.item(i, 0).text() == "xm_loop_count"]
            assert row and tab._table.item(row[0], 1).text() == "1234"
            ok("표에 값이 들어갔다: xm_loop_count = 1234")

            # 맵이 안 맞는 크기가 오면 → 안 맞음 표시, 표는 비운다
            clock[0] += 0.5
            _push(worker, DS, 500, 0x20, bytes(124))
            win._on_poll()
            assert not tab._warn.isHidden()
            assert "map mismatch" in tab._warn.text() and "124" in tab._warn.text(), tab._warn.text()
            assert tab._table.rowCount() == 0
            ok("크기가 안 맞는 0x20(124 B) -> 표를 비우고 %r" % tab._warn.text()[:40])

            # 다시 맞는 크기가 오면 회복한다
            clock[0] += 0.5
            _push(worker, DS, 501, 0x20, valid)
            win._on_poll()
            assert tab._warn.isHidden() and tab._table.rowCount() == len(M.SCALAR_NAMES)
            ok("맞는 크기가 다시 오면 경고가 사라지고 표가 채워진다")
        finally:
            RX.decode_system_values = real_decode
            RX.time = real_time
        win._worker = None
        _dispose(win)


# ============================================================================
# [11] 스키마가 늦게 와서 해석만 바뀔 때 (채널 수는 그대로)
# ============================================================================

def _four_word_module(prefix=""):
    """모듈 0xF3: `{ float a; float b; int32_t count; uint32_t flags; }` — 스키마가 있든 없든 채널이 4개다.

    스키마 없이 float32 로 읽으면 count·flags 는 정수 비트가 float 로 재해석된 값(0.000000)이고,
    스키마로 읽으면 진짜 정수다. 채널 수가 그대로라 개수만 보는 검사는 이 차이를 못 본다.
    `prefix` 를 주면 필드 이름만 다른(=CRC 가 다른) 스키마가 된다.
    """
    import schema_0xee as EE
    fields = [EE.FieldDef(prefix + "a", "V", 0, 1, 0, 1.0), EE.FieldDef(prefix + "b", "A", 0, 1, 4, 1.0),
              EE.FieldDef(prefix + "count", "n", 6, 1, 8, 1.0), EE.FieldDef(prefix + "flags", "", 7, 1, 12, 1.0)]
    return EE.encode_fragment(0xF3, 16, "FourWords", fields,
                              frame_index=0, frame_count=1, field_count_total=4)


def _four_word_data(i):
    return struct.pack("<ffiI", 1.5 + i, 2.5, 100 + i, 0xA0 + i)


def test_late_schema_same_channel_count(RX, DS, tmp):
    print("\n[11] 채널 수는 그대로인데 해석이 바뀌면(스키마가 늦게 오거나 바뀌면) — CSV 한 열에 두 가지 뜻이 섞이지 않는다")
    import glob

    frag, frag2 = _four_word_module(), _four_word_module("x_")
    data = _four_word_data

    # 프레임 순서 (종류, 번호). d = 데이터, s = 스키마(0xEE) 1번, t = 이름만 다른 스키마 2번
    plan_a = [("d", 0), ("d", 1), ("d", 2), ("s", 0), ("d", 3), ("d", 4), ("d", 5), ("d", 6)]
    plan_b = [("s", 0), ("d", 0), ("d", 1), ("d", 2), ("s", 0), ("d", 3), ("d", 4), ("t", 0), ("d", 5), ("d", 6)]

    def wire_payload(kind, i):
        if kind == "d":
            return data(i)
        return frag if kind == "s" else frag2

    def run_gui(plan, label):
        with _fresh_schema_registry(RX):
            win = _make_window(RX, os.path.join(tmp, "gui_" + label))
            worker = RX.PhAISerialWorker("TEST_PORT")
            win._worker = worker
            for seq, (kind, i) in enumerate(plan):
                _push(worker, DS, seq, 0xF3 if kind == "d" else 0xEE, wire_payload(kind, i))
                win._on_poll()           # 화면이 프레임마다 따라간다 — 헤더는 첫 프레임에서 잠긴다
            _settle(win, worker)
            tab = win._tabs[0xF3]
            win._close_all_csv()
            header, rows = _read_csv(tab.csv_path)
            info = (tab._mismatched, win._module_status.toPlainText())
            win._worker = None
            _dispose(win)
        return header, rows, info

    def run_cli(plan, label):
        stream = b"".join(DS.wire_frame(seq, 0xF3 if kind == "d" else 0xEE, wire_payload(kind, i))
                          for seq, (kind, i) in enumerate(plan))
        out = os.path.join(tmp, "cli_" + label)
        buf = io.StringIO()
        with _fresh_schema_registry(RX):
            with _patched_serial(RX, _CliSerial(_chunks(stream))):
                with contextlib.redirect_stdout(buf):
                    RX.run_cli("COM_TEST", 921600, out, total_data=False, log=False)
        header, rows = _read_csv(glob.glob(os.path.join(out, "cdc_phai_*_user_0xF3.csv"))[0])
        return header, rows, buf.getvalue()

    # ---- (a) 데이터가 먼저, 스키마가 늦게 (FW 의 실제 순서) --------------------------------
    n_num_a = 3
    n_hex_a = sum(1 for kind, _ in plan_a[plan_a.index(("s", 0)):] if kind == "d")

    def check_a(header, rows, label):
        assert header[5:] == ["ch0", "ch1", "ch2", "ch3"], (label, header)
        assert len(rows) == n_num_a + n_hex_a and all(len(r) == len(header) for r in rows), (label, rows)
        for k in range(n_num_a):                          # 스키마 전: 값이 있는 숫자 행
            assert all(c != "" for c in rows[k][5:]), (label, rows[k])
        for k in range(n_num_a, n_num_a + n_hex_a):       # 스키마 후: 같은 채널 수여도 hex 행
            assert rows[k][5:8] == ["", "", ""], (label, rows[k])
            assert bytes.fromhex(rows[k][8]) == data(k), (label, k, rows[k])

    header, rows, (n_bad, panel) = run_gui(plan_a, "a")
    check_a(header, rows, "GUI")
    assert n_bad == n_hex_a and "hex행 %d" % n_hex_a in panel, (n_bad, panel)
    ok("(a) 창: 데이터가 먼저 온 뒤 스키마가 오면 채널 수는 4 그대로여도 이후 %d 행은 hex — 예전엔 "
       "같은 열에 잘못 읽은 값과 진짜 값이 섞이고 경고도 없었다" % n_hex_a)
    header, rows, text = run_cli(plan_a, "a")
    check_a(header, rows, "CLI")
    assert "해석이 달라" in text and str(n_hex_a) in text, text
    ok("(a) CLI: 같은 스트림에서 같은 결과, 종료 요약에 hex 로 적은 행 수(%d)가 경고로 나온다" % n_hex_a)

    # ---- (b) 스키마가 먼저 (정상 경로는 hex 없이) — 같은 스키마 재전송은 그대로, 다른 스키마는 hex ----
    #  헤더가 스키마 이름으로 잠기고, 같은 스키마를 다시 받아도 숫자 행이 이어지다가,
    #  이름이 다른(=CRC 가 다른) 스키마로 바뀐 뒤부터 hex 행이다.
    n_num_b, n_hex_b = 5, 2

    def check_b(header, rows, label):
        assert header[5:] == ["a", "b", "count", "flags"], (label, header)
        assert len(rows) == n_num_b + n_hex_b and all(len(r) == len(header) for r in rows), (label, rows)
        for k in range(n_num_b):                          # 진짜 정수 값이 그대로 (100+k, 0xA0+k)
            assert rows[k][7:] == ["%.6f" % (100 + k), "%.6f" % (0xA0 + k)], (label, k, rows[k])
        for k in range(n_num_b, n_num_b + n_hex_b):
            assert rows[k][5:8] == ["", "", ""] and bytes.fromhex(rows[k][8]) == data(k), (label, k, rows[k])

    header, rows, (n_bad, panel) = run_gui(plan_b, "b")
    check_b(header, rows, "GUI")
    assert n_bad == n_hex_b, n_bad
    ok("(b) 창: 스키마가 먼저 오면 숫자 행 %d 개(같은 스키마를 다시 받아도 그대로), 다른 스키마로 바뀐 "
       "뒤 %d 행만 hex" % (n_num_b, n_hex_b))
    header, rows, text = run_cli(plan_b, "b")
    check_b(header, rows, "CLI")
    ok("(b) CLI: 같은 결과")

    # ---- 해석 지문 자체 ------------------------------------------------------------------
    with _fresh_schema_registry(RX):
        _n, _v, key_before = RX.decode_user_frame(PhAIFrame(0, 0xF3, 0, data(0), 24, 0.0))
        assert key_before is None
        RX.feed_schema_frame(PhAIFrame(1, 0xEE, 0, frag, 232, 0.0), 0.001)
        _n, _v, key_after = RX.decode_user_frame(PhAIFrame(2, 0xF3, 0, data(0), 24, 0.0))
        RX.feed_schema_frame(PhAIFrame(3, 0xEE, 0, frag, 232, 0.0), 0.002)      # FW 가 같은 스키마를 다시 보냄
        _n, _v, key_again = RX.decode_user_frame(PhAIFrame(4, 0xF3, 0, data(0), 24, 0.0))
        assert key_after is not None and key_after == key_again, (key_after, key_again)
        ok("해석 지문: 스키마 전 None -> 후 '%s' · 같은 스키마를 다시 받아도 그대로" % key_after[:24])


# ============================================================================
# [11b] 스키마가 없는 module 의 채널 수만 바뀔 때
# ============================================================================

def test_count_only_change_without_schema(RX, DS, tmp):
    print("\n[11b] 0xEE/0xEF 가 한 번도 안 오는 module 의 채널 수가 도중에 바뀌면 — 열이 밀리지 않고 그 행은 hex")
    import glob

    # 위 [11] 은 채널 수가 바뀔 때 해석 지문도 같이 바뀐다(스키마가 도착해서). 여기는 스키마가
    # 없어서 지문이 처음부터 끝까지 None 이다 — 행을 hex 로 돌리는 두 조건(채널 수 · 지문) 중
    # 채널 수만 걸리는 경우다. 4개로 시작해 3개(짧게) · 5개(길게)로 흔들렸다가 4개로 돌아온다.
    mid = 0xF5
    plan = [4, 4, 4, 3, 3, 4, 5, 5, 4]
    n_bad = sum(1 for n in plan if n != plan[0])

    def payload(i, n):
        return struct.pack("<%df" % n, *[float(10 * i + k) for k in range(n)])

    def check(header, rows, label):
        assert header[5:] == ["ch0", "ch1", "ch2", "ch3"], (label, header)
        assert len(rows) == len(plan), (label, len(rows))
        bad = [k for k, r in enumerate(rows) if len(r) != len(header)]
        assert not bad, ("%s: 행 %s 의 열 수가 헤더(%d)와 다르다 — 채널 수가 바뀐 행이 열을 밀어 놓았다: %r"
                         % (label, bad, len(header), [rows[k] for k in bad]))
        for k, n in enumerate(plan):
            is_hex = all(c == "" for c in rows[k][5:-1])
            if n == plan[0]:
                assert not is_hex, (label, k, rows[k])
                assert [float(c) for c in rows[k][5:]] == [float(10 * k + j) for j in range(4)], (label, k, rows[k])
            else:                                                  # 3개(짧다) · 5개(길다) 모두
                assert is_hex and bytes.fromhex(rows[k][-1]) == payload(k, n), (label, k, n, rows[k])

    with _fresh_schema_registry(RX):
        win = _make_window(RX, os.path.join(tmp, "gui"))
        worker = RX.PhAISerialWorker("TEST_PORT")
        win._worker = worker
        for seq, n in enumerate(plan):
            _push(worker, DS, seq, mid, payload(seq, n))
            win._on_poll()                     # 화면이 프레임마다 따라간다 — 헤더는 첫 프레임에서 잠긴다
        _settle(win, worker)
        tab = win._tabs[mid]
        win._close_all_csv()
        header, rows = _read_csv(tab.csv_path)
        n_counted, panel = tab._mismatched, win._module_status.toPlainText()
        win._worker = None
        _dispose(win)
    check(header, rows, "GUI")
    assert n_counted == n_bad and "hex행 %d" % n_bad in panel, (n_counted, panel)
    ok("(창) 채널 수 4 -> 3 -> 4 -> 5 -> 4: 행 %d개 전부 헤더와 같은 열 수, 짧은 행·긴 행 %d개는 hex "
       "(원본 payload 그대로), 카운터·상태 패널도 %d" % (len(rows), n_bad, n_bad))

    stream = b"".join(DS.wire_frame(seq, mid, payload(seq, n)) for seq, n in enumerate(plan))
    out = os.path.join(tmp, "cli")
    buf = io.StringIO()
    with _fresh_schema_registry(RX):
        with _patched_serial(RX, _CliSerial(_chunks(stream))):
            with contextlib.redirect_stdout(buf):
                RX.run_cli("COM_TEST", 921600, out, total_data=False, log=False)
    header, rows = _read_csv(glob.glob(os.path.join(out, "cdc_phai_*_user_0xF5.csv"))[0])
    check(header, rows, "CLI")
    assert ("hex 로 적은 행 %d개" % n_bad) in buf.getvalue(), buf.getvalue()
    ok("(CLI) 같은 스트림에서 같은 결과, 종료 요약에 hex 행 %d개" % n_bad)


# ============================================================================
# [12] 상태 패널은 딕셔너리를 직접 돌지 않는다
# ============================================================================

def test_status_panel_uses_snapshot(RX, DS, tmp):
    print("\n[12] 상태 패널은 router.user_modules 를 직접 순회하지 않는다 — 워커 스레드가 키를 늘리는 순간 죽는다")
    def guarded(name):
        base = getattr(dict, name)

        def method(self, *a, **k):
            # 이 딕셔너리를 부른 코드가 GUI 모듈 안이면 그 자리에서 실패시킨다.
            # (FrameRouter.user_modules_snapshot() 은 frame_router 모듈이라 통과한다.)
            if sys._getframe(1).f_globals.get("__name__") == RX.__name__:
                raise AssertionError("GUI 코드가 router.user_modules.%s 를 직접 돌았다 — "
                                     "워커 스레드가 새 module 을 넣는 순간 'dictionary changed size "
                                     "during iteration' 으로 죽는다. user_modules_snapshot() 을 쓸 것"
                                     % name)
            return base(self, *a, **k)
        return method

    class GuardedDict(dict):
        items = guarded("items")
        keys = guarded("keys")
        values = guarded("values")
        __iter__ = guarded("__iter__")

    # 양성 대조: 이 가드는 GUI 모듈 안에서 부르면 실제로 걸린다
    exec("def _probe(d):\n    return list(d.items())\n", RX.__dict__)
    try:
        try:
            RX._probe(GuardedDict(a=1))
        except AssertionError:
            pass
        else:
            raise AssertionError("가드가 GUI 모듈 안의 직접 순회를 못 잡는다 — 이 시험이 아무것도 안 막는다")
    finally:
        del RX._probe

    with _fresh_schema_registry(RX):
        win = _make_window(RX, tmp)
        w = RX.PhAISerialWorker("TEST_PORT")
        w.router.user_modules = GuardedDict()
        for seq, mid in enumerate([0xF0, 0xF1, 0xF0]):
            w.router.route(PhAIFrame(seq, mid, 0, bytes(8), 16, 0.0))
        assert isinstance(w.router.user_modules, GuardedDict) and len(w.router.user_modules) == 2
        win._update_module_status_panel(w)
        panel = win._module_status.toPlainText()
        assert "0xF0" in panel and "0xF1" in panel and "frames=2" in panel, panel
        ok("상태 패널이 module 2개를 스냅샷으로 읽었다 (직접 순회하면 가드가 걸린다)")
        _dispose(win)


# ============================================================================
# [13] CSV 리뷰어
# ============================================================================

def test_reviewer_hex_rows(RX, DS, tmp):
    print("\n[13] CSV 리뷰어 — hex 행에서 데이터가 잘리지 않고, hex 행의 seq · Tx drop 도 통계에 들어간다")
    import numpy as np
    import cdc_csv_reviewer as REV

    header_line = "time_s,pc_time_s,seq_id,module_id,tx_drops,a,b,c,d"

    # 실시간 CSV 를 쓰는 함수로 행을 만든다 — 쓰는 쪽과 읽는 쪽 규약이 같이 시험된다.
    # kind: n = 숫자 행 · h = hex 행 · h0 = 숫자로만 된 hex 행(0 바이트 16개, 숫자처럼 읽히기 쉽다)
    def row(seq, kind, drops=0, first_value=None):
        payload = bytes(16) if kind == "h0" else struct.pack("<4f", seq, seq + .5, -seq, 2.0)
        floats = [float(seq) if first_value is None else first_value, seq + .5, -float(seq), 2.0]
        vals, as_hex = RX._format_user_csv_row(4, None, floats, "KEY" if kind != "n" else None, payload)
        assert as_hex == (kind != "n"), (seq, kind)
        return "%.6f,%.6f,%d,243,%d,%s" % (seq * 0.001, seq * 0.001, seq, drops, vals)

    def write(name, rows):
        path = os.path.join(tmp, name)
        with open(path, "w", encoding="utf-8", newline="") as f:
            f.write("\n".join([header_line] + rows) + "\n")
        return path

    # A: seq 는 0..6 로 끊김이 없다. 3·4번 행이 hex 이고 Tx drop 을 5·7 실었다 (숫자 행 5번도 2 → 합 14)
    plan_a = [(0, "n", 0), (1, "n", 0), (2, "n", 0), (3, "h0", 5), (4, "h", 7), (5, "n", 2), (6, "n", 0)]
    path = write("with_hex_rows.csv", [row(s, k, d) for s, k, d in plan_a])

    def check_rows_a(data, first, label):
        assert data.shape[0] == 7, ("%s: %d 행 (기대 7) — hex 행이 빠졌거나 뒤가 잘렸다" % (label, data.shape[0]))
        vals = data[:, first:]
        assert np.isnan(vals[[3, 4]]).all(), (label, vals[[3, 4]])            # hex 행은 값 칸만 비었다
        assert np.isfinite(vals[[0, 1, 2, 5, 6]]).all(), (label, vals)

    with _capture_message_boxes() as popups:
        rv = REV.CsvReviewWindow()
        rv.load_csv(path)
        assert not popups, popups
        seqs = [int(v) for v in rv.data[:, rv.seq_col]]
        assert seqs == list(range(7)), ("리뷰어가 읽은 행 seq %r — hex 행이 빠졌거나 뒤가 잘렸다" % (seqs,))
        check_rows_a(rv.data, REV.first_data_col(rv.csv_cols), "CsvReviewWindow")
        stats = rv.lbl_seq_stats.text()
        assert stats == "Seq gaps (Δ>1): 0  |  Total Tx drops: 14", (
            "hex 행이 통계에서 빠졌다: %r (기대: seq 끊김 0, Tx drop 5+7+2=14)" % stats)
        status = rv.statusBar().currentMessage()
        assert "Outlier" not in status and "hex 행 2개" in status, status
        assert "7 rows" in rv.lbl_info.text() and "hex 행 2개" in rv.lbl_info.text(), rv.lbl_info.text()
        ok("CsvReviewWindow: 7행 전부 읽었다 (seq 0..6, hex 2행은 값 칸만 비움 — hex 행에서 잘리지 않는다)")
        ok("통계: %r — hex 행의 seq 가 이어져 끊김 0, hex 행이 실은 Tx drop 도 합계에 포함" % stats)
        rv.close()
        rv.deleteLater()

        # 양성 대조: 진짜 seq 구멍은 hex 행 옆에서도 여전히 잡힌다 (통계가 그냥 0 을 내는 게 아니다)
        plan_gap = [(0, "n", 0), (1, "n", 0), (2, "n", 0), (3, "h0", 0), (4, "h", 0), (7, "n", 0), (8, "n", 0)]
        rv = REV.CsvReviewWindow()
        rv.load_csv(write("hex_and_real_gap.csv", [row(s, k, d) for s, k, d in plan_gap]))
        assert rv.data.shape[0] == 7, rv.data.shape
        assert rv.lbl_seq_stats.text().startswith("Seq gaps (Δ>1): 1 "), rv.lbl_seq_stats.text()
        ok("hex 행 옆의 진짜 seq 구멍(4 -> 7)은 그대로 1건으로 잡힌다")
        rv.close()
        rv.deleteLater()

        # 진짜 이상치는 여전히 그 자리에서 자른다 — 그 뒤의 hex 행은 남은 행에 안 센다
        plan_out = [(0, "n", 0, None), (1, "n", 0, None), (2, "h", 0, None), (3, "n", 0, None),
                    (4, "n", 0, 1e9), (5, "h", 0, None), (6, "n", 0, None)]
        rv = REV.CsvReviewWindow()
        rv.load_csv(write("hex_and_outlier.csv", [row(s, k, d, v) for s, k, d, v in plan_out]))
        status = rv.statusBar().currentMessage()
        assert rv.data.shape[0] == 4 and "Outlier detected at row 4" in status, (rv.data.shape, status)
        assert "hex 행 1개" in status and "hex 행 1개" in rv.lbl_info.text(), (status, rv.lbl_info.text())
        ok("이상치(1e9)는 4행에서 자르고, 자른 뒤 남은 hex 행(1개)만 알린다")
        rv.close()
        rv.deleteLater()

        dlg = RX.CsvReviewDialog(path)
        assert dlg.data is not None
        check_rows_a(dlg.data, 5, "CsvReviewDialog")
        assert "hex 행 2개" in dlg.info.text() and dlg.info.text().startswith("7 rows"), dlg.info.text()
        ok("CsvReviewDialog(exe 에서 쓰이는 대체 뷰어)도 같은 7행 — hex 행은 값 칸만 비움")
        dlg.close()
        dlg.deleteLater()

        # 값이 있는 행이 하나도 없으면 조용히 빈 화면 대신 이유를 알린다
        only_hex = write("only_hex_rows.csv", [row(3, "h0"), row(4, "h")])
        rv = REV.CsvReviewWindow()
        rv.load_csv(only_hex)
        assert popups and popups[-1][0] == "warning" and "hex" in popups[-1][1][1], popups
        ok("hex 행만 있는 파일: 빈 화면 대신 이유를 알리는 경고")
        rv.close()
        rv.deleteLater()

        n_before = len(popups)
        dlg = RX.CsvReviewDialog(only_hex)
        assert len(popups) == n_before + 1 and popups[-1][0] == "critical" \
            and "숫자로 읽을 수 있는 행이 없다" in popups[-1][1][1], popups
        ok("대체 뷰어도 hex 행만 있는 파일이면 이유를 알린다")
        dlg.close()
        dlg.deleteLater()


# ============================================================================
# [14] 실제 스레드 경로
# ============================================================================

def test_threaded_connect_disconnect(RX, DS, tmp):
    print("\n[14] 실제 QThread · 시그널 경로 — Connect -> 수신 -> Disconnect (시리얼 포트만 가짜)")
    import xmlog as X

    stream, exp = DS.build_demo_session(cycles=40, drop_at=20, drop_len=3)
    sub = os.path.join(tmp, "threaded")
    os.makedirs(sub)

    win = None
    try:
        with _capture_message_boxes() as popups:
            with _fresh_schema_registry(RX):
                win = _make_window(RX, sub)
                fake = _FakeSerial(_chunks(stream, 300), idle_sleep_s=0.002)
                with _patched_serial(RX, fake):
                    win._start_connection("FAKE_PORT")
                    worker = win._worker
                    assert worker is not None and win._serial_thread is not None

                    want_user = exp.typed_frames + exp.meta_only_frames
                    pump_until(lambda: (win._total_recv >= want_user and not worker.packet_queue
                                        and worker.good >= exp.frames_ok),
                               "사용자 프레임 %d 개 소비 (현재 %d)" % (want_user, win._total_recv))
                    assert worker.good == exp.frames_ok, (worker.good, exp.frames_ok)
                    ok("워커 스레드가 %d 프레임을 받았고, 그중 사용자 프레임 %d 개를 GUI 스레드가 소비했다"
                       % (worker.good, want_user))

                    win._on_disconnect()
                    pump_until(lambda: win._worker is None, "Disconnect 완료")
                assert not popups, popups
                assert win._btn_conn.isEnabled() and not win._btn_disc.isEnabled()
                msg = win._status_bar.currentMessage()
                assert msg.startswith("Disconnected") and ".xmlog" in msg, msg
                ok("Disconnect 완료: %s..." % msg[:70])

                csvs = sorted(f for f in os.listdir(sub) if f.endswith(".csv"))
                assert len(csvs) == 2 and all("_user_0x" in f for f in csvs), csvs
                got = {f: len(_read_csv(os.path.join(sub, f))[1]) for f in csvs}
                assert sorted(got.values()) == sorted([exp.typed_frames, exp.meta_only_frames]), got
                for f in csvs:
                    header, rows = _read_csv(os.path.join(sub, f))
                    assert all(len(r) == len(header) for r in rows), f
                ok("CSV %d 개, 행 수 %s — 전부 헤더와 같은 열 수" % (len(csvs), sorted(got.values())))

                xm = [f for f in os.listdir(sub) if f.endswith(".xmlog")]
                assert len(xm) == 1, xm
                res = X.read_file(os.path.join(sub, xm[0]))
                n_data = sum(1 for r in res.records if r.rec_type == X.REC_DATA)
                assert n_data == exp.frames_ok, (n_data, exp.frames_ok)
                ok(".xmlog 에 받은 프레임 %d 개가 그대로 (워커 스레드가 쓰고 닫았다)" % n_data)
    finally:
        if win is not None:
            if win._worker is not None:            # 시험이 실패해도 스레드를 남기지 않는다
                win._worker.stop()
                if win._serial_thread is not None:
                    win._serial_thread.quit()
                    win._serial_thread.wait()
            _dispose(win)


# ============================================================================
# [15] .xmlog — GUI 와 CLI 는 같은 파일을 만든다 · 재연결은 새 파일
# ============================================================================

def _xmlog_shape(res):
    """파일의 모양 — 시각 · 호스트 도장은 빼고 무엇이 어떤 번호로 적혔는지만 본다.

    SESSION 의 `total_data_map_version` 도 빼 둔다: CLI 는 `--total-data` 일 때만, GUI 는 아예 안 적는다
    (`XmLogCapture` docstring). 여기서 비교하는 건 레코드의 모양이지 그 참고용 문자열이 아니다.
    """
    import xmlog as X
    shape = []
    for r in res.records:
        f = r.fields
        if r.rec_type == X.REC_DATA:
            shape.append(("DATA", f["module_id"], f["seq_id"], f["activation_id"], r.payload))
        elif r.rec_type == X.REC_SCHEMA_ACTIVATION:
            shape.append(("ACT", f["module_id"], f["activation_id"], r.payload))
        elif r.rec_type == X.REC_GAP:
            shape.append(("GAP", f["reason"], f["from_seq"], f["to_seq"], f["lost_count"]))
        else:
            shape.append(("SESSION", f["fw_build_id"], f["link_epoch"], f["boot_epoch"]))
    return shape


def _without_schema_frames(stream):
    """스트림에서 0xEE · 0xEF 프레임을 뺀다 -> (바이트, 남은 정상 프레임 수).

    스키마를 이미 보낸 보드는 USB 가 그대로면 채널 설명을 다시 보내지 않는다 — 창에서
    Disconnect -> Connect 만 다시 한 경우를 흉내 낸다.
    """
    from frame_router import parse_phai_frame
    out, good = bytearray(), 0
    for enc in bytes(stream).split(b"\x00"):
        if not enc:
            continue
        pkt, err = parse_phai_frame(cobs_decode(enc), 0.0)
        if err is None:
            if pkt.module_id in (0xEE, 0xEF):
                continue
            good += 1
        out += enc + b"\x00"
    return bytes(out), good


def test_xmlog_gui_cli_parity_and_reconnect_new_file(RX, DS, tmp):
    print("\n[15] .xmlog — GUI 워커와 CLI 가 같은 파일을 만든다 (스키마 발급 · 참조까지), 재연결은 새 파일, "
          "같은 초에 두 번 시작해도 파일이 둘")
    import datetime as dt
    import xmlog as X
    from xmlog_capture import XmLogCapture

    stream, exp = DS.build_demo_session(cycles=14, drop_at=10, drop_len=3)

    # ---- (a) CLI: run_cli(--log) 가 만든 파일 ----
    out = os.path.join(tmp, "cli")
    buf = io.StringIO()
    with _fresh_schema_registry(RX):
        with _patched_serial(RX, _CliSerial(_chunks(stream))):
            with contextlib.redirect_stdout(buf):
                RX.run_cli("COM_TEST", 921600, out, total_data=False, log=True)
    cli_files = [f for f in os.listdir(out) if f.endswith(".xmlog")]
    assert len(cli_files) == 1, cli_files
    cli = X.read_file(os.path.join(out, cli_files[0]))
    assert cli.stopped_reason is None, cli.stopped_reason

    # ---- (b) GUI 워커: _parse_frame 이 같은 스트림을 캡처에 넘긴 파일 ----
    gui_path = os.path.join(tmp, "gui.xmlog")
    with _fresh_schema_registry(RX):
        worker = RX.PhAISerialWorker("TEST_PORT", capture=XmLogCapture(gui_path))
        buf2 = bytearray(stream)
        n = 0
        while True:
            d = buf2.find(0)
            if d < 0:
                break
            if d > 0:
                n += 1
                worker._parse_frame(cobs_decode(bytes(buf2[:d])), 0.001 * n)
            del buf2[:d + 1]
        worker.capture.close()
    gui = X.read_file(gui_path)

    a, b = _xmlog_shape(cli), _xmlog_shape(gui)
    assert a == b, ("GUI 워커와 CLI 가 다른 파일을 만들었다 — 캡처 정책이 두 곳에 갈라졌다:\n  cli=%r\n  gui=%r"
                    % (a[:6], b[:6]))
    kinds = [x[0] for x in a]
    assert kinds.count("SESSION") == 1 and kinds.count("ACT") == 1, kinds
    typed = [x[3] for x in a if x[0] == "DATA" and x[1] == DS.MODULE_TYPED]
    assert typed[0] == 0 and typed[-1] == 1 and typed == sorted(typed), \
        "0xF0 DATA 의 번호가 (스키마 전 0 -> 스키마 뒤 1) 이 아니다: %r" % (typed,)
    assert next(x for x in a if x[0] == "SESSION")[1:] == ("unknown", 1, 0), a[0]
    ok("CLI(run_cli --log)와 GUI 워커가 레코드 %d개를 똑같이 적었다 — 스키마 %d개 발급, 0xF0 은 %d행 중 %d행이 그 번호를 단다"
       % (len(a), kinds.count("ACT"), len(typed), sum(1 for t in typed if t)))

    # ---- (c) GUI 재연결은 새 파일 — 1초 안에 다시 붙어 이름이 같아도 앞 파일을 덮어쓰지 않는다 ----
    sub = os.path.join(tmp, "reconnect")
    os.makedirs(sub)

    class _Frozen(dt.datetime):
        @classmethod
        def now(cls, tz=None):
            return dt.datetime(2026, 1, 2, 3, 4, 5)        # 두 연결이 같은 초에 걸린 것을 강제한다

    real_datetime = RX.datetime
    RX.datetime = _Frozen
    win = None
    try:
        with _capture_message_boxes() as popups, _fresh_schema_registry(RX):
            win = _make_window(RX, sub)
            assert win._chk_xmlog.isChecked()
            for round_ in (1, 2):
                fake = _FakeSerial(_chunks(stream, 300), idle_sleep_s=0.002)
                with _patched_serial(RX, fake):
                    win._start_connection("COM_R")
                    worker = win._worker
                    pump_until(lambda: worker.good >= exp.frames_ok and not worker.packet_queue
                               and not win._carry, "%d번째 연결의 프레임 소비" % round_)
                    win._on_disconnect()
                    pump_until(lambda: win._worker is None, "%d번째 연결 종료" % round_)
            assert not popups, popups
    finally:
        RX.datetime = real_datetime
        if win is not None:
            _dispose(win)

    files = sorted(f for f in os.listdir(sub) if f.endswith(".xmlog"))
    assert files == ["cdc_20260102_030405.xmlog", "cdc_20260102_030405_2.xmlog"], (
        "1초 안에 다시 연결했는데 파일이 %r — 이름이 같아 앞 파일이 덮어써졌다" % (files,))
    shapes = [_xmlog_shape(X.read_file(os.path.join(sub, f))) for f in files]
    for sh in shapes:
        assert sh[0][0] == "SESSION" and [x[0] for x in sh].count("SESSION") == 1
        assert [x[0] for x in sh].count("ACT") == 1, "새 파일은 스키마를 처음부터 다시 배워 자기 activation 을 갖는다"
        first_typed = next(x for x in sh if x[0] == "DATA" and x[1] == DS.MODULE_TYPED)
        assert first_typed[3] == 0, "새 파일의 첫 0xF0 은 스키마 전이라 0 이어야 한다 (앞 연결의 번호를 물려받았다)"
    assert shapes[0] == shapes[1], "같은 스트림을 두 번 받았으니 두 파일의 모양이 같아야 한다"
    ok("같은 초에 다시 연결해도 파일이 둘이다: %s — 각자 SESSION 으로 시작하고 스키마 1개씩" % ", ".join(files))

    # ---- (d) CLI 도 같다 — 같은 초에 두 번 시작해도 (보드마다 recv 를 하나씩 띄우는 스크립트 등) 파일이 둘 ----
    out2 = os.path.join(tmp, "cli_twice")
    RX.datetime = _Frozen
    try:
        with _fresh_schema_registry(RX):
            for _ in (1, 2):
                with _patched_serial(RX, _CliSerial(_chunks(stream))):
                    with contextlib.redirect_stdout(io.StringIO()):
                        RX.run_cli("COM_TEST", 921600, out2, total_data=False, log=True)
    finally:
        RX.datetime = real_datetime
    names = sorted(f for f in os.listdir(out2) if f.endswith(".xmlog"))
    assert names == ["cdc_20260102_030405.xmlog", "cdc_20260102_030405_2.xmlog"], (
        "같은 초에 CLI 를 두 번 시작했는데 .xmlog 가 %r — 뒤에 시작한 쪽이 앞 파일을 비웠다" % (names,))
    cli_shapes = [_xmlog_shape(X.read_file(os.path.join(out2, n))) for n in names]
    assert cli_shapes == [a, a], "같은 스트림을 받은 두 CLI 실행의 파일이 (a) 의 파일과 다르다 — 하나가 잘렸다"
    ok("CLI 를 같은 초에 두 번 시작해도 파일이 둘이다: %s" % ", ".join(names))

    # ---- (e) 같은 포트로 Disconnect -> Connect 만 다시 한 경우 (문서가 말하는 그대로다) ----
    # USB 는 그대로라 보드가 채널 설명을 다시 보내지 않는다: 새 .xmlog 에는 스키마가 없고, 그 파일을
    # 뽑으면 이름이 없다. 화면과 실시간 CSV 는 이전 이름을 그대로 쓴다.
    # 이건 지금 펌웨어의 동작이다 — 채널 이름(0xEF)을 USB 가 새로 잡힐 때(device-ready 상승 에지)
    # 또는 XM_SetUsbCustomMeta() 를 다시 부를 때 한 번 보내고, 0xEE 는 아직 보내지 않는다. 이 시험은
    # 도구 쪽 동작만 못박는다. 펌웨어가 0xEE 를 DTR 상승 · 주기로 다시 보내면
    # 같은 포트 재연결에도 스키마가 들어와 이 시나리오가 달라지니, README 와 튜토리얼의
    # '다시 연결하면' 문단과 함께 고칠 것.
    import xmlog_export as EXP
    sub2 = os.path.join(tmp, "same_port")
    os.makedirs(sub2)
    data_only, n_data_only = _without_schema_frames(stream)
    win = None
    try:
        with _capture_message_boxes() as popups, _fresh_schema_registry(RX):
            win = _make_window(RX, sub2)
            for round_, (data, n_good) in enumerate(((stream, exp.frames_ok),
                                                     (data_only, n_data_only)), 1):
                fake = _FakeSerial(_chunks(data, 300), idle_sleep_s=0.002)
                with _patched_serial(RX, fake):
                    win._start_connection("COM_S")
                    worker = win._worker
                    pump_until(lambda: worker.good >= n_good and not worker.packet_queue
                               and not win._carry, "%d번째 연결의 프레임 소비" % round_)
                    win._on_disconnect()
                    pump_until(lambda: win._worker is None, "%d번째 연결 종료" % round_)
            assert not popups, popups
    finally:
        if win is not None:
            _dispose(win)

    files = sorted(f for f in os.listdir(sub2) if f.endswith(".xmlog"))
    assert len(files) == 2, files
    first, second = (X.read_file(os.path.join(sub2, f)) for f in files)
    n_act = [sum(1 for r in res.records if r.rec_type == X.REC_SCHEMA_ACTIVATION) for res in (first, second)]
    assert n_act == [1, 0], "첫 파일은 스키마 1개, 같은 포트로 다시 연결한 파일은 0개여야 한다: %r" % (n_act,)
    out_dir = os.path.join(sub2, "export")
    with contextlib.redirect_stdout(io.StringIO()):
        written = EXP.export_csv(second, out_dir, "second", want_raw_hex=False)
    header, _rows = _read_csv([p for p in written if p.endswith("_user_0xF0.csv")][0])
    assert header[3:5] == ["ch0", "ch1"] and "state" not in header, \
        "스키마 없는 파일이 이름을 알 리 없다: %r" % (header,)
    live = sorted(f for f in os.listdir(sub2) if f.endswith("_user_0xF0.csv"))
    assert live, os.listdir(sub2)
    live_header, _rows = _read_csv(os.path.join(sub2, live[-1]))
    assert "state" in live_header, "같은 포트로 다시 연결했는데 실시간 CSV 가 이름을 잃었다: %r" % (live_header,)
    ok("Disconnect -> Connect 만 다시 하면: 새 .xmlog 는 스키마 없이(export 는 ch0.. ) · 실시간 CSV 는 이름 유지")


# ============================================================================

def _mk(base, name):
    path = os.path.join(base, name)
    os.makedirs(path)
    return path


def main():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    print("실시간 수신 화면(GUI)·CSV 시험 — 오프스크린 Qt")

    _selfcheck_import_classifier()

    try:
        import cdc_phai_receiver as RX
        import demo_stream as DS
        from PyQt5 import QtWidgets
    except ImportError as e:
        # 선택 의존성(PyQt5/pyqtgraph/pyserial)이 없거나 네이티브 라이브러리를 못 올리는 PC 만
        # SKIP 이다. 그 밖의 ImportError 는 저장소 안쪽의 진짜 결함이라 그대로 실패시킨다.
        if _is_missing_gui_dependency(e):
            print("SKIP: GUI 의존성이 없어 이 시험을 건너뛴다 (%s: %s) — "
                  "pip install pyqt5 pyqtgraph pyserial" % (type(e).__name__, e))
            return SKIP_RC
        raise

    # ⚠ 반드시 변수로 붙든다 — 위 docstring 참조.
    app = QtWidgets.QApplication.instance() or QtWidgets.QApplication([])

    tmp = tempfile.mkdtemp(prefix="xm10_test_gui_")
    try:
        tests = [
            test_two_tabs_end_to_end, test_cli_files_and_alignment, test_poll_is_bounded,
            test_new_tabs_spread_across_ticks, test_render_waits_for_backlog,
            test_backlog_is_written_on_close, test_reconnect_drains_old_session,
            test_reset_state_flushes_csv, test_reset_state_starts_a_new_session,
            test_reconnect_keeps_channel_names_unless_usb_reenumerated,
            test_pending_mouse_signal_after_reset,
            test_total_tab_throttle_and_mismatch, test_late_schema_same_channel_count,
            test_count_only_change_without_schema,
            test_status_panel_uses_snapshot, test_reviewer_hex_rows,
            test_threaded_connect_disconnect, test_xmlog_gui_cli_parity_and_reconnect_new_file,
        ]
        for k, fn in enumerate(tests, 1):
            fn(RX, DS, _mk(tmp, "t%d" % k))
        app.processEvents()
    finally:
        for w in _windows:                 # Windows 는 열린 파일이 있는 폴더를 못 지운다
            try:
                w._close_all_csv()
            except Exception:              # noqa: BLE001 — 정리 중의 실패는 원래 실패를 가리지 않는다
                pass
        shutil.rmtree(tmp, ignore_errors=True)

    print("\n전부 통과 (%d 항목)" % _passed)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
