#!/usr/bin/env python3
"""`xm10.exe` 를 만든다 — 파이썬이 없는 PC 에서도 쓰게.

    python build_exe.py                 GUI 포함 (권장, 크다)
    python build_exe.py --no-gui        콘솔 전용 슬림 빌드
    python build_exe.py --verify-only   이미 만든 exe 만 검증

만들고 나면 **그 exe 를 실제로 돌려서** 검증한다. 이 프로젝트에서 "빌드가 됐다"는
"동작한다"가 아니다 — 실제로 import 하나가 통째로 빠진 채 단위시험 전부와 `--help`
까지 통과한 적이 있다(2026-09-10). PyInstaller 는 그 결함을 더 잘 숨긴다: 동적으로
import 하는 모듈은 정적 분석에 안 잡혀서, 번들에 안 들어가도 빌드는 성공한다.
그래서 검증은 `xm10.exe selftest` + `xm10.exe demo` 를 **exe 로** 돌리는 것이다.

왜 hidden-import 를 손으로 적나
--------------------------------
`xm10.py` 는 하위 명령을 `importlib.import_module()` 로 부른다. PyInstaller 는 그걸
따라가지 못한다. 목록을 여기 두고, 아래 `_check_coverage()` 가 CDC 디렉토리와 대조해
**빠진 모듈이 있으면 빌드를 거부한다** — 목록이 조용히 뒤처지는 것을 막는다.
"""
from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys


HERE = os.path.dirname(os.path.abspath(__file__))
CDC = os.path.join(HERE, "CDC")
DIST = os.path.join(HERE, "dist")
WORK = os.path.join(HERE, "build")
EXE = os.path.join(DIST, "xm10.exe" if os.name == "nt" else "xm10")
BUILD_TIMEOUT_S = 15 * 60       # 정상 GUI 빌드의 몇 배. 이걸 넘기면 진행이 아니라 정지다.

# 동적으로 import 되는 것 전부. GUI 는 따로 뺀다(--no-gui).
HIDDEN_CORE = [
    "demo_run", "demo_stream",
    "frame_router", "schema_0xee", "schema_registry",
    "soak", "total_data_decoder", "xm_total_data_map",
    "xmlog", "xmlog_capture", "xmlog_export",
    "test_frame_router", "test_demo_stream", "test_golden_vectors",
    "test_schema", "test_total_data_decoder", "test_xmlog", "test_xmlog_chain",
]
HIDDEN_GUI = ["cdc_phai_receiver", "cdc_csv_reviewer"]

# 번들에 넣을 데이터 (원본, 번들 안 상대경로)
DATA = [
    (os.path.join(HERE, "spec", "golden"), os.path.join("spec", "golden")),
    (os.path.join(CDC, "data"), "data"),
]

# 커버리지 검사에서 빼는 파일 — 진입점이거나 위 목록에 포함될 이유가 없는 것.
COVERAGE_EXEMPT = {"__init__.py"}

# 이 앱이 쓰지 않는데 개발 PC 에 깔려 있기 쉬운 무거운 패키지. PyInstaller 는 **선택적
# import 경로까지** 정적으로 따라가므로, 빼지 않으면 그래프에 딸려 들어온다.
#
# 2026-09-10 실측: GUI 빌드가 30분 동안 멈췄다. 훅을 돌리는 격리 서브프로세스가
# `torch/__init__.py` 의 DLL 로딩에서 **네이티브 크래시**했고, WerFault 가 그 프로세스를
# 붙잡고 있어 부모가 영원히 기다렸다. 앱은 torch 를 import 하지 않는다 — 분석기가 어딘가의
# `try: import torch` 를 따라간 것이다. 빌드 시간·크기 문제가 아니라 **빌드가 끝나지 않는**
# 문제라 제외가 필수다.
EXCLUDE_ALWAYS = [
    "torch", "torchvision", "torchaudio", "tensorflow", "jax", "onnxruntime",
    "scipy", "pandas", "matplotlib", "numba", "llvmlite", "sympy", "h5py",
    "PIL", "cv2", "sklearn", "IPython", "jupyter", "notebook", "tkinter",
]
# Qt 는 **하나만** 실어야 한다. 수신기는 PyQt5 를 쓰는데 pyqtgraph 는 단독으로 import
# 되면 PySide6(Qt 6) 를 먼저 고른다 — 한 번들에 Qt5 와 Qt6 가 같이 실리면 실행할 때 죽는다.
EXCLUDE_OTHER_QT = ["PySide6", "shiboken6", "PySide2", "shiboken2", "PyQt6"]
EXCLUDE_ALL_QT = EXCLUDE_OTHER_QT + ["PyQt5", "pyqtgraph"]


def _check_coverage(with_gui: bool) -> list:
    """CDC 안의 모든 `.py` 가 hidden-import 목록에 있는지. 없으면 이름을 돌려준다."""
    listed = set(HIDDEN_CORE) | (set(HIDDEN_GUI) if with_gui else set())
    if not with_gui:
        listed |= set(HIDDEN_GUI)      # 일부러 뺀 것이므로 누락으로 세지 않는다
    missing = []
    for f in sorted(os.listdir(CDC)):
        if not f.endswith(".py") or f in COVERAGE_EXEMPT:
            continue
        name = f[:-3]
        if name not in listed:
            missing.append(name)
    return missing


def build(with_gui: bool, clean: bool) -> int:
    missing = _check_coverage(with_gui)
    if missing:
        print("hidden-import 목록이 CDC 와 어긋난다 — 빌드하지 않는다.")
        print("  목록에 없는 모듈: %s" % ", ".join(missing))
        print("  build_exe.py 의 HIDDEN_CORE 에 추가하거나, 넣지 않을 이유를")
        print("  COVERAGE_EXEMPT 에 적는다. 조용히 빠지면 exe 에서만 죽는다.")
        return 2

    hidden = list(HIDDEN_CORE) + (HIDDEN_GUI if with_gui else [])
    argv = [sys.executable, "-m", "PyInstaller",
            "--onefile", "--console", "--noconfirm",
            "--name", "xm10",
            "--distpath", DIST, "--workpath", WORK,
            "--specpath", WORK,
            "--paths", CDC]
    if clean:
        argv.append("--clean")
    for m in hidden:
        argv += ["--hidden-import", m]
    for m in EXCLUDE_ALWAYS + (EXCLUDE_OTHER_QT if with_gui else EXCLUDE_ALL_QT):
        argv += ["--exclude-module", m]
    for src, dest in DATA:
        if os.path.isdir(src):
            argv += ["--add-data", "%s%s%s" % (src, os.pathsep, dest)]
        else:
            print("  (없어서 건너뜀: %s)" % src)
    argv.append(os.path.join(HERE, "xm10.py"))

    # pyqtgraph 의 바인딩 선택을 못박는다. 훅이 pyqtgraph 를 격리 프로세스에서 단독 import
    # 하면 PySide6 가 깔린 PC 에서는 그쪽을 고른다 — 위 EXCLUDE 와 이 변수가 함께 있어야
    # 번들 안 Qt 가 하나로 정리된다.
    env = dict(os.environ)
    env["PYQTGRAPH_QT_LIB"] = "PyQt5"

    print("PyInstaller 실행 — GUI %s · 제외 %d개 패키지"
          % ("포함" if with_gui else "제외",
             len(EXCLUDE_ALWAYS) + len(EXCLUDE_OTHER_QT if with_gui else EXCLUDE_ALL_QT)))
    # 정상 빌드는 GUI 포함이라도 몇 분이다. 훅 서브프로세스가 네이티브 크래시하면
    # WerFault 가 붙잡아 **영원히** 기다리게 되므로, 시간을 걸어 두고 걸리면 어디를 볼지 말한다.
    try:
        r = subprocess.run(argv, env=env, timeout=BUILD_TIMEOUT_S)
    except subprocess.TimeoutExpired:
        print("")
        print("PyInstaller 가 %d초 안에 끝나지 않았다 — 멈춘 것이다." % BUILD_TIMEOUT_S)
        print("  흔한 원인: 훅 격리 서브프로세스가 어떤 패키지의 네이티브 DLL 을 import 하다")
        print("  죽고 WerFault.exe 가 붙잡고 있는 상태. 작업 관리자에서 WerFault.exe 의 -p 뒤")
        print("  PID 가 python 이면 그것이다. 그 패키지를 EXCLUDE_ALWAYS 에 넣는다.")
        return 3
    if r.returncode != 0:
        print("PyInstaller 실패 (rc=%d)" % r.returncode)
        return r.returncode
    if not os.path.exists(EXE):
        print("빌드는 끝났는데 %s 가 없다" % EXE)
        return 1
    print("")
    print("만들어짐: %s  (%.1f MB)" % (EXE, os.path.getsize(EXE) / 1e6))
    return 0


def verify() -> int:
    """만들어진 exe 를 **실제로 돌린다.** 여기가 이 스크립트의 요점이다."""
    if not os.path.exists(EXE):
        print("검증할 exe 가 없다: %s" % EXE)
        return 2

    import tempfile
    scratch = tempfile.mkdtemp(prefix="xm10_verify_")
    steps = [
        ("도움말",       [EXE], 0),
        ("포트 나열",     [EXE, "ports"], None),      # 포트 유무는 환경 문제라 rc 무시
        ("자체 검증",     [EXE, "selftest"], 0),
        ("데모 전 구간",  [EXE, "demo", "--out", os.path.join(scratch, "demo")], 0),
        ("데모 산출물 읽기", [EXE, "export",
                          os.path.join(scratch, "demo", "demo.xmlog")], 0),
    ]
    bad = []
    try:
        for label, argv, want in steps:
            r = subprocess.run(argv, capture_output=True, text=True,
                               encoding="utf-8", errors="replace")
            ok = (want is None) or (r.returncode == want)
            print("  %-16s %s (rc=%d)" % (label, "PASS" if ok else "FAIL", r.returncode))
            if not ok:
                bad.append(label)
                for ln in ((r.stdout or "") + (r.stderr or "")).strip().splitlines()[-15:]:
                    print("      | " + ln)
        # `recv -h` 는 창을 띄우지 않지만 수신기 모듈을 **import 한다** — PyQt5 와
        # pyqtgraph 가 번들에 제대로 들어갔는지는 그 import 에서만 드러난다. Qt 플러그인이
        # 빠진 번들은 빌드가 성공하고 실행할 때 죽는 전형적인 실패다.
        # GUI 를 뺀 빌드에서는 실패가 정상이므로, rc 로 **어느 빌드인지 판별**해 보고한다.
        r = subprocess.run([EXE, "recv", "-h"], capture_output=True, text=True,
                           encoding="utf-8", errors="replace")
        if r.returncode == 0:
            print("  %-16s PASS (GUI 포함 — PyQt5/pyqtgraph 적재 확인)" % "GUI 모듈 적재")
        else:
            print("  %-16s 없음 (콘솔 전용 빌드 — recv 의 그래프 창은 못 쓴다)"
                  % "GUI 모듈")
            print("      | " + ((r.stdout or "") + (r.stderr or "")).strip()
                  .splitlines()[0][:100])
    finally:
        shutil.rmtree(scratch, ignore_errors=True)

    print("")
    if bad:
        print("exe 검증 실패: %s" % ", ".join(bad))
        return 1
    print("exe 검증 통과 — 파이썬 없이 도는 것을 실제로 확인했다.")
    print("배포할 파일: %s" % EXE)
    return 0


def main() -> int:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    ap = argparse.ArgumentParser(description="xm10 실행파일 빌드 + 검증")
    ap.add_argument("--no-gui", action="store_true",
                    help="Qt 를 빼고 콘솔 전용으로 (recv 의 그래프 창은 못 쓴다)")
    ap.add_argument("--clean", action="store_true", help="PyInstaller 캐시부터 지우고")
    ap.add_argument("--verify-only", action="store_true", help="빌드 없이 검증만")
    a = ap.parse_args()

    try:
        import PyInstaller  # noqa: F401
    except ImportError:
        print("PyInstaller 가 없다:  pip install pyinstaller")
        return 2

    if not a.verify_only:
        rc = build(with_gui=not a.no_gui, clean=a.clean)
        if rc:
            return rc
    print("")
    print("만든 exe 를 실제로 돌려 본다:")
    return verify()


if __name__ == "__main__":
    raise SystemExit(main())
