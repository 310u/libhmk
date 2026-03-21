import json

import utils


def build_keyboard_metadata(kb_json: dict, driver):
    num_keys = kb_json["keyboard"]["num_keys"]
    num_layers = kb_json["keyboard"]["num_layers"]

    features = kb_json.get("features", {})
    metadata_features = {
        "rgb": bool(features.get("rgb", False)),
        "joystick": bool(features.get("joystick", False)),
        "encoder": bool(
            features.get("encoder", False) or kb_json.get("encoder", {}).get("map", [])
        ),
    }

    analog_keys = set()
    analog = kb_json.get("analog", {})
    mux = analog.get("mux", {})
    for row in mux.get("matrix", []):
        for key in row:
            if isinstance(key, int) and 0 <= key < num_keys:
                analog_keys.add(key)
    analog_keys = sorted(analog_keys)

    led_map = []
    led_coords = []
    mod_led_indices = []
    encoder_keys = []
    rgb = kb_json.get("rgb", {})
    if metadata_features["rgb"] and "led_map" in rgb:
        led_map = [key for key in rgb.get("led_map", []) if isinstance(key, int)]
        mod_keys = set(rgb.get("mod_keys", []))
        mod_led_indices = [
            led_index
            for led_index, key_index in enumerate(led_map)
            if key_index in mod_keys
        ]

        # Derive normalized LED coordinates from keyboard layout and LED map.
        key_coords = {}
        y_pos = 0.0
        for row in kb_json["layout"]["keymap"]:
            x_pos = 0.0
            for key_data in row:
                w = key_data.get("w", 1.0)
                h = key_data.get("h", 1.0)
                x_offset = key_data.get("x", 0.0)
                y_offset = key_data.get("y", 0.0)
                key_index = key_data.get("key")

                x_pos += x_offset
                y_pos += y_offset

                if key_index is not None:
                    key_coords[key_index] = (x_pos + w / 2.0, y_pos + h / 2.0)

                x_pos += w
            y_pos += 1.0

        if key_coords:
            min_x = min(coord[0] for coord in key_coords.values())
            max_x = max(coord[0] for coord in key_coords.values())
            min_y = min(coord[1] for coord in key_coords.values())
            max_y = max(coord[1] for coord in key_coords.values())
            range_x = max_x - min_x if max_x > min_x else 1.0
            range_y = max_y - min_y if max_y > min_y else 1.0

            for key_index in led_map:
                if key_index in key_coords:
                    x, y = key_coords[key_index]
                    led_coords.append(
                        {
                            "x": int((x - min_x) / range_x * 255),
                            "y": int((y - min_y) / range_y * 255),
                        }
                    )
                else:
                    led_coords.append({"x": 0, "y": 0})

    for encoder_index, encoder in enumerate(kb_json.get("encoder", {}).get("map", [])):
        label = encoder.get("label", f"Encoder {encoder_index + 1}")
        for direction in ("cw", "ccw"):
            key_index = encoder[direction]
            if not isinstance(key_index, int) or not 0 <= key_index < num_keys:
                raise ValueError(
                    f"encoder.map[{encoder_index}].{direction} must be between 0 and {num_keys - 1}"
                )
            encoder_keys.append(
                {
                    "key": key_index,
                    "encoder": encoder_index,
                    "direction": direction,
                    "label": f"{label} {'Clockwise' if direction == 'cw' else 'Counterclockwise'}",
                }
            )

    return {
        "name": kb_json["name"],
        "vendorId": kb_json["usb"]["vid"],
        "productId": kb_json["usb"]["pid"],
        "usbHighSpeed": kb_json["usb"]["port"] == "hs",
        "adcResolution": utils.get_adc_resolution(kb_json, driver),
        "numProfiles": kb_json["keyboard"]["num_profiles"],
        "numLayers": num_layers,
        "numKeys": num_keys,
        "numAdvancedKeys": kb_json["keyboard"]["num_advanced_keys"],
        "features": metadata_features,
        "layout": kb_json["layout"],
        "analogKeys": analog_keys,
        "encoderKeys": encoder_keys,
        "ledMap": led_map,
        "ledCoords": led_coords,
        "modLedIndices": mod_led_indices,
        "defaultKeymaps": utils.resolve_default_keymaps(kb_json),
    }


def build_keyboard_metadata_json(kb_json: dict, driver):
    return json.dumps(build_keyboard_metadata(kb_json, driver))
