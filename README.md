# libhmk

> Fork of [peppapighs/libhmk](https://github.com/peppapighs/libhmk) — Libraries for building Hall-effect keyboard firmware.
>
> [peppapighs/libhmk](https://github.com/peppapighs/libhmk) のフォーク — ホール効果キーボードファームウェア構築のためのライブラリ群。

The source tree is shared across multiple keyboard definitions under `keyboards/`, but each compiled firmware image is specific to the selected keyboard. Always build and flash the binary for the exact target keyboard.

ソースツリーは `keyboards/` 配下の複数キーボード定義で共有されますが、ビルドされたファームウェアイメージはキーボードごとに固有です。対象キーボードに合ったバイナリをビルド・書き込みしてください。

---

## Changes from Upstream / フォーク元からの変更点

This fork is **111 commits ahead** of upstream, adding major feature sets, architectural improvements, a native test suite, and 6 custom keyboard definitions.

本フォークは upstream から **111コミット先行** しており、大規模な機能追加・アーキテクチャ改善・ネイティブテストスイート・6機種の独自キーボード定義を追加しています。

| Category / カテゴリ | Additions / 追加内容 |
|---|---|
| **Input Devices / 入力デバイス** | Analog joystick (5 modes), rotary encoder, analog slider, optical trackball (PMW3360/PAW3395) |
| **RGB Lighting / RGB バックライト** | Per-key SK6812MINI-E LED driver, 50+ effects (static/animated/reactive/ambient/utility), binary clock |
| **Advanced Keys / 高度なキー** | Combo keys (up to 4 triggers), macro recording/playback, double-tap for tap-hold |
| **Matrix / マトリクス** | Kalman position/velocity tracking, bottom-out collision fix, idle-key fast path, 16kHz target, predictive rapid trigger, per-key distance curve (9-point, log default) |
| **Architecture / アーキテクチャ** | Queue-based HID commands, cooperative microsecond scheduler, extracted analog scan layer, centralized profile runtime, USB suspend/resume recovery, input routing boundary |
| **Diagnostics / 診断** | Integrated runtime diagnostic mode (channel identity + raw-by-step), scan rate diag, USB polling rate measurement |
| **Keyboard Defs / キーボード定義** | 6 fork-only keyboards: mochiko39he, mochiko40he, mochiko40he-rev2, mochiko40he-tb, mochiko40he-tb2, ads7953_ref |
| **MCU Peripherals / MCU周辺機能** | SPI, I2C, timer HAL drivers for both AT32F405xx and STM32F446xx |
| **Testing / テスト** | Unity-based native test suite (18 suites, 25 native test environments), memory budget validation, stack usage analysis, regression runner |
| **Documentation / ドキュメント** | 11 docs files: protocol spec, architecture notes, keyboard.json reference, keyboard setup guide, SPI ADC design, etc. |
| **Bug Fixes / バグ修正** | Stuck-key USB race condition, XInput/HID gamepad conflict, event chronological sorting, hold-tap input buffering, upstream STM32 timer + EEPROM wear reduction port |

---

## Capabilities / 実現可能な機能

libhmk is a general-purpose Hall-effect keyboard firmware: the table below summarizes what a keyboard definition can realize with this shared codebase, not a per-board support list. Status reflects validation evidence in this repository; items marked *build-tested only* have passed CI/native tests but have not yet been confirmed on physical hardware.

libhmk は汎用ホール効果キーボードファームウェアです。下表は共有コードベースでキーボード定義により実現できる機能の要約であり、機種別サポート一覧ではありません。状態は本リポジトリ内の検証実績に基づき、*ビルド検証のみ* は CI / ネイティブテストを通過しているが実機未確認の項目です。

| Capability / 機能 | Details / 詳細 | Status / 状態 |
|---|---|---|
| **Analog Matrix Input / アナログマトリクス入力** | Per-key actuation point, automatic calibration, Kalman/α-β position-velocity tracking, idle-key fast path | Validated / 実機検証済 |
| **Rapid Trigger** | Position/direction-based trigger, continuous + predictive rapid trigger | Validated / 実機検証済 |
| **Advanced Keys / 高度なキー** | Dynamic Keystroke (4 keycodes), Tap-Hold + Double-Tap, Toggle, Null Bind (SOCD/Rappy Snappy), Combo (≤4 keys), Macro record/playback | Validated / 実機検証済 |
| **Distance Curve / 距離カーブ** | Per-key switch-travel curve (up to 9 points, µm total travel), logarithmic default, Raw HID GET/SET (60-byte chunks), profile persistence (v1.17) | Build-tested only / ビルド検証のみ |
| **RGB Lighting / RGB バックライト** | Per-key SK6812MINI-E, 50+ effects (static/animated/reactive/ambient/utility), depth-reactive, binary clock; AT32=DMA/PWM, STM32=bitbang | Validated / 実機検証済 |
| **Joystick / ジョイスティック** | 5 modes (disabled/mouse/XInput sticks/scroll), deadzone, mouse speed, axis calibration | Validated / 実機検証済 |
| **Rotary Encoder / ロータリーエンコーダ** | Multiple quadrature encoders, fixed keycode or `hmkconf`-remappable virtual key, push-button via `digital` GPIO; defined in `keyboard.json` | Build-tested only / ビルド検証のみ |
| **Analog Slider / アナログスライダー** | On-board analog slider, Volume and Gamepad modes | Build-tested only / ビルド検証のみ |
| **Trackball / トラックボール** | PMW3360/PAW3395 via SPI, CPI adjustment, RGB feedback, config commands + profile persistence (v1.16); available on `mochiko40he-tb` / `-tb2` | Validated / 実機検証済 |
| **Direct GPIO Digital Inputs / デジタルGPIO直接入力** | Spare GPIO pins mapped to key indices without a matrix (encoder push-buttons, side buttons, toggles) | Validated / 実機検証済 |
| **Gamepad / HID** | XInput (Windows) + HID gamepad fallback, NKRO with 6KRO BIOS fallback, 8kHz polling (AT32 high-speed USB) | Validated / 実機検証済 |
| **Configurability / 設定** | Web configurator [hmkconf](https://github.com/310u/hmkconf) (no recompile), profiles × layers, EEPROM emulation, per-profile persistence | Validated / 実機検証済 |
| **Diagnostics / 診断** | Integrated runtime diagnostic mode, scan-rate diag, USB polling-rate measurement | Validated / 実機検証済 |
| **MCU Support / MCU対応** | AT32F405xx (ADC matrix, DMA/PWM RGB, SPI/I2C/timer), STM32F446xx (bitbang RGB, SPI/I2C/timer); analog backend `mcu_adc` (`spi_adc` reserved, not implemented) | AT32: Validated / STM32: Build-tested (drivers + native tests; no in-tree STM32 keyboard) |

### Keyboard Definitions / キーボード定義

The keyboard definitions in [`keyboards/`](keyboards/) are examples created by the repository author (and upstream); they are not the limit of what this firmware can drive.

[`keyboards/`](keyboards/) 配下のキーボード定義はリポジトリ作成者（および upstream）による一例であり、本ファームウェアで駆動できる機種はこれに限られません。

- Upstream 由来: `he16`, `he60`, `he60-v2`, `m256-whe`
- Fork 独自 (6): `mochiko39he`, `mochiko40he` (主要開発ターゲット / primary dev target), `mochiko40he-rev2`, `mochiko40he-tb` (trackball / トラックボール), `mochiko40he-tb2` (trackball / トラックボール), `ads7953_ref` (SPI ADC reference / SPI ADC リファレンス)

---

## Features / 機能

### Core (from upstream / upstream 由来)

- **Analog Input / アナログ入力**: Customizable actuation point per key / キーごとのアクチュエーションポイント設定
- **Rapid Trigger**: Register press/release based on key position change and direction / キー位置の変化量と方向に基づく高速トリガー
- **Continuous Rapid Trigger**: Deactivate Rapid Trigger only when fully released / 完全に離したときのみ無効化
- **Null Bind (SOCD + Rappy Snappy)**: Monitor 2 keys, select active one by behavior / 2キーの競合解決
- **Dynamic Keystroke / ダイナミックキーストローク**: Up to 4 keycodes per key, 4 actions per keystroke / 1キーに最大4キーコード・4アクション
- **Tap-Hold**: Different keycode on tap vs. hold / タップとホールドで異なるキーコード
- **Toggle**: Toggle between press and release; hold for normal behavior / 押すたびに ON/OFF 切替
- **N-Key Rollover**: NKRO with automatic 6KRO fallback in BIOS / BIOS では自動 6KRO フォールバック
- **Automatic Calibration / 自動キャリブレーション**: No user intervention required / ユーザー操作不要
- **EEPROM Emulation / EEPROM エミュレーション**: Internal flash memory, no external EEPROM / 内蔵フラッシュで代用
- **Web Configurator / Web コンフィグ**: Configure via [hmkconf](https://github.com/310u/hmkconf) without recompiling / 再コンパイル不要
- **Tick Rate**: Customizable for Tap-Hold and Dynamic Keystroke / Tap-Hold・Dynamic Keystroke の刻み幅を設定可能
- **8kHz Polling Rate**: Supported on AT32F405xx / AT32F405xx で対応
- **Gamepad**: XInput for Windows, HID gamepad fallback for Linux/macOS / Windows は XInput、他 OS は HID ゲームパッド

### Fork Additions / フォークによる追加機能

#### Input Devices / 入力デバイス

**Joystick / ジョイスティック** — 5 operating modes:

| Mode / モード | Description / 説明 |
|---|---|
| Disabled / 無効 | Joystick input is ignored |
| Mouse / マウス | Controls the mouse cursor / マウスカーソル操作 |
| XInput Left Stick | Maps to left analog stick in gamepad mode / ゲームパッド左スティック |
| XInput Right Stick | Maps to right analog stick in gamepad mode / ゲームパッド右スティック |
| Scroll / スクロール | Scroll wheel + horizontal scroll / 縦横スクロール |

- Configurable deadzone, mouse speed, axis calibration / デッドゾーン・マウス速度・軸キャリブレーション設定可能
- Joystick switch mapped as a remappable key / ジョイスティックスイッチはリマップ可能なキーとして割当
- Mode cycling via `SP_JOY_MODE_NEXT` keycode / `SP_JOY_MODE_NEXT` でモード切替
- `SP_JOY_SCROLL_MO` to temporarily switch to scroll mode / `SP_JOY_SCROLL_MO` で一時的にスクロールモード

**Rotary Encoder / ロータリーエンコーダ** — Supports multiple quadrature encoders defined in `keyboard.json` (`encoder` / `encoder.hardware`; generated into `board_def.h`). Each direction emits a fixed keycode or an `hmkconf`-remappable virtual key. Encoder push-buttons can be exposed via `digital` GPIO inputs.

複数のインクリメンタルエンコーダに対応。`keyboard.json`（`encoder` / `encoder.hardware`、`board_def.h` へ自動生成）で定義します。各方向に固定キーコードまたは `hmkconf` でリマップ可能な仮想キーを割当可能。エンコーダのプッシュボタンは `digital` GPIO 入力として公開可能。

> [!WARNING]
> Firmware implementation is included and build-tested, but not yet validated on real hardware.
> ファームウェア実装はビルドテスト済みですが、実機検証は未了です。

**Slider / スライダー** — On-board analog slider with Volume and Gamepad modes.

オンボードアナログスライダー。音量コントロール / ゲームパッド軸の2モード。

> [!WARNING]
> Slider support has been implemented in software but is not yet fully tested on physical hardware.
> ソフトウェア実装済みですが、実機での完全なテストは未了です。

**Trackball / トラックボール** — PMW3360/PAW3395 optical sensor driver via SPI with CPI adjustment and RGB feedback. Trackball configuration (CPI, etc.) is readable/writable over Raw HID (`COMMAND_GET_TRACKBALL_CONFIG` / `COMMAND_SET_TRACKBALL_CONFIG`) and persisted per profile (config version v1.16). Available on `mochiko40he-tb` and `mochiko40he-tb2`.

PMW3360/PAW3395 光学センサーを SPI 経由で駆動。CPI 調整・RGB フィードバック付き。トラックボール設定（CPI など）は Raw HID（`COMMAND_GET_TRACKBALL_CONFIG` / `COMMAND_SET_TRACKBALL_CONFIG`）で読み書きでき、プロファイルに永続化されます（設定バージョン v1.16）。`mochiko40he-tb` および `mochiko40he-tb2` で使用可能。

**Direct GPIO Digital Inputs / デジタルGPIO直接入力** — Map spare GPIO pins to key indices without a row/column matrix. Useful for encoder push-buttons, side buttons, or mode toggles.

使用していない GPIO ピンをキーインデックスに直接割当可能。エンコーダのプッシュボタンやサイドボタン、モード切替スイッチに適しています。

#### RGB Lighting / RGB バックライト

Per-key RGB backlighting via SK6812MINI-E LEDs with 50+ effects:

SK6812MINI-E によるキーごとの RGB バックライト。50以上のエフェクトを搭載:

| Category / カテゴリ | Effects / エフェクト |
|---|---|
| Static / 静的 | Solid Color, Alphas/Mods, Gradients |
| Animated / 動的 | Breathing, Rainbow, Cycle, Spiral, Pinwheel, and more |
| Reactive / リアクティブ | Typing Heatmap (key press depth), Reactive, Splash, Nexus |
| Ambient / アンビエント | Digital Rain, Pixel Rain, Raindrops, Starlight, Riverflow |
| Utility / ユーティリティ | **Binary Clock** — host-synchronized time in binary LED layout (requires `hmkconf` RGB tab open) |

Many RGB effect names and animation formulas are adapted from QMK's [RGB Matrix](https://docs.qmk.fm/features/rgb_matrix) / RGB Light effect set. `ANALOG` and `PER_KEY` are libhmk-specific extensions. If you redistribute firmware derived from these effects, keep the corresponding source available and preserve attribution.

多くの RGB エフェクト名とアニメーション式は QMK の [RGB Matrix](https://docs.qmk.fm/features/rgb_matrix) / RGB Light から改変しています。`ANALOG` と `PER_KEY` は libhmk 独自の拡張です。これらのエフェクトを含むファームウェアを再配布する場合は、対応するソースコードを公開し、帰属表示を維持してください。

#### Advanced Keys / 高度なキー機能

- **Combo Keys / コンボキー**: Up to 4 trigger keys simultaneously → different keycode. Configurable timing window, layer-aware.
  最大4キー同時押しで別のキーコードを発行。タイミングウィンドウ設定可能、レイヤー対応。
- **Macro Keys / マクロキー**: Record and playback key sequences (Press, Release, Tap, Delay actions).
  キーシーケンスの記録・再生 (Press / Release / Tap / Delay)。
- **Double-Tap**: Optional double-tap keycode for Tap-Hold keys.
  Tap-Hold キーでのダブルタップ検出。

#### Distance Curve / 距離カーブ

Per-key switch-travel distance curve (config version v1.17) maps normalized ADC reading to physical travel distance:

- Up to **9 control points** per key in normalized [0,255] coordinates, plus `total_travel_um` in micrometers.
- **Logarithmic curve by default** (factory/fallback): distance rises quickly near the start of travel and flattens near bottom-out, reproducing the previous lookup-table behavior.
- `num_points == 0` selects the linear (identity) curve.
- Readable/writable over Raw HID via `COMMAND_GET_DISTANCE_CURVE_CONFIG` / `COMMAND_SET_DISTANCE_CURVE_CONFIG` in 60-byte chunks.
- Persisted per profile in EEPROM; the v1.17 profile migration applies the default curve.

> [!NOTE]
> Build-tested (unit-tested) but not yet validated on physical hardware.
> ビルド（ユニット）テスト済みですが、実機での検証は未了です。

キーごとのスイッチストローク距離カーブ（設定バージョン v1.17）で、正規化した ADC 読み値を物理ストローク距離へ変換します。1キーあたり最大**9制御点**（正規化 [0,255] 座標）と `total_travel_um`（μm）、既定は**対数カーブ**（旧ルックアップテーブル相当）。Raw HID の `COMMAND_GET_DISTANCE_CURVE_CONFIG` / `COMMAND_SET_DISTANCE_CURVE_CONFIG`（60バイトチャンク）で読み書きし、プロファイルごとに EEPROM へ永続化。v1.17 移行時に既定カーブが適用されます。

#### Architecture & Improvements / アーキテクチャ改善

- **Kalman Filter**: Runtime-configurable position/velocity gains, damping, velocity thresholds, bottom-out handling, noise deadzone. Readable and persistable over Raw HID.
  カルマンフィルタ: 位置・速度ゲイン、ダンピング、速度閾値、ボトムアウト処理、ノイズ不感帯を実行時に設定・永続化可能。
- **Predictive Rapid Trigger**: Anticipates key press/release from trajectory before crossing the threshold.
  キーの軌道から閾値通過前に押下・解放を予測。
- **Event Chronological Sorting**: Key events sorted by actual event time for correct ordering during rapid input.
  キーイベントを実際の発生時刻でソートし、高速入力時の順序を保証。
- **Hold-Tap Input Buffering**: Keys pressed during undecided Tap-Hold are buffered and replayed after resolution.
  Tap-Hold 確定前に押されたキーをバッファリングし、確定後に再生。
- **Queued Raw HID Commands**: Commands dequeued from main loop, not TinyUSB callback. Host must wait for response before next command.
  Raw HID コマンドをキューイングし、メインループで処理。ホストは応答待ち必須。
- **XInput/HID Gamepad Conflict Fix**: HID gamepad automatically deactivated when XInput is active.
  XInput 有効時に HID ゲームパッドを自動無効化し二重入力を防止。
- **Stuck Key Bug Fix**: Fixed race condition where key release reports could be permanently lost during USB send timeout.
  USB 送信タイムアウト時にキー解放レポートが消失する競合状態を修正。
- **Microsecond Cooperative Scheduler**: Background tasks (RGB, encoders, etc.) run at configurable intervals without blocking the scan loop.
  マイクロ秒精度の協調的スケジューラ。RGB・エンコーダ等のバックグラウンドタスクをスキャンループをブロックせず実行。
- **USB Suspend/Resume Recovery**: Robust recovery from long suspend, including polling for missing TinyUSB resume callbacks.
  長時間サスペンドからの安定復帰。TinyUSB の resume コールバック欠落時のポーリング復旧対応。
- **Upstream Sync**: Ported STM32F446 timer adjustments and EEPROM flash wear reduction (on-demand bottom-out threshold saving).
  upstream の STM32F446 タイマー調整とEEPROMフラッシュ摩耗軽減を移植。

#### Integrated Diagnostic Mode (Mochiko40HE) / 統合診断モード

The production `mochiko40he` firmware includes runtime diagnostics without a separate flash image:

量産用 `mochiko40he` ファームウェアは、別イメージの書き込みなしに実行時診断を利用できます:

- Pause normal input, run channel-identity and raw-by-step tests, resume input
  通常入力を一時停止 → チャンネル識別・ステップ生値テスト → 入力再開
- Auto-exit after 30s without diagnostic traffic / 30秒間診断通信がないと自動終了
- No stuck keys on mode enter/exit / モード遷移時にキー固着なし

The legacy `mochiko40he_diag` environment is still generated for low-level development. Normal users should use the integrated production firmware.

旧来の `mochiko40he_diag` 環境はローレベル開発用に引き続き生成されます。通常利用は統合版を推奨。

---

## Getting Started / はじめ方

### Prerequisites / 前提条件

- [PlatformIO](https://platformio.org/)
- [Python 3](https://www.python.org/)

### Building / ビルド

1. Clone the repository:
   ```bash
   git clone https://github.com/310u/libhmk.git
   ```

2. Generate the PlatformIO config and the keyboard's `board_def.h`:
   ```bash
   python setup.py -k <keyboard>
   ```
   This writes `platformio.ini` (plus the `<keyboard>_recovery` environment) and regenerates `keyboards/<keyboard>/board_def.h` from `keyboard.json` via [`scripts/generate_board_def.py`](scripts/generate_board_def.py).
   Rerun `setup.py` whenever switching keyboard targets or editing hardware fields in `keyboard.json`.
   `platformio.ini` と `<keyboard>_recovery` 環境を生成し、`keyboard.json` から [`scripts/generate_board_def.py`](scripts/generate_board_def.py) 経由で `keyboards/<keyboard>/board_def.h` を再生成します。キーボードの切り替え時、および `keyboard.json` のハードウェア項目の変更時は都度 `setup.py` を再実行してください。

3. Build:
   ```bash
   pio run
   ```
   Outputs: `.pio/build/<keyboard>/firmware.{bin,elf}`

4. Flash via DFU (after setting `upload_protocol = dfu` in `platformio.ini`):
   ```bash
   pio run --target upload
   ```
   Or use [WebUSB DFU](https://devanlai.github.io/webdfu/dfu-util/) (recommended).
   または [WebUSB DFU](https://devanlai.github.io/webdfu/dfu-util/) を使用（推奨）。

---

## Development / 開発

### Adding a New Keyboard / 新規キーボードの追加

Create a directory under `keyboards/` with:

`keyboards/` 配下に以下を含むディレクトリを作成:

- **`keyboard.json`** (required / 必須): Single source of truth for firmware metadata, hardware (matrix, RGB, joystick, encoder, trackball, clock/timings), layout, and keymap. Schema: [`scripts/schema/keyboard.schema.json`](scripts/schema/keyboard.schema.json)
- **`board_def.h`** (generated / 自動生成): Written by `setup.py` from `keyboard.json` — do not edit by hand. Only legacy (unmigrated) keyboards or special compile-time overrides use a manual `board_def.h`.
- **`config.h`** (optional / 任意): Additional configuration beyond `keyboard.json`

See [New Keyboard Setup Guide / 新規キーボード設定ガイド](docs/new_keyboard_setup.md) for step-by-step instructions, and [keyboard.json Reference / keyboard.json リファレンス](docs/keyboard_json_reference.md) for every schema section.

For runtime architecture overview: [Runtime Architecture Notes / ランタイムアーキテクチャノート](docs/runtime_architecture.md).

### Testing / テスト

This fork includes a Unity-based native test suite: 18 test suites across 25 `native_test_*` environments. 本フォークは Unity ベースのネイティブテストスイート（18スイート、`native_test_*` 環境25）を含みます。

```bash
# Run all tests / 全テスト実行
pio test

# Run a single suite / 単一スイート実行
pio test -e native_test_<name>

# With sanitizers / サニタイザ付き
LIBHMK_NATIVE_SANITIZERS=1 python setup.py -k mochiko40he && pio test
```

### Regression / リグレッション

```bash
python scripts/run_regression.py -k mochiko40he
```

Runs: setup → native tests → firmware build + recovery build.

実行内容: setup → ネイティブテスト → ファームウェアビルド + リカバリービルド。

### CI Verification Order / CI 検証順序

1. `python setup.py -k mochiko40he && pio test`
2. `LIBHMK_NATIVE_SANITIZERS=1 python setup.py -k mochiko40he && pio test`
3. `LIBHMK_STACK_USAGE=1 python setup.py -k <kb> && pio run`
4. `python scripts/check_memory_budget.py --keyboard <kb> --elf .pio/build/<kb>/firmware.elf`
5. `python scripts/summarize_stack_usage.py --build-dir .pio/build/<kb> --keyboard <kb>`

### Matrix Divider Validation (Mochiko40HE) / マトリクス分周検証

```bash
python setup.py -k mochiko40he
pio run -e mochiko40he_matrix_div4   # divider=4, budget=125μs
pio run -e mochiko40he_matrix_div2   # divider=2, budget=63μs
```

### Diagnostics / 診断

Integrated diagnostic mode commands available via `hmkconf` Developer tab or CLI:

診断モードは `hmkconf` の Developer タブまたは CLI から操作可能:

```bash
python scripts/scan_rate_diag.py --diagnostic-mode status
python scripts/scan_rate_diag.py --diagnostic-mode on
python scripts/scan_rate_diag.py --diagnostic-mode off
```

---

## Porting / 移植

### Hardware Driver Structure / ハードウェアドライバ構造

| Directory | Purpose / 用途 |
|---|---|
| [`hardware/`](hardware/) | MCU-specific headers (`config.h`, `board_def.h`) |
| [`include/hardware/`](include/hardware/) | Driver interface headers / ドライバインタフェース |
| [`src/hardware/`](src/hardware/) | Driver implementations / ドライバ実装 |
| [`linker/`](linker/) | Linker scripts / リンカスクリプト |
| [`scripts/drivers.py`](scripts/drivers.py) | Driver configuration per MCU; each implements the `Driver` class / MCUごとのドライバ設定 |

Supported MCUs / 対応MCU:

| MCU | Peripherals / 周辺機能 |
|---|---|
| **AT32F405xx** | ADC matrix, joystick, encoder, DMA/PWM RGB, SPI, I2C, timer |
| **STM32F446xx** | ADC matrix, joystick, encoder, bitbang RGB, SPI, I2C, timer |

---

## Acknowledgements / 謝辞

- [peppapighs/libhmk](https://github.com/peppapighs/libhmk) — Original project this fork is based on / 本フォークのベース
- [hathach/tinyusb](https://github.com/hathach/tinyusb) — USB stack
- [qmk/qmk_firmware](https://github.com/qmk/qmk_firmware) — EEPROM emulation, matrix scanning, RGB Matrix / RGB Light effects
- [@riskable](https://github.com/riskable) — Pioneering custom Hall-effect keyboard firmware / ホール効果キーボードファームウェアの先駆者
- [@heiso](https://github.com/heiso/) — [macrolev](https://github.com/heiso/macrolev) and development support / 開発支援
- [Wooting](https://wooting.io/) — Pioneering Hall-effect gaming keyboards / ホール効果ゲーミングキーボードの先駆者
- [GEONWORKS](https://geon.works/) — Venom 60HE PCB, inspiring the web configurator
- [@devanlai](https://github.com/devanlai) — [WebUSB DFU](https://devanlai.github.io/webdfu/dfu-util/)
