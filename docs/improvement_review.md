# libhmk 次世代拡張：コードベース検証に基づく対案

本ドキュメントは `docs/runtime_architecture.md` の記述およびコードベースの実装詳細に基づき、原案7項目の実現可能性・優先度を再評価し、修正ロードマップを提案します。

---

## 総評：原案の評価とズレのあった前提

### 高く評価する点
- **提案⑤（コード生成）** および **提案⑦（アトミック・トランザクション）** は、コードベース検証の結果、即効性が高く優先度を上げるべき優れた提案です。
- **提案④（マルチバスSPI DMA）** は、`make.py` に既にADS7953のSPI ADCバックエンドコード生成が存在することを確認。更なる共通化の方向性は正しい。

### 実装の実態と合っていない前提
- **提案①（ジッター問題）**: メインループは `WFI` + ADC DMA完了割り込みで駆動しており、マトリクススキャン周期はソフトウェアのループカウントではなく**ADCハードウェアのサンプリングレートで決まる**。`loop_count` による間引き対象はRGB/コマンド処理など低速処理のみで、これらにジッターがあっても実害はない。
- **提案⑥（RGB固定小数点化）**: ターゲットMCU（AT32F405 = Cortex-M4F）は **FPU内蔵**。単精度 `sinf` / `cosf` は1サイクルで実行される。`rgb_math.h` の実装も既に `sinf` を使用。固定小数点化は**逆効果**（精度劣化 + コード増加）。実際のボトルネックはフレームバッファのメモリアクセス帯域とSPI/DMA出力。
- **提案③（適応型ダウンサンプリング）**: 現在のADCスキャンはDMA駆動のシーケンシャル全チャンネルスキャン。ブロック単位のMUX切り替え周波数変更にはアーキテクチャレベルの再設計が必要で、単なる拡張では収まらない。

---

## 修正ロードマップ（7項目→5項目に再編）

### Phase 1（基本: 高優先度、低リスク）

#### 1-1. コンパイル時コード生成の本格化【旧⑤を昇格】

**現状**: `scripts/gen_rgb_coords.py` と `scripts/metadata.py` が既にコード生成を行っている。ただし `eeconfig_t` 内の配列はマクロ定義の固定サイズ。

```
// eeconfig.h の現状:
actuation_t actuation_map[NUM_KEYS];  // NUM_KEYS=41でも256でも同じ構造体サイズ
advanced_key_t advanced_keys[NUM_ADVANCED_KEYS];  // 常に最大64
```

**具体策**:
1. `scripts/gen_layout.py`（新設）が `keyboard.json` を読み、`NUM_KEYS` / `NUM_ADVANCED_KEYS` / `NUM_LAYERS` に最適化された `eeconfig_t` 構造体（の一部分）を `.h` として生成
2. キーマップデフォルト値の配列も同様に生成 → 現状のマクロ `DEFAULT_KEYMAPS` (`NUM_PROFILES * NUM_LAYERS * NUM_KEYS` の3次元配列) を、キーボードに合わせたサイズで生成
3. `advanced_keys` 配列サイズを `NUM_ADVANCED_KEYS` から実際の定義数に縮小（未使用エントリを排除）

**効果**: 16キーの `he16` で数KBのSRAM削減。`NUM_ADVANCED_KEYS=64` が実際の使用数に変わる。

---

#### 1-2. Raw HID アトミック・トランザクション【旧⑦】

**現状**: 各 `COMMAND_SET_*` は受信即 `wear_leveling_write()` を呼ぶ。複数パケットにまたがる設定変更（全キーマップの書き換えなど）は、途中で切断されると不完全な状態がフラッシュに書き込まれる。

**具体策**:
1. `eeconfig.c` に `eeconfig_shadow` (RAM上のコピー) と `bool shadow_dirty` を追加
2. `COMMAND_BEGIN_TRANSACTION` (新ID) 受信時の動作:
   - `memcpy(&shadow, eeconfig, sizeof(eeconfig_t))`
   - `shadow_dirty = true`
3. 通常の `COMMAND_SET_*` 受信時の動作を変更:
   - `shadow_dirty` が `true` → シャドウに書き込み
   - `false` → 従来通り直接書き込み（後方互換）
4. `COMMAND_COMMIT` (新ID) 受信時: `wear_leveling_write(0, &shadow, sizeof(eeconfig_t))`
5. `COMMAND_ROLLBACK` (新ID) 受信時: `shadow_dirty = false`（シャドウ破棄）

**効果**: 転送途中切断による Bricking を完全防止。後方互換性あり（従来ホストはトランザクションを使わず直接書き込みを継続可能）。

---

### Phase 2（拡張: 中優先度、設計フェーズ含む）

#### 2-1. GPIO EXTI 抽象化 + イベントキュー【旧②】

**現状**: エンコーダ（`encoder.c`）とトラックボール（`trackball.c`）はポーリング。ただし `WFI` + DMA駆動のループでアイドル消費電力は既に低い。

**具体策**:
1. `include/hardware/exti_api.h` を新設。API:
   ```c
   typedef void (*exti_callback_t)(uint8_t pin, bool rising);
   bool exti_register(uint8_t port, uint8_t pin, exti_callback_t cb,
                      bool rising, bool falling);
   void exti_unregister(uint8_t pin);
   ```
2. イベントキュー（ロックフリーリングバッファ、`event_trace.h` を参考に実装）に割り込みハンドラがイベントをエンキュー
3. `main.c` のループ先頭でキューを消費し、`input_routing.h` 経由でレイアウトパイプラインに投入

**効果**:
- トラックボール `MOTION` ピンの応答がポーリング周期依存からサブミリ秒に改善
- 割り込み駆動によりWFI起床頻度低下 → 無線展開時の省電力に直結
- 実装規模: `exti_api.c` ~100行 + 各ドライバの修正 ~50行ずつ

**有線運用のみの場合は後回し推奨**。無線展開のPhaseで本格化するのが合理的。

---

#### 2-2. SPI DMA ピンポンエンジン共通化【旧④】

**現状**: `make.py` は既にADS7953のSPI ADC設定（`SPI_ADC_BUS_IDS`, `SPI_ADC_DEVICE_SCAN_CHANNELS` 等）をコード生成している。ただしHALレベルのDMAピンポンバッファ共通化は未実装。

**具体策**:
1. `include/hardware/spi_dma_api.h` を新設:
   ```c
   typedef struct {
       SPI_TypeDef *spi;
       uint32_t bus_id;
       void (*cs_select)(uint8_t device_idx);
       void (*cs_deselect)(uint8_t device_idx);
   } spi_dma_bus_t;
   
   typedef struct {
       uint16_t *buf_a;
       uint16_t *buf_b;
       volatile bool buf_a_ready;
       volatile bool buf_b_ready;
       uint16_t transfer_len;
       // ...
   } spi_dma_pingpong_t;
   
   bool spi_dma_init(const spi_dma_bus_t *bus);
   bool spi_dma_pingpong_start(spi_dma_pingpong_t *pp);
   spi_dma_pingpong_t *spi_dma_pingpong_wait_ready(uint32_t bus_id,
                                                    uint32_t timeout_us);
   ```
2. 転送完了コールバックはDMA割り込み内で `buf_x_ready` フラグを立てるだけ（最小処理）
3. 各バスの完了を同期待機する `wait_any` / `wait_all` を提供（複数ADCの同時サンプリング要求に対応）

**効果**: ADS7953以外のSPI ADC（例: AD7490）や、他のSPIセンサー追加時の移植工数を大幅削減。

---

### Phase 3（将来: 無線展開・アーキテクチャ再設計時に再検討）

#### 3-1. 適応型ダウンサンプリング【旧③、Cortex-M0+ 無線コプロセッサ連携も視野に】

**保留理由**: 現在のDMA駆動シーケンシャルスキャンアーキテクチャでは、ブロック単位のMUX間引きの実装コストが高い。加えて、現在のAT32F405（200MHz Cortex-M4F）ではスキャン周期が十分に短く、省電力の優先度が低い。

**将来検討条件**:
- 無線（BLE）対応でバッテリ駆動が前提になった場合
- サブ GHz 帯無線コプロとCortex-M0+ センサーハブの2チップ構成を採る場合
  - センサーハブ側でこの処理を行い、変化時のみ無線コプロに通知するアーキテクチャが現実的

---

## フェーズごとの対応表

| Phase | 項目 | コード変更規模 | 既存コードへの影響 |
|-------|------|---------------|-------------------|
| 1-1 | コード生成 | `scripts/gen_layout.py` (新規 ~200行) + `scripts/make.py` の拡張 | 低（生成ヘッダをincludeするだけ） |
| 1-2 | アトミックトランザクション | `commands.c` ~+30行, `eeconfig.c` ~+50行, `hid_protocol.md` 更新 | 低（新コマンド追加、後方互換） |
| 2-1 | EXTI抽象化 + イベントキュー | `hardware/exti_api.h/.c` (新規 ~150行), 各ドライバ修正 ~50行 | 中（ドライバ側のポーリング→割り込み移行） |
| 2-2 | SPI DMAエンジン | `hardware/spi_dma_api.h/.c` (新規 ~200行) + ADS7953ドライバ修正 | 中（現ドライバのDMA制御を共通化） |
| 3-1 | 適応型ダウンサンプリング | アーキテクチャ再設計レベル | 高 |

---

## 原案からの主な変更点

| 原案 | 対案の扱い | 理由 |
|------|-----------|------|
| ① マイクロ秒スケジューラ | **削除** | WFI+DMA駆動で既に決定論的。問題の前提が誤り |
| ⑥ RGB固定小数点化 | **削除** | Cortex-M4FにFPU搭載。`sinf` は1サイクル。固定小数点化は性能低下 |
| ⑤ コード生成 | Phase 1 に昇格 | 既存コード生成パイプラインの延長で即効性が高い |
| ④ SPI DMAエンジン | Phase 2 に降格 | ADS7953対応は始まっているが汎用HAL化は設計フェーズが必要 |
| ③ 適応型ダウンサンプリング | Phase 3 に維持 | 無線展開時の再設計が前提 |

---

## 参考: コード検証で確認した関連ファイル

| ファイル | 役割 |
|----------|------|
| `src/main.c:115-151` | WFI+DMA駆動ループ。`loop_count` は低優先タスクの間引きのみ |
| `src/rgb_math.h:28-38` | `sin8()` が `sinf()` を使用。FPU有効時はレジスタ命令1発 |
| `scripts/make.py:114-198` | SPI ADC (ADS7953) のコード生成パイプラインが既に存在 |
| `scripts/gen_rgb_coords.py` | 既存のコード生成実装。このパターンを踏襲可能 |
| `scripts/metadata.py` | 同様に `metadata.h` 自動生成 |
| `src/commands.c:38-44` | シングルリクエスト/レスポンスキューイング |
| `include/eeconfig.h:101-128` | `eeconfig_t` 全構造体。全てマクロサイズ固定配列 |
| `src/eeconfig.c:61-63` | `eeconfig_init()` は `wl_cache` を直接ポインタ参照 |
| `docs/hid_protocol.md:86` | "Changes take effect immediately" — まさに改善対象の記述 |
