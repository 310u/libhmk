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

*All structs are packed (`__attribute__((packed))`). The byte order is little-endian.*
