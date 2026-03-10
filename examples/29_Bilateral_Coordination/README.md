# 예제 29: 좌우 협응 제어 (Bilateral Coordination)

본 예제는 **좌우 고관절 협응(bilateral coordination)** 제어를 구현합니다. 두 다리의 움직임을 역위상 대칭으로 커플링하여 자연스러운 보행 패턴을 유도합니다.

> 📖 API 레퍼런스: [H10 Control & Data](../../docs/api-reference/02-h10-control-n-data.md) · [Task State Machine](../../docs/api-reference/01-task-state-machine.md)
>
> 📄 논문 레퍼런스: Duschau-Wicke, A., et al. (2010). *Patient-cooperative strategies for robot-aided treadmill training.* IEEE Transactions on Neural Systems and Rehabilitation Engineering, 18(4), 374–385.

## 🎯 학습 목표 (Objective)

* **역위상 대칭 커플링**: 우측이 신전할 때 좌측이 굴곡, 즉 `θ_R ≈ −θ_L` 관계를 유도합니다.
* **커플링 토크**: `Δθ_R = θ_R + θ_L` (대칭 오차)에 비례한 교정 토크를 계산합니다.
* **비대칭 감지**: `|Δθ| > 5°` 시 LED 알림으로 재활 진행을 모니터링합니다.

### 제어 법칙

```
Δθ_R = θ_R + θ_L       (역위상 오차 — 0이면 완벽한 대칭)
τ_R = −K_c·Δθ_R − B_c·Δω_R
τ_L = +K_c·Δθ_R + B_c·Δω_R  (반대 방향)
```

---

## ⚙️ 동작 원리 (How it Works)

### 제어 모드 (CoordMode_t)

| 모드 | 동작 |
|------|------|
| `SYMMETRIC` | 기본 역위상 커플링 |
| `ASSIST_RIGHT` | 우측 방향 비대칭 보조 |
| `ASSIST_LEFT` | 좌측 방향 비대칭 보조 |

### 버튼 조작

| 버튼 | 동작 |
|------|------|
| BTN1 클릭 | 커플링 강성 K_c 순환 (0.5 → 1.0 → 2.0 → 0.0 Nm/deg) |
| BTN2 클릭 | 협응 모드 순환 (SYMMETRIC → ASSIST_R → ASSIST_L) |
| BTN3 클릭 | 비대칭 임계값 순환 (3° → 5° → 10°) |

---

## 🚀 실행 방법 (How to Use)

1. 빌드 후 XM10에 플래시합니다.
2. KIT H10 전원 ON → ASSIST MODE → 보행 시작.
3. **Body Data 선택 사항** — 없이도 동작하나 정확도 향상을 위해 설정 권장.
4. 좌우 다리가 자연스러운 역위상 패턴을 유지하는지 확인합니다.
5. LED3이 켜지면 비대칭 상태(`|θ_R + θ_L| > 임계값`) 경고입니다.
6. USB CDC에서 `BILAT | ΔθR:... K_c:... τR:... τL:... [ASYM]` 출력 확인.

---

## 💡 직접 해보기 (Things to Try)

* **K_c=0 vs K_c=2.0**: 커플링 없을 때와 강한 커플링에서 보행 대칭성 차이를 비교합니다.
* **ASSIST_RIGHT 모드**: 우측 다리가 약한 편측 마비 시뮬레이션에서 우측 보조 효과를 관찰합니다.
* **비대칭 임계값**: 임계값을 3°로 좁히면 LED3이 자주 켜집니다. 실제 재활 모니터링에 활용하세요.

---

## ⚠️ 주의사항

Body Data는 선택 사항이나 설정 시 정확도가 향상됩니다. 커플링 강도 K_c가 너무 크면 착용자의 자발적 움직임이 제한될 수 있습니다.
