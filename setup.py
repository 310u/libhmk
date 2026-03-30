import sys

sys.path.append("scripts")

import argparse
import configparser
import os
import scripts.utils as utils


if __name__ == "__main__":
    keyboards = [
        keyboard
        for keyboard in os.listdir("keyboards")
        if os.path.isdir(os.path.join("keyboards", keyboard))
    ]

    parser = argparse.ArgumentParser(description="PlatformIO Project Setup")
    parser.add_argument(
        "--keyboard", "-k", choices=keyboards, required=True, help="Select a keyboard"
    )
    args = parser.parse_args()

    keyboard: str = args.keyboard
    driver = utils.get_driver(keyboard)

    build_flags = ["${env.build_flags}"]
    build_src_flags = [
        "${env.build_src_flags}",
        "-Werror",
        "-Wall",
        "-Wextra",
        "-Wsign-conversion",
        "-Wswitch-default",
        "-Wswitch",
        "-Wdouble-promotion",
        "-Wstrict-prototypes",
        "-Wno-unused-parameter",
    ]
    extra_scripts = [
        "pre:scripts/get_deps.py",
        "pre:scripts/validate.py",
        "pre:scripts/make.py",
        "pre:scripts/metadata.py",
    ]
    lib_deps = ["https://github.com/hathach/tinyusb.git#0.20.0"]

    pio_config = configparser.ConfigParser()
    pio_config[f"env:{keyboard}"] = {
        "board": driver.platformio.board,
        "board_build.ldscript": f"linker/{driver.platformio.ldscript}",
        "build_flags": "\n".join(build_flags),
        "build_src_filter": "${env.build_src_filter}",
        "build_src_flags": "\n".join(build_src_flags),
        "extra_scripts": "\n".join(extra_scripts),
        "framework": driver.platformio.framework,
        "lib_deps": "\n".join(lib_deps),
        "platform": driver.platformio.platform,
        "test_ignore": "*",
        "upload_protocol": "dfu",
    }
    pio_config[f"env:{keyboard}_recovery"] = {
        "board": driver.platformio.board,
        "board_build.ldscript": f"linker/{driver.platformio.ldscript}",
        "build_flags": "\n".join(build_flags + ["-DRECOVERY_RESET_CURRENT_PROFILE_RGB"]),
        "build_src_filter": "${env.build_src_filter}",
        "build_src_flags": "\n".join(build_src_flags),
        "extra_scripts": "\n".join(extra_scripts),
        "framework": driver.platformio.framework,
        "custom_keyboard_name": keyboard,
        "lib_deps": "\n".join(lib_deps),
        "platform": driver.platformio.platform,
        "test_ignore": "*",
        "upload_protocol": "dfu",
    }

    def native_test_env(test_filter, build_src_filter, extra_flags=None):
        return {
            "platform": "native",
            "test_framework": "unity",
            "test_filter": test_filter,
            "test_build_src": "yes",
            "build_src_filter": build_src_filter,
            "build_flags": "\n".join([common_test_flags, *(extra_flags or [])]),
        }

    # Native unit test environments
    common_test_flags = "-I include\n-include test/test_config.h"
    pio_config["env:native_test_advanced_keys"] = native_test_env(
        "test_advanced_keys",
        "+<advanced_keys.c> +<advanced_key_combo.c> "
        "+<advanced_key_dynamic_keystroke.c> +<advanced_key_macro.c> "
        "+<advanced_key_null_bind.c> +<advanced_key_tap_hold.c> "
        "+<advanced_key_toggle.c>",
    )
    pio_config["env:native_test_layout"] = native_test_env(
        "test_layout",
        "+<layout.c> +<profile_runtime.c>",
        ["-DRGB_ENABLED=1"],
    )
    pio_config["env:native_test_event_pipeline"] = native_test_env(
        "test_event_pipeline",
        "+<advanced_keys.c> +<advanced_key_combo.c> "
        "+<advanced_key_dynamic_keystroke.c> +<advanced_key_macro.c> "
        "+<advanced_key_null_bind.c> +<advanced_key_tap_hold.c> "
        "+<advanced_key_toggle.c> +<deferred_actions.c> +<layout.c>",
    )
    pio_config["env:native_test_hid"] = native_test_env(
        "test_hid",
        "+<hid.c>",
        [
            "-I test/test_hid",
            "-DCFG_TUSB_MCU=0",
            "-DBOARD_USB_FS=1",
        ],
    )
    pio_config["env:native_test_hid_usbmon_diag"] = native_test_env(
        "test_hid",
        "+<hid.c>",
        [
            "-I test/test_hid",
            "-DCFG_TUSB_MCU=0",
            "-DBOARD_USB_FS=1",
            "-DUSBMON_DIAGNOSTIC_RAW_HID_STREAM=1",
        ],
    )
    pio_config["env:native_test_xinput"] = native_test_env(
        "test_xinput",
        "+<xinput.c>",
        [
            "-I test/test_xinput",
            "-DJOYSTICK_ENABLED",
            "-DCFG_TUSB_MCU=0",
            "-DBOARD_USB_FS=1",
        ],
    )
    pio_config["env:native_test_matrix"] = native_test_env(
        "test_matrix",
        "+<matrix.c>",
    )
    pio_config["env:native_test_analog_scan"] = native_test_env(
        "test_analog_scan",
        "+<analog_scan.c>",
        [
            "-DADC_NUM_CHANNELS=4",
            "-DADC_NUM_MUX_INPUTS=2",
            "-DADC_MUX_INPUT_CHANNELS='{0, 1}'",
            "-DADC_NUM_MUX_SELECT_PINS=1",
            "-DADC_MUX_SELECT_PORTS='{0}'",
            "-DADC_MUX_SELECT_PINS='{0}'",
            "-DADC_MUX_INPUT_MATRIX='{{1, 3}, {2, 0}}'",
            "-DADC_NUM_RAW_INPUTS=2",
            "-DADC_RAW_INPUT_CHANNELS='{0, 0}'",
            "-DADC_RAW_INPUT_VECTOR='{4, 0}'",
        ],
    )
    pio_config["env:native_test_migration"] = native_test_env(
        "test_migration",
        "+<migration.c>",
        [
            "-DRGB_ENABLED=1",
            "-DJOYSTICK_ENABLED=1",
        ],
    )
    pio_config["env:native_test_joystick"] = native_test_env(
        "test_joystick",
        "+<joystick.c>",
        [
            "-I test/test_joystick",
            "-lm",
            "-DJOYSTICK_ENABLED=1",
            "-DJOYSTICK_X_ADC_INDEX=0",
            "-DJOYSTICK_Y_ADC_INDEX=1",
            "-DJOYSTICK_SW_PORT=GPIOA",
            "-DJOYSTICK_SW_PIN=GPIO_PIN_0",
        ],
    )
    pio_config["env:native_test_rgb_animated"] = native_test_env(
        "test_rgb_animated",
        "+<rgb_animated.c>",
        [
            "-DRGB_ENABLED=1",
            "-DNUM_LEDS=4",
        ],
    )
    pio_config["env:native_test_encoder"] = native_test_env(
        "test_encoder",
        "+<encoder.c>",
        [
            "-I test/test_encoder",
            "-DENCODER_NUM=1",
            "-DENCODER_A_PORTS='{GPIOA}'",
            "-DENCODER_A_PINS='{GPIO_PIN_0}'",
            "-DENCODER_B_PORTS='{GPIOA}'",
            "-DENCODER_B_PINS='{GPIO_PIN_1}'",
            "-DENCODER_CW_KEYS='{4}'",
            "-DENCODER_CCW_KEYS='{5}'",
            "-DENCODER_INPUT_ACTIVE_HIGH",
        ],
    )
    pio_config["env:native_test_deferred_actions"] = native_test_env(
        "test_deferred_actions",
        "+<deferred_actions.c>",
    )
    pio_config["env:native_test_stm32_rgb"] = native_test_env(
        "test_stm32_rgb",
        "+<hardware/stm32f446xx/rgb.c>",
        [
            "-I test/test_stm32_rgb",
            "-DRGB_ENABLED=1",
            "-DNUM_LEDS=4",
            "-DRGB_DATA_PORT=GPIOA",
            "-DRGB_DATA_PIN=GPIO_PIN_8",
        ],
    )
    pio_config["env:native_test_usb_runtime"] = native_test_env(
        "test_usb_runtime",
        "+<usb_runtime.c>",
        ["-I test/test_usb_runtime"],
    )
    pio_config["env:native_test_dummy"] = {
        "platform": "native",
        "test_framework": "unity",
        "test_filter": "test_dummy",
        "test_build_src": "no",
    }

    with open("platformio.ini", "w") as f:
        pio_config.write(f)
