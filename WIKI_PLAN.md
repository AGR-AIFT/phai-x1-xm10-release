# Extension_Module GitHub 사이트 & Wiki 구성 계획

## 현재 문제점

1. **문서 분산**: README, quick-start-guide, api-reference, examples에 문서가 흩어져 있음
2. **중복 관리**: `01-hardware-setup.md`에 3개 가이드가 합쳐져 있고, `02`, `03`은 별도 존재
3. **PDF/ZIP 중복**: 마크다운 문서와 동일한 내용의 PDF/ZIP이 레포지토리에 함께 존재 → 용량 비효율
4. **버전 불일치**: CubeIDE 버전 등이 문서별로 상이
5. **Navigation 부재**: 문서 간 네비게이션 링크 부족

---

## 제안: GitHub 구조 재설계

### Repository 구조 (정리 후)

```
Extension_Module/
├── README.md                    # 랜딩 페이지 (간결한 소개 + 네비게이션)
├── CHANGELOG.md                 # 버전별 변경 이력
├── CONTRIBUTING.md              # 기여 가이드
├── LICENSE
│
├── XM10_SDK/                    # SDK 프로젝트 (변경 없음)
│   └── Extension_Module/
│
├── examples/                    # 예제 코드 (변경 없음)
│   ├── 01_Button_LED_Basic/
│   ├── ...
│   └── 13_Resistive_Mode/
│
├── PythonDecoder/               # Python 도구 (변경 없음)
│
└── docs/                        # 통합 문서 디렉토리 (신규)
    ├── README.md                # 문서 인덱스
    ├── quick-start/
    │   ├── 01-hardware-setup.md
    │   ├── 02-software-setup.md
    │   └── 03-first-build.md
    ├── api-reference/
    │   ├── 01-task-state-machine.md
    │   ├── 02-h10-control-n-data.md
    │   ├── 03-led-btn-control.md
    │   ├── 04-external-io.md
    │   └── 05-usb-connectivity.md
    └── architecture/
        └── system-overview.md
```

**변경 포인트:**
- `quick-start-guide/` → `docs/quick-start/`로 이동
- `api-reference/` → `docs/api-reference/`로 이동
- PDF/ZIP 파일 제거 (GitHub Releases에서 제공)
- `docs/architecture/` 추가 (시스템 아키텍처 상세 문서)

---

## Wiki 구성 계획

GitHub Wiki는 **심화 가이드, 이론적 배경, FAQ** 등 레포지토리 코드와 직접 관련 없는 참조 자료를 위치시킵니다.

### Wiki 사이드바 구조

```
📚 Home
│
├── 🚀 Getting Started
│   ├── Quick Start Guide
│   ├── Hardware Setup
│   ├── Software Setup (CubeIDE v2.0.0)
│   └── First Build & Debug
│
├── 📖 Tutorials
│   ├── Part 1: 기본 기능
│   │   ├── Button & LED
│   │   ├── External I/O (GPIO/ADC)
│   │   └── Safety Logic
│   ├── Part 2: USB 기능
│   │   ├── CDC 시리얼 통신
│   │   ├── MSC 데이터 로깅
│   │   └── PhAI Studio 연동
│   └── Part 3: H10 제어
│       ├── Passive Mode (P-Vector)
│       ├── Active Assist (토크 제어)
│       └── Resistive Mode
│
├── 📜 API Reference
│   ├── Task State Machine (TSM)
│   ├── H10 Data & Control
│   ├── LED & Button
│   ├── External I/O
│   └── USB Connectivity
│
├── 🏛️ Architecture
│   ├── System Overview
│   ├── FW Layer 구조
│   │   ├── IOIF V3.0
│   │   ├── AGR DOP V2 Protocol
│   │   ├── AGR PnP V2
│   │   └── ISR-to-Task Pattern
│   ├── IPO Model (2ms Control Loop)
│   └── CAN-FD Communication
│
├── 🔧 Advanced Topics
│   ├── DIO ↔ ADC 동적 전환
│   ├── 커스텀 센서 허브 연동
│   ├── IMU Hub Module 사용법
│   ├── PhAI V2 프로토콜 명세
│   └── Python Decoder 사용법
│
├── 🐍 Python Tools
│   ├── CDC Receiver (실시간 모니터링)
│   ├── CSV Reviewer (후처리 분석)
│   └── MSC Data Decoder (바이너리 → CSV)
│
├── ❓ FAQ & Troubleshooting
│   ├── 빌드 오류 해결
│   ├── USB 연결 문제
│   ├── CAN-FD 통신 문제
│   └── 알려진 이슈
│
└── 📋 Changelog
    ├── v2.0.0 (현재)
    ├── v1.0.1
    └── v1.0.0
```

### Wiki 페이지별 역할

| 페이지 | 내용 | 소스 |
|--------|------|------|
| **Home** | 프로젝트 소개, 빠른 네비게이션, 배지 | README.md 기반 간결 버전 |
| **Getting Started** | Quick Start의 Wiki 버전 (이미지 풍부) | `docs/quick-start/` 기반 |
| **Tutorials** | 각 예제별 상세 설명, 회로도, 실행 결과 | `examples/*/README.md` 기반 확장 |
| **API Reference** | 전체 API 명세 (Wiki 형식) | `docs/api-reference/` 미러링 |
| **Architecture** | 시스템 설계 철학, 레이어 상세 설명 | **Wiki 전용 심화 콘텐츠** |
| **Advanced Topics** | 고급 활용법, 프로토콜 명세 | **Wiki 전용 심화 콘텐츠** |
| **Python Tools** | Python 도구 사용법 | `PythonDecoder/` 기반 |
| **FAQ** | 자주 묻는 질문, 트러블슈팅 | **Wiki 전용** (이슈 기반 축적) |
| **Changelog** | 버전별 변경 이력 | `RELEASE_*.md` 기반 |

---

## README.md 개선안

현재 README가 너무 길고 모든 정보를 담으려 합니다. 세련된 GitHub 프로젝트의 README 패턴:

### 구조

```
1. 프로젝트 배너 / 뱃지
2. 한 줄 소개
3. 핵심 기능 (4개 카드)
4. Quick Start (3단계)
5. 문서 링크 표 (Wiki, API, Examples)
6. 시스템 요구사항
7. 라이선스
```

### 개선 포인트

- **튜토리얼 전체 목록 → Wiki로 이동**: README에는 Part 1/2/3 요약만, 상세는 Wiki 링크
- **아키텍처 다이어그램 → 1개만**: 가장 핵심적인 1개만 README에, 나머지 Wiki
- **Quick Start → 3줄 요약**: 설치 → 빌드 → 실행, 상세는 Wiki
- **뱃지 추가**: `v2.0.0`, `STM32CubeIDE 2.0.0`, `CAN-FD`, `FreeRTOS`
- **문서 네비게이션 표** 추가:

```markdown
| 📚 문서 | 링크 |
|---------|------|
| Quick Start Guide | [Wiki](../../wiki/Getting-Started) |
| API Reference | [api-reference/](./api-reference) |
| 예제 코드 | [examples/](./examples) |
| 시스템 아키텍처 | [Wiki](../../wiki/Architecture) |
| Changelog | [CHANGELOG.md](./CHANGELOG.md) |
```

---

## Releases 페이지 개선

### 릴리즈 Assets 구성

```
v2.0.0
├── XM10_SDK.zip          # 전체 SDK (소스 + 라이브러리)
├── libXM_Lib.a           # 정적 라이브러리 단독 다운로드
├── XM10_Quick_Start.pdf  # Quick Start PDF (오프라인용)
└── Source code (auto)    # GitHub 자동 생성
```

### 릴리즈 노트 포맷

- Breaking Changes 명확히 표시
- 마이그레이션 가이드 포함
- 변경 통계 (파일 수, 줄 수)

---

## 구현 우선순위

| 순서 | 작업 | 난이도 | 영향도 |
|------|------|--------|--------|
| 1 | Wiki Home + Getting Started 페이지 생성 | 낮음 | 높음 |
| 2 | README.md 간결화 + 네비게이션 표 추가 | 낮음 | 높음 |
| 3 | Wiki Tutorials 섹션 작성 | 중간 | 높음 |
| 4 | Wiki Architecture 섹션 작성 | 중간 | 중간 |
| 5 | docs/ 디렉토리 구조 정리 | 낮음 | 중간 |
| 6 | CHANGELOG.md 생성 | 낮음 | 중간 |
| 7 | Wiki Advanced Topics 작성 | 높음 | 중간 |
| 8 | PDF/ZIP 파일 Releases로 이관 | 낮음 | 낮음 |
| 9 | Wiki FAQ 초기 작성 | 낮음 | 낮음 |
