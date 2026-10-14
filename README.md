# DummyMouse for Raspberry Pi Pico 2 W

A small Bluetooth Low Energy (BLE) HID mouse for the Raspberry Pi Pico 2 W. It sends one relative one-pixel movement once per minute and cycles through **up, down, left, and right**.

日本語版: [README.ja.md](README.ja.md)

## Features

- Advertises as `DummyMouse` and uses the standard HID over GATT profile.
- Starts the one-minute timer when the host enables HID input notifications.
- Sends a one-pixel relative movement in the order up, down, left, right, then repeats.
- Accepts Just Works pairing and stores bond data in flash.
- Supports one active Bluetooth connection at a time.
- Reports connection, notification, and send status over USB CDC.

The direction sequence starts at up after a device reboot. It advances only after BTstack accepts a report for sending.

## Hardware and software

- Raspberry Pi Pico 2 W
- A USB data cable for programming and optional serial logs
- A computer with Bluetooth Low Energy support
- Raspberry Pi Pico SDK 2.2.0, CMake, Ninja, Python 3, Git, and an Arm GNU Toolchain

The project uses BTstack provided by the Pico SDK. See the [Pico SDK documentation](https://datasheets.raspberrypi.com/pico/raspberry-pi-pico-c-sdk.pdf) for SDK setup details.

## Build on Windows

Clone the Pico SDK with its submodules. For example:

```powershell
git clone --branch 2.2.0 --recurse-submodules https://github.com/raspberrypi/pico-sdk.git C:\pico\pico-sdk
```

Install CMake, Ninja, Python 3, Git, and the Arm GNU Toolchain. Make sure `cmake` and `ninja` are on `PATH`, then configure and build:

```powershell
$env:PICO_SDK_PATH = 'C:\pico\pico-sdk'

cmake -S . -B build -G Ninja `
  -DPICO_BOARD=pico2_w `
  -DPICO_TOOLCHAIN_PATH='C:\path\to\arm-none-eabi\bin'

cmake --build build
```

Set `PICO_TOOLCHAIN_PATH` to the `bin` directory containing `arm-none-eabi-gcc.exe`. If your toolchain is already discoverable by CMake, you can omit that option. The UF2 image is written to `build/dummymouse.uf2`.

To change the interval, configure with `-DDUMMYMOUSE_REPORT_INTERVAL_MS=<milliseconds>`. For example, use `-DDUMMYMOUSE_REPORT_INTERVAL_MS=15000` for a 15-second diagnostic build. The default is 60,000 ms.

## Flash the Pico

1. Unplug the Pico from USB.
2. Hold the **BOOTSEL** button while connecting it to USB.
3. Copy `build/dummymouse.uf2` to the mounted `RP2350` drive.
4. The Pico reboots and starts advertising as `DummyMouse`.

## Pair with a computer

Open the computer's Bluetooth settings, add a Bluetooth device, and select `DummyMouse`. Complete the Just Works pairing prompt if shown. Once the host connects and enables HID input notifications, the first movement is sent one minute later.

If a previously paired copy of the device reports a driver error after firmware changes, remove `DummyMouse` from the host's Bluetooth settings and pair it again. This clears the host's cached GATT information.

## USB serial logs

Open the Pico's USB serial port at **115200 baud, 8 data bits, no parity, 1 stop bit**, with DTR enabled. The log reports connection and notification events, send readiness, movement direction, and the BTstack send status. A status of `0x00` means BTstack accepted the report for sending.

## Project layout

- `src/main.c` implements BLE advertising, HID reports, the timer, and USB logs.
- `dummymouse.gatt` defines the GAP, Device Information, and HID services.
- `include/btstack_config.h` contains the BTstack configuration for this firmware.
- `pico_sdk_import.cmake` locates the Pico SDK through `PICO_SDK_PATH` or a sibling `pico-sdk` directory.

## Licensing

This repository does not yet include a license for the project-specific code. Public visibility on GitHub does not by itself grant permission to reuse it. The Pico SDK and BTstack have separate terms; the BTstack version used by the Pico SDK is free for non-commercial use, and commercial use requires a separate license. See the [Pico SDK license](https://github.com/raspberrypi/pico-sdk/blob/2.2.0/LICENSE.TXT) and [BTstack license](https://github.com/bluekitchen/btstack/blob/master/LICENSE).
