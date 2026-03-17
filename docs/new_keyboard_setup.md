# New Keyboard Setup Guide

This guide explains how to add support for a new keyboard to `libhmk`.

## 1. Directory Structure

All keyboard-specific configurations are stored in the `keyboards/` directory. Create a new folder for your keyboard:

```text
libhmk/
└── keyboards/
    └── <your_keyboard_name>/
        ├── keyboard.json   (Required: Metadata and configuration)
        ├── board_def.h     (Optional: Feature pin definitions)
        └── config.h        (Optional: Additional compile-time overrides)
```

## 2. Configuration (`keyboard.json`)

The `keyboard.json` file is the heart of your keyboard definition. It defines the matrix, pins, USB metadata, and hardware features.

### Key Sections:

- **`usb`**: Set your Vendor ID (VID), Product ID (PID), and USB speed (`fs` for Full Speed, `hs` for High Speed).
- **`hardware`**: Specify the MCU driver (e.g., `at32f405xx`).
- **`analog`**: Configure the scanning matrix.
    - `mux`: Define multiplexer select pins and input pins.
    - `matrix`: A 2D array mapping matrix intersections to key indexes.
- **`rgb`**: (If applicable) Define RGB pin and LED count.
- **`layout`**: Defines how the keys are physically positioned for the web configurator.

Refer to [`keyboards/mochiko39he/keyboard.json`](../keyboards/mochiko39he/keyboard.json) as a complete example.

## 3. Optional Headers

Use `board_def.h` for hardware-backed optional features, and `config.h` for any extra compile-time overrides.

Example `board_def.h` for a slider or joystick switch mapped into the matrix:
```c
#define SLIDER_KEY_INDEX 39
#define JOYSTICK_SW_KEY_INDEX 40
```

## 4. Building the Firmware

Use the provided `setup.py` script to generate the environment for your keyboard:

1. Open a terminal in the `libhmk` root.
2. Run the setup script:
   ```bash
   python setup.py -k <your_keyboard_name>
   ```
   This generates a `platformio.ini` file tailored to your keyboard.
3. Build using PlatformIO:
   ```bash
   pio run
   ```

## 5. Calibration and Verification

1. **Initial Calibration**: Once flashed, use the Web Configurator ([hmkconf](https://github.com/310u/hmkconf)) to perform the initial calibration.
2. **Rest Value**: Ensure the sensors are stable at rest.
3. **Bottom-out**: Verify that all keys can reach their maximum travel distance.
4. **Analog RGB**: Test depth-reactive effects to ensure ADC readings are correctly mapped to LEDs.

## Tips for Success

- **Check Pins**: Double-check that your pin names (e.g., `A3`, `B10`) match your PCB schematic and the MCU datasheet.
- **Start Simple**: Disable RGB and advanced features until your basic matrix scanning is confirmed working.
- **Use the Console**: Use debug logging (if enabled) to monitor ADC raw values during development.
