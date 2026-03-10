import json
import os
import sys

def gen_rgb_coords(keyboard_dir):
    kb_json_path = os.path.join(keyboard_dir, "keyboard.json")
    with open(kb_json_path, "r") as f:
        kb_json = json.load(f)

    if "rgb" not in kb_json or "led_map" not in kb_json["rgb"]:
        print("No RGB led_map found in keyboard.json")
        return

    led_map = kb_json["rgb"]["led_map"]
    mod_keys = set(kb_json["rgb"].get("mod_keys", []))
    num_keys = kb_json["keyboard"]["num_keys"]
    led_map_set = set(led_map)
    for key_index in mod_keys:
        if not isinstance(key_index, int) or key_index < 0 or key_index >= num_keys:
            raise ValueError(f"rgb.mod_keys contains out-of-range key index: {key_index}")
        if key_index not in led_map_set:
            raise ValueError(
                f"rgb.mod_keys contains key index not present in rgb.led_map: {key_index}"
            )
    layout = kb_json["layout"]["keymap"]

    # Calculate coordinates for all keys
    key_coords = {}
    y_pos = 0.0
    for row in layout:
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
                # Store center of the key
                key_coords[key_index] = (x_pos + w/2.0, y_pos + h/2.0)
            
            x_pos += w
        y_pos += 1.0

    if not key_coords:
        return

    # Normalize coordinates to 0-255
    min_x = min(c[0] for c in key_coords.values())
    max_x = max(c[0] for c in key_coords.values())
    min_y = min(c[1] for c in key_coords.values())
    max_y = max(c[1] for c in key_coords.values())

    range_x = max_x - min_x if max_x > min_x else 1.0
    range_y = max_y - min_y if max_y > min_y else 1.0

    led_coords = []
    led_is_mod = []
    for key_index in led_map:
        if key_index in key_coords:
            x, y = key_coords[key_index]
            nx = int((x - min_x) / range_x * 255)
            ny = int((y - min_y) / range_y * 255)
            led_coords.append((nx, ny))
        else:
            led_coords.append((0, 0))
        led_is_mod.append(1 if key_index in mod_keys else 0)

    # Generate header
    output_path = os.path.abspath(
        os.path.join(os.path.dirname(__file__), "..", "include", "rgb_coords.h")
    )
    
    with open(output_path, "w") as f:
        f.write("#pragma once\n\n")
        f.write("#include \"rgb.h\"\n\n")
        f.write("#if defined(RGB_ENABLED)\n\n")
        f.write("typedef struct {\n    uint8_t x;\n    uint8_t y;\n} led_point_t;\n\n")
        f.write(f"const led_point_t rgb_led_coords[NUM_KEYS] = {{\n")
        
        for i, (x, y) in enumerate(led_coords):
            comma = "," if i < len(led_coords) - 1 else ""
            f.write(f"    {{{x}, {y}}}{comma}\n")
        
        f.write("};\n\n")
        f.write(f"const uint8_t rgb_led_is_mod[NUM_KEYS] = {{\n")
        for i, is_mod in enumerate(led_is_mod):
            comma = "," if i < len(led_is_mod) - 1 else ""
            f.write(f"    {is_mod}{comma}\n")
        f.write("};\n\n")
        f.write("#endif\n")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python gen_rgb_coords.py <keyboard_dir>")
    else:
        gen_rgb_coords(sys.argv[1])
