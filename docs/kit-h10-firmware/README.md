# KIT H10 펌웨어 & 컨텐츠 파일 업데이트

> 📌 **이 페이지를 읽고 나면**: XM FW 버전에 맞는 KIT H10 (CM / ESP32 / SAM10) 펌웨어 + ContentsFiles 를 USB Stick 또는 SD카드 방식으로 업데이트할 수 있습니다.
> ⏱️ 예상 학습 시간: 20분 (실습 30~60분)
> 🧰 사전 지식: KIT H10 HW 구조 ([docs/architecture/README.md](../architecture/README.md))
> 🎯 핵심: 호환성 매트릭스 — **XM v2.1.1 ↔ H10 v2.3.0 ↔ ContentsFiles 2026.04** (혼용 금지)

> ⚠️ **버전 혼용 금지** — XM v2.1.1 + H10 v1.0.x 또는 그 반대 조합 시 CAN-FD 프로토콜 불일치로 통신 오류. 매트릭스 그대로 적용 필수.

XM10은 KIT H10과 CAN-FD로 연동되므로 **XM FW 버전에 맞는 KIT H10 펌웨어 및 컨텐츠 파일**을 사용해야 합니다. 버전이 불일치하면 통신 오류 또는 예기치 않은 동작이 발생할 수 있습니다.

> **반드시 XM FW 버전에 대응하는 KIT H10 FW 및 ContentsFiles를 적용하세요.**

---

## 🎯 한눈 의사결정 가이드

| 상황 | 무엇이 필요한가 | 어디로 |
|------|---------------|-------|
| **새 XM10 받음, H10 새것** | 호환성 매트릭스 확인 → 일치하면 그대로 사용 | ↓ 매트릭스 |
| **XM10 v2.1.1 인데 H10 가 구버전** | CM/SAM10/ESP32 + ContentsFiles 업데이트 | ↓ 펌웨어 업데이트 (USB Stick) |
| **펌웨어는 OK, 모션맵/음성 갱신** | ContentsFiles 만 SD카드 교체 | ↓ 컨텐츠 파일 업데이트 (SD카드) |
| **업데이트 도중 빨간 LED** | 오류 발생 | ↓ 오류 대응 표 |
| **수동 작업 불가** | 지원 요청 | [Discussions Q&A](https://github.com/AGR-EXO/Extension_Module/discussions/categories/q-a) |

---

## 버전 호환성 매트릭스

| XM FW 버전 | KIT H10 FW 버전 | ContentsFiles | 비고 |
| :---: | :---: | :---: | :--- |
| **v2.1.1** | **CM v2.3.0 / ESP32 v2.3.0 / SAM10 v2.3.0** | **2026.04** | **최신 — 권장** |
| v2.0.x | CM v2.3.0 / ESP32 v2.3.0 / SAM10 v2.3.0 | 2025.02 | Previous |
| v1.0.x | 기존 출하 버전 | 기존 출하 버전 | Legacy |

> **v1.0.x 사용자:** XM FW를 v2.1.1으로 업그레이드하지 않고 v1.0.x를 그대로 사용하는 경우, KIT H10 FW도 기존 출하 버전을 유지해야 합니다. XM v2.1.1과 구 버전 H10 FW를 혼용하면 프로토콜 불일치로 정상 동작하지 않습니다.
> 또한, v1.0.x와 연동되는 KIT H10 FW는 모터 통신 오류 등 자주 발생되는 문제가 해결되지 않은 버전이므로 XM과 KIT H10의 FW는 최신 버전으로 업그레이드하길 권장합니다.

---

## 다운로드

GitHub Releases에서 다운로드할 수 있습니다.

**[Releases 페이지 바로가기](https://github.com/AGR-EXO/Extension_Module/releases)**

### XM v2.1.1 대응 파일

| 파일명 | 용도 | 크기 |
| :--- | :--- | :---: |
| `SUIT_CM_APP_2_3_0.bin` | Control Module (CM) 펌웨어 | 299 KB |
| `SUIT_ESP32_FW_2_3_0.bin` | ESP32 모듈 펌웨어 | 1,030 KB |
| `SUIT_SAM10_APP_2_3_0.bin` | Motor Driver (SAM10/MD) 펌웨어 | 284 KB |
| `ContentsFiles.zip` | SD카드 컨텐츠 파일 (음성, FSM, 모션맵 등) | 6,974 KB |
| `KIT_H10_FW_Update.pdf` | 펌웨어 업데이트 매뉴얼 (상세) | 1,451 KB |
| `KIT_H10_ContentsFiles_Update.pdf` | 컨텐츠 파일 업데이트 매뉴얼 (상세) | 1,443 KB |

---

## 펌웨어 업데이트 (USB Stick 방식)

KIT H10의 CM과 SAM10(MD) 펌웨어를 USB Stick을 이용하여 업데이트합니다.

### KIT H10 펌웨어 저장 구조

KIT H10의 각 모듈(CM, SAM10)의 내부 Flash에는 **부트로더**와 **어플리케이션 펌웨어** 두 영역이 존재합니다.

| 영역 | 역할 |
| :--- | :--- |
| **부트로더** | 전원 인가 시 최초 실행. 업데이트가 필요하면 수행하고, 아니면 어플리케이션을 구동 |
| **어플리케이션 FW** | 실제 로봇 동작에 필요한 기능이 구현된 메인 펌웨어 |

### 준비물

1. **USB-C 지원 USB Stick** (또는 USB-C 변환 어댑터 + USB-A Stick)
2. USB Stick은 **FAT32**로 포맷 (최소 10MB 여유 공간)
3. 펌웨어 바이너리 파일을 USB Stick 루트에 저장

> **파일명 규칙 (반드시 준수)**
> - CM: `SUIT_CM_APP_{major}_{minor}_{patch}.bin`
> - MD(SAM10): `SUIT_SAM10_APP_{major}_{minor}_{patch}.bin`
> - 버전 번호는 각각 0~255 범위

### 업데이트 절차

#### 시나리오 1: CM만 업데이트

USB Stick에 CM 파일만 넣으면 CM만 업데이트됩니다.

#### 시나리오 2: CM + SAM10(MD) 모두 업데이트

USB Stick에 CM, SAM10 파일을 모두 넣으면 순차적으로 업데이트됩니다.

```
CM 업데이트 → 완료 → 왼쪽 SAM10 업데이트 → 완료 → 오른쪽 SAM10 업데이트 → 전체 완료
```

#### 단계별 진행

| 단계 | 동작 | LED 상태 |
| :---: | :--- | :--- |
| 1 | USB Stick을 CM 모듈 하단의 **USB-C 포트**에 삽입 | - |
| 2 | `+` 버튼(보조력 증가)을 **누른 상태**로 전원 ON | - |
| 3 | 부트로더 진입 (1.5초 SAM10 복구 신호 대기) | 흰색 LED 전체 ON |
| 4 | 비프음 → 버튼에서 손을 뗌. 업데이트 대상 파일 확인 | 연녹색 LED (대상 모듈 수만큼 점등) |
| 5 | CM 업데이트 진행 → 완료 | 흰색 LED 진행률 표시 → 연녹색 1개 OFF |
| 6 | (CM+MD 모드) MD 파일 다운로드 → FDCAN 통신 준비 | 초록색 LED ON |
| 7 | 왼쪽 SAM10 업데이트 진행 | 흰색 LED 진행률 표시 |
| 8 | 왼쪽 완료 → 오른쪽 SAM10 업데이트 진행 | 연녹색 LED OFF → 오른쪽 진행 |
| 9 | 전체 완료 → 전원 OFF | **파란색 LED** + 3초 간격 비프음 |

### LED 상태 요약

| LED 색상 | 의미 |
| :--- | :--- |
| 흰색 | 업데이트 진행 중 (0~100%, 10% 단위) |
| 연녹색 | 업데이트 대상 모듈 표시 (3개: CM+SAM10 L/R, 1개: CM만, 2개: SAM10 L/R만) |
| 초록색 | CM ↔ SAM10 FDCAN 통신 준비 완료 |
| **파란색** | **모든 업데이트 완료** |
| **붉은색** | **오류 발생** (2초 간격 비프음) |

### 오류 대응

| 상황 | 증상 | 해결 방법 |
| :--- | :--- | :--- |
| USB Stick 미삽입 | 흰색 ON → 비프음 → OFF → 일반 부팅 | USB Stick 삽입 후 재시도 |
| 파일 없음 / 파일명 오류 | 흰색 ON → 비프음 → OFF → 5~6초 후 붉은색 | 파일명 규칙 확인 후 재시도 |
| CM/SAM10 버전명 오류 | 연녹색 3개 ON → 붉은색 | 바이너리 파일의 버전 번호(0~255) 확인 |
| SAM10만 버전명 오류 | CM 업데이트 완료 → 붉은색 | SAM10 파일명 수정 후 재시도 |

> 상세 매뉴얼(사진 포함)은 Release에 첨부된 **`KIT_H10_FW_Update.pdf`** 를 참조하세요.

---

## 컨텐츠 파일 업데이트 (SD카드 방식)

KIT H10의 CM 모듈 내부 SD카드에 저장된 컨텐츠 파일을 업데이트합니다.

### 컨텐츠 파일 구조

CM 내부의 SD카드(SanDisk Micro SD Max Endurance)에는 다음 폴더가 존재합니다:

| 폴더 | 내용 |
| :--- | :--- |
| `AudioFiles/` | H10 음성 파일 |
| `ContentsFiles/` | FSM 제어 기반 정보 파일, FSM 기반 모션 맵, 로봇 기본 정보 |
| `LOG/` | H10 에러 로그 |
| `RobotData/` | H10 자체 데이터 저장소 |

### 업데이트 절차

| 단계 | 동작 | 비고 |
| :---: | :--- | :--- |
| 1 | **KIT H10 전원 OFF** 및 배터리 분리 | 안전을 위해 반드시 전원 차단 |
| 2 | CM 모듈과 어패럴 분리 | 어패럴 고정부 순서대로 해제 |
| 3 | CM 모듈과 프레임 분리 | 나사 해제 |
| 4 | SD카드 커버 분리 후 **SD카드 탈착** | 핀셋/송곳으로 커버를 분리 |
| 5 | SD카드 리더기를 이용하여 PC에 연결 | - |
| 6 | 기존 컨텐츠 파일 **삭제** 후 새 파일 복사 | `ContentsFiles.zip` 압축 해제 내용 복사 |
| 7 | SD카드 장착 → CM 재조립 | - |
| 8 | 전원 ON 후 정상 동작 확인 | FW가 최신이어야 정상 동작 |

> **주의:** 컨텐츠 파일 교체 시 반드시 **기존 파일을 모두 삭제** 후 새 파일을 복사하세요.

> 상세 매뉴얼(사진 포함)은 Release에 첨부된 **`KIT_H10_ContentsFiles_Update.pdf`** 를 참조하세요.

### 수동 작업이 어려운 경우

| 채널 | 용도 |
|------|------|
| [GitHub Discussions Q&A](https://github.com/AGR-EXO/Extension_Module/discussions/categories/q-a) | 일반 질문 + 절차 문의 |
| [GitHub Issues](https://github.com/AGR-EXO/Extension_Module/issues) | 버그 리포트 + 기능 제안 |
| Release 의 **`KIT_H10_FW_Update.pdf`** | 펌웨어 업데이트 사진 매뉴얼 |
| Release 의 **`KIT_H10_ContentsFiles_Update.pdf`** | 컨텐츠 파일 업데이트 사진 매뉴얼 |

---

## 자주 묻는 질문

**Q: XM v2.1.1을 설치했는데 KIT H10 FW를 업데이트하지 않으면?**
> CAN-FD 프로토콜이 변경되었으므로 통신 오류가 발생합니다. 반드시 대응하는 H10 FW로 업데이트하세요.

**Q: ESP32 펌웨어는 어떻게 업데이트하나요?**
> ESP32 FW(`SUIT_ESP32_FW_2_3_0.bin`)는 CM이 부팅 시 자동으로 ESP32에 전송하여 업데이트합니다. USB Stick에 함께 넣어두면 CM 업데이트 과정에서 자동 처리됩니다.

**Q: v1.0.x XM FW를 계속 사용해도 되나요?**
> 가능합니다. 단, KIT H10 FW도 기존 출하 버전을 그대로 유지해야 합니다. v1.0.x XM FW와 v2.3.0 H10 FW를 혼용하면 안 됩니다.

**Q: 업데이트 중 오류(붉은색 LED)가 발생하면?**
> 전원을 끄고 USB Stick의 파일명과 포맷(FAT32)을 확인한 후 다시 시도하세요. 반복되면 위 담당자에게 문의하세요.
