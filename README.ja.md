# DummyMouse for Raspberry Pi Pico 2 W

Raspberry Pi Pico 2 W向けのBluetooth Low Energy（BLE）HIDマウスです。1分ごとに相対1ピクセルの移動を送り、方向は**上、下、左、右**の順に切り替わります。

English: [README.md](README.md)

## 機能

- `DummyMouse` としてBLE広告を行い、標準のHID over GATTプロファイルを使用します。
- ホストがHID入力通知を有効にすると、1分間隔のタイマーが始まります。
- 上、下、左、右の順に相対1ピクセル移動し、繰り返します。
- Just Works方式のペアリングを受け入れ、ボンド情報をフラッシュに保存します。
- Bluetooth接続は同時に1台までです。
- 接続、通知、送信状態をUSB CDCに出力します。

方向はPicoの再起動後に「上」から始まります。BTstackが送信を受け付けた後にだけ、次の方向へ進みます。

## 必要なもの

- Raspberry Pi Pico 2 W
- 書き込み用のUSBデータケーブル（シリアルログ取得にも使用）
- Bluetooth Low Energyに対応したPC
- Raspberry Pi Pico SDK 2.2.0、CMake、Ninja、Python 3、Git、Arm GNU Toolchain

このプロジェクトはPico SDKに含まれるBTstackを使用します。SDKのセットアップ詳細は[Raspberry Pi Pico C/C++ SDKドキュメント](https://datasheets.raspberrypi.com/pico/raspberry-pi-pico-c-sdk.pdf)を参照してください。

## Windowsでビルド

サブモジュールを含めてPico SDKを取得します。

```powershell
git clone --branch 2.2.0 --recurse-submodules https://github.com/raspberrypi/pico-sdk.git C:\pico\pico-sdk
```

CMake、Ninja、Python 3、Git、Arm GNU Toolchainをインストールします。`cmake` と `ninja` にPATHが通っている状態で、次を実行します。

```powershell
$env:PICO_SDK_PATH = 'C:\pico\pico-sdk'

cmake -S . -B build -G Ninja `
  -DPICO_BOARD=pico2_w `
  -DPICO_TOOLCHAIN_PATH='C:\path\to\arm-none-eabi\bin'

cmake --build build
```

`PICO_TOOLCHAIN_PATH` には `arm-none-eabi-gcc.exe` がある `bin` ディレクトリを指定します。ツールチェーンがCMakeから見つかる場合は、このオプションを省略できます。UF2は `build/dummymouse.uf2` に生成されます。

間隔は `-DDUMMYMOUSE_REPORT_INTERVAL_MS=<ミリ秒>` で変更できます。たとえば診断用に15秒間隔にする場合は `-DDUMMYMOUSE_REPORT_INTERVAL_MS=15000` を指定します。既定値は60,000ミリ秒です。

## Picoへ書き込む

1. PicoをUSBから外します。
2. **BOOTSEL**ボタンを押したままUSBに接続します。
3. `build/dummymouse.uf2` をマウントされた `RP2350` ドライブへコピーします。
4. Picoが再起動し、`DummyMouse` としてBLE広告を始めます。

## PCとペアリングする

PCのBluetooth設定でBluetoothデバイスを追加し、`DummyMouse` を選択します。Just Worksの確認画面が表示されたら、ペアリングを許可します。PCが接続し、HID入力通知を有効にすると、1分後に最初の移動を送信します。

ファームウェア更新後、以前にペアリングしたデバイスでドライバーエラーが出る場合は、PCのBluetooth設定から `DummyMouse` を削除して再ペアリングしてください。PC側にキャッシュされたGATT情報が更新されます。

## USBシリアルログ

PicoのUSBシリアルポートを **115200 baud、8データビット、パリティなし、1ストップビット**で開き、DTRを有効にします。接続、通知購読、送信可能状態、方向、BTstackの送信ステータスが出力されます。ステータス `0x00` はBTstackがレポートを送信キューに受け付けたことを示します。

## ファイル構成

- `src/main.c`: BLE広告、HIDレポート、タイマー、USBログ
- `dummymouse.gatt`: GAP、Device Information、HIDサービス
- `include/btstack_config.h`: このファームウェア用のBTstack設定
- `pico_sdk_import.cmake`: `PICO_SDK_PATH` または隣接する `pico-sdk` ディレクトリからSDKを探します

## ライセンス

このリポジトリには、プロジェクト固有コードのライセンスはまだ設定されていません。GitHubで公開しても、それだけで再利用の許可が与えられるわけではありません。Pico SDKとBTstackにはそれぞれ別の利用条件があります。Pico SDKに含まれるBTstackは非商用利用向けで、商用利用には別途ライセンスが必要です。[Pico SDKのライセンス](https://github.com/raspberrypi/pico-sdk/blob/2.2.0/LICENSE.TXT)と[BTstackのライセンス](https://github.com/bluekitchen/btstack/blob/master/LICENSE)を確認してください。
