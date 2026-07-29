    # libhmk リポジトリのレポート

## 1. プロジェクト概要
libhmkは、Hall-effectキーボードファームウェア用のライブラリで、ジョイスティックサポート、ロータリーエンコーダーサポート、RGBライト、コンボ/マクロキー、および他の改善が追加されています。

## 2. ファイル構造
- `README.md`: プロジェクトの概要と使い方を説明しています。
- `keyboards/mochiko40he/keyboard.json`: Mochiko40HEキーボードの設定ファイルです。

## 3. keyboard.json の詳細

### キーボード情報
- **名称**: Mochiko40HE
- **製造元**: Lady Tortie
- **メンテナ**: satoyu

### USB 情報
- **VID**: `0x0108`
- **PID**: `0x0111`

### キーボード設定
- **プロファイル数**: 4
- **レイヤー数**: 4
- **キー数**: 41
- **高度なキー数**: 32

### 機能
- **RGB**: true
- **Joystick**: true

### RGB 設定
- **LED マップ**: 各キーの LED 場所を指定
- **Mod キー**: 特殊な LED を管理するキーを指定

### ハードウェア設定
- **HSE 値**: 12000000
- **ドライバー**: at32f405xx

### メモリ予算
- **最小 Flash ヘッドルーム**: 32768
- **最小 RAM ヘッドルーム**: 16384
- **最大スタックフレーム**: 320

### アナログ設定
- **ADC インバート**: false
- **MUX 設定**: ADC 電源と入力ピンのマッピング
- **ラWM 設定**: 原子検出信号とキーマトリックスのマッピング

### カレンブレーション
- **初期静止値**: 2400
- **初期ボトムアウトしきい値**: 650

### レイアウト
- **ラベル**: default
- **キーマップ**: 各キーのキーコードを指定

### キーマップ詳細
- 各レイヤーで異なるキーコードが割り当てられています。
- `MO(x)` は x 番目のレイヤーに切り替えます。

## 4. アルゴリズムについて
libhmk では、主なアルゴリズムと機能の実装方法を以下にまとめます：

### アナログ入力
- 各キーのアクティベーションポイントをカスタマイズ可能です。
- キーの位置変化とその方向に基づいてキープレスまたはリリースを登録します。
- 持続的な素早くトリガー（Rapid Trigger）は、キーが完全に解放されたときにのみ非活性になります。

### Null Bind (SOCD + Rappy Snappy)
- 2つのキーを監視し、選択された動作に基づいてどちらのキーがアクティブかを選択します。

### ダイナミックキーストローク
- 単一のキーに最大4つのキーコードを割り当て、異なる部分のキーストロークに対して異なるアクションが可能です。

### Tap-Hold
- キーがタップまたはホールドされたかどうかに基づいて異なるキーコードを送信します。

### Toggle
- キープレスとキーリリースの間でトグルします。キーを押し続けると通常の動作を行います。

### N-Key Rollover
- 6キークロロバーサポートが自動的にBIOSにフォールバックします。

### 自動調整
- ユーザー介入なしでアナログ入力を自動調整します。

### EEPROM 模擬
- 内部フラッシュメモリを使用してEEPROMを模倣します。

### Web Configurator
- [hmkconf](https://github.com/310u/hmkconf) を使用してファームウェアを構成し、再コンパイルすることなく設定できます。

### Tick Rate
- Tap-Holdとダイナミックキーストロークのカスタマイズ可能なティックレートがあります。

### 8kHz ポーリングレート
- ソフトウェアサポートが一部のマイクロコントローラ（e.g., AT32F405xx）で利用可能です。

### Gamepad
- XInputゲームパッドモードがWindows用、HIDゲームパッドフォールバックがLinux/macOS用です。

## 5. RPT（Predictive Rapid Trigger）追加レポート

### 5.1 概要
RPT（Predictive Rapid Trigger）は、ホールエフェクトキーボードにおけるキー離脱応答を改善するための機能である。従来の Rapid Trigger（RT）は、キーが最下点から `rt_up` 分以上戻った時点で離脱を判定していた。RPT は押下進行中の **急減速** を検出し、ユーザが離し始めた瞬間に `rt_up` を一時的に最小値（1 distance unit）まで引き下げることで、物理的な戻り動作が顕在化する前に離脱判定を行う。

本機能はビルド時に `MATRIX_RT_PREDICTIVE_ENABLE=1` で有効化され、デフォルトは無効（0）である。

### 5.2 追加の背景と目的
#### 背景
- 高速タイピングやゲーミング用途では、キー離脱の遅延が入力応答に影響を与える。
- 従来の RT では、指が実際に離し始めてから `rt_up` 分の変位が発生するまで離脱が遅れる。

#### 目的
- 急減速を検出することで、離脱判定を先行させる。
- ADC のノイズや微振動には反応せず、意図的な離し動作のみを検出する。
- 既存 RT ロジックと共存し、有効時のみ追加判定を行う。

### 5.3 実装詳細
#### 5.3.1 設定マクロ
`include/matrix.h` に追加されたマクロ：

| マクロ | デフォルト | 説明 |
| --- | --- | --- |
| `MATRIX_RT_PREDICTIVE_ENABLE` | 0 | RPT 機能の有効/無効 |
| `MATRIX_RT_DECEL_THRESHOLD` | 15 | 減速度検出閾値（ADC 値/scan） |

#### 5.3.2 キー状態構造体の拡張
`key_state_t` に速度計算用フィールドを追加：

- `prev_adc_filtered`: 前回スキャンのフィルタ済み ADC 値
- `prev_velocity`: 前回スキャンの符号付き ADC 速度

これにより、各スキャンで `velocity = current - prev`、さらに `acceleration = velocity - prev_velocity` を計算し、減速度の有無を判定する。

#### 5.3.3 matrix.c ロジック
`src/matrix.c` の `matrix_scan_fast()` 内で、以下の処理を追加：

1. フィルタ済み ADC 値から速度を計算
2. 前回速度との差分から加速度を計算
3. キーが `is_pressed` かつ `key_dir == KEY_DIR_DOWN` の状態で、速度と加速度の符号が逆（急減速）かつ、加速度の絶対値が `MATRIX_RT_DECEL_THRESHOLD` を超えた場合、RPT トリガーと判定
4. トリガー時、`effective_rt_up = 1` にして、次のわずかな上向き変位で即離脱

```c
#if MATRIX_RT_PREDICTIVE_ENABLE
uint8_t effective_rt_up = rt_up;
if (state->is_pressed && state->key_dir == KEY_DIR_DOWN &&
    ((int32_t)velocity * (int32_t)acceleration < 0) &&
    (acceleration > MATRIX_RT_DECEL_THRESHOLD ||
     acceleration < -MATRIX_RT_DECEL_THRESHOLD)) {
    effective_rt_up = 1;
}
#endif
```

### 5.4 ビルド・有効化方法
`setup.py` に `mochiko40he_prt` 環境を追加。以下のフラグを含む：

```c
-DMATRIX_RT_PREDICTIVE_ENABLE=1
-DMATRIX_RT_DECEL_THRESHOLD=15
```

ビルドコマンド：

```bash
python setup.py -k mochiko40he
pio run -e mochiko40he_prt
```

なお、RPT を有効化したビルドでは、matrix スキャンを 16kHz 級（divider=2）で動作させるため、`board_def.h` において以下の調整も行われた：

- `MATRIX_PROCESSING_DIVIDER = 2`
- `MATRIX_SCHEDULER_BUDGET_US = 63`
- 各種背景タスク（USB、layout、XInput、RGB など）の実行間隔を引き上げ
- `HMK_USB_TASK_INTERVAL` 等の間隔マクロを追加

### 5.5 検証ツール
RPT の実機検証用に以下のツールを追加した：

| ファイル | 言語 | 用途 |
| --- | --- | --- |
| `scripts/prt_test.py` | Python | 指定キーの ADC/distance を CSV としてログ取得 |
| `scripts/prt_test.js` | Node.js | Python 版と同等の取得ツール |
| `prt_calibrate_and_capture.js` | Node.js | キャリブレーション後、長時間のキー動作をキャプチャ |
| `analyze_prt_state.js` | Node.js | キャプチャ CSV に対し、RPT 有効/無効のイベント比較をシミュレート |

これらを用いて、RPT 有効時の離脱位置と無効時の離脱位置を比較し、効果を定量化する。

### 5.6 テストと影響範囲
- `test/test_hid/test_hid.c` の Raw HID 診断ペイロード型を更新
  - `last_mode_counts` の型を `uint16_t` から `uint8_t` に縮小
  - `idle_keys_detected`、`active_keys_detected`、`reserved[2]` を追加
- `include/wear_leveling.h` のビットフィールドレイアウトを、コンパイラ依存を避けるため明示的ビット演算に変更
- 既存の RT 動作は `MATRIX_RT_PREDICTIVE_ENABLE=0` の場合、完全に無効化され、コンパイル時分岐でサイズ/速度への影響を回避

### 5.7 未検証・今後の課題
- 実機での長時間タイピング/ゲーミングにおける体感検証
- `MATRIX_RT_DECEL_THRESHOLD` の最適化（15 は初期値）
- 他のキーボード定義（mochiko40he 以外）への適用検証
- 16kHz 運用時の消費電力と熱への影響測定

## 6. 次のステップ
1. README.md の内容を要約する。
2. keyboard.json の詳細情報を整理する。
3. レポートを完成させる。