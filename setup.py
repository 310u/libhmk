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

    # Native unit test environments
    common_test_flags = "-I include\n-include test/test_config.h"
    pio_config["env:native_test_advanced_keys"] = {
        "platform": "native",
        "test_framework": "unity",
        "test_filter": "test_advanced_keys",
        "test_build_src": "yes",
        "build_src_filter": "+<advanced_keys.c>",
        "build_flags": common_test_flags,
    }
    pio_config["env:native_test_layout"] = {
        "platform": "native",
        "test_framework": "unity",
        "test_filter": "test_layout",
        "test_build_src": "yes",
        "build_src_filter": "+<layout.c>",
        "build_flags": common_test_flags,
    }
    pio_config["env:native_test_hid"] = {
        "platform": "native",
        "test_framework": "unity",
        "test_filter": "test_hid",
        "test_build_src": "yes",
        "build_src_filter": "+<hid.c>",
        "build_flags": "\n".join(
            [
                common_test_flags,
                "-I test/test_hid",
                "-DCFG_TUSB_MCU=0",
                "-DBOARD_USB_FS=1",
            ]
        ),
    }
    pio_config["env:native_test_dummy"] = {
        "platform": "native",
        "test_framework": "unity",
        "test_filter": "test_dummy",
        "test_build_src": "no",
    }

    with open("platformio.ini", "w") as f:
        pio_config.write(f)
