# keyboard.json リファレンス

`keyboard.json` はキーボードのファームウェアビルドおよびWebコンフィギュレータで使用される設定ファイルです。各キーボードの `keyboards/<keyboard_name>/keyboard.json` に配置します。

> [!IMPORTANT]
> 2026年9月より、ハードウェア関連の設定はすべて `keyboard.json` で定義します。`board_def.h` は `setup.py` が `keyboard.json` の内容から自動生成するため、手動で作成・編集する必要はありません。

---

## 全体構造

```json
{
  "name": "...",
  "manufacturer": "...",
  "maintainer": "...",
  "usb": { ... },
  "keyboard": { ... },
  "features": { ... },
  "hardware": { ... },
  "analog": { ... },
  "digital": { ... },
  "encoder": { ... },
  "calibration": { ... },
  "wear_leveling": { ... },
  "memory_budget": { ... },
  "layout": { ... },
  "keymap": [ ... ],
  "keymaps": [ ... ],
  "actuation": { ... }
}
```

> [!NOTE]
> `features`、`wear_leveling`、`memory_budget`、`keymap`/`keymaps`、`actuation` はオプションです。それ以外は必須フィールドです。

---

## 基本情報

| フィールド | 型 | 必須 | 説明 |
|---|---|---|---|
| `name` | string | ✅ | キーボード名（USB製品名にも使用） |
| `manufacturer` | string | ✅ | メーカー名（USBメーカー名にも使用） |
| `maintainer` | string | ✅ | メンテナー名 |

---

## `usb` — USB設定

| フィールド | 型 | 必須 | 説明 |
|---|---|---|---|
| `vid` | string | ✅ | USB Vendor ID（`"0xXXXX"` 形式） |
| `pid` | string | ✅ | USB Product ID（`"0xXXXX"` 形式） |
| `port` | `"fs"` \| `"hs"` | ✅ | `"fs"` = Full Speed、`"hs"` = High Speed（8kHzポーリング対応） |

```json
"usb": {
  "vid": "0x0108",
  "pid": "0x0111",
  "port": "hs"
}
```

---

## `keyboard` — キーボード基本構成

| フィールド | 型 | 範囲 | 必須 | 説明 |
|---|---|---|---|---|
| `num_profiles` | integer | 1–8 | ✅ | プロファイル数 |
| `num_layers` | integer | 1–8 | ✅ | レイヤー数 |
| `num_keys` | integer | 1–256 | ✅ | キー数 |
| `num_advanced_keys` | integer | 1–64 | ✅ | アドバンストキースロット数 |

```json
"keyboard": {
  "num_profiles": 4,
  "num_layers": 4,
  "num_keys": 41,
  "num_advanced_keys": 32
}
```

---

## `features` — 機能フラグ（オプション） {#features}

LEDやジョイスティックなどのオプション機能を有効化します。Webコンフィギュレータで対応するタブの表示/非表示を制御します。

| フィールド | 型 | デフォルト | 説明 |
|---|---|---|---|
| `rgb` | boolean | `false` | RGB LEDバックライトを有効化 |
| `joystick` | boolean | `false` | アナログジョイスティックを有効化 |
| `encoder` | boolean | `false` | ロータリーエンコーダー設定タブを有効化。通常は `encoder.map` から自動判定 |

```json
"features": {
  "rgb": true,
  "joystick": true,
  "encoder": true
}
```

> [!IMPORTANT]
> `features` は主にファームウェア機能とWebコンフィギュレータ上の表示を有効化します。実際にRGBやジョイスティック、ロータリーエンコーダーなどを使う場合は、必要に応じて `board_def.h` に対応するハードウェア定義も記述してください。詳細は[`board_def.h` セクション](#board_def)を参照してください。

---

## `hardware` — ハードウェア設定

| フィールド | 型 | 必須 | 説明 |
|---|---|---|---|
| `hse_value` | integer | ✅ | 外部高速発振器の周波数（Hz） |
| `cpu_hz` | integer | — | CPU/システムクロック周波数（Hz）。省略時はPlatformIOボード定義のデフォルト |
| `driver` | string | ✅ | ハードウェアドライバ名（現状は `"at32f405xx"` または `"stm32f446xx"`） |

```json
"hardware": {
  "hse_value": 12000000,
  "cpu_hz": 160000000,
  "driver": "at32f405xx"
}
```

---

## `analog` — アナログ入力設定

| フィールド | 型 | 必須 | 説明 |
|---|---|---|---|
| `backend` | `"mcu_adc"` \| `"spi_adc"` | — | アナログサンプリング backend。省略時は `"mcu_adc"`。`"spi_adc"` は将来の外部 SPI ADC 用の予約値で、現状は未実装 |
| `adc_resolution` | integer | — | ADC分解能（省略時はMCUの最大値） |
| `invert_adc` | boolean | — | ADC値とキーストローク距離が反比例する場合 `true` |
| `delay` | integer | — | ADCスキャン間の遅延（μs） |
| `mux` | object | — | アナログマルチプレクサ設定 |
| `raw` | object | — | 直接ADC入力設定 |

### `analog.mux` — マルチプレクサ設定

| フィールド | 型 | 説明 |
|---|---|---|
| `select` | string[] | マルチプレクサ選択線のGPIOピン名 |
| `input` | (string\|integer)[] | ADC入力チャンネル（GPIOピン名または整数） |
| `matrix` | integer[][] | マルチプレクサチャンネルから物理キー番号へのマッピング。`1` = キー0、`2` = キー1、`0` = 未接続 |

### `analog.raw` — 直接ADC入力設定

ジョイスティックなど、マルチプレクサを経由しない直接ADC入力に使用します。

| フィールド | 型 | 説明 |
|---|---|---|
| `input` | (string\|integer)[] | 直接ADC入力チャンネル |
| `vector` | integer[] | 各入力に対応する物理キー番号。`1` = キー0、`0` = 未接続 |

````json
"analog": {
  "backend": "mcu_adc",
  "invert_adc": false,
  "mux": {
    "select": ["C1", "C2", "C3"],
    "input": ["A3", "A4", "A5", "A6", "A7"],
    "matrix": [
      [1, 2, 11, 12, 21, 22, 31, 32],
      [3, 4, 13, 14, 23, 24, 33, 34],
      ...
    ]
  },
  "raw": {
    "input": ["A0", "A1"],
    "vector": [42, 43]
  }
}
````

> [!TIP]
> `raw` セクションはジョイスティックのアナログ軸など、マルチプレクサを経由しない入力に利用できます。`vector` の値に `keyboard.num_keys` より大きい物理キー番号を指定すると、通常のキーとしてはマッピングされず内部入力としてのみ保持されます。

> [!IMPORTANT]
> `analog.backend` は将来の backend 差し替えポイントです。現在使える値は実質 `"mcu_adc"` のみで、`"spi_adc"` を指定すると build 時に明示的にエラーになります。

> [!NOTE]
> `analog.mux.matrix` / `analog.raw.vector` / `digital.vector` は 1-based ですが、`layout.key` やデフォルトキーマップ配列は 0-based です。

---

## `digital` — 直接GPIO入力設定（オプション）

普通のスイッチなど、ADCを使わない GPIO 直結入力をキーとして追加したい場合に使用します。

| フィールド | 型 | 説明 |
|---|---|---|
| `input` | string[] | 各デジタル入力に対応する GPIO ピン名 |
| `vector` | integer[] | 各入力に対応する物理キー番号。`1` = キー0、`0` = 未接続 |
| `pull` | `"none"` \| `"up"` \| `"down"` | 内部プル抵抗設定（省略時は `"up"`） |
| `active_low` | boolean | `true` なら Low レベルを押下扱い（省略時は `true`） |

```json
"digital": {
  "input": ["B8", "B9"],
  "vector": [42, 43],
  "pull": "up",
  "active_low": true
}
```

> [!TIP]
> `digital.vector` も `analog.raw.vector` と同じく 1-based の物理キー番号です。プルアップで GND に落とす普通のタクトスイッチなら、`pull: "up"` と `active_low: true` のままで使えます。

> [!TIP]
> `digital` で割り当てた入力は通常キーとして扱われるため、`keymap` に載せれば `hmkconf` から普通にリマップできます。

---

## `encoder` — ロータリーエンコーダー予約キー設定（オプション）

`hmkconf` からロータリーエンコーダーの回転方向ごとの動作を変更したい場合に使用します。各方向を通常の keymap の 0-based key index に割り当て、`board_def.h` 側の GPIO 定義と組み合わせて使います。

| フィールド | 型 | 説明 |
|---|---|---|
| `map` | object[] | 各エンコーダーの CW/CCW に対応する仮想キー定義 |
| `map[].label` | string | `hmkconf` 上の表示名（省略時は `Encoder N`） |
| `map[].cw` | integer | 時計回りに1デテント進んだときの 0-based key index |
| `map[].ccw` | integer | 反時計回りに1デテント進んだときの 0-based key index |

```json
"encoder": {
  "map": [
    {
      "label": "Main Encoder",
      "cw": 41,
      "ccw": 42
    }
  ]
}
```

> [!IMPORTANT]
> `encoder.map[].cw` / `encoder.map[].ccw` は `layout.key` やデフォルトキーマップと同じ 0-based key index です。通常は `layout.keymap` に出さない隠しキーとして末尾に確保するため、`keyboard.num_keys` と `keymap` / `keymaps` の配列長にはこれらの仮想キーも含めてください。

> [!TIP]
> 押しボタン付きエンコーダーのスイッチ部分は `digital` セクションで通常キーとして追加できます。

> [!TIP]
> `encoder.map` が metadata に含まれていると、`hmkconf` には専用の `Encoder` タブが表示されます。compile-time 固定出力にしたいだけなら `ENCODER_CW_KEYCODES` / `ENCODER_CCW_KEYCODES` を `board_def.h` に定義する方法もありますが、その場合は `hmkconf` からは変更できません。

---

## `calibration` — キャリブレーション設定

| フィールド | 型 | 必須 | 説明 |
|---|---|---|---|
| `initial_rest_value` | integer | ✅ | キーの初期静止時ADC値 |
| `initial_bottom_out_threshold` | integer | ✅ | ボトムアウト検出に必要な最小ADC変化量 |

```json
"calibration": {
  "initial_rest_value": 2400,
  "initial_bottom_out_threshold": 650
}
```

---

## `wear_leveling` — ウェアレベリング設定（オプション）

EEPROM エミュレーション用のフラッシュ領域サイズを設定します。

| フィールド | 型 | デフォルト | 説明 |
|---|---|---|---|
| `virtual_size` | integer | `8192` | 仮想ストレージサイズ（バイト）。RAM上に展開されるため、RAMサイズに注意 |
| `write_log_size` | integer | `65536` | 書き込みログサイズ（バイト） |

```json
"wear_leveling": {
  "virtual_size": 8192,
  "write_log_size": 65536
}
```

---

## `memory_budget` — CI用メモリ予算（オプション）

CI の size check / stack usage check に使う閾値です。指定しない場合はスクリプト側のデフォルトを使います。

| フィールド | 型 | デフォルト | 説明 |
|---|---|---|---|
| `min_flash_headroom` | integer | `16384` | ウェアレベリング予約を差し引いた後に最低限残したい flash 余裕量（バイト） |
| `min_ram_headroom` | integer | `8192` | heap/stack 予約を差し引いた後に最低限残したい RAM 余裕量（バイト） |
| `max_stack_frame` | integer | — | `-fstack-usage` の `.su` レポートに対する任意の関数単位上限（バイト） |

```json
"memory_budget": {
  "min_flash_headroom": 32768,
  "min_ram_headroom": 16384,
  "max_stack_frame": 1024
}
```

> [!TIP]
> `max_stack_frame` はまず未指定でレポート収集だけ始めて、十分に観測できてから閾値を固定する運用がおすすめです。

---

## `layout` — レイアウト定義

Webコンフィギュレータでのキーボードの描画方法を定義します。

| フィールド | 型 | 必須 | 説明 |
|---|---|---|---|
| `labels` | (string\|string[])[] | — | レイアウトオプションのラベル。文字列 = トグル、配列 = セレクト |
| `keymap` | object[][] | ✅ | 各行のキー配置メタデータ |

### キー配置オブジェクト

| フィールド | 型 | デフォルト | 説明 |
|---|---|---|---|
| `key` | integer | — (必須) | キーインデックス（0始まり） |
| `w` | number | `1` | キー幅（1U単位） |
| `h` | number | `1` | キー高さ（1U単位） |
| `x` | number | `0` | X座標オフセット |
| `y` | number | `0` | Y座標オフセット |
| `r` | number | `0` | 回転角度（度数法）。Alice配列などの傾いたキーに使用 |
| `option` | [number, number] | — | レイアウトオプション `[ラベルインデックス, 選択肢]` |
| `label` | string | — | キーに表示するラベル（コンフィギュレータ用） |

### レイアウトオプションの使い方

`labels` でレイアウトバリエーションを定義し、各キーの `option` で所属を指定します。

```json
"layout": {
  "labels": [
    "Split Backspace",
    "Split R-Shift",
    ["Bottom Row", "6.25U", "7U", "Split Spacebar"]
  ],
  "keymap": [
    [
      { "key": 14, "w": 2, "option": [0, 0] },
      { "key": 13, "option": [0, 1] },
      { "key": 15, "option": [0, 1] }
    ]
  ]
}
```

- `"Split Backspace"` はトグル: `option: [0, 0]` = 通常、`option: [0, 1]` = 分割
- `["Bottom Row", "6.25U", "7U", ...]` はセレクト: `option: [2, 0]` = 6.25U、`option: [2, 1]` = 7U、...

---

## `keymap` / `keymaps` — デフォルトキーマップ（どちらか必須）

### `keymap` — 全プロファイル共通のデフォルトキーマップ

`[レイヤー][キー]` の2次元配列。キーコード文字列を使用します。

```json
"keymap": [
  ["KC_Q", "KC_W", "KC_E", ..., "MO(1)"],
  ["KC_TRNS", "KC_TRNS", ...]
]
```

### `keymaps` — プロファイルごとのデフォルトキーマップ

`[プロファイル][レイヤー][キー]` の3次元配列。

```json
"keymaps": [
  [
    ["KC_Q", "KC_W", ...],
    ["KC_TRNS", ...]
  ],
  [
    ["KC_A", "KC_S", ...],
    ["KC_TRNS", ...]
  ]
]
```

> [!NOTE]
> `keymap` と `keymaps` のどちらか一方を指定する必要があります。`keymaps` が指定されている場合はそちらが優先されます。`keymap` のみの場合、全プロファイルで同じキーマップが使用されます。

### 使用可能なキーコード文字列

| カテゴリ | 例 |
|---|---|
| 英字 | `KC_A` ~ `KC_Z` |
| 数字 | `KC_1` ~ `KC_0` |
| ファンクション | `KC_F1` ~ `KC_F24` |
| 修飾キー | `KC_LCTL`, `KC_LSFT`, `KC_LALT`, `KC_LGUI`, `KC_RCTL`, `KC_RSFT`, `KC_RALT`, `KC_RGUI` |
| 特殊キー | `KC_ENT`, `KC_ESC`, `KC_BSPC`, `KC_TAB`, `KC_SPC`, `KC_DEL`, `KC_INS` |
| 矢印 | `KC_UP`, `KC_DOWN`, `KC_LEFT`, `KC_RGHT` |
| メディア | `KC_MUTE`, `KC_VOLU`, `KC_VOLD`, `KC_MPLY`, `KC_MNXT`, `KC_MPRV` |
| マウス | `MS_BTN1` ~ `MS_BTN5` |
| レイヤー | `MO(0)` ~ `MO(7)` |
| プロファイル | `PF(0)` ~ `PF(7)` |
| 透過 | `KC_TRNS`（`_______` でも可） |
| 無効 | `KC_NO`（`XXXXXXX` でも可） |
| システム | `SP_BOOT`, `PF_SWAP`, `PF_NEXT`, `KY_LOCK`, `LY_LOCK` |

---

## `actuation` — アクチュエーション設定（オプション）

| フィールド | 型 | デフォルト | 説明 |
|---|---|---|---|
| `actuation_point` | integer (0–255) | `128` | デフォルトのアクチュエーションポイント |

```json
"actuation": {
  "actuation_point": 128
}
```

---

## `kalman` — Kalman フィルタ設定（オプション）

アナログキー入力に適用される steady-state Kalman（α-β）フィルタのデフォルトパラメータを定義します。Webコンフィギュレータはこの情報を使って、キーボードごとの推奨値を表示します。省略した場合は `include/matrix.h` のコンパイル時デフォルトが使用されます。

| フィールド | 型 | デフォルト | 範囲 | 説明 |
|---|---|---|---|---|
| `position_gain` | number | `0.35` | 0.0–1.0 | 位置推定の更新ゲイン（α）。高いほど raw ADC の変化に素早く追従するが、ノイズも通しやすくなる |
| `velocity_gain` | number | `0.05` | 0.0–1.0 | 速度推定の更新ゲイン（β）。高いほど速度変化に素早く追従するが、滑らかさが減る |
| `velocity_damping` | number | `0.90` | 0.0–1.0 | 停止時・底打ち保持中の速度減衰率。小さいほど速度推定値が素早く 0 に戻る |
| `rt_down_min_velocity` | number | `0.3` | 0.0–10.0 | 後方互換用の予約値。RT押下・再押下の判定には使用されない |
| `rt_up_min_velocity` | number | `0.3` | 0.0–10.0 | 後方互換用の予約値。RT解放の判定には使用されない |
| `innovation_event_threshold` | number | `5.0` | 0.0–100.0 | 底打ち衝突検出の固定 innovation 閾値（distance units）。予測値に対する残差がこの値を超える負値で検出 |
| `bottom_out_hold_scans` | integer | `4` | 0–100 | 底打ち検出scanの後、減衰・縮小した rt_up を追加適用する scan 数 |
| `bottom_out_rt_up` | integer | `5` | 0–255 | 底打ち保持中に一時的に使用する縮小 rt_up 値 |
| `noise_deadzone` | integer | `2` | 0–20 | rest 値上の ADC units をノイズとして扱い、distance を 0 にクランプする |

```json
"kalman": {
  "position_gain": 0.35,
  "velocity_gain": 0.05,
  "velocity_damping": 0.90,
  "rt_down_min_velocity": 0.3,
  "rt_up_min_velocity": 0.3,
  "innovation_event_threshold": 5.0,
  "bottom_out_hold_scans": 4,
  "bottom_out_rt_up": 5,
  "noise_deadzone": 2
}
```

> [!NOTE]
> 現在のファームウェアではこれらの値は **コンパイル時に固定** されており、`keyboard.json` の変更を反映するにはファームウェアの再ビルドが必要です。将来的な firmware/hmkconf 対応でランタイム調整が可能になる予定です。

---

## `hardware.spi` — SPI バス設定（オプション）

トラックボールセンサーなど、SPI ペリフェラルを使用するデバイスの共通設定です。

| フィールド | 型 | 必須 | 説明 |
|---|---|---|---|
| `buses` | object[] | — | SPI バス定義の配列 |

### `hardware.spi.buses[]` — SPI バス定義

| フィールド | 型 | 必須 | 説明 |
|---|---|---|---|
| `instance` | string | ✅ | SPI ペリフェラル名（例: `"SPI3"`） |
| `clock_hz` | integer | ✅ | SPI ペリフェラルへの入力クロック周波数（Hz） |
| `sck_pin` | string | ✅ | SCK ピン名（例: `"C10"`） |
| `miso_pin` | string | — | MISO ピン名（例: `"C11"`） |
| `mosi_pin` | string | — | MOSI ピン名（例: `"C12"`） |
| `pin_mux` | string | — | GPIO ミューティング（AT32 のみ）。例: `"GPIO_MUX_6"` |
| `pin_af` | string | — | GPIO アルタネートファンクション（STM32 のみ） |

```json
"hardware": {
  "spi": {
    "buses": [
      {
        "instance": "SPI3",
        "clock_hz": 108000000,
        "sck_pin": "C10",
        "miso_pin": "C11",
        "mosi_pin": "C12",
        "pin_mux": "GPIO_MUX_6"
      }
    ]
  }
}
```

> [!NOTE]
> AT32 は `pin_mux`、STM32 は `pin_af` を使用します。

---

## `hardware.clock` — クロック/PLL オーバーライド（オプション）

ボード固有の PLL 設定やバス分周比を上書きします。省略時は MCU のデフォルト値が使用されます。

| フィールド | 型 | 説明 |
|---|---|---|
| `pll_ns` | integer | PLL 倍率（N 値） |
| `pll_ms` | integer | PLL 倍率（M 値） |
| `pll_fp` | string | PLL FOUT ポスト分周比（例: `"CRM_PLL_FP_4"`） |
| `pll_fu` | string | PLL FOUT フレックション設定（例: `"CRM_PLL_FU_12"`） |
| `apb2_div` | string | APB2 バス分周比（例: `"CRM_APB2_DIV_1"`） |
| `apb1_div` | string | APB1 バス分周比（例: `"CRM_APB1_DIV_2"`） |

```json
"hardware": {
  "clock": {
    "pll_ns": 48,
    "pll_ms": 1,
    "pll_fp": "CRM_PLL_FP_4",
    "pll_fu": "CRM_PLL_FU_12",
    "apb2_div": "CRM_APB2_DIV_1",
    "apb1_div": "CRM_APB1_DIV_2"
  }
}
```

> [!CAUTION]
> PLL 設定を誤るとシステムクロックが不安定になります。 MCU データシートと一致させ、変更後は必ず動作確認してください。

---

## `hardware.timings` — バックグラウンドタスクスケジューラ（オプション）

協調型スケジューラによる各バックグラウンドタスクの実行間隔（interval）と位相（phase）を設定します。省略時はファームウェアのデフォルト値が使用されます。

| タスク名 | 説明 |
|---|---|
| `usb` | USB ポーリング |
| `layout` | レイアウト処理 |
| `xinput` | XInput ゲームパッド |
| `command` | コマンド処理 |
| `trackball` | トラックボール polling |
| `joystick` | ジョイスティック入力 |
| `encoder` | エンコーダー入力 |
| `slider` | スライダー入力 |
| `rgb` | RGB LED アップデート |

各タスクは `{ "interval": <μs>, "phase": <μs> }` の形式で設定します。

- `interval`: タスクの実行間隔（マイクロ秒）
- `phase`: タスクの開始位相（マイクロ秒）。複数タスクの実行タイミングをずらすために使用

```json
"hardware": {
  "timings": {
    "usb":       { "interval":   8, "phase":  0 },
    "layout":    { "interval":  64, "phase":  0 },
    "xinput":    { "interval":  64, "phase": 16 },
    "command":   { "interval": 128, "phase":  8 },
    "trackball": { "interval": 128, "phase":  4 },
    "joystick":  { "interval": 128, "phase": 20 },
    "encoder":   { "interval": 128, "phase": 36 },
    "slider":    { "interval": 128, "phase": 52 },
    "rgb":       { "interval": 256, "phase": 28 }
  }
}
```

> [!TIP]
> USB タスクは最優先で実行されるため、`interval` を小さくしてポーリングレートを上げられます。他のタスクは `phase` をずらして CPU 負荷を分散させます。

---

## `rgb.hardware` — RGB LED ハードウェア設定（オプション）

RGB LED を有効にする場合、`rgb.hardware` で LED ドライバーのピン、タイマー、DMA 設定を指定します。このセクションを記述すると、自動生成される `board_def.h` に対応するマクロが生成されます。

### AT32F405xx（DMA/PWM ドライバー）

```json
"rgb": {
  "led_map": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9],
  "hardware": {
    "num_leds": 10,
    "data_pin": "A10",
    "data_pin_source": "GPIO_PINS_SOURCE10",
    "data_pin_mux": "GPIO_MUX_1",
    "timer": "TMR1",
    "timer_channel": "TMR_SELECT_CHANNEL_3",
    "timer_dma_request": "TMR_OVERFLOW_DMA_REQUEST",
    "timer_dmamux_request": "DMAMUX_DMAREQ_ID_TMR1_OVERFLOW",
    "dma_channel": "DMA1_CHANNEL2",
    "dma_mux_channel": "DMA1MUX_CHANNEL2",
    "dma_transfer_flag": "DMA1_FDT2_FLAG",
    "dma_clear_flag": "DMA1_GL2_FLAG",
    "reset_time_ns": 300000,
    "dma_frame_repeats": 2,
    "bitbang_frame_repeats": 2
  }
}
```

### STM32F446xx（bitbang ドライバー）

```json
"rgb": {
  "led_map": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9],
  "hardware": {
    "num_leds": 10,
    "data_pin": "A8",
    "timer": "TIM1",
    "timer_channel": "TIM_CHANNEL_1",
    "dma_channel": "DMA1_Channel2",
    "reset_time_ns": 300000,
    "dma_frame_repeats": 2,
    "bitbang_frame_repeats": 2
  }
}
```

| フィールド | 型 | 必須 | 説明 |
|---|---|---|---|
| `num_leds` | integer | ✅ | RGB LED の総数 |
| `data_pin` | string | ✅ | RGB データ線の GPIO ピン名 |
| `data_pin_source` | string | — | GPIO ピンソース（AT32 のみ） |
| `data_pin_mux` | string | — | GPIO ミューティング（AT32 のみ） |
| `timer` | string | ✅ | 使用するハードウェアタイマー |
| `timer_channel` | string | ✅ | タイマーチャンネル |
| `timer_dma_request` | string | — | DMA リクエストソース（AT32 のみ） |
| `timer_dmamux_request` | string | — | DMAMUX リクエスト（AT32 のみ） |
| `dma_channel` | string | ✅ | DMA チャンネル |
| `dma_mux_channel` | string | — | DMAMUX チャンネル（AT32 のみ） |
| `dma_transfer_flag` | string | — | DMA 転送完了フラグ |
| `dma_clear_flag` | string | — | DMA グローバルクリアフラグ |
| `reset_time_ns` | integer | — | RGB リセット時間（ナノ秒） |
| `dma_frame_repeats` | integer | — | DMA フレームの繰り返し回数 |
| `bitbang_frame_repeats` | integer | — | bitbang フレームの繰り返し回数 |

> [!NOTE]
> MCU ドライバーに応じて必要なフィールドが異なります。AT32 は DMA/M マルチプレクサ関連のフィールドが必要で、STM32 は最小限の設定で動作します。

---

## `joystick` — ジョイスティックハードウェア設定（オプション）

アナログジョイスティックのハードウェア設定です。`features.joystick: true` と組み合わせて使用します。

| フィールド | 型 | 必須 | 説明 |
|---|---|---|---|
| `enabled` | boolean | ✅ | ジョイスティックを有効化 |
| `sw_pin` | string | ✅ | スイッチ（押し込み）の GPIO ピン名 |
| `x_adc_index` | integer | ✅ | X 軸の ADC 入力インデックス |
| `y_adc_index` | integer | ✅ | Y 軸の ADC 入力インデックス |
| `sw_key_index` | integer | ✅ | スイッチに対応する 0-based キーインデックス |

```json
"joystick": {
  "enabled": true,
  "sw_pin": "A9",
  "x_adc_index": 0,
  "y_adc_index": 1,
  "sw_key_index": 40
}
```

> [!NOTE]
> ジョイスティックのアナログ軸は `analog.raw` セクションで設定されたADC入力を使用します。`x_adc_index` / `y_adc_index` は `analog.raw.input` 配列のインデックスを指します。

> [!TIP]
> `sw_key_index` はオプションです。定義した場合、押し込みスイッチは通常キーとして公開され、自動マウスクリックは送信されません。定義しない場合はマウス/スクロールモード中のクリック用スイッチとして扱われます。

---

## `trackball` — トラックボールハードウェア設定（オプション）

トラックボールセンサーの SPI と GPIO 設定です。`features.trackball: true` と `hardware.spi` と組み合わせて使用します。

| フィールド | 型 | 必須 | 説明 |
|---|---|---|---|
| `enabled` | boolean | ✅ | トラックボールを有効化 |
| `sensor` | string | ✅ | センサー名（例: `"PAW3395"`） |
| `spi_bus` | integer | ✅ | 使用する SPI バスのインデックス |
| `cs_pin` | string | ✅ | チップセレクトの GPIO ピン名 |
| `motion_pin` | string | ✅ | モーション割り込みの GPIO ピン名 |
| `spi_frequency_hz` | integer | — | SPI クロック周波数（Hz）。省略時は 8MHz |
| `cpi_default` | integer | — | デフォルト CPI 設定。省略時はセンサーのデフォルト値 |
| `spi_hold_time_us` | integer | — | SPI トランザクション間の CS 高レベル保持時間（μs） |

```json
"trackball": {
  "enabled": true,
  "sensor": "PAW3395",
  "spi_bus": 0,
  "cs_pin": "A15",
  "motion_pin": "D2",
  "spi_frequency_hz": 8000000,
  "cpi_default": 1600
}
```

> [!TIP]
> `spi_bus` の値は `hardware.spi.buses` 配列のインデックスです。

---

## `encoder.hardware` — ロータリーエンコーダー GPIO 設定（オプション）

ロータリーエンコーダーの GPIO ピンとプル設定です。`encoder.map` と組み合わせて使用します。

| フィールド | 型 | 必須 | 説明 |
|---|---|---|---|
| `a_pins` | string[] | ✅ | 各エンコーダーの A 相 GPIO ピン名 |
| `b_pins` | string[] | ✅ | 各エンコーダーの B 相 GPIO ピン名 |
| `invert_directions` | integer[] | — | エンコーダーごとの回転方向反転フラグ（0 or 1） |
| `pullup` | boolean | — | 内部プルアップを有効化（省略時は `true`） |

```json
"encoder": {
  "map": [
    { "label": "Scroll", "cw": 42, "ccw": 43 }
  ],
  "hardware": {
    "a_pins": ["B7"],
    "b_pins": ["B6"],
    "invert_directions": [0],
    "pullup": true
  }
}
```

> [!TIP]
> 回転方向が逆の場合、`invert_directions` に `1` を指定するか、A/B 相のピンを入れ替えてください。

---

## `analog.spi` — 外部 SPI ADC 設定（オプション）

外部 SPI ADC（ADS7953）を使用する場合の設定です。`analog.backend: "spi_adc"` と組み合わせて使用します。

| フィールド | 型 | 必須 | 説明 |
|---|---|---|---|
| `driver` | string | ✅ | ADC ドライバー名（現状 `"ads7953"` のみ） |
| `frequency_hz` | integer | — | SPI クロック周波数（Hz）。省略時は 20MHz |
| `range` | string | — | 入力レンジ。`"vref"`（0-Vref）または `"2xvref"`（0-2×Vref） |
| `buses` | object[] | ✅ | SPI ADC バス定義の配列 |

### `analog.spi.buses[]` — SPI ADC バス定義

| フィールド | 型 | 必須 | 説明 |
|---|---|---|---|
| `bus` | integer | ✅ | SPI バス番号 |
| `gpio` | object | — | 通常の SPI バス定義と GPIO が異なる場合のオーバーライド |
| `devices` | object[] | ✅ | バスに接続されたデバイスの配列 |

### `analog.spi.buses[].devices[]` — ADS7953 デバイス定義

| フィールド | 型 | 必須 | 説明 |
|---|---|---|---|
| `cs` | string | ✅ | チップセレクトの GPIO ピン名 |
| `map` | integer[] | ✅ | 16 チャンネルの物理キー番号マッピング |

```json
"analog": {
  "backend": "spi_adc",
  "spi": {
    "driver": "ads7953",
    "frequency_hz": 1000000,
    "buses": [
      {
        "bus": 0,
        "gpio": {
          "sck_pin": "A0",
          "miso_pin": "A1",
          "mosi_pin": "C3"
        },
        "devices": [
          {
            "cs": "C1",
            "map": [6, 7, 8, 9, 10, 16, 17, 18, 19, 20, 0, 0, 0, 0, 0, 0]
          },
          {
            "cs": "C2",
            "map": [26, 27, 28, 29, 30, 32, 35, 36, 38, 40, 0, 0, 0, 0, 0, 0]
          }
        ]
      }
    ]
  }
}
```

> [!NOTE]
> `map` の値は 1-based の物理キー番号です。`0` は未接続を意味します。

---

## `board_def.h` — 自動生成ハードウェア定義ヘッダー {#board_def}

`keyboards/<keyboard_name>/board_def.h` は `setup.py` が `keyboard.json` の内容から**自動生成**するため、手動で作成・編集する必要はありません。

### 生成されるマクロ

以下のセクションが `keyboard.json` に記述されると、対応する C マクロが `board_def.h` に生成されます:

| JSON セクション | 生成されるマクロ |
|---|---|
| `rgb.hardware` | `RGB_ENABLED`, `RGB_DATA_PIN`, `RGB_DATA_PORT`, `RGB_TIMER`, `RGB_DMA_CHANNEL`, など |
| `joystick` | `JOYSTICK_ENABLED`, `JOYSTICK_SW_PIN`, `JOYSTICK_X_ADC_INDEX`, など |
| `encoder.hardware` | `ENCODER_NUM`, `ENCODER_A_PORTS`, `ENCODER_A_PINS`, など |
| `trackball` | `TRACKBALL_ENABLED`, `TRACKBALL_SENSOR_*`, など |
| `hardware.spi` | `SPI_*_INSTANCE`, `SPI_*_SCK_PIN`, など |
| `analog.spi` | `SPI_ADC_*`, など |

### 生成の確認

`setup.py` を実行すると、`keyboards/<keyboard_name>/board_def.h` が自動生成されます:

```bash
python setup.py -k mochiko40he
```

> [!IMPORTANT]
> 生成された `board_def.h` は `.gitignore` に追加されているため、Git にコミットされません。再生成は `setup.py` の実行のみで可能です。

### 旧来の手動 board_def.h

移行が完了していないレガシーキーボードでは、従来通り手動で `board_def.h` を作成する必要があります。その場合、`keyboard.json` にハードウェアセクションを記述しないでください。

---

## 完全な設定例

### Mochiko40HE Trackball（RGB + トラックボール + エンコーダー）

```json
{
  "name": "Mochiko40HE Trackball",
  "manufacturer": "Lady Tortie",
  "maintainer": "satoyu",
  "usb": {
    "vid": "0x0108",
    "pid": "0x0112",
    "port": "hs"
  },
  "keyboard": {
    "num_profiles": 4,
    "num_layers": 4,
    "num_keys": 44,
    "num_advanced_keys": 32
  },
  "features": {
    "rgb": true,
    "encoder": true
  },
  "rgb": {
    "led_map": [38, 36, 32, 33, 34, 35, 37, 39, 29, 28],
    "hardware": {
      "num_leds": 40,
      "data_pin": "A10",
      "data_pin_source": "GPIO_PINS_SOURCE10",
      "data_pin_mux": "GPIO_MUX_1",
      "timer": "TMR1",
      "timer_channel": "TMR_SELECT_CHANNEL_3",
      "timer_dma_request": "TMR_OVERFLOW_DMA_REQUEST",
      "timer_dmamux_request": "DMAMUX_DMAREQ_ID_TMR1_OVERFLOW",
      "dma_channel": "DMA1_CHANNEL2",
      "dma_mux_channel": "DMA1MUX_CHANNEL2",
      "dma_transfer_flag": "DMA1_FDT2_FLAG",
      "dma_clear_flag": "DMA1_GL2_FLAG",
      "reset_time_ns": 300000,
      "dma_frame_repeats": 2,
      "bitbang_frame_repeats": 2
    }
  },
  "hardware": {
    "hse_value": 12000000,
    "driver": "at32f405xx",
    "spi": {
      "buses": [
        {
          "instance": "SPI3",
          "clock_hz": 108000000,
          "sck_pin": "C10",
          "miso_pin": "C11",
          "mosi_pin": "C12",
          "pin_mux": "GPIO_MUX_6"
        }
      ]
    }
  },
  "analog": {
    "backend": "spi_adc",
    "spi": {
      "driver": "ads7953",
      "buses": [
        {
          "bus": 0,
          "gpio": { "sck_pin": "A0", "miso_pin": "A1", "mosi_pin": "C3" },
          "devices": [
            { "cs": "C1", "map": [6,7,8,9,10,16,17,18,19,20,0,0,0,0,0,0] },
            { "cs": "C2", "map": [26,27,28,29,30,32,35,36,38,40,0,0,0,0,0,0] }
          ]
        }
      ]
    }
  },
  "digital": {
    "input": ["A2", "A9"],
    "vector": [41, 42]
  },
  "trackball": {
    "enabled": true,
    "sensor": "PAW3395",
    "spi_bus": 0,
    "cs_pin": "A15",
    "motion_pin": "D2",
    "spi_frequency_hz": 8000000,
    "cpi_default": 1600
  },
  "encoder": {
    "map": [
      { "label": "Scroll", "cw": 42, "ccw": 43 }
    ],
    "hardware": {
      "a_pins": ["B7"],
      "b_pins": ["B6"],
      "invert_directions": [0],
      "pullup": true
    }
  },
  "calibration": {
    "initial_rest_value": 2400,
    "initial_bottom_out_threshold": 650
  },
  "layout": {
    "keymap": [ ... ]
  },
  "keymap": [
    ["KC_Q", "KC_W", ...],
    ...
  ]
}
```

### Mochiko40HE（RGB + ジョイスティック搭載）

```json
{
  "name": "Mochiko40HE",
  "manufacturer": "Lady Tortie",
  "maintainer": "satoyu",
  "usb": {
    "vid": "0x0108",
    "pid": "0x0111",
    "port": "hs"
  },
  "keyboard": {
    "num_profiles": 4,
    "num_layers": 4,
    "num_keys": 41,
    "num_advanced_keys": 32
  },
  "features": {
    "rgb": true,
    "joystick": true
  },
  "hardware": {
    "hse_value": 12000000,
    "driver": "at32f405xx"
  },
  "analog": {
    "invert_adc": false,
    "mux": {
      "select": ["C1", "C2", "C3"],
      "input": ["A3", "A4", "A5", "A6", "A7"],
      "matrix": [ ... ]
    },
    "raw": {
      "input": ["A0", "A1"],
      "vector": [41, 42]
    }
  },
  "calibration": {
    "initial_rest_value": 2400,
    "initial_bottom_out_threshold": 650
  },
  "layout": {
    "labels": ["default"],
    "keymap": [
      [{ "key": 0 }, { "key": 1 }, ...],
      ...
      [{ "key": 40, "x": 4.5, "y": 4.5, "label": "STICK" }]
    ]
  },
  "keymap": [
    ["KC_Q", "KC_W", ..., "MS_BTN1"],
    ["KC_TRNS", ...],
    ...
  ]
}
```

### HE60（RGB/ジョイスティックなし）

```json
{
  "name": "HE60",
  "manufacturer": "ABS0",
  "maintainer": "peppapighs",
  "usb": {
    "vid": "0xAB50",
    "pid": "0xAB60",
    "port": "fs"
  },
  "keyboard": {
    "num_profiles": 4,
    "num_layers": 4,
    "num_keys": 67,
    "num_advanced_keys": 32
  },
  "hardware": {
    "hse_value": 16000000,
    "driver": "stm32f446xx"
  },
  "analog": {
    "invert_adc": true,
    "mux": {
      "select": ["C13", "C14", "C15"],
      "input": ["A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7", "B0"],
      "matrix": [ ... ]
    }
  },
  "calibration": {
    "initial_rest_value": 2400,
    "initial_bottom_out_threshold": 650
  },
  "layout": {
    "labels": [
      "Split Backspace",
      "Split R-Shift",
      ["Bottom Row", "6.25U", "7U", "Split Spacebar"]
    ],
    "keymap": [ ... ]
  },
  "keymap": [
    ["KC_ESC", "KC_1", "KC_2", ...],
    ...
  ]
}
```

このように、RGB/ジョイスティック/スライダーを使わないボードでは `board_def.h` を作らず `keyboard.json` のみで完結できます。
