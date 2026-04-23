# This program is free software: you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program. If not, see <https://www.gnu.org/licenses/>.

import json
import os
from drivers import *


class CompilerFlags:
    def __init__(self):
        self.flags = []

    # Get the list of compiler flags
    def get_flags(self):
        return self.flags

    # Add a preprocessor definition to the compiler flags
    def define(self, name: str, value: int | str | None = None):
        match value:
            case None:
                self.flags.append(f"-D{name}")
            case int() | str():
                self.flags.append(f"-D{name}='{value}'")

    # Add a linker definition to the compiler flags
    def linker_defsym(self, name: str, value: int | str | None = None):
        match value:
            case None:
                self.flags.append(f"-Wl,--defsym,{name}")
            case int() | str():
                self.flags.append(f"-Wl,--defsym,{name}={value}")

    # Add an include path to the compiler flags
    def include(self, path: str):
        self.flags.append(f"-I{path}")


def get_keyboard_name(env):
    try:
        keyboard = env.GetProjectOption("custom_keyboard_name")
        if keyboard:
            return keyboard
    except Exception:
        pass

    return env["PIOENV"]


# Load the keyboard JSON configuration file
def get_kb_json(keyboard: str):
    with open(os.path.join("keyboards", keyboard, "keyboard.json"), "r") as f:
        return json.load(f)


# Load the driver based on the driver name
def get_driver_by_name(driver: str):
    match driver:
        case "stm32f446xx":
            return STM32F446XX
        case "at32f405xx":
            return AT32F405XX
        case _:
            raise ValueError(f"Unsupported driver: {driver}")


# Load the driver based on the keyboard configuration
def get_driver(keyboard: str):
    kb_json = get_kb_json(keyboard)
    return get_driver_by_name(kb_json["hardware"]["driver"])


# Convert a Python list to a C array initializer
def to_c_array(arr: list | bytes):
    return f"{{{', '.join(to_c_array(x) if isinstance(x, list) else str(x) for x in arr)}}}"


# Convert a Python dictionary to a C struct initializer
def to_c_struct(value: dict):
    return f"{{{', '.join(f'.{k} = {v}' for k, v in value.items())}}}"


# Convert a Pyhon list to a C array slice definition
def to_slice_def(name: str, arr: list | bytes):
    return f"#define {name.upper()} {', '.join(str(x) for x in arr)}"


# Get the ADC resolution, or default to the maximum resolution supported by the MCU
def get_adc_resolution(kb_json: dict, driver: Driver):
    return kb_json["analog"].get("adc_resolution", driver.metadata.adc.max_resolution)


# Resolve per-profile default keymaps
def resolve_default_keymaps(kb_json: dict) -> list[list[list[str]]]:
    if "keymaps" in kb_json:
        return kb_json["keymaps"]
    else:
        # Fallback to default keymap
        if "keymap" not in kb_json:
            raise ValueError(
                "Default keymap must be specified when no per-profile default keymaps are specified"
            )
        return [kb_json["keymap"]] * kb_json["keyboard"]["num_profiles"]


def iter_layout_keys(kb_json: dict):
    for row_index, row in enumerate(kb_json.get("layout", {}).get("keymap", [])):
        for col_index, key_data in enumerate(row):
            if not isinstance(key_data, dict):
                continue

            key_index = key_data.get("key")
            if key_index is not None:
                yield row_index, col_index, key_index


def validate_spi_adc_mapping(kb_json: dict):
    analog = kb_json.get("analog", {})
    if analog.get("backend", "mcu_adc") != "spi_adc":
        return

    spi = analog.get("spi")
    if spi is None:
        raise ValueError("analog.spi must be defined when analog.backend='spi_adc'")

    num_keys = kb_json["keyboard"]["num_keys"]
    assigned_physical_keys: dict[int, str] = {}
    seen_bus_ids: set[int] = set()
    seen_cs_pins: dict[str, str] = {}
    active_channel_count = 0

    def record_physical_key(physical_key: int, source: str):
        previous = assigned_physical_keys.get(physical_key)
        if previous is not None:
            raise ValueError(
                f"Physical key {physical_key} is assigned more than once: {previous} and {source}"
            )
        assigned_physical_keys[physical_key] = source

    for bus_entry in spi.get("buses", []):
        bus_id = bus_entry["bus"]
        if bus_id in seen_bus_ids:
            raise ValueError(f"SPI ADC bus {bus_id} is configured more than once")
        seen_bus_ids.add(bus_id)

        for device_index, device in enumerate(bus_entry["devices"]):
            source_prefix = f"analog.spi bus {bus_id} device {device_index}"
            cs = device["cs"]
            previous_cs = seen_cs_pins.get(cs)
            if previous_cs is not None:
                raise ValueError(
                    f"SPI ADC chip select {cs} is reused by {previous_cs} and {source_prefix}"
                )
            seen_cs_pins[cs] = source_prefix

            device_map = device["map"]
            if len(device_map) != 16:
                raise ValueError(
                    f"{source_prefix} must declare exactly 16 ADS7953 channel slots"
                )

            for channel_index, physical_key in enumerate(device_map):
                if physical_key == 0:
                    continue

                active_channel_count += 1
                if physical_key > num_keys:
                    raise ValueError(
                        f"{source_prefix} channel {channel_index} maps to physical key {physical_key}, which exceeds keyboard.num_keys={num_keys}"
                    )
                record_physical_key(
                    physical_key, f"{source_prefix} channel {channel_index}"
                )

    if active_channel_count == 0:
        raise ValueError("analog.spi must map at least one ADS7953 channel")

    digital = kb_json.get("digital", {})
    for input_index, physical_key in enumerate(digital.get("vector", [])):
        if physical_key == 0:
            continue
        if physical_key > num_keys:
            raise ValueError(
                f"digital.vector[{input_index}]={physical_key} exceeds keyboard.num_keys={num_keys}"
            )
        record_physical_key(physical_key, f"digital.vector[{input_index}]")

    for row_index, col_index, key_index in iter_layout_keys(kb_json):
        if not isinstance(key_index, int):
            raise ValueError(
                f"layout.keymap[{row_index}][{col_index}].key must be an integer"
            )
        if not 0 <= key_index < num_keys:
            raise ValueError(
                f"layout.keymap[{row_index}][{col_index}].key={key_index} is out of range for keyboard.num_keys={num_keys}"
            )

        physical_key = key_index + 1
        if physical_key not in assigned_physical_keys:
            raise ValueError(
                f"layout key {key_index} (physical key {physical_key}) is not assigned in analog.spi map or digital.vector"
            )
