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
- **`hardware`**: Specify the MCU driver. Current in-tree drivers are `at32f405xx` and `stm32f446xx`.
- **`analog`**: Configure the scanning matrix.
    - `backend`: Select the analog sampling backend. Use `mcu_adc` for current in-tree keyboards. `spi_adc` is reserved for future external ADC support and is not implemented yet.
    - `mux`: Define multiplexer select pins and input pins.
    - `matrix`: A 2D array mapping matrix intersections to physical key numbers.
- **`digital`**: (Optional) Configure direct GPIO-backed switch inputs.
- **`encoder`**: (Optional) Reserve virtual key indices for rotary encoder CW/CCW actions so they can be remapped in `hmkconf`.
- **`rgb`**: (If applicable) Define RGB LED layout metadata for the configurator and effect engine.
- **`layout`**: Defines how the keys are physically positioned for the web configurator.

Refer to [`keyboards/mochiko39he/keyboard.json`](../keyboards/mochiko39he/keyboard.json) as a complete example.

> [!IMPORTANT]
> `analog.mux.matrix`, `analog.raw.vector`, and `digital.vector` use 1-based physical key numbers, with `0` meaning "not connected". In contrast, `layout.key` and default keymaps use 0-based key indices.

> [!NOTE]
> `analog.backend` is optional and defaults to `mcu_adc`. The `spi_adc` value is accepted by the schema as a reserved future option, but the firmware currently rejects it at build time because the SPI ADC backend has not been implemented yet.

Example direct digital switch configuration:
```json
"digital": {
  "input": ["B8", "B9"],
  "vector": [42, 43],
  "pull": "up",
  "active_low": true
}
```

`digital` inputs become normal keys once they are mapped to a physical key
number, so they can be remapped from `hmkconf` just like any other key.

Example rotary encoder mapping for `hmkconf`:
```json
"encoder": {
  "map": [
    {
      "label": "Main Encoder",
      "cw": 41,
      "ccw": 42
    }
  ]
}
```

`encoder.map` uses 0-based key indices. These are usually hidden virtual keys,
so `keyboard.num_keys` and the default keymap arrays must include them even if
they are not present in `layout.keymap`.

When keyboard metadata contains `encoder.map`, `hmkconf` exposes those virtual
keys in a dedicated `Encoder` tab. If you instead use fixed
`ENCODER_CW_KEYCODES` / `ENCODER_CCW_KEYCODES` in `board_def.h`, the encoder
still works, but its directions are no longer remappable from `hmkconf`.

## 3. Optional Headers

Use `board_def.h` for hardware-backed optional features, and `config.h` for any extra compile-time overrides.

- `board_def.h` is optional. You only need it when your keyboard uses optional hardware features such as RGB, joystick, rotary encoder, slider, or board-specific tuning macros.
- `config.h` is also optional, and is the right place for compile-time overrides that are not simple pin/feature definitions.
- GPIO macro names are driver-specific. For example, AT32 uses `GPIO_PINS_10`, while STM32 uses `GPIO_PIN_10`.

Example `board_def.h` for a slider or joystick switch mapped into the matrix:
```c
#define SLIDER_KEY_INDEX 39
#define JOYSTICK_SW_KEY_INDEX 40
```

Example `board_def.h` for a rotary encoder:
```c
#define ENCODER_NUM 1
#define ENCODER_A_PORTS {GPIOA}
#define ENCODER_A_PINS {GPIO_PINS_0}
#define ENCODER_B_PORTS {GPIOA}
#define ENCODER_B_PINS {GPIO_PINS_1}
```

Note: Rotary encoder support is implemented in the firmware, but it has not
been verified on real hardware yet. Treat the pin assignment and direction
settings above as the expected configuration, and confirm them on your board.

If you want a fixed compile-time encoder output instead of an `hmkconf`
remappable binding, you can omit `keyboard.json.encoder` and define
`ENCODER_CW_KEYCODES` / `ENCODER_CCW_KEYCODES` in `board_def.h` instead.

Example RGB definitions:

AT32F405xx with the default DMA/PWM RGB driver:
```c
#define RGB_ENABLED 1
#define RGB_DATA_PIN GPIO_PINS_10
#define RGB_DATA_PORT GPIOA
#define RGB_DATA_PIN_SOURCE GPIO_PINS_SOURCE10
#define RGB_DATA_PIN_MUX GPIO_MUX_1
#define RGB_TIMER TMR1
#define RGB_TIMER_CLOCK CRM_TMR1_PERIPH_CLOCK
#define RGB_TIMER_CHANNEL TMR_SELECT_CHANNEL_3
#define RGB_TIMER_DMA_REQUEST TMR_OVERFLOW_DMA_REQUEST
#define RGB_TIMER_DMAMUX_REQUEST DMAMUX_DMAREQ_ID_TMR1_OVERFLOW
#define RGB_DMA_CHANNEL DMA1_CHANNEL2
#define RGB_DMA_MUX_CHANNEL DMA1MUX_CHANNEL2
#define RGB_DMA_TRANSFER_FLAG DMA1_FDT2_FLAG
#define RGB_DMA_CLEAR_FLAG DMA1_GL2_FLAG
```

STM32F446xx with the built-in bitbang RGB driver:
```c
#define RGB_ENABLED 1
#define RGB_DATA_PIN GPIO_PIN_8
#define RGB_DATA_PORT GPIOA
```

## 4. Building the Firmware

Use the provided `setup.py` script to generate the environment for your keyboard:

1. Open a terminal in the `libhmk` root.
2. Run the setup script:
   ```bash
   python setup.py -k <your_keyboard_name>
   ```
   This regenerates `platformio.ini` for the selected keyboard only, plus the
   matching `<your_keyboard_name>_recovery` environment.
3. Build using PlatformIO:
   ```bash
   pio run
   ```

If you switch to another keyboard target later, run `python setup.py -k ...` again before building.

`libhmk` is a shared codebase, but the generated binaries are keyboard-specific.
Always flash the artifact built for the exact keyboard definition you selected
in `setup.py`.

## 5. Calibration and Verification

1. **Initial Calibration**: Once flashed, use the Web Configurator ([hmkconf](https://github.com/310u/hmkconf)) to perform the initial calibration.
2. **Rest Value**: Ensure the sensors are stable at rest.
3. **Bottom-out**: Verify that all keys can reach their maximum travel distance.
4. **Analog RGB**: Test depth-reactive effects to ensure ADC readings are correctly mapped to LEDs.

## Tips for Success

- **Check Pins**: Double-check that your pin names (e.g., `A3`, `B10`) match your PCB schematic and the MCU datasheet.
- **Start Simple**: Disable RGB and advanced features until your basic matrix scanning is confirmed working.
- **Keep Indexing Straight**: `analog.mux.matrix` / `analog.raw.vector` / `digital.vector` are 1-based, but `layout.key` is 0-based.
- **Match the Driver**: `stm32f446xx` and `at32f405xx` do not use exactly the same `board_def.h` macros for RGB.
- **Use the Console**: Use debug logging (if enabled) to monitor ADC raw values during development.
