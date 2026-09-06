# コード精査レポート (2026-09-06)

`src/` 全体を対象に `platformio check`（cppcheck）による静的解析と、コード構造・関数サイズ・テスト網羅・未検証機能の棚卸しを実施した結果の記録。

## 1. 静的解析結果（`pio check` = cppcheck, 合計 263 件）

| 重要度 | 件数 | 内容 | 判定 |
|---|---|---|---|
| high:error | 1 | `joystick.c:26` `#error Unsupported GPIO backend` | 誤検知（cppcheck が `__has_include("at32f402_405.h")` を解決できず、バックエンドが未選択と誤判定。実ビルドは正常） |
| medium:warning | 4 | `migration.c` 無意味なポインタ加算 | **真の指摘（デッドコード）。→ 修正済み** |
| low:portability | 1 | `migration.c:444` float* ⇔ uint8_t* キャスト | 意図的（構造体のバイト列コピー） |
| low:style | ~241 | `unusedFunction`, `constVariablePointer` 等 | 単一ファイル解析の限界。大部分はホストテスト視点での「未使用」検知 |

### low:style の個別真偽判定

| 箇所 | 指摘 | 判定 |
|---|---|---|
| `layout.c:251` | unsigned `< 0` | 誤検知。`RGB_EFFECT_OFF = 0` との `<= 0` 比較で意味あり |
| `layout.c:615` | `!rhs->pressed` 常に真 | 意図的（コメント付きの明示）。実害なし |
| `matrix.c:613` | `bottom_out_hold == 0` 常に false | 誤検知。uint16_t オーバーフローガード（`bottom_out_hold_scans + 1` が 65536 → 0 にラップするのを防ぐ） |
| `rgb_reactive.c:123` | `ax > ay` 常に false | 誤検知。uint16_t 同士の正常な比較 |
| `wear_leveling.c:229` | `write_len > 2` 常に false | 誤検知。`WL_MAX_BYTES_PER_ENTRY = 6` で 3〜6 バイト時に第2ワードを書く意味のある判定 |

### 対応した真の指摘（フェーズ1）

`migration.c` の `v1_F_profile_config_func`（892-893 行）と `v1_10_profile_config_func`（930-931 行）から、後続利用のない `dst += sizeof(joystick_config); src += ...;` を削除した。

- 理由: `profile_config_func` は `dst`/`src` を「プロファイル領域先頭を指す値」として受け取る。コピー本体は `migration_memcpy(&dst, &src, ...)`（ポインタのポインタで進める）と生 `memcpy(dst, &joystick_config, ...)` が担う。末尾の `dst += / src +=` は次のステップが無いため完全にデッドコード。
- 検証: `pio test -e native_test_migration` → 9 ケース全パス。

## 2. 巨大関数一覧（`src/` 直下 .c 内、上位）

| 行数 | 関数 | ファイル |
|---|---|---|
| 619 | `command_process` | commands.c |
| 301 | `rgb_static_render` | rgb_static.c |
| 276 | `matrix_scan_fast` | matrix.c |
| 257 | `rgb_task` | rgb.c |
| 227 | `xinput_task` | xinput.c |
| 113 | `layout_register` | layout.c |
| 99 | `main` | main.c |

### `command_process`（53 コマンドの平坦 switch）

- 構造: `success`（bool）をローカル管理し、末尾で `out->command_id` に反映。`COMMAND_VERIFY(cond)` マクロが `success = false; break;` を埋め込む。
- 分割の難しさ: `success` / `break` / `out_buf` の共有と、`#if` によるコマンド群の切替。
- **実施内容（フェーズ3）**: 末尾の診断 4 コマンド（`COMMAND_GET_MATRIX_SCAN_DIAGNOSTICS` / `COMMAND_GET_ANALOG_SCAN_DIAGNOSTICS`）の `out` 書き込み部分を、`command_fill_matrix_scan_diagnostics()` / `command_fill_analog_scan_diagnostics()` の static ヘルパーへ機械抽出した。`COMMAND_VERIFY` を使わない純粋な書き込み処理のみのため、挙動維持。
- 検証: `pio test -e native_test_commands` → 20 ケース全パス（診断系 4 テスト含む）。

## 3. 未検証機能・テスト不足の棚卸し

| 項目 | 状態 | 備考 |
|---|---|---|
| slider | ビルドのみ・実機未検証 | `src/slider.c`（44 行）、テストディレクトリなし |
| encoder | ビルドのみ・実機未検証 | `src/encoder.c`（368 行）、`test/test_encoder` あり（ホストテストのみ） |
| SPI ADC バックエンド（ADS7953） | 未実装（リザーブ） | ソース内に ADS7953 / spi_adc 参照なし。`keyboard.json.analog.backend: "spi_adc"` は将来拡張 |
| ジョイスティック | 実機依存の確認多数 | SW はキー index 40、raw ADC は 41+。軸方向・取り付け方向は要実機確認 |
| RGB バイナリクロック | ホスト連動前提 | hmkconf の RGB タブが開いている時のみホストが時刻を定期プッシュ |

### 補足（テスト網羅の概況）

- 既存テスト: 18 環境（`pio test`）。直近で全パスを確認済み。
- `test_encoder` は存在するが `test_slider` は存在しない（slider はテスト未整備）。
