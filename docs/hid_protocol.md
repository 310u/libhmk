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

## Paging and Offsets
Because the HID reports are limited to 64 bytes, bulk data (such as Keymaps, Actuation arrays, Macros, and Metadata) is split into chunks.
Commands like `COMMAND_GET_KEYMAP` take an `offset` (the starting index) in the payload, and return a chunk of data. `COMMAND_SET_KEYMAP` takes `offset`, `len` (number of items), and the item payload. 

## EEPROM Synchronization
Write commands (`COMMAND_SET_*`) directly modify the in-memory cache and write to the internal flash using the `wear_leveling_write` mechanism. Changes take effect immediately.

*All structs are packed (`__attribute__((packed))`). The byte order is little-endian.*
