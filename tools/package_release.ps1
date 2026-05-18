<#
.SYNOPSIS
  XM10 Extension Module SDK release ZIP packager (Option B 평탄 구조).

.DESCRIPTION
  ZIP root 단일 폴더 안에 SDK 코드 + 학습 문서 + AI 진입점이 한 번에 들어가도록
  Rev1.1.zip / Rev2.0.zip 을 생성합니다. 학생이 압축 풀면 그 폴더가 곧
  STM32CubeIDE Import root + Claude Code 진입 폴더.

.PARAMETER Rev
  생성할 리비전. "1.1" / "2.0" / "all" (기본 "all").

.PARAMETER Version
  버전 문자열 (예: "2.2.0"). 파일명은 항상 RevX.X.zip 으로 단순화 (호환성).
  Tag/release-notes 기록용. 미지정 시 git 최신 태그 사용.

.PARAMETER OutDir
  결과 ZIP 출력 위치. 기본은 레포 루트.

.EXAMPLE
  .\tools\package_release.ps1
  # → Rev1.1.zip + Rev2.0.zip 생성

.EXAMPLE
  .\tools\package_release.ps1 -Rev 1.1 -Version 2.2.0
  # → Rev1.1.zip 만 생성 (v2.2.0 컨텍스트)

.NOTES
  Option B: 단일 폴더 평탄화. v2.1.1 까지의 옵션 A (XM10_SDK/Rev*/Extension_Module/
  깊은 경로) 와 다름. 다음 검증 항목 통과 확인:
    - Test-Path "<폴더>\.project"                            (CubeIDE Import root)
    - Test-Path "<폴더>\CLAUDE.md"                           (Rev 특화 AI 진입점)
    - Test-Path "<폴더>\AGENTS.md"                           (Codex 진입점)
    - Test-Path "<폴더>\.claude\skills\student-onboard"      (온보딩 skill)
    - Test-Path "<폴더>\docs\getting-started"                (학습 문서)
    - Test-Path "<폴더>\examples\00_Quick_Start\README.md"   (예제 학습 가이드)
    - Test-Path "<폴더>\XM_FW\libXM_Lib.a"                   (정적 라이브러리)
#>

[CmdletBinding()]
param(
    [ValidateSet("1.1", "2.0", "all")]
    [string]$Rev = "all",

    [string]$Version = "",

    [string]$OutDir = ""
)

$ErrorActionPreference = "Stop"

# 레포 루트 = 본 스크립트 부모의 부모
$ScriptPath = $MyInvocation.MyCommand.Path
if (-not $ScriptPath) { $ScriptPath = $PSCommandPath }
if (-not $ScriptPath) {
    Write-Error "Cannot resolve script path — run with: powershell -File .\tools\package_release.ps1"
    exit 1
}
$RepoRoot = Split-Path -Parent (Split-Path -Parent $ScriptPath)
if (-not $OutDir) { $OutDir = $RepoRoot }

# 버전 자동 감지 (release notes 출력용)
if (-not $Version) {
    Push-Location $RepoRoot
    try {
        $tag = git describe --tags --abbrev=0 2>$null
        if ($LASTEXITCODE -eq 0 -and $tag) {
            $Version = $tag -replace '^v', ''
        } else {
            $Version = "unknown"
        }
    } finally { Pop-Location }
}

Write-Host "=== XM10 SDK Release Packager ===" -ForegroundColor Cyan
Write-Host "Version : $Version"
Write-Host "Repo    : $RepoRoot"
Write-Host "Out dir : $OutDir"
Write-Host ""

function New-OneRevZip {
    param([string]$RevTag)

    $sdkSrc = Join-Path $RepoRoot "XM10_SDK\Rev$RevTag\Extension_Module"
    if (-not (Test-Path $sdkSrc)) {
        throw "SDK source dir not found: $sdkSrc"
    }

    $zipPath = Join-Path $OutDir "Rev$RevTag.zip"
    if (Test-Path $zipPath) {
        Write-Host "  Removing existing $zipPath" -ForegroundColor Yellow
        Remove-Item $zipPath -Force
    }

    # 임시 staging dir — 이름 충돌 회피 위해 GUID 사용
    $stage = Join-Path $env:TEMP "xm10_pkg_$([guid]::NewGuid().Guid.Substring(0,8))"
    $stageRoot = Join-Path $stage "Extension_Module"
    New-Item -ItemType Directory -Path $stageRoot -Force | Out-Null

    try {
        Write-Host "[Rev$RevTag] Staging at $stageRoot" -ForegroundColor Cyan

        # 1) SDK 코드 — XM10_SDK/Rev*/Extension_Module/* → ZIP root 평탄화
        Write-Host "  (1/3) SDK code"
        # robocopy: 빠르고 안정적 + 디렉토리 제외 지원 (/XD)
        # /E   : 빈 디렉토리 포함 모든 서브디렉토리 복사
        # /XD  : 디렉토리 이름 제외
        # /NFL /NDL /NJH /NJS /NP : 출력 간소화
        # /XD 는 절대 경로 권장 — sdkSrc 기준 절대 경로로 변환
        $excludeNames = @("Debug", "Release", "build", "build_cmake", "build_rev1.1", "build_rev2.0", ".settings")
        $excludeDirs  = $excludeNames | ForEach-Object { Join-Path $sdkSrc $_ }
        $robocopyArgs = @(
            $sdkSrc, $stageRoot, "/E"
            "/XD"
        ) + $excludeDirs + @("/NFL", "/NDL", "/NJH", "/NJS", "/NP")
        $null = robocopy @robocopyArgs
        # robocopy exit code 0-7 = success, 8+ = failure
        if ($LASTEXITCODE -ge 8) {
            throw "robocopy failed (exit $LASTEXITCODE) copying SDK from $sdkSrc"
        }

        # 2) 학습 문서 + 예제 + AI 진입점 — top-level → ZIP root
        Write-Host "  (2/3) docs + examples + .claude + AGENTS.md"
        $topLevelAssets = @(
            "docs",
            "examples",
            ".claude",
            "AGENTS.md",
            "README.md",
            "CHANGELOG.md",
            "LICENSE"
        )
        foreach ($asset in $topLevelAssets) {
            $src = Join-Path $RepoRoot $asset
            if (Test-Path $src) {
                Copy-Item -Path $src -Destination $stageRoot -Recurse -Force
            } else {
                Write-Host "    (skip — not found: $asset)" -ForegroundColor DarkGray
            }
        }

        # 3) ZIP 생성
        Write-Host "  (3/3) Compress → $zipPath"
        Compress-Archive -Path "$stageRoot\*" -DestinationPath $zipPath -Force

        # 검증 — 핵심 항목 ZIP 내부에 존재 확인
        Write-Host "  Verifying ZIP contents..."
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $archive = [System.IO.Compression.ZipFile]::OpenRead($zipPath)
        try {
            $required = @(
                ".project",
                "CLAUDE.md",
                "AGENTS.md",
                ".claude/skills/student-onboard",
                "docs/getting-started",
                "Examples/00_Quick_Start/README.md",
                "XM_FW/libXM_Lib.a"
            )
            $missing = @()
            # Windows ZIP entries use backslash; normalize for compare
            $entryPaths = $archive.Entries | ForEach-Object { $_.FullName -replace '\\', '/' }
            foreach ($req in $required) {
                $found = $entryPaths | Where-Object { $_ -like "$req*" -or $_ -like "*/$req*" } | Select-Object -First 1
                if (-not $found) {
                    $missing += $req
                }
            }
            if ($missing.Count -gt 0) {
                Write-Host "    [WARN] missing in ZIP:" -ForegroundColor Yellow
                $missing | ForEach-Object { Write-Host "      - $_" -ForegroundColor Yellow }
            } else {
                Write-Host "    [OK] all required entries present" -ForegroundColor Green
            }
        } finally {
            $archive.Dispose()
        }

        $sizeMB = [math]::Round((Get-Item $zipPath).Length / 1MB, 1)
        Write-Host "[Rev$RevTag] DONE — $zipPath  ($sizeMB MB)" -ForegroundColor Green
        Write-Host ""
    } finally {
        # staging dir 정리
        if (Test-Path $stage) {
            Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}

# 실행
$revs = @()
if ($Rev -eq "all") { $revs = @("1.1", "2.0") }
else { $revs = @($Rev) }

foreach ($r in $revs) {
    New-OneRevZip -RevTag $r
}

Write-Host "=== Packaging complete ===" -ForegroundColor Cyan
Write-Host "Next steps:"
Write-Host "  1. gh release upload v$Version Rev1.1.zip Rev2.0.zip"
Write-Host "  2. (BIN 자산도 같이 업로드) gh release upload v$Version AGR_Bootloader.bin SUIT_*.bin"
Write-Host "  3. gh release edit v$Version --draft=false   # Publish"
