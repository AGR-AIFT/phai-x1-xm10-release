/**
 ******************************************************************************
 * @file    agr_mw_version.h
 * @author  HyundoKim
 * @brief   AGR_MW 라이브러리 버전 번호 (표시용)
 * @version 0.2.0
 * @date    Oct 29, 2026
 *
 * @details
 * 이 레포의 릴리스 번호다. **정본은 git 어노테이트 태그 `mw-vX.Y.Z`** 이고,
 * 이 헤더는 사람이 읽기 위한 사본이다 — 태그를 만들 때 둘을 같게 맞춘다.
 *
 * [아직 아무 파일도 include 하지 않는다]
 * 소비 모듈이 런타임에 버전을 보고하거나 분기할 필요가 생기면 그때 include 한다.
 * 지금 include 가 0건인 것은 의도된 상태다.
 *
 * [왜 DOP/ 에 두는가]
 * 소비 빌드 6곳 모두의 include 경로에 공통으로 들어 있는 MW 폴더가 `DOP/` 뿐이다.
 *
 * [올리는 기준] (하니스 `docs/plans/PLAN-20261007-phoundation.md` §4 "2-a 진행 방법")
 * - `0.y.z` 동안 **y** = 새 API · 공개 헤더 · 와이어(통신 형식)가 바뀔 때
 * - **z** = 그 밖의 코드 수정
 * - 문서 · `.claude/` 만 바뀌면 올리지 않는다
 *
 * [다른 번호와의 관계]
 * 통신 규약 판(DOP V3) · 구조(Arch V2) · 모듈 FW 버전 · 부트로더 `bl-vX.Y.Z` ·
 * 모듈이 정하는 `0x1018:03`(와이어 계약 판)과는 **별개인 세 번째 번호**다.
 * 이력은 `docs/CHANGELOG.md`.
 ******************************************************************************
 */

#ifndef AGR_MW_VERSION_H
#define AGR_MW_VERSION_H

#define AGR_MW_VERSION_MAJOR    0
#define AGR_MW_VERSION_MINOR    2
#define AGR_MW_VERSION_PATCH    0

#endif /* AGR_MW_VERSION_H */
