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
- GP14のボタンでマウス移動を停止／再開できます。停止中もBLE接続は維持します。
- GP15の外付けLEDで接続待ち、接続中、動作中、停止中の状態を確認できます。
- GP16のパイロットLEDは、ファームウェア起動中に常時点灯します。

方向はPicoの再起動後に「上」から始まります。BTstackが送信を受け付けた後にだけ、次の方向へ進みます。

## LEDと停止／再開ボタン

Pico 2 Wの電源を切ってから、次のように接続します。GPIO番号と基板上の物理ピン番号は異なるので注意してください。

| 部品 | GPIO | 物理ピン | 接続 |
|---|---:|---:|---|
| 状態LED | GP15 | 20 | GP15 → 330 Ω抵抗 → LEDの長い足（＋）。短い足（－）を物理ピン18のGNDへ |
| モーメンタリーボタン（通常開） | GP14 | 19 | GP14と物理ピン18のGNDの間に接続 |
| パイロットLED | GP16 | 21 | GP16 → 330 Ω抵抗 → LEDの長い足（＋）。短い足（－）を物理ピン23のGNDへ |
| 共通GND | — | 18 | LEDとボタンで共用 |

パイロットLEDのGNDは物理ピン23を使い、状態LED／ボタンのGND（物理ピン18）と共通にします。どちらのLEDにも330 Ωの直列抵抗を入れてください。パイロットLEDはマウス機能の停止中も点灯します。

ボタンは内部プルアップを使用するため、外付け抵抗は不要です。押すたびに停止／再開が切り替わります。停止中は状態LEDが消灯し、再開後は通知が有効な接続であれば60秒後に次の移動を送ります。電源投入時は動作状態で始まり、停止状態は保存されません。

LED表示は、未接続では消灯、BLE接続中でHID通知待ちでは速く点滅（約4回／秒）、HID通知が有効な動作中は遅く点滅（2秒ごとに短く1回）、停止中は消灯です。

## 必要なもの

- Raspberry Pi Pico 2 W
- 書き込み用のUSBデータケーブル（シリアルログ取得にも使用）
- Bluetooth Low Energyに対応したPC
- Raspberry Pi Pico SDK 2.2.0、CMake、Ninja、Python 3、Git、Arm GNU Toolchain
- Rust 1.85以降と`thumbv8m.main-none-eabi`ターゲット（Rust版ビルド時）

このプロジェクトはPico SDKに含まれるBTstackを使用します。SDKのセットアップ詳細は[Raspberry Pi Pico C/C++ SDKドキュメント](https://datasheets.raspberrypi.com/pico/raspberry-pi-pico-c-sdk.pdf)を参照してください。

## Windowsでビルド

サブモジュールを含めてPico SDKを取得します。

```powershell
git clone --branch 2.2.0 --recurse-submodules https://github.com/raspberrypi/pico-sdk.git C:\pico\pico-sdk
```

CMake、Ninja、Python 3、Git、Arm GNU Toolchainをインストールします。`cmake` と `ninja` にPATHが通っている状態で、次を実行します。既定ではC版アプリケーションをビルドします。

```powershell
$env:PICO_SDK_PATH = 'C:\pico\pico-sdk'

cmake -S . -B build -G Ninja `
  -DPICO_BOARD=pico2_w `
  -DPICO_TOOLCHAIN_PATH='C:\path\to\arm-none-eabi\bin'

cmake --build build
```

### Rust版をビルドする場合

rustupでRustをインストールし、Cortex-M33ターゲットを追加してから、別のビルドディレクトリを使います。

```powershell
rustup target add thumbv8m.main-none-eabi
$env:PICO_SDK_PATH = 'C:\pico\pico-sdk'

cmake -S . -B build-rust -G Ninja `
  -DPICO_BOARD=pico2_w `
  -DDUMMYMOUSE_RUST=ON `
  -DPICO_TOOLCHAIN_PATH='C:\path\to\arm-none-eabi\bin'

cmake --build build-rust
```

Rustアプリは`no_std`で動作し、BLE/HIDにはPico SDKのBTstackを使います。SDK初期化とBTstack／GPIO APIへの接続には小さなC境界層を使用します。picotoolが利用できる場合、UF2は`build-rust/dummymouse.uf2`に生成されます。C版とRust版はPico 2 WのCortex-M33向けに互換性のある設定でビルドされます。

`PICO_TOOLCHAIN_PATH` には `arm-none-eabi-gcc.exe` がある `bin` ディレクトリを指定します。ツールチェーンがCMakeから見つかる場合は、このオプションを省略できます。UF2は `build/dummymouse.uf2` に生成されます。

レポート間隔は、どちらの版もCMakeの設定時に`-DDUMMYMOUSE_REPORT_INTERVAL_MS=<ミリ秒>`で変更できます。たとえば診断用に15秒間隔にする場合は、設定コマンドに`-DDUMMYMOUSE_REPORT_INTERVAL_MS=15000`を追加します。既定値は60,000ミリ秒です。

## Picoへ書き込む

1. PicoをUSBから外します。
2. **BOOTSEL**ボタンを押したままUSBに接続します。
3. 選んだ版のUF2をマウントされた`RP2350`ドライブへコピーします。
   - C版：`build/dummymouse.uf2`
   - Rust版：`build-rust/dummymouse.uf2`
4. Picoが再起動し、`DummyMouse` としてBLE広告を始めます。

## PCとペアリングする

PCのBluetooth設定でBluetoothデバイスを追加し、`DummyMouse` を選択します。Just Worksの確認画面が表示されたら、ペアリングを許可します。PCが接続し、HID入力通知を有効にすると、1分後に最初の移動を送信します。

ファームウェア更新後、以前にペアリングしたデバイスでドライバーエラーが出る場合は、PCのBluetooth設定から `DummyMouse` を削除して再ペアリングしてください。PC側にキャッシュされたGATT情報が更新されます。

## USBシリアルログ

PicoのUSBシリアルポートを **115200 baud、8データビット、パリティなし、1ストップビット**で開き、DTRを有効にします。接続、通知購読、送信可能状態、方向、BTstackの送信ステータスが出力されます。ステータス `0x00` はBTstackがレポートを送信キューに受け付けたことを示します。

## ファイル構成

- `src/main.c`: C版アプリケーション
- `rust/src/lib.rs`: Rustアプリケーションの状態管理。`src/rust_main.c`はPico SDKおよびBTstackとの境界層
- `dummymouse.gatt`: GAP、Device Information、HIDサービス
- `include/btstack_config.h`: このファームウェア用のBTstack設定
- `pico_sdk_import.cmake`: `PICO_SDK_PATH` または隣接する `pico-sdk` ディレクトリからSDKを探します

## ライセンス

このリポジトリ内のプロジェクト独自のコード、文書、設定ファイルは、第三者コンポーネントを除き **CC0 1.0 Universal** でパブリックドメインに提供します。詳細は[LICENSE](LICENSE)と[CC0の法的条文](https://creativecommons.org/publicdomain/zero/1.0/legalcode.en)を参照してください。

Pico SDKとBTstackは別の第三者依存物であり、CC0の対象ではありません。Pico SDKに含まれるBTstackは非商用利用向けで、商用利用には別途ライセンスが必要です。[Pico SDKのライセンス](https://github.com/raspberrypi/pico-sdk/blob/2.2.0/LICENSE.TXT)と[BTstackのライセンス](https://github.com/bluekitchen/btstack/blob/master/LICENSE)を確認してください。
