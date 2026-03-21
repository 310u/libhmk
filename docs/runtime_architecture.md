# Runtime Architecture Notes

This note summarizes the main runtime boundaries in `libhmk` after the recent refactors.

## Input Flow

- Analog backends are responsible only for acquiring samples.
- `analog_scan.c` converts sampled channel buffers into key-facing values and raw input values.
- `matrix.c` turns those values into press/release events based on calibration and actuation settings.
- `layout.c` resolves key indices into keycodes, layer actions, advanced keys, and deferred actions.

## Non-Matrix Inputs

- Inputs that should behave like normal keys should enter through key indices.
- `digital` GPIO inputs are exposed as normal key indices through the analog/matrix path.
- Encoder directions can also be exposed as virtual key indices when `keyboard.json.encoder.map` is used.
- Inputs that need to emit direct keycodes or HID actions should go through `input_routing.h`.

`input_routing` exists to keep external input modules from calling `layout` or `hid` internals directly.

## Profile Reload Path

- Persistent settings are stored in `eeconfig`.
- `profile_runtime.c` is the single place that reapplies the current profile into live runtime state.
- `layout_init()` and command handlers both use `profile_runtime_reload_current()` instead of duplicating reload logic.

When a new feature adds profile-backed state, it should hook into `profile_runtime_apply_current()`.

## USB Runtime

- `usb_runtime.c` owns TinyUSB runtime resync and long-suspend recovery.
- Board drivers should only provide hardware setup and USB connect/disconnect primitives.
- `main.c` calls `usb_runtime_init()` during startup and `usb_runtime_task()` from the main loop.

This keeps USB suspend/resume policy out of MCU-specific `board.c` files.

## Future Analog Backends

- `keyboard.json.analog.backend` defaults to `mcu_adc`.
- `spi_adc` is reserved for future external ADC support.
- A future SPI ADC backend should only gather samples and then feed them into `analog_scan_store_samples()`.
- Current candidate parts are TI `ADS7953` and ADI `AD7490`.
- Keep chip-specific SPI framing, channel sequencing, and pipeline latency handling below the generic analog backend boundary.

That split is intentional: adding a new ADC transport should not require reworking `matrix.c`, `layout.c`, or profile handling.
