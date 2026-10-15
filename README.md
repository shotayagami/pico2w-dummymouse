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
- A button on GP14 pauses and resumes mouse movement while keeping the BLE connection.
- An external LED on GP15 indicates connection, operation, and pause status.
- An external pilot LED on GP16 stays on while the firmware is running.

The direction sequence starts at up after a device reboot. It advances only after BTstack accepts a report for sending.

## Status LED and pause/resume button

Power off the Pico 2 W before wiring. GPIO numbers differ from the physical header pin numbers.

| Part | GPIO | Physical pin | Connection |
|---|---:|---:|---|
| Status LED | GP15 | 20 | GP15 → 330 Ω resistor → LED anode (long leg); connect the cathode (short leg) to GND on physical pin 18 |
| Momentary normally-open button | GP14 | 19 | Connect between GP14 and GND on physical pin 18 |
| Pilot LED | GP16 | 21 | GP16 → 330 Ω resistor → LED anode (long leg); connect the cathode (short leg) to GND on physical pin 23 |
| Common ground | — | 18 | Shared by the LED and button |

Connect pilot LED ground (physical pin 23) to the same common ground as the status LED and button (physical pin 18). Use a 330 Ω series resistor for each LED. The pilot LED stays on even while mouse movement is paused.

The button uses the internal pull-up, so no external resistor is needed. Each press toggles pause/resume. The status LED turns off while paused. Resuming restarts the 60-second delay when HID notifications are enabled. The device starts active after power-up and does not persist the paused state.

The LED is off while disconnected, blinks quickly (about four times per second) while connected and waiting for HID notifications, blinks slowly (one short pulse every two seconds) while operating, and is off while paused.

## Hardware and software

- Raspberry Pi Pico 2 W
- A USB data cable for programming and optional serial logs
- A computer with Bluetooth Low Energy support
- Raspberry Pi Pico SDK 2.2.0, CMake, Ninja, Python 3, Git, and an Arm GNU Toolchain
- Rust 1.85 or later and the `thumbv8m.main-none-eabi` target (Rust build only)

The project uses BTstack provided by the Pico SDK. See the [Pico SDK documentation](https://datasheets.raspberrypi.com/pico/raspberry-pi-pico-c-sdk.pdf) for SDK setup details.

## Build on Windows

Clone the Pico SDK with its submodules. For example:

```powershell
git clone --branch 2.2.0 --recurse-submodules https://github.com/raspberrypi/pico-sdk.git C:\pico\pico-sdk
```

Install CMake, Ninja, Python 3, Git, and the Arm GNU Toolchain. Make sure `cmake` and `ninja` are on `PATH`. The default build uses the C application:

```powershell
$env:PICO_SDK_PATH = 'C:\pico\pico-sdk'

cmake -S . -B build -G Ninja `
  -DPICO_BOARD=pico2_w `
  -DPICO_TOOLCHAIN_PATH='C:\path\to\arm-none-eabi\bin'

cmake --build build
```

### Build the Rust application instead

Install Rust with rustup, add its Cortex-M33 target, and configure a separate build directory:

```powershell
rustup target add thumbv8m.main-none-eabi
$env:PICO_SDK_PATH = 'C:\pico\pico-sdk'

cmake -S . -B build-rust -G Ninja `
  -DPICO_BOARD=pico2_w `
  -DDUMMYMOUSE_RUST=ON `
  -DPICO_TOOLCHAIN_PATH='C:\path\to\arm-none-eabi\bin'

cmake --build build-rust
```

The Rust application uses `no_std` and Pico SDK's BTstack for BLE/HID. A small C boundary layer initializes the SDK and provides access to BTstack and GPIO APIs. The UF2 image is written to `build-rust/dummymouse.uf2` when picotool is available. The C and Rust toolchains are configured for compatible Pico 2 W Cortex-M33 targets.

Set `PICO_TOOLCHAIN_PATH` to the `bin` directory containing `arm-none-eabi-gcc.exe`. If your toolchain is already discoverable by CMake, you can omit that option. The UF2 image is written to `build/dummymouse.uf2`.

The report interval can be changed for either build with `-DDUMMYMOUSE_REPORT_INTERVAL_MS=<milliseconds>` at CMake configure time. For example, add `-DDUMMYMOUSE_REPORT_INTERVAL_MS=15000` to the configure command for a 15-second diagnostic build. The default is 60,000 ms.

## Flash the Pico

1. Unplug the Pico from USB.
2. Hold the **BOOTSEL** button while connecting it to USB.
3. Copy the UF2 for the selected build to the mounted `RP2350` drive:
   - C build: `build/dummymouse.uf2`
   - Rust build: `build-rust/dummymouse.uf2`
4. The Pico reboots and starts advertising as `DummyMouse`.

## Pair with a computer

Open the computer's Bluetooth settings, add a Bluetooth device, and select `DummyMouse`. Complete the Just Works pairing prompt if shown. Once the host connects and enables HID input notifications, the first movement is sent one minute later.

If a previously paired copy of the device reports a driver error after firmware changes, remove `DummyMouse` from the host's Bluetooth settings and pair it again. This clears the host's cached GATT information.

## USB serial logs

Open the Pico's USB serial port at **115200 baud, 8 data bits, no parity, 1 stop bit**, with DTR enabled. The log reports connection and notification events, send readiness, movement direction, and the BTstack send status. A status of `0x00` means BTstack accepted the report for sending.

## Project layout

- `src/main.c` is the C application.
- `rust/src/lib.rs` contains the Rust application state machine; `src/rust_main.c` provides its Pico SDK and BTstack boundary layer.
- `dummymouse.gatt` defines the GAP, Device Information, and HID services.
- `include/btstack_config.h` contains the BTstack configuration for this firmware.
- `pico_sdk_import.cmake` locates the Pico SDK through `PICO_SDK_PATH` or a sibling `pico-sdk` directory.

## Licensing

The project-specific code, documentation, and configuration in this repository are dedicated to the public domain under **CC0 1.0 Universal**, except for third-party components. See [LICENSE](LICENSE) and the [CC0 legal code](https://creativecommons.org/publicdomain/zero/1.0/legalcode.en).

The Pico SDK and BTstack are separate third-party dependencies and are not relicensed by CC0. The BTstack version used by the Pico SDK is free for non-commercial use; commercial use requires a separate license. See the [Pico SDK license](https://github.com/raspberrypi/pico-sdk/blob/2.2.0/LICENSE.TXT) and [BTstack license](https://github.com/bluekitchen/btstack/blob/master/LICENSE).
