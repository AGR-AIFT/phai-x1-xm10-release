# Release Packaging Tools

> 이 폴더의 스크립트는 배포 ZIP 에 포함되지 않습니다.
> 학생이 받는 ZIP 안의 `tools/` 는 SDK 트리의 빌드 스크립트로, 여기와 별개입니다.

---

## `package_release.py` — SDK ZIP 생성

`XM10_SDK/Rev*/Extension_Module/` 의 SDK 트리와 레포 루트의 학습 자산을 합쳐
`Rev1.1.zip` / `Rev2.0.zip` 을 만듭니다. 압축을 풀면 그 폴더가 그대로
STM32CubeIDE Import root 이자 Claude Code 진입 폴더가 됩니다.

```bash
python tools/package_release.py --version X.Y.Z --rev all    # 양 Rev (기본)
python tools/package_release.py --version X.Y.Z --rev 2.0    # 한 쪽만
python tools/package_release.py --version X.Y.Z --rev all --out-dir C:/releases
```

`--version` 은 완료 안내 문구에만 쓰입니다. ZIP 안의 `version.h` 는 패키징 **전에**
양 Rev 모두 갱신되어 있어야 합니다.

### ZIP 구성

```
Rev2.0.zip  →  Extension_Module/
├── XM_FW/            공개 헤더 + libXM_Lib.a + boot_fw_info.c
├── XM_Apps/          사용자 코드 자리 (Control_Task/)
├── Examples/         예제 — Rev1.1 = 40개, Rev2.0 = 45개
├── Core/ Drivers/ Compatible/ FATFS/ Middlewares/ CMSIS/
├── tools/            빌드 보조 스크립트
├── .project .cproject *.ld startup_*.s CMakeLists.txt Extension_Module.launch
├── docs/             학습 문서
├── pc-data-tool/     xm10 PC 도구 (두 Rev 공통)
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

---

`package_release.ps1` 은 예전 PowerShell 판이라 쓰지 않습니다. `package_release.py` 를 사용하세요.
