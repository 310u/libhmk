# New Keyboard Setup Guide

This guide explains how to add support for a new keyboard to `libhmk`.

## 1. Directory Structure

All keyboard-specific configurations are stored in the `keyboards/` directory. Create a new folder for your keyboard:

```text
libhmk/
└── keyboards/
    └── <your_keyboard_name>/
        ├── keyboard.json   (Required: Metadata and configuration)
        ├── board_def.h     (Generated: Feature pin definitions)
        └── config.h        (Optional: Additional compile-time overrides)
```

`board_def.h` is generated automatically by `setup.py` from the hardware-related
sections of `keyboard.json`. You should not edit it by hand; put all hardware
configuration in `keyboard.json` instead.

## 2. Configuration (`keyboard.json`)

The `keyboard.json` file is the heart of your keyboard definition. It defines the matrix, pins, USB metadata, and hardware features.

### Key Sections:

- **`usb`**: Set your Vendor ID (VID), Product ID (PID), and USB speed (`fs` for Full Speed, `hs` for High Speed).
- **`hardware`**: Specify the MCU driver. Current in-tree drivers are `at32f405xx` and `stm32f446xx`. Use `cpu_hz` only when the keyboard overrides the default MCU clock tree.
- **`analog`**: Configure the scanning matrix.
    - `backend`: Select the analog sampling backend. Use `mcu_adc` for the MCU ADC path, or `spi_adc` for supported external SPI ADC designs such as ADS7953 on AT32F405xx.
    - `mux`: Define multiplexer select pins and input pins.
    - `matrix`: A 2D array mapping matrix intersections to physical key numbers.
- **`digital`**: (Optional) Configure direct GPIO-backed switch inputs.
- **`encoder`**: (Optional) Reserve virtual key indices for rotary encoder CW/CCW actions so they can be remapped in `hmkconf`.
- **`rgb`**: (If applicable) Define RGB LED layout metadata for the configurator and effect engine.
- **`layout`**: Defines how the keys are physically positioned for the web configurator.
- **`matrix`** (Optional): Low-level matrix scanning and scheduler tuning.
- **`hardware.timings`** (Optional): Background task intervals and phases for the cooperative scheduler.
- **`hardware.spi`** (Optional): General SPI bus configuration for peripherals such as trackball sensors.
- **`hardware.clock`** (Optional): Board-specific PLL and bus divider overrides.
- **`rgb.hardware`** (Optional): RGB LED driver pin/timer/DMA definitions.
- **`joystick`** (Optional): Joystick switch pin and ADC indices.
- **`trackball`** (Optional): Trackball sensor SPI and GPIO configuration.
- **`encoder.hardware`** (Optional): Rotary encoder GPIO pin and pull mode configuration.
- **`analog.spi.buses[].gpio`** (Optional): SPI ADC bus GPIO pins when they differ from the general SPI bus.

Refer to [`keyboards/mochiko39he/keyboard.json`](../keyboards/mochiko39he/keyboard.json) as a complete example, and to [`keyboards/mochiko40he/keyboard.json`](../keyboards/mochiko40he/keyboard.json) for a board that uses the generated `board_def.h` workflow.

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

### Matrix and scheduler tuning

```json
"matrix": {
    "processing_divider": 2,
    "scheduler_max_catchup_scans": 4,
    "scheduler_budget_us": 63,
    "adc_sample_delay_default": 1,
    "analog_scan_key_version_delta": 0,
    "detailed_scan_diagnostics": 0,
    "idle_raw_fast_path_margin": 24,
    "live_scan_timing_diagnostics": 0
}
```

### Background task timings

```json
"hardware": {
    "hse_value": 12000000,
    "driver": "at32f405xx",
    "timings": {
        "usb": { "interval": 8, "phase": 0 },
        "layout": { "interval": 64, "phase": 0 },
        "xinput": { "interval": 64, "phase": 16 },
        "command": { "interval": 128, "phase": 8 },
        "trackball": { "interval": 128, "phase": 4 },
        "joystick": { "interval": 128, "phase": 20 },
        "encoder": { "interval": 128, "phase": 36 },
        "slider": { "interval": 128, "phase": 52 },
        "rgb": { "interval": 256, "phase": 28 }
    }
}
```

### RGB hardware (AT32F405xx DMA/PWM driver)

```json
"rgb": {
    "led_map": [ ... ],
    "mod_keys": [ ... ],
    "hardware": {
        "num_leds": 40,
        "data_pin": "A10",
        "data_pin_source": "GPIO_PINS_SOURCE10",
        "data_pin_mux": "GPIO_MUX_1",
        "timer": "TMR1",
        "timer_channel": "TMR_SELECT_CHANNEL_3",
        "timer_dma_request": "TMR_OVERFLOW_DMA_REQUEST",
        "timer_dmamux_request": "DMAMUX_DMAREQ_ID_TMR1_OVERFLOW",
        "dma_channel": "DMA1_CHANNEL2",
        "dma_mux_channel": "DMA1MUX_CHANNEL2",
        "dma_transfer_flag": "DMA1_FDT2_FLAG",
        "dma_clear_flag": "DMA1_GL2_FLAG",
        "reset_time_ns": 300000,
        "dma_frame_repeats": 2,
        "bitbang_frame_repeats": 2
    }
}
```

### Joystick hardware

```json
"joystick": {
    "enabled": true,
    "sw_pin": "A9",
    "x_adc_index": 0,
    "y_adc_index": 1,
    "sw_key_index": 40
}
```

### SPI bus hardware (for trackball sensors)

```json
"hardware": {
    "spi": {
        "buses": [
            {
                "instance": "SPI3",
                "clock_hz": 108000000,
                "sck_pin": "C10",
                "miso_pin": "C11",
                "mosi_pin": "C12",
                "pin_mux": "GPIO_MUX_6"
            }
        ]
    }
}
```

For STM32 targets, omit `pin_mux` and supply `pin_af` instead.

### Trackball hardware

```json
"trackball": {
    "enabled": true,
    "sensor": "PAW3395",
    "spi_bus": 0,
    "cs_pin": "B6",
    "motion_pin": "B7",
    "spi_frequency_hz": 8000000,
    "cpi_default": 1600
}
```

### Encoder hardware

```json
"encoder": {
    "map": [
        {
            "label": "Main Encoder",
            "cw": 41,
            "ccw": 42
        }
    ],
    "hardware": {
        "a_pins": ["B10"],
        "b_pins": ["B12"],
        "invert_directions": [0],
        "pullup": true
    }
}
```

### Clock/PLL overrides

```json
"hardware": {
    "clock": {
        "pll_ns": 48,
        "pll_ms": 1,
        "pll_fp": "CRM_PLL_FP_4",
        "pll_fu": "CRM_PLL_FU_12",
        "apb2_div": "CRM_APB2_DIV_1",
        "apb1_div": "CRM_APB1_DIV_2"
    }
}
```

### SPI ADC GPIO overrides

When the SPI ADC buses use different GPIO pins than the general SPI bus, add a `gpio` object to each bus:

```json
"analog": {
    "backend": "spi_adc",
    "spi": {
        "driver": "ads7953",
        "buses": [
            {
                "bus": 0,
                "gpio": {
                    "sck_pin": "A0",
                    "miso_pin": "A1",
                    "mosi_pin": "C3"
                },
                "devices": [ ... ]
            }
        ]
    }
}
```

## 3. Optional Headers

For keyboards that use the new JSON-driven workflow, `setup.py` generates
`board_def.h` automatically, so you do not need to write it by hand. You only
need a manual `board_def.h` for legacy keyboards that have not been migrated
yet, or for unusual compile-time overrides that are not covered by the JSON
schema.

- `config.h` is optional, and is the right place for compile-time overrides that are not simple pin/feature definitions.
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
   This regenerates `platformio.ini` for the selected keyboard, the matching
   `<your_keyboard_name>_recovery` environment, and the generated
   `keyboards/<your_keyboard_name>/board_def.h` header.
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
