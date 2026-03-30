# Release Smoke Test

リリース前に、対象キーボードの実機で最低限確認したい項目です。CI と native test が通っていても、USB 再接続、ADC 配線、flash 永続化のような実機依存の問題はここで拾います。

## 事前準備

- 対象キーボードに最新の `firmware.bin` を書き込む
- `hmkconf` か RAW HID で設定変更できる環境を用意する
- USB 2.0/3.0 の両方で試せるとより安心

## 基本動作

- USB 接続直後に正常に列挙される
- すべてのキーが押下/解放でき、チャタリングや stuck key がない
- レイヤー切り替えとプロファイル切り替えが反映される
- 既定の keymap で主要キーが期待どおりに送信される

## 永続化

- `hmkconf` から keymap を1キー変更して再起動後も保持される
- actuation / rapid trigger 設定を変更して再起動後も保持される
- factory reset 後にデフォルト設定へ戻る
- profile duplicate / reset が壊れず、現在プロファイルでも反映される

## アナログ入力

- 全キーの rest 値が大きく外れていない
- 端のキーと中央のキーを含めて bottom-out まで反応する
- 再キャリブレーション後に入力不能キーが出ない
- 放置後の bottom-out threshold 保存が次回起動で破綻しない

## USB / ホスト復帰

- suspend 後の resume で入力が復帰する
- 長時間 suspend 後も再接続ループや無反応にならない
- OS 再起動やケーブル抜き差し後に再列挙できる
- RAW HID 応答が短いコマンドでも壊れない

## オプション機能

- RGB 搭載機は on/off、effect 変更、brightness 変更が反映される
- ジョイスティック搭載機は全モードで軸方向と押し込みが正しい
- エンコーダー搭載機は低速/高速の両方で CW/CCW が取りこぼれない
- スライダーや追加 GPIO 入力がある機種は全入力を一通り確認する

## リリース判定

- 上の項目で再現する問題がない
- `pio test` が通っている
- 対象キーボードの firmware build, size budget, stack usage report が CI で通っている
