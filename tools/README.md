# Release Packaging Tools

> 유지보수자용 문서입니다. 이 폴더의 스크립트는 배포 ZIP 에 포함되지 않습니다.
> 학생이 받는 ZIP 안의 `tools/` 는 SDK 트리의 빌드 스크립트로, 여기와 별개입니다.

---

## `package_release.py` — SDK ZIP 생성

`XM10_SDK/Rev*/Extension_Module/` 의 SDK 트리와 레포 루트의 학습 자산을 합쳐
`Rev1.1.zip` / `Rev2.0.zip` 을 만듭니다. 압축을 풀면 그 폴더가 그대로
STM32CubeIDE Import root 이자 Claude Code 진입 폴더가 됩니다.

```bash
python tools/package_release.py --version 2.6.0 --rev all    # 양 Rev (기본)
python tools/package_release.py --version 2.6.0 --rev 2.0    # 한 쪽만
python tools/package_release.py --version 2.6.0 --rev all --out-dir C:/releases
```

`--version` 은 완료 안내 문구에만 쓰입니다. ZIP 안의 `version.h` 는 패키징 **전에**
양 Rev 모두 갱신되어 있어야 합니다.

### ZIP 구성

```
Rev2.0.zip  →  Extension_Module/
├── XM_FW/            공개 헤더 + libXM_Lib.a + boot_fw_info.c
├── XM_Apps/          사용자 코드 자리 (Control_Task/)
├── Examples/         예제 — Rev1.1 = 42개, Rev2.0 = 45개
├── Core/ Drivers/ Compatible/ FATFS/ Middlewares/ CMSIS/
├── tools/build/      pre/post-build 스크립트 (size_report, version_generator,
│                     patch_fw_info, fw_packager, patch_cubemx_overrides)
├── tools/codegen/    Total Data 맵 생성기 + xm_total_data.yaml
├── .project .cproject *.ld startup_*.s CMakeLists.txt Extension_Module.launch
├── docs/             학습 문서
├── .claude/          Claude Code 진입점 (student-onboard, example-helper)
└── CLAUDE.md AGENTS.md README.md CHANGELOG.md LICENSE
```

Rev2.0 에는 `LWIP/` (Ethernet) 이 추가로 들어갑니다.

### 알아둘 동작 두 가지

**git 추적 파일만 담습니다.** `git add` 되지 않은 파일은 경고 없이 ZIP 에서 빠집니다.
헤더나 소스를 새로 추가한 릴리스라면 패키징 전에 `git status` 로 확인하세요.
추적되지 않은 개인 설정(`.claude/settings.local.json`, `.cache/` 등)이 섞여 들어가지
않는 것도 같은 이유입니다.

**루트의 소문자 `examples/` 는 일부러 제외합니다.** SDK 트리에 이미 Rev 별로 정합된
`Examples/`(대문자)가 있고, Windows 는 대소문자를 구분하지 않아 두 폴더가 병합되면
한쪽 Rev 의 예제가 다른 Rev ZIP 으로 넘어갑니다. 루트 `examples/` 는 GitHub 웹에서
읽는 용도입니다.

### 자체 검증

ZIP 생성 직후 `.project`, `CLAUDE.md`, `XM_FW/libXM_Lib.a` 등 핵심 항목의 존재만
확인하고 `[OK] all required entries present` 를 출력합니다. 최소 확인이므로,
실제 출하 판정은 아래 심층 검증으로 합니다.

---

## 심층 검증 — `verify_release_zip.py`

개발 레포(`phai-x1-xm10-develop`)에 있습니다. 패키징 스크립트와 독립적으로 ZIP 을 열어
검사하며, **양 Rev PASS 가 릴리스 조건**입니다.

```bash
python <개발레포>/tools/verify_release_zip.py --version 2.6.0
```

릴리스 레포 경로는 개발 레포의 형제 폴더로 자동 해석되므로 보통 그대로 실행하면 됩니다.

검사 항목: 금지 엔트리 유입, `libXM_Lib.a` 해시 정합, `version.h` 버전 일치,
예제 개수·Rev 정합, `.launch` 경로 스크럽, 코드젠 산출물 정합, 개발 경로 유출,
필수 파일 존재.

지금까지 이 게이트가 잡아낸 것: 미추적 헤더 누락, 대소문자 병합으로 인한 예제 혼입,
동기화 누락으로 인한 헤더 회귀. 통과했다고 동작이 보장되지는 않으므로 하드웨어 검증은
별도로 합니다.

---

## 릴리스 업로드

```bash
# 1. ZIP 생성 + 검증
python tools/package_release.py --version X.Y.Z --rev all
python <개발레포>/tools/verify_release_zip.py --version X.Y.Z

# 2. 릴리스 생성 (본문 = 릴리스 노트)
gh release create vX.Y.Z Rev1.1.zip Rev2.0.zip \
  --target Develop --title "vX.Y.Z" --notes-file docs/release-notes/vX.Y.Z.md

# 3. 부트로더 / KIT H10 펌웨어 / 컨텐츠 파일 첨부
gh release upload vX.Y.Z AGR_Bootloader.bin \
  SUIT_H10_Binary_<날짜>.zip SUIT_ContentsFiles_<날짜>.zip

# 4. 이미 공개된 자산을 교체할 때
gh release upload vX.Y.Z Rev1.1.zip Rev2.0.zip --clobber
```

`--target` 에 짧은 SHA 를 넣으면 거부됩니다(`422 target_commitish is invalid`).
브랜치명을 쓰세요.

전체 릴리스 절차(라이브러리 재빌드, 심볼 게이트, 예제 전수 빌드, dev→release 동기화,
`version.h` 갱신, 양 빌드시스템 클린 빌드)는 개발 레포의 릴리스 규칙 문서를 따릅니다.
여기 적힌 것은 패키징·업로드 구간뿐입니다.

---

## `package_release.ps1` — 사용하지 마세요

`package_release.py` 이전에 쓰던 PowerShell 판입니다. 현재 다음 두 가지가 어긋나
있어 그대로 실행하면 잘못된 ZIP 이 나옵니다.

- 루트의 소문자 `examples/` 를 복사합니다 → Rev 간 예제 혼입.
- `git ls-files` 가 아니라 디스크 트리를 통째로 복사합니다 → 추적되지 않은 개인 설정·캐시 유입.

히스토리 참조용으로만 남겨둔 파일입니다.
