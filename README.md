# libhmk

Fork of [peppapighs/libhmk](https://github.com/peppapighs/libhmk) — Libraries for building a Hall-effect keyboard firmware.

This fork adds joystick support, RGB lighting, combo/macro keys, and various improvements for custom keyboard builds.

## Table of Contents

- [Features](#features)
- [Fork Additions](#fork-additions)
- [Getting Started](#getting-started)
- [Development](#development)
- [Porting](#porting)
- [Acknowledgements](#acknowledgements)

## Features

### Core (from upstream)

- **Analog Input**: Customizable actuation point for each key and many other features.
- **Rapid Trigger**: Register a key press or release based on the change in key position and the direction of that change.
- **Continuous Rapid Trigger**: Deactivate Rapid Trigger only when the key is fully released.
- **Null Bind (SOCD + Rappy Snappy)**: Monitor 2 keys and select which one is active based on the chosen behavior.
- **Dynamic Keystroke**: Assign up to 4 keycodes to a single key with up to 4 actions for different parts of the keystroke.
- **Tap-Hold**: Send a different keycode depending on whether the key is tapped or held.
- **Toggle**: Toggle between key press and key release. Hold the key for normal behavior.
- **N-Key Rollover**: Support for N-Key Rollover with automatic fallback to 6-Key Rollover in BIOS.
- **Automatic Calibration**: Automatically calibrate the analog input without requiring user intervention.
- **EEPROM Emulation**: No external EEPROM required. Emulate EEPROM using internal flash memory.
- **Web Configurator**: Configure the firmware using [hmkconf](https://github.com/310u/hmkconf) without recompiling.
- **Tick Rate**: Customizable tick rate for Tap-Hold and Dynamic Keystroke.
- **8kHz Polling Rate**: Support for 8kHz polling rate on some microcontrollers (e.g., AT32F405xx).
- **Gamepad**: XInput gamepad mode, allowing the keyboard to be used as a game controller.

### Fork Additions

> [!WARNING]
> Hardware features like **Joystick Support**, **Slider Support**, and **RGB Lighting** have been implemented in software but are not yet fully tested on physical hardware.

#### Analog RGB
New lighting effects that react to key press depth:
- **Depth-Reactive Lighting**: LEDs change color or brightness based on how far each key is pressed, providing immediate visual feedback for analog input.
- **Wooting-style Effects**: Implementation of reactive layers that blend with background effects.

#### Slider Support
On-board analog slider support with 2 operating modes:

| Mode | Description |
|------|-------------|
| Disabled | Slider input is ignored |
| Volume | Controls system audio volume (Relative HID keys) |
| Gamepad | Maps to a gamepad axis (e.g., Left Trigger or Right Stick Y) |

#### Joystick Support
On-board analog joystick support with 5 operating modes:

| Mode | Description |
|------|-------------|
| Disabled | Joystick input is ignored |
| Mouse | Joystick controls the mouse cursor |
| XInput Left Stick | Maps to left analog stick in gamepad mode |
| XInput Right Stick | Maps to right analog stick in gamepad mode |
| Scroll | Joystick controls scroll wheel / horizontal scroll |

- Configurable deadzone, mouse speed, and axis calibration
- Joystick switch (click) is mapped as the 41st key and fully remappable
- Mode cycling via `SP_JOY_MODE_NEXT` keycode

#### RGB Lighting
Per-key RGB backlighting via SK6812MINI-E LEDs with 50+ effects:

- Static effects: Solid Color, Alphas/Mods, Gradients
- Animated effects: Breathing, Rainbow, Cycle, Spiral, Pinwheel, and more
- Reactive effects: Typing Heatmap, Reactive, Splash, Nexus
- Ambient effects: Digital Rain, Pixel Rain, Raindrops, Starlight, Riverflow
- Per-key color configuration and adjustable global brightness / speed

#### Combo Keys
Press multiple keys simultaneously to trigger a different keycode:

- Up to 4 trigger keys per combo
- Configurable combo term (timing window)
- Layer-aware combo detection

#### Macro Keys
Record and playback key sequences:

- Press, Release, Tap, and Delay actions
- Configurable timing for natural playback

#### Additional Improvements
- **Event Chronological Sorting**: Key events are sorted by actual event time for correct ordering during rapid input.
- **Hold-Tap Input Buffering**: Keys pressed during undecided Tap-Hold are buffered and replayed after resolution, preventing missed modifiers.
- **Double-Tap**: Optional double-tap keycode for Tap-Hold keys.
- **HID Gamepad Descriptor**: Custom HID report descriptor for gamepad compatibility on Linux and other OS without XInput support.
- **XInput/HID Conflict Fix**: The HID gamepad functionality is now intelligently deactivated when XInput mode is active to prevent dual-input conflicts on Windows.
- **Stuck Key Bug Fix**: Fixed a race condition where key release reports could be permanently lost due to USB send timeout handling.
- **Upstream Sync**: Ported STM32F446 timer adjustments and EEPROM flash wear reduction optimizations (on-demand bottom-out threshold saving).

## Getting Started

### Prerequisites

- [PlatformIO](https://platformio.org/)
- [Python 3](https://www.python.org/)

### Building the Firmware

1. Clone the repository:

   ```bash
   git clone https://github.com/310u/libhmk.git
   ```

2. Open the project in PlatformIO, such as through Visual Studio Code.

3. Run `python setup.py -k <YOUR_KEYBOARD>` to generate the `platformio.ini` file.

4. Wait for PlatformIO to finish initializing the environment.

5. Build the firmware using either `pio run` in the PlatformIO Core CLI or through the PlatformIO IDE's "Build" option. The firmware binaries will be generated in the `.pio/build/<YOUR_KEYBOARD>/` directory with the following files:

   - `firmware.bin`: The binary firmware file
   - `firmware.elf`: The ELF firmware file

6. Flash the firmware to your keyboard using your preferred method (e.g., DFU, ISP). If your keyboard has a DFU bootloader, you can set `upload_protocol = dfu` in `platformio.ini` and use the command `pio run --target upload` or the PlatformIO IDE's "Upload" option while the keyboard is in DFU mode. If your browser supports WebUSB, you can also use [WebUSB DFU](https://devanlai.github.io/webdfu/dfu-util/) (Recommended method).

## Development

### Developing a New Keyboard

To develop a new keyboard, create a new directory under `keyboards/` with your keyboard's name. This directory should include the following files:

- `keyboard.json`: A JSON file containing metadata about your keyboard, used for both firmware compilation and the web configurator. Refer to [`scripts/schema/keyboard.schema.json`](scripts/schema/keyboard.schema.json) for the schema.
- `config.h` (Optional): Additional configuration header for your keyboard to define custom configurations beyond what's specified in `keyboard.json`.

For a step-by-step guide on creating a new keyboard definition, see the [New Keyboard Setup Guide](docs/new_keyboard_setup.md).

You can use an existing keyboard implementation as a reference. If your keyboard hardware isn't currently supported by the firmware, you'll need to implement the necessary drivers and features. See the [Porting](#porting) section for more details.

## Porting

### Hardware Driver Structure

Hardware drivers follow this directory structure:

- [`hardware/`](hardware/): Contains hardware-specific header files. Each subdirectory may contain `config.h` and `board_def.h` for additional configuration, and board-specific definitions, respectively.
- [`include/hardware/`](include/hardware/): Contains hardware driver interface headers that declare functions to be implemented
- [`src/hardware/`](src/hardware/): Contains hardware driver implementations of the functions declared in the header files
- [`linker/`](linker/): Contains linker scripts for supported microcontrollers
- [`scripts/drivers.py`](scripts/drivers.py): Contains the driver configuration for each supported microcontroller. Each driver must implement the `Driver` class.

You can refer to existing hardware drivers as examples when implementing support for new hardware.

## Acknowledgements

- [peppapighs/libhmk](https://github.com/peppapighs/libhmk) — Original project this fork is based on.
- [hathach/tinyusb](https://github.com/hathach/tinyusb) for the USB stack.
- [qmk/qmk_firmware](https://github.com/qmk/qmk_firmware) for inspiration, including EEPROM emulation and matrix scanning.
- [@riskable](https://github.com/riskable) for pioneering custom Hall-effect keyboard firmware development.
- [@heiso](https://github.com/heiso/) for his [macrolev](https://github.com/heiso/macrolev) and his helpfulness throughout the development process.
- [Wooting](https://wooting.io/) for pioneering Hall-effect gaming keyboards and introducing many advanced features based on analog input.
- [GEONWORKS](https://geon.works/) for the Venom 60HE PCB and inspiring the web configurator.
- [@devanlai](https://github.com/devanlai) for [WebUSB DFU](https://devanlai.github.io/webdfu/dfu-util/).
