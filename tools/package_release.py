#!/usr/bin/env python3
"""
XM10 Extension Module SDK release ZIP packager (Option B 평탄 구조).

PowerShell 변형 (tools/package_release.ps1) 의 Python 포트.
사용:
    python tools/package_release.py --version 2.2.1 --rev all
"""
from __future__ import annotations

import argparse
import os
import shutil
import sys
import tempfile
import zipfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

# robocopy /XD 와 동일 — staging 시 제외할 디렉토리
EXCLUDE_DIRS = {
    "Debug", "Release", "build", "build_cmake", "build_check",
    "build_rev1.1", "build_rev2.0", ".settings",
}

# ZIP root 의 Extension_Module/ 에 직접 들어갈 top-level 자산 (레포 루트 기준)
TOP_LEVEL_ASSETS = [
    "docs", "examples", ".claude",
    "AGENTS.md", "README.md", "CHANGELOG.md", "LICENSE",
]

# ZIP 검증 — 핵심 항목 (path prefix)
REQUIRED = [
    ".project",
    "CLAUDE.md",
    "AGENTS.md",
    ".claude/skills/student-onboard",
    "docs/getting-started",
    "Examples/00_Quick_Start/README.md",
    "XM_FW/libXM_Lib.a",
]


def copy_tree_filtered(src: Path, dst: Path) -> int:
    """src → dst 복사, EXCLUDE_DIRS 제외. 복사한 파일 수 반환."""
    count = 0
    for root, dirs, files in os.walk(src):
        # in-place modify dirs 로 os.walk 가 제외 디렉토리 descend 안 함
        dirs[:] = [d for d in dirs if d not in EXCLUDE_DIRS]
        rel = Path(root).relative_to(src)
        for f in files:
            src_f = Path(root) / f
            dst_f = dst / rel / f
            dst_f.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src_f, dst_f)
            count += 1
    return count


def zip_directory(src: Path, zip_path: Path) -> None:
    """src 폴더의 내용을 zip_path 로 압축 (src 자체는 ZIP root 의 하위 폴더로 들어감)."""
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as zf:
        for root, dirs, files in os.walk(src):
            for f in files:
                full = Path(root) / f
                arcname = full.relative_to(src.parent)  # ZIP 안에서 Extension_Module/ 로 시작
                zf.write(full, arcname)


def verify_zip(zip_path: Path) -> list[str]:
    """REQUIRED 항목이 ZIP 안에 존재하는지 검증. missing 항목 리스트 반환."""
    with zipfile.ZipFile(zip_path) as zf:
        entries = [name.replace("\\", "/") for name in zf.namelist()]
    missing = []
    for req in REQUIRED:
        if not any(req in e for e in entries):
            missing.append(req)
    return missing


def package_one_rev(rev_tag: str, version: str, out_dir: Path) -> None:
    sdk_src = REPO_ROOT / "XM10_SDK" / f"Rev{rev_tag}" / "Extension_Module"
    if not sdk_src.is_dir():
        raise RuntimeError(f"SDK source not found: {sdk_src}")

    zip_path = out_dir / f"Rev{rev_tag}.zip"
    if zip_path.exists():
        print(f"  Removing existing {zip_path}")
        zip_path.unlink()

    with tempfile.TemporaryDirectory(prefix="xm10_pkg_") as stage_root:
        stage = Path(stage_root) / "Extension_Module"
        stage.mkdir(parents=True, exist_ok=True)

        print(f"[Rev{rev_tag}] Staging at {stage}")

        # 1) SDK 코드 — XM10_SDK/Rev*/Extension_Module/* → ZIP root 의 Extension_Module/
        print("  (1/3) SDK code")
        n = copy_tree_filtered(sdk_src, stage)
        print(f"        copied {n} files")

        # 2) 학습 자산 — 레포 루트의 docs, examples, .claude, *.md → ZIP root 의 Extension_Module/
        print("  (2/3) docs + examples + .claude + AGENTS.md + README.md + CHANGELOG.md + LICENSE")
        for asset in TOP_LEVEL_ASSETS:
            src = REPO_ROOT / asset
            if src.is_dir():
                shutil.copytree(src, stage / asset, dirs_exist_ok=True,
                                ignore=shutil.ignore_patterns(*EXCLUDE_DIRS))
            elif src.is_file():
                shutil.copy2(src, stage / asset)
            else:
                print(f"    (skip — not found: {asset})")

        # 3) ZIP 생성
        print(f"  (3/3) Compress → {zip_path}")
        zip_directory(stage, zip_path)

    # 검증
    print("  Verifying ZIP contents...")
    missing = verify_zip(zip_path)
    if missing:
        print("    [WARN] missing in ZIP:")
        for m in missing:
            print(f"      - {m}")
    else:
        print("    [OK] all required entries present")

    size_mb = zip_path.stat().st_size / (1024 * 1024)
    print(f"[Rev{rev_tag}] DONE — {zip_path}  ({size_mb:.1f} MB)\n")


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--version", default="", help="Release version (e.g. 2.2.1)")
    p.add_argument("--rev", choices=["1.1", "2.0", "all"], default="all")
    p.add_argument("--out-dir", default=str(REPO_ROOT))
    args = p.parse_args()

    out_dir = Path(args.out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    print("=== XM10 SDK Release Packager (Python) ===")
    print(f"Version : {args.version or '(auto from git tag)'}")
    print(f"Repo    : {REPO_ROOT}")
    print(f"Out dir : {out_dir}\n")

    revs = ["1.1", "2.0"] if args.rev == "all" else [args.rev]
    for r in revs:
        package_one_rev(r, args.version, out_dir)

    print("=== Packaging complete ===")
    print("Next steps:")
    print(f"  1. gh release create v{args.version} Rev1.1.zip Rev2.0.zip")
    print("     --title 'v{0}' --notes-file docs/release-notes/v{0}.md".format(args.version))
    print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
