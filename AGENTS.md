# AGENTS.md — libhmk (310u fork)

Fork building Hall-effect keyboard firmware for AT32F405xx / STM32F446xx MCUs using PlatformIO.

## Build system

- `python setup.py -k <keyboard>` generates `platformio.ini` (gitignored) + `include/metadata.h` (gitignored).
- Build: `pio run -e <keyboard>` produces `.pio/build/<keyboard>/firmware.{bin,elf}`.
- Rerun `setup.py` whenever switching keyboard targets or changing `keyboard.json`.

## Tests

- **Unity** test framework, embedded as a PlatformIO native test platform.
- Run all: `pio test` (runs every `native_test_*` env).
- Single env: `pio test -e native_test_<name>`. See `setup.py` for env list (or `pio test --list-environments`).
- With sanitizers: `LIBHMK_NATIVE_SANITIZERS=1 python setup.py -k <keyboard> && pio test`.
- Regression: `python scripts/run_regression.py -k mochiko40he` (setup → native tests → firmware build + recovery).

## CI / verification order

1. `python setup.py -k mochiko40he && pio test`
2. `LIBHMK_NATIVE_SANITIZERS=1 python setup.py -k mochiko40he && pio test`
3. Firmware build per keyboard: `LIBHMK_STACK_USAGE=1 python setup.py -k $kb && pio run`
4. `python scripts/check_memory_budget.py --keyboard $kb --elf .pio/build/$kb/firmware.elf`
5. `python scripts/summarize_stack_usage.py --build-dir .pio/build/$kb --keyboard $kb`

## Code style

- Clang-format: LLVM style (`.clang-format`).
- Build flags: `-Werror -Wall -Wextra -Wsign-conversion -Wswitch-default -Wswitch -Wdouble-promotion -Wstrict-prototypes -Wno-unused-parameter`.
- Clang-tidy: specific bugprone/clang-analyzer checks, warnings-as-errors, `FormatStyle: none`.

## Architecture

- `analog_scan.c` → `matrix.c` → `layout.c` → HID is the core input pipeline.
- `profile_runtime.c` is the single reload point for all profile-backed state.
- `usb_runtime.c` owns TinyUSB suspend/resume; board code only provides HW setup.
- Raw HID is single-flight (host must wait for response before next command).
- `input_routing.h` is the boundary for non-matrix inputs (digital GPIO, encoder virtual keys).

## Keyboard definitions

- `keyboards/<name>/` requires `keyboard.json` (schema: `scripts/schema/keyboard.schema.json`), optional `board_def.h` and `config.h`.
- `board_def.h` sets hardware macros (RGB pins, joystick, encoder, matrix tuning).
- `keyboard.json.analog.backend`: `"mcu_adc"` (default) or `"spi_adc"` (future).

## Key quirks

- `libhmk/` subdirectory is an orphaned nested git repo — do not touch.
- **Mochiko40HE** is the primary dev target. Matrix validation builds: `mochiko40he_matrix_div4`, `mochiko40he_matrix_div2`.
- Binary clock time sync needs `hmkconf` RGB tab open (host pushes periodically).
- Joystick switch is key index 40 (board_def.h); raw ADC inputs use indices 41+.
- SPI ADC backend for ADS7953 is reserved but not yet implemented.
- Slider and encoder firmware are build-tested but not validated on hardware.

# AI development rules

## 基本方針

- ユーザーはコードを直接編集しない
- 実装、ビルド、エラー修正まで一貫して行う
- 作業開始前に既存構造を調査する
- 大規模変更の前に実装計画を提示する
- 一度に一つの機能だけ変更する
- 変更後は必ずビルドする
- ビルドエラーを残したまま完了扱いにしない
- 動作確認できない項目は「未検証」と明記する
- 既存機能を理由なく削除しない
- 外部依存を追加する前に理由を説明する
- 作業終了時に変更ファイルとGit diffを要約する

## ハードウェア関連

- GPIO割り当てを推測しない
- MCU型番、ピン配置、クロック設定を既存ファイルから確認する
- USB、ブートローダー、フラッシュ設定を無断で変更しない
- センサの軸方向や取り付け方向を推測しない
- 実機依存の確認項目を一覧化する
- ファームウェア書き込みは明示的に指示された場合だけ行う

## Git

- 作業開始時にgit statusを確認する
- ユーザーの既存変更を上書きしない
- git reset --hardやgit clean -fdを無断で実行しない
- コミットは明示的に依頼された場合だけ行う