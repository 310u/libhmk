# keyboard.json リファレンス

`keyboard.json` はキーボードのファームウェアビルドおよびWebコンフィギュレータで使用される設定ファイルです。各キーボードの `keyboards/<keyboard_name>/keyboard.json` に配置します。

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
  "calibration": { ... },
  "wear_leveling": { ... },
  "layout": { ... },
  "keymap": [ ... ],
  "keymaps": [ ... ],
  "actuation": { ... }
}
```

> [!NOTE]
> `features`、`wear_leveling`、`keymap`/`keymaps`、`actuation` はオプションです。それ以外は必須フィールドです。

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

```json
"features": {
  "rgb": true,
  "joystick": true
}
```

> [!IMPORTANT]
> `features` は主にファームウェア機能とWebコンフィギュレータ上の表示を有効化します。実際にRGBやジョイスティックなどを使う場合は、必要に応じて `board_def.h` に対応するハードウェア定義も記述してください。詳細は[`board_def.h` セクション](#board_def)を参照してください。

---

## `hardware` — ハードウェア設定

| フィールド | 型 | 必須 | 説明 |
|---|---|---|---|
| `hse_value` | integer | ✅ | 外部高速発振器の周波数（Hz） |
| `driver` | string | ✅ | ハードウェアドライバ名（現状は `"at32f405xx"` または `"stm32f446xx"`） |

```json
"hardware": {
  "hse_value": 12000000,
  "driver": "at32f405xx"
}
```

---

## `analog` — アナログ入力設定

| フィールド | 型 | 必須 | 説明 |
|---|---|---|---|
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

> [!NOTE]
> `analog.mux.matrix` / `analog.raw.vector` は 1-based ですが、`layout.key` やデフォルトキーマップ配列は 0-based です。

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

## `board_def.h` — ハードウェア定義ヘッダー {#board_def}

`keyboards/<keyboard_name>/board_def.h` はオプションです。RGB、ジョイスティック、スライダーなどの追加ハードウェア機能を使う場合や、ボード固有の compile-time macro が必要な場合にのみ作成します。

### RGB LED 有効時

#### STM32F446xx

```c
#define RGB_ENABLED 1
#define RGB_DATA_PIN GPIO_PIN_8
#define RGB_DATA_PORT GPIOA
```

STM32F446xx のRGB driverは現在 bitbang 実装です。`RGB_DATA_PIN` と `RGB_DATA_PORT` を定義すれば動作します。

#### AT32F405xx

```c
#define RGB_ENABLED 1
#define RGB_DATA_PIN GPIO_PINS_10
#define RGB_DATA_PORT GPIOA
#define RGB_DATA_PIN_SOURCE GPIO_PINS_SOURCE10
#define RGB_DATA_PIN_MUX GPIO_MUX_1
#define RGB_TIMER TMR1
#define RGB_TIMER_CLOCK CRM_TMR1_PERIPH_CLOCK
#define RGB_TIMER_CHANNEL TMR_SELECT_CHANNEL_3
#define RGB_TIMER_DMA_REQUEST TMR_OVERFLOW_DMA_REQUEST
#define RGB_TIMER_DMAMUX_REQUEST DMAMUX_DMAREQ_ID_TMR1_OVERFLOW
#define RGB_DMA_CHANNEL DMA1_CHANNEL2
#define RGB_DMA_MUX_CHANNEL DMA1MUX_CHANNEL2
#define RGB_DMA_TRANSFER_FLAG DMA1_FDT2_FLAG
#define RGB_DMA_CLEAR_FLAG DMA1_GL2_FLAG
```

AT32F405xx は標準で DMA/PWM RGB driver を使用します。

### ジョイスティック有効時

```c
#define JOYSTICK_ENABLED 1
#define JOYSTICK_SW_PIN GPIO_PINS_9      // スイッチ（押し込み）のGPIOピン
#define JOYSTICK_SW_PORT GPIOA           // スイッチのGPIOポート

#define JOYSTICK_X_ADC_INDEX 0           // analog.raw の入力インデックス（X軸）
#define JOYSTICK_Y_ADC_INDEX 1           // analog.raw の入力インデックス（Y軸）
#define JOYSTICK_SW_KEY_INDEX 40         // スイッチに対応する0-basedキーインデックス
```

> [!NOTE]
> ジョイスティックのアナログ軸は `analog.raw` セクションで設定されたADC入力を使用します。`JOYSTICK_X_ADC_INDEX` / `JOYSTICK_Y_ADC_INDEX` は `analog.raw.input` 配列のインデックスを指します。

> [!TIP]
> `JOYSTICK_SW_KEY_INDEX` はオプションです。定義した場合、押し込みスイッチは通常キーとして公開され、自動マウスクリックは送信されません。定義しない場合はマウス/スクロールモード中のクリック用スイッチとして扱われます。

### スライダー有効時

```c
#define SLIDER_KEY_INDEX 39              // スライダーに対応する0-basedキーインデックス
```

---

## 完全な設定例

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
