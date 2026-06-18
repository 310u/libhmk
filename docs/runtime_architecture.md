# Runtime Architecture Notes

This note summarizes the main runtime boundaries in `libhmk` after the recent refactors.

## Input Flow

- Analog backends are responsible only for acquiring samples.
- `analog_scan.c` converts sampled channel buffers into key-facing values and raw input values.
- When diagnostic firmware is enabled, `analog_scan.c` also keeps a latest
  `raw_by_step` snapshot for low-rate channel-identity analysis without adding
  analysis work to the DMA/ISR fast path.
- `matrix.c` turns the latest analog snapshot into press/release events based
  on calibration and actuation settings.
- `layout.c` resolves key indices into keycodes, layer actions, advanced keys, and deferred actions.

## Matrix Scheduling

- `matrix_scan_fast()` owns the latency-sensitive filter, distance, and Rapid
  Trigger work.
- `matrix_scan_housekeeping()` owns deferred RGB dispatch, continuous
  calibration maintenance, and other slower housekeeping.
- `matrix_task()` is generation-gated and processes the latest snapshot once
  per due matrix generation window.
- Latest-snapshot builds do not replay missed raw generations. When the main
  loop falls behind, the firmware coalesces generations and records counters
  instead of re-running the matrix filter against the same raw frame.

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

## Raw HID Commands

- `hid.c` should keep the TinyUSB `SET_REPORT` callback lightweight and avoid
  doing full configuration writes or profile reloads inline.
- Raw HID command payloads are queued through `command_enqueue()` and then
  processed from `command_task()` in the main loop.
- `commands.c` still defers the response write until the raw HID interface is
  ready, so both the request side and response side stay non-blocking relative
  to the USB callback.
- The current transport is intentionally single-flight: hosts should wait for
  each response before sending the next command instead of pipelining requests.
- Diagnostic firmware exposes additional channel-identity commands and metadata
  capabilities so host tools can opt into Developer mode without probing the
  normal fast path.

## Future Analog Backends

- `keyboard.json.analog.backend` defaults to `mcu_adc`.
- `spi_adc` is reserved for future external ADC support.
- A future SPI ADC backend should only gather samples and then feed them into `analog_scan_store_samples()`.
- Current candidate parts are TI `ADS7953` and ADI `AD7490`.
- Keep chip-specific SPI framing, channel sequencing, and pipeline latency handling below the generic analog backend boundary.
- For the current `ADS7953 x 4 + central trackball sensor` proposal, see `docs/ads7953_pmw3360_design.md`.

That split is intentional: adding a new ADC transport should not require reworking `matrix.c`, `layout.c`, or profile handling.
