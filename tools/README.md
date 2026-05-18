# Release Packaging Tools

## `package_release.ps1` — SDK ZIP 생성

학생이 ZIP 한 번 풀면 그 폴더가 곧 STM32CubeIDE Import root + Claude Code 진입 폴더가 되도록
**Option B 평탄 구조**로 `Rev1.1.zip` / `Rev2.0.zip` 을 생성합니다.

### 사용

```powershell
# 양 Rev 동시 생성 (가장 흔한 케이스)
.\tools\package_release.ps1

# 특정 Rev 만
.\tools\package_release.ps1 -Rev 1.1
.\tools\package_release.ps1 -Rev 2.0

# 다른 출력 경로
.\tools\package_release.ps1 -OutDir C:\releases
```

### ZIP 안에 들어가는 항목

```
Rev1.1.zip (압축 풀면) → Extension_Module/
├── 🔧 SDK 코드   ← XM10_SDK/Rev1.1/Extension_Module/* 평탄화
│   ├── XM_FW/ (헤더 + libXM_Lib.a + boot_fw_info.c)
│   ├── XM_Apps/Control_Task/ (학생 코드 자리)
│   ├── Core/, Drivers/, Compatible/, FATFS/, Middlewares/
│   ├── .project, .cproject, *.ld, startup_*.s
│   └── CMakeLists.txt
│
├── 📖 docs/      ← 학습 문서 (release repo top-level 사본)
├── 🎓 examples/  ← 51 README + 47 .c (예제별 학습 가이드)
├── 🤖 .claude/   ← Claude Code 진입점 (student-onboard, example-helper)
├── 📌 CLAUDE.md  (Rev 특화 — 1.1 또는 2.0)
├── 📌 AGENTS.md  (Codex 진입점)
├── 📌 README.md, CHANGELOG.md, LICENSE
```

Rev 2.0 추가 항목: LWIP 미들웨어 (Ethernet 지원)

### 자동 검증

스크립트가 ZIP 생성 후 다음을 확인합니다:
- `.project` (CubeIDE Import 가능?)
- `CLAUDE.md`, `AGENTS.md` (AI 진입점)
- `.claude/skills/student-onboard/`
- `docs/getting-started/`
- `examples/00_Quick_Start/README.md`
- `XM_FW/libXM_Lib.a`

누락 시 `[WARN]` 출력 — 모두 OK 시 `[OK] all required entries present`.

### 권장 업로드 흐름 (v2.2.0 기준)

```powershell
# 1. ZIP 생성
.\tools\package_release.ps1

# 2. Draft release 에 ZIP + BIN 자산 업로드
gh release upload v2.2.0 Rev1.1.zip Rev2.0.zip
gh release upload v2.2.0 AGR_Bootloader.bin
gh release upload v2.2.0 SUIT_CM_APP_2_3_0.bin SUIT_SAM10_APP_2_3_0.bin SUIT_ESP32_FW_2_3_0.bin

# 3. (옵션) ContentsFiles 가 갱신됐다면
gh release upload v2.2.0 ContentsFiles_20260515.zip

# 4. Draft → 정식 공개
gh release edit v2.2.0 --draft=false
```

### v2.1.1 와의 차이

| 항목 | v2.1.1 (옵션 A) | v2.2.0+ (옵션 A 패키징 / 옵션 B 구조) |
|---|---|---|
| ZIP root | `XM10_SDK/Rev*/Extension_Module/...` 깊은 경로 | 단일 `Extension_Module/` 평탄 |
| `docs/` 포함 | ❌ | ✅ |
| `examples/` 포함 | ❌ (코드만) | ✅ (README 포함 51개) |
| `.claude/` 포함 | ❌ | ✅ (온보딩 skill) |
| `CLAUDE.md`/`AGENTS.md` | ❌ | ✅ |
| MAX_PATH 위험 | 가능 | 해소 |

→ v2.2.0 부터 학생이 압축 풀자마자 Claude Code 가 학습 컨텍스트 전부 접근.
