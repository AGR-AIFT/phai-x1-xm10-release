/**
 ******************************************************************************
 * @file    ioif_agrb_version.h
 * @author  HyundoKim
 * @brief   [IOIF] 라이브러리 버전 번호 (표시용)
 * @version 0.3.2
 * @date    Nov 8, 2026
 *
 * @details
 * 이 레포의 릴리스 번호다. **정본은 git 어노테이트 태그 `ioif-vX.Y.Z`** 이고,
 * 이 헤더는 사람이 읽기 위한 사본이다 — 태그를 만들 때 둘을 같게 맞춘다.
 *
 * [아직 아무 파일도 include 하지 않는다]
 * 소비 모듈이 런타임에 버전을 보고하거나 분기할 필요가 생기면 그때 include 한다.
 * 지금 include 가 0건인 것은 의도된 상태다 — 쓰이지 않는 매크로를 미리 넣어 둔 것이
 * 아니라, 번호를 코드 트리 안에서도 찾을 수 있게 둔 것이다.
 *
 * [올리는 기준]
 * - `0.y.z` 동안 **y** = 새 API · 공개 헤더 · 와이어 형식이 바뀔 때
 * - **z** = 그 밖의 코드 수정
 * - 문서 · `.claude/` 만 바뀌면 올리지 않는다
 *
 * [다른 번호와의 관계]
 * 구조(Arch V1 · V2) · 통신(DOP V1~V3) · 모듈 FW 버전 · 부트로더 `bl-vX.Y.Z` 와는
 * **별개인 세 번째 번호**다. 이력은 `docs/CHANGELOG.md`.
 ******************************************************************************
 */

#ifndef IOIF_AGRB_VERSION_H
#define IOIF_AGRB_VERSION_H

#define IOIF_VERSION_MAJOR      0
#define IOIF_VERSION_MINOR      3
#define IOIF_VERSION_PATCH      2

#endif /* IOIF_AGRB_VERSION_H */
