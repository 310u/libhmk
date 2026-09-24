# カルマンフィルタ移行計画（α-β → 適応カルマン）

## 1. 背景

現行の `src/matrix.c:30-53` の `matrix_kalman_update` は、実際には **α-βフィルタ（g-hフィルタ）** であり、厳密なカルマンフィルタではない。

```c
predicted_pos = pos + velocity;                      // 等速モデル・dt=1スキャン
innovation    = measurement - predicted_pos;
pos      = predicted_pos + position_gain * innovation;  // α = 0.35
velocity += velocity_gain * innovation;                  // β = 0.05
```

固定ゲイン α/β に加え、`velocity_damping=0.90`（静止・ボトムアウト時の別途速度減衰）と、固定閾値 `innovation_event_threshold=5.0` による衝突検出を後付けしている。

**要点**: α-βフィルタは「等速モデルカルマンフィルタの定常解」であり、現行実装は既に定常カルマンと数学的に等価。カルマン化の価値は**ゲインを時間・状況で適応させる**こと（非定常性）にのみ存在する。

## 2. 目標

1. 固定ゲイン α-β を、**per-key 適応カルマンフィルタ**へ置換する。
2. 「押下開始の高速追従」と「静止時のジッタ抑制」の二律背反を、活性化リセット＋適応Qで両立する。
3. ボトムアウト衝突検出を、固定閾値から **NIS（正規化イノベーション）ゲート**へ原理的に置換する。
4. hmkconf/EEPROM の外部 API（α/β）は維持し、ワイヤフォーマットを変えない。

## 3. 確定した設計方針

| 項目 | 決定 |
|---|---|
| 外部 API | `position_gain`(α)/`velocity_gain`(β) を維持。Q/R は内部導出 |
| フィルタ領域 | 距離空間（現行踏襲） |
| 共分散 | **per-key P**（3 float / キー） |
| 衝突検出 | NIS ゲートへ置換 |
| MCU/RAM | AT32F405。最大 `NUM_KEYS=69`（he60-v2）でも P 追加は 828 B、問題なし |

## 4. 状態とモデル

状態 `x = [pos, velocity]^T`（pos: 距離 0-255、velocity: 距離/スキャン）

```
F = [[1, 1], [0, 1]]              // 等速モデル
H = [1, 0]                        // 観測は位置のみ
Q = q · [[1/4, 1/2], [1/2, 1]]    // 加速度をプロセスノイズとみなす
R = R(x)                          // 観測ノイズ分散（距離²、位置依存）
```

### 状態追加（`include/matrix.h` の `key_state_t`）
```c
float pos, velocity, innovation;  // 既存
float p_cov[3];                   // 追加: P00, P01, P11（対称2x2共分散）
```

## 5. キー毎の再帰（アクティブキーのみ）

```c
// predict（等速・dt=1）
pos_pred = pos + velocity
P00p = p_cov[0] + 2*p_cov[1] + p_cov[2] + 0.25*q
P01p = p_cov[1] + p_cov[2] + 0.5*q
P11p = p_cov[2] + q

// update
S   = P00p + R(x)                 // イノベーション共分散
K0  = P00p / S
K1  = P01p / S
y   = z - pos_pred
pos      = pos_pred + K0*y
velocity = velocity + K1*y
p_cov[0] = P00p * R(x)/S
p_cov[1] = P01p * R(x)/S
p_cov[2] = P11p - P01p*P01p/S
NIS = y*y / S                      // ゲート用
```

導出根拠:
- `F P Fᵀ = [[a+2b+c, b+c],[b+c, c]]`（P=[[a,b],[b,c]]）
- `Q = q·G Gᵀ`, `G=[1/2, 1]`
- 更新は `P = (I - K H) P_pred`、`1-K0 = R/S` を利用して乗算を削減

## 6. 変数・ゲインの決め方

| 変数 | 決定方法 | 目安 |
|---|---|---|
| **R0（基準観測ノイズ）** | 静止キーの ADC 実測分散を距離換算 | σ_ADC≒1.5、線形勾配≒0.39 → σ_z≒0.58 → R0≒0.34 |
| **R(x)** | 線形変換により勾配一定 | 定数 R0 で確定 |
| **q_base** | 現在の α=0.35 から定常リカッチ(A.R.E.)を逆算。Λ=√(q/r) 経由 | オフラインPythonで求解 |
| **q_high** | q_base × k（k≈10〜100）、NIS超過時のみ適用 | 適応Q用 |
| **P0** | P00=255², P11=(ピーク速度)²≈36, P01=0 | 活性化時にリセット |
| **NIS閾値** | χ²(1自由度, 99%) ≈ 6.63 | コンパイル時定数 |

### 重要な知見
1. **α=0.35, β=0.05 は自己整合的でない**。離散 CV モデルの定常カルマンゲインは 1 自由度（トラッキング指数 Λ）であり、α と β を独立指定不可。設計ガイドライン（臨界制動 β=α²/(2-α)）では β≈0.074。
   → **α を主ノブとして Λ を決定し、β は整合値に置換**。現状「β=0.05」相当の追加平滑は `velocity_damping` が担う。
2. グローバル共有 P だと定常で α-β に退化するため、**per-key P が必須**（活性化リセットで過渡高ゲインを得る）。

## 7. 適応機構（カルマン固有の利点を引き出す）

| 機構 | 実装 | 利点 |
|---|---|---|
| **活性化リセット** | アイドル fast-path で毎回 `p_cov = P0` を書き込み → 動き出し初回から高ゲイン過渡 | 押下開始の高速追従＋静止時の低ゲインを両立 |
| **適応Q** | `NIS > χ²閾値` で q を一時増大（または P00 インフレート） | ボトムアウト衝突・急反転への原理的応答 |
| **位置依存 R(x)** | 線形化により勾配一定 | 定数 R0 で動作 |

## 8. コード変更（ファイル別）

### `include/matrix.h`
- `key_state_t` に `float p_cov[3]` 追加
- コンパイル時マクロ追加: `MATRIX_KALMAN_MEASUREMENT_NOISE`, `MATRIX_KALMAN_NIS_THRESHOLD`, `MATRIX_KALMAN_Q_MANEUVER_FACTOR`
- `kalman_config_t` は現状維持（ワイヤ互換）

### `src/matrix.c`
- `matrix_kalman_update` を predict/update に書き換え（上記 5. の式）
- アイドル fast-path（`matrix_scan_fast` 内）で `p_cov = P0` リセット追加
- 衝突検出を `innovation < -thr` → `NIS > thr²/S && y < 0 && near_bottom && moving_down` に変更
- 適応Q: NIS 超過時の q 切替（`velocity_damping`/`bottom_out_hold` は初期温存）

### `scripts/`
- A.R.E. 求解・α↔Λ マッピング・ゲイン導出の検証スクリプト

### テスト（`test/`）
- ネイティブ Unity テスト: 合成ランプ+ノイズ入力に対し
  1. 定常ゲインが設定 α に一致
  2. 押下開始応答が現行α-β以下
  3. 静止ジッタが現行以下
  4. 衝突スパイクを NIS ゲートが検出
  5. P が正定値・有界

## 9. 構成・ワイヤフォーマットへの影響

- α/β は維持（ワイヤ/EEPROM フォーマット不変）。α から q_base 導出、β は整合値に置換（`velocity_gain` フィールドは互換のため残す）
- R0・NIS閾値・qインフレ率はまずコンパイル時マクロで導入。実機チューニング後に runtime config 化を検討
- `velocity_damping`/`bottom_out_hold` は初期段階では温存し、適応Qで代替できることを検証後に段階撤去

## 10. 検証手順

1. `python setup.py -k mochiko40he && pio test`（ネイティブテスト）
2. `LIBHMK_NATIVE_SANITIZERS=1 python setup.py -k mochiko40he && pio test`
3. ファームウェアビルド + メモリバジェット確認
4. 実機: 静止分散から R0 確定、RT 応答/ジッタを現行ファームと比較（未検証項目として明記）

## 11. 残論点

1. R(x) 位置依存: 線形化により勾配が一定なので、v1 から定数 R0 とし位置依存は導入しない（解決済み）。
2. 適応Q: 2モード（q_base/q_high 切替）vs 連続 NIS 比例インフレ
3. `velocity_damping` の扱い: 初版は温存でよいか
