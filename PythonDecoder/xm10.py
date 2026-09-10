#!/usr/bin/env python3
"""XM10 USB-CDC 스트리밍 / 로깅 / 디코딩 — 단일 진입점.

    python xm10.py demo                  보드 없이 전 구간 시연 (여기서 시작)
    python xm10.py ports                 연결된 시리얼 포트 나열
    python xm10.py recv                  실시간 수신 (그래프 GUI)
    python xm10.py recv --cli --log      실시간 수신 (콘솔) + .xmlog 저장
    python xm10.py soak --minutes 30     보드 실측 — 손실 0 인지 판정
    python xm10.py export FILE --csv DIR .xmlog 를 요약하거나 CSV 로
    python xm10.py selftest              보드 없이 도는 자체 검증 전부

PyInstaller 로 묶으면 `xm10.exe demo` 처럼 파이썬 없이도 같은 명령을 쓴다
(`python build_exe.py` 참조).

왜 진입점을 하나로 모았나
--------------------------
지금까지 이 디렉토리는 목적별 스크립트 묶음이었다. 그건 만든 사람에게는 자연스럽지만,
XM10 을 처음 받은 사람은 어느 파일부터 열어야 하는지 알 수 없다. 실행파일로 배포하면
그 문제가 더 심해진다 — `.py` 를 열어 볼 수도 없기 때문이다.

각 하위 명령은 **원래 모듈의 인자를 그대로** 넘긴다. 여기서 옵션을 다시 정의하지
않는다. 옵션이 두 군데 적히면 반드시 한쪽이 뒤처지고, 뒤처진 쪽이 문서가 된다.
"""
from __future__ import annotations

import importlib
import os
import sys


VERSION = "1.0"

FROZEN = getattr(sys, "frozen", False)
HERE = os.path.dirname(os.path.abspath(__file__))
_CDC = os.path.join(HERE, "CDC")
if os.path.isdir(_CDC) and _CDC not in sys.path:
    sys.path.insert(0, _CDC)          # 소스에서 실행할 때
elif HERE not in sys.path:
    sys.path.insert(0, HERE)          # 번들 안에서는 모듈이 최상위에 펴진다


# 하위 명령 -> (모듈, 함수, 한 줄 설명)
COMMANDS = {
    "demo":     ("demo_run",          "main", "보드 없이 전 구간 시연 + 판정"),
    "ports":    (None,                None,   "연결된 시리얼 포트 나열"),
    "recv":     ("cdc_phai_receiver", "main", "실시간 수신 (GUI 기본, --cli 로 콘솔)"),
    "soak":     ("soak",              "main", "보드 실측 — 손실/CRC/재동기 0 판정"),
    "export":   ("xmlog_export",      "main", ".xmlog 요약 · CSV 내보내기"),
    "selftest": (None,                None,   "보드 없이 도는 자체 검증 전부"),
}

# selftest 가 도는 순서. `run_tests.py` 와 같은 목록이되, 여기서는 **한 프로세스 안**에서
# 돈다 — 실행파일로 묶었을 때 하위 파이썬 프로세스를 띄울 수 없기 때문이다.
SELFTEST_SUITES = [
    ("frame_router (wire parsing)",   "test_frame_router",        "main",     ()),
    ("total_data_decoder self-test",  "total_data_decoder",       "_selftest", ()),
    ("wire -> 0x20 decode",           "test_total_data_decoder",  "main",     ()),
    ("schema 0xEE/0xEF/registry",     "test_schema",              "main",     ()),
    ("golden vectors (cross-impl)",   "test_golden_vectors",      "main",     ()),
    ("xmlog byte ABI",                "test_xmlog",               "main",     ()),
    ("wire -> xmlog -> CSV",          "test_xmlog_chain",         "main",     ()),
    ("demo_stream builders",          "demo_stream",              "_selfcheck", ()),
    ("demo stream vs test encoder",   "test_demo_stream",         "main",     ()),
    ("soak accounting self-test",     "soak",                     "main",     ("--selftest",)),
    # `--out` 은 아래에서 임시 디렉토리로 채운다 — 검증이 작업 디렉토리에 산출물을
    # 흘리면 안 된다. 데모를 눈으로 보려면 `xm10 demo` 를 따로 부른다.
    ("end-to-end demo",               "demo_run",                 "main",     ("--quiet", "--out")),
]


def _usage() -> str:
    lines = ["XM10 USB-CDC 도구  v%s" % VERSION, "",
             "사용법:  %s <명령> [옵션...]" % _prog(), ""]
    for name, (_m, _f, desc) in COMMANDS.items():
        lines.append("  %-9s %s" % (name, desc))
    lines += ["",
              "각 명령의 옵션은 그 명령에 -h 를 붙여 본다:  %s recv -h" % _prog(),
              "처음이면 여기서 시작:  %s demo" % _prog()]
    return "\n".join(lines)


def _prog() -> str:
    return os.path.basename(sys.executable) if FROZEN else "python xm10.py"


def _delegate(module_name: str, func_name: str, argv) -> int:
    """원래 모듈의 진입점을 그 모듈이 직접 실행된 것처럼 부른다."""
    mod = importlib.import_module(module_name)
    saved = sys.argv
    sys.argv = [module_name] + list(argv)
    try:
        rc = getattr(mod, func_name)()
        return 0 if rc is None else int(rc)
    finally:
        sys.argv = saved


def cmd_ports(argv) -> int:
    """포트를 나열하고 XM10 후보를 표시한다. 없으면 없다고 분명히 말한다."""
    try:
        import serial.tools.list_ports as lp
    except ImportError:
        print("pyserial 이 없다:  pip install pyserial")
        return 2

    ports = list(lp.comports())
    if not ports:
        print("시리얼 포트가 하나도 없다.")
        print("  보드를 USB-C 로 연결하고, 장치 관리자에서 COM 포트가 잡히는지 본다.")
        return 1

    print("포트 %d개:" % len(ports))
    hits = 0
    for p in ports:
        vidpid = ("%04X:%04X" % (p.vid, p.pid)) if p.vid is not None else "-"
        # STMicroelectronics VID. 이걸로 단정하지는 않는다 — 표시만 하고 선택은 사람이.
        mark = "  <- XM10 후보" if (p.vid == 0x0483) else ""
        if mark:
            hits += 1
        print("  %-8s %-11s %s%s" % (p.device, vidpid, p.description or "", mark))
    if not hits:
        print("")
        print("XM10 으로 보이는 포트가 없다 (STMicroelectronics VID 0483 없음).")
        print("  블루투스 가상 포트만 잡히는 경우가 흔하다 — 그건 XM10 이 아니다.")
    return 0


def cmd_selftest(argv) -> int:
    """모든 자체 검증을 **한 프로세스 안에서** 돌린다.

    `run_tests.py` 와 무엇이 다른가: 저쪽은 개발용이라 하위 프로세스를 띄우고
    pyflakes 정적 게이트까지 돌린다(소스가 있어야 한다). 이쪽은 실행파일 안에서도
    돌아야 하므로 in-process 다. 정적 게이트는 **여기서 못 돈다고 분명히 적는다** —
    조용히 빠지면 돌았다고 오해한다.
    """
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")

    import shutil
    import tempfile

    verbose = "-v" in argv or "--verbose" in argv
    scratch = tempfile.mkdtemp(prefix="xm10_selftest_")
    results = []
    try:
        results = _run_suites(verbose, scratch)
    finally:
        shutil.rmtree(scratch, ignore_errors=True)

    bad = [r for r in results if not r[1]]
    print("")
    print("자체 검증 %d/%d 통과" % (len(results) - len(bad), len(results)))
    print("  (pyflakes undefined-name 게이트는 소스에서만 돈다: python run_tests.py)")

    # GUI 없이 만든 빌드에서는 **실시간 디코딩 경로가 검사되지 않는다.** 그 경로는
    # 한때 0xEE 스키마를 받고도 값을 float32 로 뭉개고 있었고(감사 #7), 그걸 잡는
    # 시험은 수신기 모듈을 import 해야 돈다. 통과 개수만 보고 다 됐다고 읽으면 안 되니
    # 여기서 분명히 적는다.
    try:
        importlib.import_module("cdc_phai_receiver")
    except Exception:
        print("")
        print("  ⚠ 수신기 모듈(cdc_phai_receiver)을 못 불러 **실시간 디코딩 경로는")
        print("    검사되지 않았다.** GUI 를 뺀 빌드(--no-gui)이거나 PyQt5 가 없다.")
        print("    전체를 검사하려면 소스에서:  python run_tests.py")

    if bad:
        print("  실패: " + ", ".join(r[0] for r in bad))
        return 1
    return 0


def _run_suites(verbose: bool, scratch: str) -> list:
    results = []
    for label, module_name, func_name, args in SELFTEST_SUITES:
        if args and args[-1] == "--out":       # 산출물은 임시 디렉토리로
            args = tuple(args) + (os.path.join(scratch, module_name),)
        sys.stdout.write("  %-32s " % label)
        sys.stdout.flush()
        buf = None
        if not verbose:                     # 통과한 항목의 수다한 출력은 삼킨다
            import io
            buf = io.StringIO()
            real, sys.stdout = sys.stdout, buf
        try:
            rc = _delegate(module_name, func_name, args)
            err = None
        except BaseException as e:          # AssertionError 포함 — 러너가 죽으면 안 된다
            rc, err = 1, "%s: %s" % (type(e).__name__, e)
        finally:
            if buf is not None:
                sys.stdout = real
        ok = (rc == 0)
        results.append((label, ok, err, buf.getvalue() if buf else ""))
        print("PASS" if ok else "FAIL")
        if not ok and buf is not None:
            tail = [ln for ln in buf.getvalue().splitlines() if ln.strip()][-12:]
            for ln in tail:
                print("      | " + ln)
            if err:
                print("      | " + err)
    return results


def main() -> int:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")

    argv = sys.argv[1:]
    if not argv or argv[0] in ("-h", "--help", "help"):
        print(_usage())
        return 0
    if argv[0] in ("-V", "--version", "version"):
        print("xm10 %s" % VERSION)
        return 0

    cmd, rest = argv[0], argv[1:]
    if cmd not in COMMANDS:
        print("모르는 명령: %r" % cmd)
        print("")
        print(_usage())
        return 2

    if cmd == "ports":
        return cmd_ports(rest)
    if cmd == "selftest":
        return cmd_selftest(rest)

    module_name, func_name, _desc = COMMANDS[cmd]
    try:
        return _delegate(module_name, func_name, rest)
    except ImportError as e:
        print("%s 를 부를 수 없다: %s" % (cmd, e))
        if cmd == "recv":
            print("  GUI 는 PyQt5 와 pyqtgraph 가 필요하다:  pip install PyQt5 pyqtgraph")
        return 2
    except KeyboardInterrupt:
        print("")
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
