#!/usr/bin/env python3
"""pc-data-tool 전체 검증 — 보드도 시리얼 포트도 없이 돈다.

    python run_tests.py

무엇이 도는가
-------------
  frame_router          와이어 파싱·시퀀스 회계
  total_data_decoder    0x20 디코더 단독 자기검사
  test_total_data_decoder   와이어 -> 0x20 디코드
  test_xmlog            .xmlog 바이트 ABI (손으로 적은 골든 바이트)
  test_xmlog_chain      와이어 -> .xmlog -> CSV 전 구간
  test_xmlog_activation 스키마(activation)의 수명 — 발급 · 참조 · 재연결 · 시점별 디코딩
  test_gui_tabs         실제 창(오프스크린 Qt)에 합성 스트림 — 탭·CSV·0x20 표 (PyQt5 필요)
  undefined-name 게이트  pyflakes (있으면)

SKIP 은 PASS 가 아니다
----------------------
시험 파일이 종료코드 77 로 끝나면 "이 환경에서는 못 돌렸다" 는 뜻이다(automake 의 관례).
GUI 시험이 PyQt5 없이 조용히 0 으로 끝나면 "창을 검사했다" 로 읽히는데 실제로는 아무것도
안 돈 것이다 — 그래서 SKIP 으로 따로 세고, 마지막 줄에 통과 수와 나란히 적는다.

왜 undefined-name 게이트가 따로 있나
------------------------------------
2026-09-10 에 `--total-data` 경로가 **import 문이 통째로 빠진 채** 머지 직전까지 갔다.
`--help` 는 통과했다 — 그 이름들은 `run_cli()` 안에서만 쓰이는데 `--help` 는 거기까지
가지 않기 때문이다. 단위 시험도 전부 통과했다 — 그 파일을 안 지나가기 때문이다.

"시험이 통과했다" 가 "실행된다" 를 뜻하지 않는 구간이 실재한다. 정적 검사가 그 구간을
덮는다. pyflakes 가 없으면 이 게이트는 **건너뛴다고 크게 적고** 통과시킨다 —
조용히 넘어가면 없는 것과 같다.
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CDC = os.path.join(HERE, "CDC")

# 시험 파일이 이 종료코드로 끝나면 SKIP 이다 (0 = 통과, 그 밖 = 실패).
SKIP_RC = 77

SUITES = [
    ("frame_router (wire parsing)", "test_frame_router.py"),
    ("total_data_decoder self-test", "total_data_decoder.py"),
    ("wire -> 0x20 decode", "test_total_data_decoder.py"),
    ("schema 0xEE/0xEF/registry", "test_schema.py"),
    ("golden vectors (cross-impl)", "test_golden_vectors.py"),
    ("xmlog byte ABI", "test_xmlog.py"),
    ("wire -> xmlog -> CSV", "test_xmlog_chain.py"),
    ("xmlog schema (activation) lifecycle", "test_xmlog_activation.py"),
    ("soak accounting self-test", "soak.py --selftest"),
    ("demo stream builders", "demo_stream.py"),
    ("demo stream vs test encoder", "test_demo_stream.py"),
    # 데모는 전 구간을 한 번 더 밟는다 — 조각 시험이 다 통과해도 이어 붙이면
    # 틀리는 자리가 있다. `--out` 은 임시 디렉토리로 준다(_run 이 채운다).
    ("end-to-end demo", "demo_run.py --quiet --out {tmp}"),
    # 실제 MainWindow 를 오프스크린 Qt 로 띄워 탭·CSV·0x20 표를 확인한다. PyQt5 가 없으면
    # 종료코드 77 로 끝나 아래에서 SKIP 으로 집계된다.
    ("gui: tabs / CSV / 0x20 table (offscreen Qt)", "test_gui_tabs.py"),
]

# 정적 검사 대상 — 실행 경로가 얕아 시험이 못 덮는 파일들
LINT_TARGETS = [
    "cdc_phai_receiver.py",
    "schema_0xee.py",
    "schema_registry.py",
    "soak.py",
    "cdc_csv_reviewer.py",
    "frame_router.py",
    "total_data_decoder.py",
    "xmlog.py",
    "xmlog_capture.py",
    "xmlog_export.py",
    "demo_stream.py",
    "demo_run.py",
    "test_gui_tabs.py",
]


def _run(script, tmp):
    # 항목이 "파일 --플래그" 형태일 수 있다 (soak 은 --selftest 가 있어야 보드 없이 돈다).
    # `{tmp}` 는 산출물을 남기는 항목이 작업 디렉토리를 더럽히지 않게 하는 자리다.
    argv = [a.format(tmp=tmp) for a in script.split()]
    return subprocess.run([sys.executable] + argv, cwd=CDC,
                          capture_output=True, text=True,
                          encoding="utf-8", errors="replace")


def lint_undefined_names():
    """반환: (상태, 줄들). 상태 = 'ok' | 'fail' | 'skip'."""
    try:
        import pyflakes  # noqa: F401
    except ImportError:
        return "skip", ["pyflakes 없음 — pip install pyflakes"]

    r = subprocess.run([sys.executable, "-m", "pyflakes"] + LINT_TARGETS,
                       cwd=CDC, capture_output=True, text=True,
                       encoding="utf-8", errors="replace")
    out = (r.stdout or "") + (r.stderr or "")
    # 미사용 import 는 기존 잡음이라 게이트로 쓰지 않는다. 잡으려는 건
    # "이름이 어디에도 정의되지 않음"·구문 오류처럼 **실행되면 죽는** 것들이다.
    fatal = [ln for ln in out.splitlines()
             if ln.strip() and ("undefined name" in ln
                                or "expected" in ln
                                or "invalid syntax" in ln
                                or "syntax error" in ln.lower())]
    if fatal:
        return "fail", fatal
    noise = [ln for ln in out.splitlines() if ln.strip()]
    return "ok", noise


def main():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")

    print("pc-data-tool 검증 — 보드 없이")
    print("")

    failed = []
    skipped = []

    status, lines = lint_undefined_names()
    if status == "ok":
        print("  PASS  undefined-name 게이트 (pyflakes)")
        if lines:
            print("        참고: 치명적이지 않은 경고 %d 건 (미사용 import 등)" % len(lines))
    elif status == "skip":
        skipped.append("undefined-name 게이트")
        print("  SKIP  undefined-name 게이트 — %s" % lines[0])
        print("        ⚠ 이 게이트가 없으면 '시험은 통과하는데 실행하면 죽는' 결함을 못 잡는다")
    else:
        failed.append("undefined-name 게이트")
        print("  FAIL  undefined-name 게이트")
        for ln in lines:
            print("        " + ln)

    import shutil
    import tempfile
    tmp = tempfile.mkdtemp(prefix="xm10_run_tests_")

    for label, script in SUITES:
        path = os.path.join(CDC, script.split()[0])
        if not os.path.exists(path):
            failed.append(label)
            print("  FAIL  %s — 파일 없음 (%s)" % (label, script))
            continue
        r = _run(script, tmp)
        tail = ((r.stdout or "") + (r.stderr or "")).strip().splitlines()
        if r.returncode == 0:
            print("  PASS  %s" % label)
        elif r.returncode == SKIP_RC:
            skipped.append(label)
            print("  SKIP  %s" % label)
            if tail:
                print("        " + tail[-1])
        else:
            failed.append(label)
            print("  FAIL  %s  (rc=%d)" % (label, r.returncode))
            for ln in tail[-12:]:
                print("        " + ln)

    shutil.rmtree(tmp, ignore_errors=True)

    print("")
    total = len(SUITES) + 1
    passed = total - len(failed) - len(skipped)
    if failed:
        print("%d/%d FAILED — %s" % (len(failed), total, ", ".join(failed)))
        return 1
    if skipped:
        print("%d/%d passed, %d SKIP — %s" % (passed, total, len(skipped), ", ".join(skipped)))
        print("(SKIP 은 통과가 아니다: 그 부분은 이 환경에서 검사되지 않았다)")
    else:
        print("%d/%d passed" % (passed, total))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
