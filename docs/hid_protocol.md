# Custom WebHID Protocol Specification for libhmk

This document specifies the custom HID protocol used by `hmkconf` to communicate with a keyboard running `libhmk`.

## Overview

The communication happens over the `USB_ITF_RAW_HID` interface.
Each HID report (both input and output) is `RAW_HID_EP_SIZE` bytes (typically 64 bytes).
The first byte of the data payload is always the `command_id`.

## Transport

Host -> Keyboard (Out Report):
1 byte: `command_id`
`N` bytes: command payload (size varies depending on command, maximum `RAW_HID_EP_SIZE - 1`)

Keyboard -> Host (In Report):
1 byte: `command_id` (Will echo back the requested `command_id` on success, or `COMMAND_UNKNOWN` (255) on failure).
`N` bytes: response payload

The firmware accepts at most one queued command request and one pending
response at a time. Hosts should treat the protocol as strictly request/response
and wait for the matching input report before sending the next output report.
If a second command is sent while a request or response is still pending, it may
be dropped.

## Command List

| Command ID | Enum Name | Description |
|---|---|---|
| `0` | `COMMAND_FIRMWARE_VERSION` | Returns the firmware version. |
| `1` | `COMMAND_REBOOT` | Reboots the keyboard. |
| `2` | `COMMAND_BOOTLOADER` | Enters DFU/Bootloader mode. |
| `3` | `COMMAND_FACTORY_RESET` | Clears all custom settings and restores defaults. |
| `4` | `COMMAND_RECALIBRATE` | Forces a recalibration of the analog matrix. |
| `5` | `COMMAND_ANALOG_INFO` | Request filtered ADC values and calculated distances for keys. |
| `6` | `COMMAND_GET_CALIBRATION` | Retrieves the global EEPROM calibration settings. |
| `7` | `COMMAND_SET_CALIBRATION` | Updates the global EEPROM calibration settings. |
| `8` | `COMMAND_GET_PROFILE` | Returns the active profile index. |
| `9` | `COMMAND_GET_OPTIONS` | Retrieves global EEPROM options. |
| `10` | `COMMAND_SET_OPTIONS` | Updates global EEPROM options. |
| `11` | `COMMAND_RESET_PROFILE` | Resets a specific profile to default settings. |
| `12` | `COMMAND_DUPLICATE_PROFILE` | Duplicates settings from one profile to another. |
| `13` | `COMMAND_GET_METADATA` | Retrieves static keyboard metadata (JSON format). |
| `14` | `COMMAND_GET_SERIAL` | Retrieves the hardware serial number of the keyboard. |
| `15` | `COMMAND_SAVE_CALIBRATION_THRESHOLD` | Saves the current per-key bottom-out thresholds to persistent storage immediately. |
| `16` | `COMMAND_ANALOG_INFO_RAW` | Request raw ADC values and calculated distances for keys. |
| `128` | `COMMAND_GET_KEYMAP` | Reads a chunk of the keymap matrix for a profile/layer. |
| `129` | `COMMAND_SET_KEYMAP` | Writes a chunk of the keymap matrix for a profile/layer. |
| `130` | `COMMAND_GET_ACTUATION_MAP`| Reads actuation points for keys. |
| `131` | `COMMAND_SET_ACTUATION_MAP`| Writes actuation points for keys. |
| `132` | `COMMAND_GET_ADVANCED_KEYS`| Reads Advanced Keys (Tap-Hold, Dynamic Keystroke, Combos, etc.). |
| `133` | `COMMAND_SET_ADVANCED_KEYS`| Writes Advanced Keys configurations. |
| `134` | `COMMAND_GET_TICK_RATE` | Returns the tick rate used by Advanced Keys logic. |
| `135` | `COMMAND_SET_TICK_RATE` | Updates the tick rate. |
| `136` | `COMMAND_GET_GAMEPAD_BUTTONS`| Reads gamepad button mappings. |
| `137` | `COMMAND_SET_GAMEPAD_BUTTONS`| Writes gamepad button mappings. |
| `138` | `COMMAND_GET_GAMEPAD_OPTIONS`| Reads gamepad analog curve options. |
| `139` | `COMMAND_SET_GAMEPAD_OPTIONS`| Writes gamepad analog curve options. |
| `140` | `COMMAND_GET_MACROS` | Reads macro sequence data. |
| `141` | `COMMAND_SET_MACROS` | Writes macro sequence data. |
| `142` | `COMMAND_GET_RGB_CONFIG` | Reads a chunk of the active profile's RGB configuration. |
| `143` | `COMMAND_SET_RGB_CONFIG` | Writes a chunk of the active profile's RGB configuration. |
| `144` | `COMMAND_GET_JOYSTICK_STATE` | Returns the live joystick state and calibration outputs. |
| `145` | `COMMAND_GET_JOYSTICK_CONFIG` | Reads the current profile's joystick configuration. |
| `146` | `COMMAND_SET_JOYSTICK_CONFIG` | Writes the current profile's joystick configuration. |
| `147` | `COMMAND_SET_HOST_TIME` | Pushes host wall-clock time into runtime-only firmware features such as the binary clock effect. |
| `148` | `COMMAND_GET_TRACKBALL_STATE` | Returns the live trackball diagnostic state. |
| `149` | `COMMAND_GET_MATRIX_SCAN_DIAGNOSTICS` | Returns matrix scan timing diagnostics. |
| `150` | `COMMAND_RESET_MATRIX_SCAN_DIAGNOSTICS` | Clears accumulated matrix scan diagnostics. |
| `151` | `COMMAND_GET_ANALOG_SCAN_DIAGNOSTICS` | Returns analog full-scan diagnostics. |
| `152` | `COMMAND_RESET_ANALOG_SCAN_DIAGNOSTICS` | Clears accumulated analog scan diagnostics counters. |
| `153` | `COMMAND_GET_ANALOG_RAW_CHANNELS` | Reads raw direct ADC channels, if present. |
| `154` | `COMMAND_GET_ANALOG_DEBUG_FRAMES` | Reads backend-specific debug frames, if available. |
| `155` | `COMMAND_GET_ANALOG_SCAN_CONFIG` | Reads the active runtime MUX scan configuration. |
| `156` | `COMMAND_SET_ANALOG_SCAN_CONFIG` | Updates and persists the runtime MUX scan configuration. |
| `157` | `COMMAND_CAPTURE_ANALOG_DIAG_BASELINE` | Copies the latest raw-by-step snapshot into the diagnostic baseline buffer. |
| `158` | `COMMAND_RUN_ANALOG_CHANNEL_IDENTITY_TEST` | Analyzes the latest raw-by-step snapshot against the saved baseline for one expected key. |
| `159` | `COMMAND_GET_ANALOG_RAW_BY_STEP` | Returns one MUX step row of raw, baseline, and delta diagnostic data. |
| `160` | `COMMAND_GET_KALMAN_CONFIG` | Reads the active runtime Kalman filter configuration. |
| `161` | `COMMAND_SET_KALMAN_CONFIG` | Updates and persists the runtime Kalman filter configuration. |
| `162` | `COMMAND_GET_DIAGNOSTIC_MODE` | Reads whether integrated diagnostic mode is active. |
| `163` | `COMMAND_SET_DIAGNOSTIC_MODE` | Starts or stops integrated diagnostic mode. |

## Paging and Offsets
Because the HID reports are limited to 64 bytes, bulk data (such as Keymaps, Actuation arrays, Macros, and Metadata) is split into chunks.
Commands like `COMMAND_GET_KEYMAP` take an `offset` (the starting index) in the payload, and return a chunk of data. `COMMAND_SET_KEYMAP` takes `offset`, `len` (number of items), and the item payload. 

## EEPROM Synchronization
Write commands (`COMMAND_SET_*`) directly modify the in-memory cache and write to the internal flash using the `wear_leveling_write` mechanism. Changes take effect immediately.

`COMMAND_SET_HOST_TIME` is a runtime-only update and does not write to flash.

## Analog Scan Runtime Config

`COMMAND_GET_ANALOG_SCAN_CONFIG` and `COMMAND_SET_ANALOG_SCAN_CONFIG` use this
packed payload:

```c
struct analog_scan_config {
  uint16_t mux_sample_delay_us;
};
```

The firmware validates `mux_sample_delay_us` in the inclusive range `1..50`.
For boards without a mux-scanned ADC pipeline, `GET` returns `0` and `SET`
fails with `COMMAND_UNKNOWN`.

## Integrated Diagnostic Mode

Firmware advertising `diagnostics.integratedMode: true` supports runtime
diagnostic mode without requiring a separate firmware image. Both mode commands
use a packed one-byte boolean payload (`0` inactive, `1` active).

While active, normal keyboard output and background peripherals are paused while
USB, analog scanning, matrix processing, and diagnostic commands continue. The
firmware returns to normal input automatically after 30 seconds without
diagnostic traffic. Hosts should explicitly send `COMMAND_SET_DIAGNOSTIC_MODE`
with `0` when leaving the diagnostic UI.

Baseline capture, channel identity, and raw-by-step commands require diagnostic
mode to be active on integrated firmware.

## Kalman Filter Runtime Config

`COMMAND_GET_KALMAN_CONFIG` and `COMMAND_SET_KALMAN_CONFIG` share this packed
payload:

```c
struct kalman_config {
  float position_gain;                // 0.0 - 1.0
  float velocity_gain;                // 0.0 - 1.0
  float velocity_damping;             // 0.0 - 1.0
  float rt_down_min_velocity;         // Legacy reserved field; must be >= 0.0
  float rt_up_min_velocity;           // Legacy reserved field; must be >= 0.0
  float innovation_event_threshold;   // > 0.0 (distance units)
  uint16_t bottom_out_hold_scans;     // 0 .. 65535
  uint8_t bottom_out_rt_up;             // 0 .. 255
  uint16_t noise_deadzone;            // 0 .. ADC_MAX_VALUE
};
```

These parameters control how the matrix fast path estimates key position and
velocity and how bottom-out collision detection arms the Rapid Trigger release
threshold. `SET` rejects any value outside the ranges above and returns
`COMMAND_UNKNOWN` without changing the runtime state or flash.

Default values are taken from the firmware compile-time macros and are also
advertised in `COMMAND_GET_METADATA` under the `kalman` object so the host can
restore defaults without hard-coding them.

## Metadata Diagnostics Capability

`COMMAND_GET_METADATA` may include a top-level `diagnostics` object. Normal
firmware keeps every field disabled. Diagnostic firmware advertises:

```json
{
  "diagnostics": {
    "debugFirmware": true,
    "diagChannelIdentity": true,
    "rawByStep": true,
    "muxSteps": 8,
    "adcLanes": 5,
    "supportsDelaySweep": true,
    "supportsWalkingKeyTest": true
  }
}
```

When `diagChannelIdentity` is enabled, `keyToStepLane` maps each logical key
index to its expected MUX step and ADC lane.

## Matrix Scan Diagnostics

`COMMAND_GET_MATRIX_SCAN_DIAGNOSTICS` returns this packed payload:

```c
struct matrix_scan_diagnostics_report {
  uint32_t matrix_scan_count;
  uint32_t matrix_scan_hz;
  uint32_t last_matrix_scan_us;
  uint32_t max_matrix_scan_us;
  uint32_t raw_scan_hz;
  uint32_t last_raw_scan_us;
  uint32_t max_raw_scan_us;
  uint32_t full_scan_generation;
  uint32_t missed_generation_count;
  uint32_t matrix_processing_divider;
  uint32_t intentional_skip_count;
  uint32_t coalesced_generation_count;
  uint32_t overload_missed_generation_count;
  uint32_t scheduler_budget_exhausted_count;
  uint32_t matrix_catchup_scan_count;
  uint16_t expected_matrix_scan_hz;
  uint8_t matrix_fast_overrun_count;
};
```

`raw_scan_*` describes the ADC/MUX full-scan pipeline, while `matrix_scan_*`
describes how often the firmware actually ran `matrix_scan_fast()` in the main
loop. When `matrix_processing_divider` is greater than `1`, the matrix/RT path
is intentionally decimated relative to the raw scan rate.

`expected_matrix_scan_hz` is the divider-derived target rate based on the
current raw scan estimate. `matrix_fast_overrun_count` is appended as a
byte-sized convenience counter for live-timing validation builds and saturates
at `255`.

`missed_generation_count` now reports the same overload-only value as
`overload_missed_generation_count` for backward compatibility with older host
tools that only know about a single missed-generation field.

The scheduler-specific counters are:

- `intentional_skip_count`: generations skipped on purpose because the selected
  divider did not schedule them for matrix processing.
- `coalesced_generation_count`: raw generations folded into a later
  latest-snapshot matrix pass instead of being replayed individually.
- `overload_missed_generation_count`: scheduled generations that were missed
  because the main loop observed a newer generation before the fast path ran.
- `scheduler_budget_exhausted_count`: `matrix_task()` exceeded its time budget
  or ended the call while a newer due generation was already pending.
- `matrix_catchup_scan_count`: reserved for ring-buffered implementations that
  replay historical raw generations. Latest-snapshot firmware keeps this at `0`.

## Analog Scan Diagnostics

`COMMAND_GET_ANALOG_SCAN_DIAGNOSTICS` returns this packed payload:

```c
struct analog_scan_diagnostics_report {
  uint16_t mux_sample_delay_us;
  uint16_t mux_step_count;
  uint32_t scan_count;
  uint32_t last_scan_cycles;
  uint32_t max_scan_cycles;
  uint32_t last_scan_us;
  uint32_t max_scan_us;
  uint32_t estimated_scan_hz;
  uint32_t bad_channel_id_count;
  uint32_t dma_overrun_count;
  uint32_t overrun_count;
  uint32_t spi_error_count;
  uint32_t missed_scan_count;
};
```

For mux-based MCU ADC backends, `scan_count`, `last_scan_*`, `max_scan_*`, and
`estimated_scan_hz` refer to a full mux sweep across every select state, not an
individual mux step.

## Diagnostic Channel Identity

`COMMAND_RUN_ANALOG_CHANNEL_IDENTITY_TEST` uses this packed request payload:

```c
struct analog_channel_identity_test_request {
  uint8_t expected_key;
  uint16_t min_delta;
  uint8_t max_secondary_ratio_percent;
};
```

The response payload is:

```c
struct analog_channel_identity_test_response {
  uint8_t expected_key;
  uint8_t expected_step;
  uint8_t expected_lane;
  uint8_t observed_max_step;
  uint8_t observed_max_lane;
  uint8_t observed_logical_key;
  uint8_t observed_second_step;
  uint8_t observed_second_lane;
  uint16_t observed_max_delta;
  uint16_t observed_second_delta;
  uint16_t min_delta;
  uint8_t max_secondary_ratio_percent;
  uint8_t failure_reason;
  bool pass;
};
```

`min_delta == 0` uses the firmware default threshold. Likewise,
`max_secondary_ratio_percent == 0` uses the firmware default crosstalk limit.

`COMMAND_GET_ANALOG_RAW_BY_STEP` takes:

```c
struct analog_raw_by_step_request {
  uint8_t step;
};
```

and returns one row of the current diagnostic snapshot:

```c
struct analog_raw_by_step_response {
  uint8_t step;
  uint8_t lane_count;
  uint16_t raw_by_lane[8];
  uint16_t baseline_by_lane[8];
  uint16_t delta_by_lane[8];
};
```

All three diagnostic commands return `COMMAND_UNKNOWN` unless the connected
firmware advertises `diagnostics.diagChannelIdentity = true`.

*All structs are packed (`__attribute__((packed))`). The byte order is little-endian.*
