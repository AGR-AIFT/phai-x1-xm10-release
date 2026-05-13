---
name: student-onboard
description: |
  XM10 처음 시작 학생 온보딩. STM32CubeIDE 설치 → SDK ZIP 다운로드 + Import → 빌드 → 플래시 → LED 점등까지
  6단계 phased 자동 안내. 학생이 사용자 코드 작성 직전 단계 (Ex.00 Quick Start) 까지 도달시키는 것이 목표.
  배포 형태: per-Rev ZIP (Rev1.1.zip / Rev2.0.zip) via GitHub Releases — git clone 미사용.

  TRIGGER when:
    - 사용자가 "처음 시작", "환경 구축", "XM10 시작", "수업에서 받았어", "getting started" 언급
    - 사용자가 GitHub URL (Extension_Module) 또는 Release 페이지 URL 공유 후 첫 응답 단계
    - 사용자가 "/student-onboard" 명시 호출
    - CLAUDE.md 자동 로드 직후 사용자가 onboarding 의도 표시

  DO NOT TRIGGER when:
    - ~/.xm10-onboard-done 파일 존재 (이미 완료한 학생)
    - 사용자가 코드 수정/빌드 중 (이미 환경 구축 완료)
    - 사용자가 예제 트러블슈팅만 요청 (→ example-helper Skill 사용)
    - 사용자가 명시적으로 수동 진행 요청 ("자동 안내 끄고 직접 할게")
---

# student-onboard Skill

XM10 보드를 처음 받는 학생을 위한 phased 환경 구축 가이드. MCU/CubeIDE 경험 0 인 학부생을 가정.

## 진행 원칙

- 한 번에 한 Phase 만. 각 Phase 끝에 학생이 직접 확인 가능한 ✅ 체크포인트.
- 학생이 막히면 즉시 다음 Phase 로 넘어가지 말고 원인 진단.
- 모든 명령은 PowerShell 기준 (Windows 가정). macOS/Linux 학생은 등가 명령 안내.
- AI 가 실행하는 시스템 변경 명령 (Start-Process, 파일 생성 등) 은 사용자 허가 1회 후 진행.

## Phases — 상세 안내는 각 phase 파일 참조

| # | 이름 | 목표 | 상세 |
|---|------|------|------|
| 0 | 사전 점검 | OS / 한글 경로 / 관리자 권한 / 이미 완료 여부 확인 | (본 파일 하단) |
| 1 | CubeIDE 설치 | STM32CubeIDE 다운로드 페이지 자동 오픈 + 설치 검증 | [phases/01-install-cubeide.md](phases/01-install-cubeide.md) |
| 2 | Download + Import | GitHub Releases 에서 본인 Rev ZIP 다운로드 + 압축 해제 + CubeIDE Existing Project Import | [phases/02-download-and-import.md](phases/02-download-and-import.md) |
| 3 | Build | Build All + `.elf` 생성 확인 | [phases/03-build-firmware.md](phases/03-build-firmware.md) |
| 4 | Flash | ST-Link 연결 + Debug/Run 으로 플래시 | [phases/04-flash-firmware.md](phases/04-flash-firmware.md) |
| 5 | LED 검증 | 전원 LED + Status LED 점등 확인 | [phases/05-verify-leds.md](phases/05-verify-leds.md) |
| 6 | 핸드오프 | Ex.00 Quick Start 로 학생 자율 학습 시작 | [phases/06-handoff.md](phases/06-handoff.md) |

## Phase 0 — 사전 점검 (본 파일에 포함)

다음 4개를 순차 확인:

```powershell
# 1) 이미 온보딩 완료된 학생인지
Test-Path $HOME\.xm10-onboard-done
```
- True → 학생에게 "이미 환경 구축이 완료된 것으로 보입니다. 다시 진행할까요?" 묻고 사용자 결정 따름
- False → 다음 검사

```powershell
# 2) 현재 작업 디렉토리에 한글이 포함됐는지
$pwd.Path
```
- 한글 포함 → 차단. "Claude Code 작업 폴더 또는 레포 clone 위치에 한글이 포함되어 있습니다. CubeIDE 빌드 시 MAX_PATH/인코딩 문제가 발생합니다. `C:\dev\` 같은 영문 경로로 이동 후 재시작해주세요." 안내 후 대기.
- 미포함 → 다음 검사

```powershell
# 3) Claude Code 가 PowerShell 권한 있는지 (단순 echo 로 검증)
echo "ok"
```
- 실패 → "PowerShell 실행 권한 또는 Claude Code 권한 설정 확인 필요" 안내
- 성공 → 다음 검사

```powershell
# 4) PowerShell 압축 해제 도구 (Windows 10+ 기본 내장) 존재
Get-Command Expand-Archive
```
- 실패 → "PowerShell 5.0+ 필요 (Windows 10 이상). Windows 업데이트 확인 또는 [7-Zip](https://www.7-zip.org/) 설치" 안내
- 성공 → Phase 1 진입 안내:
   ```
   ✅ Phase 0 통과. 이제 Phase 1 (STM32CubeIDE 설치) 로 진행할게요.
   준비됐으면 "다음" 또는 "Phase 1 시작" 이라고 말씀해주세요.
   ```

> 💡 git 은 본 SDK 사용에 필수가 아닙니다. SDK 는 GitHub Releases ZIP 으로 받으므로 git clone 이 필요하지 않습니다. git 이 있다면 업데이트 추적/PR 등에 편하지만 학습 진행에는 무관.

## 트리거 메커니즘

- **1차 (텍스트 매칭)**: description 의 트리거 키워드 → AI 가 본 skill 호출 판단
- **2차 (CLAUDE.md 안내)**: 학생이 CLAUDE.md 의 "처음 시작 안내" 박스를 보고 명시적으로 트리거
- **3차 (URL-first cold path)**: 학생이 GitHub 레포 URL 또는 Releases 페이지 URL 만 공유한 cold 상태 → AI 가 `WebFetch` 로 본 SKILL.md 또는 CLAUDE.md 읽고 진입. SDK 는 git clone 이 아닌 `Rev1.1.zip` / `Rev2.0.zip` 다운로드 + 압축 해제 (Phase 2 참조).

## 종료 조건

Phase 5 통과 시 다음 명령으로 sentinel 생성:
```powershell
New-Item -ItemType File $HOME\.xm10-onboard-done -Force
```
이후 본 skill 은 자동 트리거되지 않음. 학생이 재시작 원하면 sentinel 삭제.

## 학생이 막혔을 때 (어느 Phase 에서든)

1. 증상을 한 문장으로 받기
2. [docs/troubleshooting.md](../../../docs/troubleshooting.md) 의 해당 섹션 인용
3. 그래도 안 풀리면 GitHub Issue 템플릿 (`bug_report.yml` / `hardware_issue.yml`) 안내

## Logging (선택)

학생 사용 통계 (개인정보 X, 단순 phase 완료 카운트) 가 필요하면:
```bash
echo "$(date -u +%Y-%m-%dT%H:%M:%S)|student-onboard|phase=${PHASE}|status=${STATUS}" >> $HOME/.xm10-onboard.log
```
기본 비활성화. 개인 정보 수집 의도 없음.
