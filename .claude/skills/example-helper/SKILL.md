---
name: example-helper
description: |
  XM10 예제 트러블슈팅 — 학생이 "Ex.XX 안 돼" / "빌드 실패" / "LED 안 켜져" 등 호소 시,
  해당 예제 .c 의 @warning + README ⚠️ 섹션 + docs/troubleshooting.md 를 인용해 답한다.

  TRIGGER when:
    - 사용자가 "Ex.XX" + 부정형 ("안 돼", "안 켜져", "실패", "에러") 동시 언급
    - 사용자가 "예제 빌드 실패", "플래시 안 됨", "CDC 연결 실패", "LED 안 켜져" 등 트러블 키워드
    - 사용자가 "/example-helper" 명시 호출
    - 사용자가 Phase 6 (handoff) 통과 후 첫 코드 변경 시도 중 막힘

  DO NOT TRIGGER when:
    - 환경 구축 단계 (Phase 0~5 진행 중 → student-onboard 가 처리)
    - 사용자가 새 예제 작성/기여 요청 (편집 작업)
    - 일반 API 질문 (→ docs/api-reference/ 직접 참조)
---

# example-helper Skill

학생이 특정 예제에서 막혔을 때 빠르게 진단·답변하는 dispatcher.

## Process

### 1. 증상 추출

학생 메시지에서 다음 4개 정보를 파악:
1. **예제 번호** (Ex.XX) — 없으면 "어느 예제인가요?" 질문
2. **단계** — 빌드 / 플래시 / 실행 / 결과 확인
3. **에러 메시지 전문** (있다면 그대로 받기, 변형 X)
4. **HW 상태** — 보드 전원, ST-Link, USB-CDC 케이블 연결 여부

### 2. 1차 참조 — 해당 예제 자체

해당 예제 폴더 (`examples/<번호>_*/`) 에서:
- `*.c` 헤더의 `@note` / `@warning` / `@see` 블록
- `README.md` 의 "⚠️ 흔한 실수" 섹션
- `README.md` 의 "2️⃣ 사전 지식" — 학생이 빠뜨린 선행 학습이 있는지

대부분의 학생 질문은 이 단계에서 해결됨.

### 3. 2차 참조 — 일반 트러블슈팅

[docs/troubleshooting.md](../../../docs/troubleshooting.md) 의 카테고리:
- 빌드 에러 (`undefined reference`, `region overflowed`, `file not found`)
- 플래시 에러 (`No ST-Link detected`, `Target no device found`)
- 한글 경로 / MAX_PATH
- USB-CDC 충돌 (PhAI Studio 동시점유)
- MSC mount 실패

### 4. 3차 참조 — API 레퍼런스

[docs/api-reference/](../../../docs/api-reference/) 의 함수 시그니처 + 예시 + ⚠️ 섹션.
학생이 API 호출 방식이 틀린 경우 인용.

### 5. 응답 포맷 (학생 친화)

```
[증상] <학생 보고 요약>
[원인 후보] <1~3개 가장 가능성 높은 것>
[해결]
  1. <첫 시도 — 가장 비용 낮음>
  2. <두 번째 시도>
[참조] <예제 README 또는 docs 경로>
[그래도 안 되면] <GitHub Issue 템플릿 안내>
```

## 자주 묻는 패턴 (Cheatsheet)

| 증상 | 1차 의심 | 빠른 해결 |
|------|----------|-----------|
| Ex.07/08/09 — "PhAI Studio 안 잡힘" | USB-CDC 동시점유 | 시리얼 터미널 종료, PhAI 단독 실행 (`@warning` 참조) |
| Ex.10* — "MSC 마운트 실패" | FAT32 + 32KB cluster 미준수 | USB 메모리 포맷 재시도 (FAT32, AU=32KB) |
| Ex.14~17 — "제어 발산" | PD gain 너무 큼 | gain 절반으로 줄이고 재실험 |
| `undefined reference to '__weak_xxx'` | XM_Lib --whole-archive 미적용 | SDK 폴더 손상 의심, Phase 2 재실행 |
| "LED 안 켜짐" | 플래시 실패 또는 hang | Phase 4/5 재진행 |

## Anti-pattern (이 Skill 이 하지 말 것)

- 코드를 임의로 수정하지 않음. 학생이 직접 시도하도록 안내만.
- 정확한 에러 메시지 없이 추측 진단 금지. "에러 메시지 전체를 그대로 붙여주세요" 먼저 요청.
- 환경 구축 (Phase 0~5) 영역은 본 Skill 이 다루지 않음 — `student-onboard` 호출 권유.

## 막혔을 때

- 본 Skill 로도 해결 안 되면: GitHub Issue (`bug_report.yml`) 템플릿 안내 + 학생이 시도한 단계 / 에러 메시지 / HW 상태 첨부 권유.
