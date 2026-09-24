import sys

sys.path.append("scripts")

import argparse
import configparser
import os
import scripts.generate_board_def as generate_board_def
import scripts.utils as utils


def env_flag_enabled(name: str) -> bool:
    return os.getenv(name, "").lower() not in ("", "0", "false", "no")


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
    kb_json = utils.get_kb_json(keyboard)

    # Generate board_def.h from keyboard.json so keyboard definitions stay in
    # a single JSON file.  The generated header is consumed by scripts/make.py.
    generate_board_def.write_board_def(keyboard)

    driver = utils.get_driver(keyboard)
    cpu_hz = kb_json["hardware"].get("cpu_hz")
    native_sanitizers_enabled = env_flag_enabled("LIBHMK_NATIVE_SANITIZERS")
    stack_usage_enabled = env_flag_enabled("LIBHMK_STACK_USAGE")

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
    if stack_usage_enabled:
        build_src_flags.append("-fstack-usage")
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
        **(
            {
                "custom_diag_channel_identity": "true",
                "custom_diag_runtime": "true",
            }
            if keyboard == "mochiko40he"
            else {}
        ),
        "custom_keyboard_name": keyboard,
        "lib_deps": "\n".join(lib_deps),
        "platform": driver.platformio.platform,
        "test_ignore": "*",
        "upload_protocol": "dfu",
    }
    if keyboard == "mochiko40he":
        matrix_div2_base_flags = [
            "-DMATRIX_PROCESSING_DIVIDER=2",
            "-DMATRIX_SCHEDULER_BUDGET_US=63",
            "-DMATRIX_DETAILED_SCAN_DIAGNOSTICS=0",
            "-DMATRIX_LIVE_SCAN_TIMING_DIAGNOSTICS=1",
        ]
        pio_config[f"env:{keyboard}_matrix_div4"] = {
            "board": driver.platformio.board,
            "board_build.ldscript": f"linker/{driver.platformio.ldscript}",
            "build_flags": "\n".join(
                build_flags
                + [
                    "-DMATRIX_PROCESSING_DIVIDER=4",
                    "-DMATRIX_SCHEDULER_BUDGET_US=125",
                    "-DMATRIX_DETAILED_SCAN_DIAGNOSTICS=0",
                    "-DMATRIX_LIVE_SCAN_TIMING_DIAGNOSTICS=1",
                ]
            ),
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
        pio_config[f"env:{keyboard}_matrix_div2"] = {
            "board": driver.platformio.board,
            "board_build.ldscript": f"linker/{driver.platformio.ldscript}",
            "build_flags": "\n".join(
                build_flags
                + [
                    flag
                    for flag in matrix_div2_base_flags
                    if not flag.startswith("-DMATRIX_SCHEDULER_BUDGET_US=")
                ]
                + [
                    "-DHMK_STAGGER_BACKGROUND_TASKS=0",
                    "-DADC_SAMPLE_DELAY_DEFAULT=1",
                    "-DMATRIX_SCHEDULER_BUDGET_US=1200",
                    "-DHMK_LAYOUT_TASK_INTERVAL=32",
                    "-DHMK_LAYOUT_TASK_PHASE=0",
                    "-DHMK_XINPUT_TASK_INTERVAL=32",
                    "-DHMK_XINPUT_TASK_PHASE=16",
                    "-DHMK_COMMAND_TASK_INTERVAL=64",
                    "-DHMK_COMMAND_TASK_PHASE=8",
                    "-DHMK_TRACKBALL_TASK_INTERVAL=64",
                    "-DHMK_TRACKBALL_TASK_PHASE=4",
                    "-DHMK_JOYSTICK_TASK_INTERVAL=64",
                    "-DHMK_JOYSTICK_TASK_PHASE=20",
                    "-DHMK_ENCODER_TASK_INTERVAL=64",
                    "-DHMK_ENCODER_TASK_PHASE=36",
                    "-DHMK_SLIDER_TASK_INTERVAL=64",
                    "-DHMK_SLIDER_TASK_PHASE=52",
                    "-DHMK_RGB_TASK_INTERVAL=128",
                    "-DHMK_RGB_TASK_PHASE=28",
                ]
            ),
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
        pio_config[f"env:{keyboard}_matrix_div2_no_rgb"] = {
            "board": driver.platformio.board,
            "board_build.ldscript": f"linker/{driver.platformio.ldscript}",
            "build_flags": "\n".join(
                build_flags + matrix_div2_base_flags + ["-DHMK_ENABLE_RGB_TASK=0"]
            ),
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
        pio_config[f"env:{keyboard}_matrix_div2_no_layout"] = {
            "board": driver.platformio.board,
            "board_build.ldscript": f"linker/{driver.platformio.ldscript}",
            "build_flags": "\n".join(
                build_flags
                + matrix_div2_base_flags
                + [
                    "-DHMK_ENABLE_LAYOUT_TASK=0",
                    "-DHMK_ENABLE_XINPUT_TASK=0",
                ]
            ),
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
        pio_config[f"env:{keyboard}_matrix_div2_no_inputs"] = {
            "board": driver.platformio.board,
            "board_build.ldscript": f"linker/{driver.platformio.ldscript}",
            "build_flags": "\n".join(
                build_flags
                + matrix_div2_base_flags
                + [
                    "-DHMK_ENABLE_TRACKBALL_TASK=0",
                    "-DHMK_ENABLE_JOYSTICK_TASK=0",
                    "-DHMK_ENABLE_ENCODER_TASK=0",
                    "-DHMK_ENABLE_SLIDER_TASK=0",
                ]
            ),
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
        pio_config[f"env:{keyboard}_matrix_div2_min_tasks"] = {
            "board": driver.platformio.board,
            "board_build.ldscript": f"linker/{driver.platformio.ldscript}",
            "build_flags": "\n".join(
                build_flags
                + matrix_div2_base_flags
                + [
                    "-DHMK_ENABLE_LAYOUT_TASK=0",
                    "-DHMK_ENABLE_XINPUT_TASK=0",
                    "-DHMK_ENABLE_TRACKBALL_TASK=0",
                    "-DHMK_ENABLE_JOYSTICK_TASK=0",
                    "-DHMK_ENABLE_ENCODER_TASK=0",
                    "-DHMK_ENABLE_SLIDER_TASK=0",
                    "-DHMK_ENABLE_RGB_TASK=0",
                ]
            ),
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
        pio_config[f"env:{keyboard}_matrix_div2_tuned"] = {
            "board": driver.platformio.board,
            "board_build.ldscript": f"linker/{driver.platformio.ldscript}",
            "build_flags": "\n".join(
                build_flags
                + matrix_div2_base_flags
                + [
                    "-DHMK_STAGGER_BACKGROUND_TASKS=0",
                    "-DHMK_LAYOUT_TASK_INTERVAL=8",
                    "-DHMK_LAYOUT_TASK_PHASE=0",
                    "-DHMK_XINPUT_TASK_INTERVAL=8",
                    "-DHMK_XINPUT_TASK_PHASE=4",
                    "-DHMK_COMMAND_TASK_INTERVAL=8",
                    "-DHMK_COMMAND_TASK_PHASE=2",
                    "-DHMK_TRACKBALL_TASK_INTERVAL=16",
                    "-DHMK_TRACKBALL_TASK_PHASE=1",
                    "-DHMK_JOYSTICK_TASK_INTERVAL=16",
                    "-DHMK_JOYSTICK_TASK_PHASE=5",
                    "-DHMK_ENCODER_TASK_INTERVAL=16",
                    "-DHMK_ENCODER_TASK_PHASE=9",
                    "-DHMK_SLIDER_TASK_INTERVAL=16",
                    "-DHMK_SLIDER_TASK_PHASE=13",
                    "-DHMK_RGB_TASK_INTERVAL=32",
                    "-DHMK_RGB_TASK_PHASE=11",
                ]
            ),
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
        pio_config[f"env:{keyboard}_matrix_div2_tuned2"] = {
            "board": driver.platformio.board,
            "board_build.ldscript": f"linker/{driver.platformio.ldscript}",
            "build_flags": "\n".join(
                build_flags
                + matrix_div2_base_flags
                + [
                    "-DHMK_STAGGER_BACKGROUND_TASKS=0",
                    "-DADC_SAMPLE_DELAY_DEFAULT=1",
                    "-DHMK_LAYOUT_TASK_INTERVAL=16",
                    "-DHMK_LAYOUT_TASK_PHASE=0",
                    "-DHMK_XINPUT_TASK_INTERVAL=16",
                    "-DHMK_XINPUT_TASK_PHASE=8",
                    "-DHMK_COMMAND_TASK_INTERVAL=16",
                    "-DHMK_COMMAND_TASK_PHASE=4",
                    "-DHMK_TRACKBALL_TASK_INTERVAL=32",
                    "-DHMK_TRACKBALL_TASK_PHASE=2",
                    "-DHMK_JOYSTICK_TASK_INTERVAL=32",
                    "-DHMK_JOYSTICK_TASK_PHASE=10",
                    "-DHMK_ENCODER_TASK_INTERVAL=32",
                    "-DHMK_ENCODER_TASK_PHASE=18",
                    "-DHMK_SLIDER_TASK_INTERVAL=32",
                    "-DHMK_SLIDER_TASK_PHASE=26",
                    "-DHMK_RGB_TASK_INTERVAL=64",
                    "-DHMK_RGB_TASK_PHASE=14",
                ]
            ),
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
        pio_config[f"env:{keyboard}_matrix_div2_tuned3"] = {
            "board": driver.platformio.board,
            "board_build.ldscript": f"linker/{driver.platformio.ldscript}",
            "build_flags": "\n".join(
                build_flags
                + [
                    flag
                    for flag in matrix_div2_base_flags
                    if not flag.startswith("-DMATRIX_SCHEDULER_BUDGET_US=")
                ]
                + [
                    "-DHMK_STAGGER_BACKGROUND_TASKS=0",
                    "-DADC_SAMPLE_DELAY_DEFAULT=1",
                    "-DMATRIX_SCHEDULER_BUDGET_US=1200",
                    "-DHMK_LAYOUT_TASK_INTERVAL=16",
                    "-DHMK_LAYOUT_TASK_PHASE=0",
                    "-DHMK_XINPUT_TASK_INTERVAL=16",
                    "-DHMK_XINPUT_TASK_PHASE=8",
                    "-DHMK_COMMAND_TASK_INTERVAL=32",
                    "-DHMK_COMMAND_TASK_PHASE=4",
                    "-DHMK_TRACKBALL_TASK_INTERVAL=32",
                    "-DHMK_TRACKBALL_TASK_PHASE=2",
                    "-DHMK_JOYSTICK_TASK_INTERVAL=32",
                    "-DHMK_JOYSTICK_TASK_PHASE=10",
                    "-DHMK_ENCODER_TASK_INTERVAL=32",
                    "-DHMK_ENCODER_TASK_PHASE=18",
                    "-DHMK_SLIDER_TASK_INTERVAL=32",
                    "-DHMK_SLIDER_TASK_PHASE=26",
                    "-DHMK_RGB_TASK_INTERVAL=64",
                    "-DHMK_RGB_TASK_PHASE=14",
                ]
            ),
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
        pio_config[f"env:{keyboard}_matrix_div2_tuned4"] = {
            "board": driver.platformio.board,
            "board_build.ldscript": f"linker/{driver.platformio.ldscript}",
            "build_flags": "\n".join(
                build_flags
                + [
                    flag
                    for flag in matrix_div2_base_flags
                    if not flag.startswith("-DMATRIX_SCHEDULER_BUDGET_US=")
                ]
                + [
                    "-DHMK_STAGGER_BACKGROUND_TASKS=0",
                    "-DADC_SAMPLE_DELAY_DEFAULT=1",
                    "-DMATRIX_SCHEDULER_BUDGET_US=1200",
                    "-DHMK_LAYOUT_TASK_INTERVAL=32",
                    "-DHMK_LAYOUT_TASK_PHASE=0",
                    "-DHMK_XINPUT_TASK_INTERVAL=32",
                    "-DHMK_XINPUT_TASK_PHASE=16",
                    "-DHMK_COMMAND_TASK_INTERVAL=64",
                    "-DHMK_COMMAND_TASK_PHASE=8",
                    "-DHMK_TRACKBALL_TASK_INTERVAL=64",
                    "-DHMK_TRACKBALL_TASK_PHASE=4",
                    "-DHMK_JOYSTICK_TASK_INTERVAL=64",
                    "-DHMK_JOYSTICK_TASK_PHASE=20",
                    "-DHMK_ENCODER_TASK_INTERVAL=64",
                    "-DHMK_ENCODER_TASK_PHASE=36",
                    "-DHMK_SLIDER_TASK_INTERVAL=64",
                    "-DHMK_SLIDER_TASK_PHASE=52",
                    "-DHMK_RGB_TASK_INTERVAL=128",
                    "-DHMK_RGB_TASK_PHASE=28",
                ]
            ),
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
        pio_config[f"env:{keyboard}_matrix_div2_tuned5"] = {
            "board": driver.platformio.board,
            "board_build.ldscript": f"linker/{driver.platformio.ldscript}",
            "build_flags": "\n".join(
                build_flags
                + [
                    flag
                    for flag in matrix_div2_base_flags
                    if not flag.startswith("-DMATRIX_SCHEDULER_BUDGET_US=")
                ]
                + [
                    "-DHMK_STAGGER_BACKGROUND_TASKS=0",
                    "-DADC_SAMPLE_DELAY_DEFAULT=1",
                    "-DMATRIX_SCHEDULER_BUDGET_US=1400",
                    "-DHMK_LAYOUT_TASK_INTERVAL=40",
                    "-DHMK_LAYOUT_TASK_PHASE=0",
                    "-DHMK_XINPUT_TASK_INTERVAL=40",
                    "-DHMK_XINPUT_TASK_PHASE=20",
                    "-DHMK_COMMAND_TASK_INTERVAL=128",
                    "-DHMK_COMMAND_TASK_PHASE=16",
                    "-DHMK_TRACKBALL_TASK_INTERVAL=64",
                    "-DHMK_TRACKBALL_TASK_PHASE=4",
                    "-DHMK_JOYSTICK_TASK_INTERVAL=64",
                    "-DHMK_JOYSTICK_TASK_PHASE=20",
                    "-DHMK_ENCODER_TASK_INTERVAL=64",
                    "-DHMK_ENCODER_TASK_PHASE=36",
                    "-DHMK_SLIDER_TASK_INTERVAL=64",
                    "-DHMK_SLIDER_TASK_PHASE=52",
                    "-DHMK_RGB_TASK_INTERVAL=128",
                    "-DHMK_RGB_TASK_PHASE=28",
                ]
            ),
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
    analog = kb_json.get("analog", {})
    if "mux" in analog:
        pio_config[f"env:{keyboard}_diag"] = {
            "board": driver.platformio.board,
            "board_build.ldscript": f"linker/{driver.platformio.ldscript}",
            "build_flags": "\n".join(build_flags),
            "build_src_filter": "${env.build_src_filter}",
            "build_src_flags": "\n".join(build_src_flags),
            "extra_scripts": "\n".join(extra_scripts),
            "framework": driver.platformio.framework,
            "custom_diag_channel_identity": "true",
            "custom_keyboard_name": keyboard,
            "custom_usb_product_name": f"{kb_json['name']} Diagnostic",
            "lib_deps": "\n".join(lib_deps),
            "platform": driver.platformio.platform,
            "test_ignore": "*",
            "upload_protocol": "dfu",
        }
    if cpu_hz is not None:
        env_names = [f"env:{keyboard}", f"env:{keyboard}_recovery"]
        if keyboard == "mochiko40he":
            env_names.extend(
                [
                    f"env:{keyboard}_matrix_div4",
                    f"env:{keyboard}_matrix_div2",
                    f"env:{keyboard}_matrix_div2_no_rgb",
                    f"env:{keyboard}_matrix_div2_no_layout",
                    f"env:{keyboard}_matrix_div2_no_inputs",
                    f"env:{keyboard}_matrix_div2_min_tasks",
                    f"env:{keyboard}_matrix_div2_tuned",
                    f"env:{keyboard}_matrix_div2_tuned2",
                    f"env:{keyboard}_matrix_div2_tuned3",
                    f"env:{keyboard}_matrix_div2_tuned4",
                    f"env:{keyboard}_matrix_div2_tuned5",
                ]
            )
        if f"env:{keyboard}_diag" in pio_config:
            env_names.append(f"env:{keyboard}_diag")
        for env_name in env_names:
            pio_config[env_name]["board_build.f_cpu"] = f"{cpu_hz}L"

    def native_test_env(test_filter, build_src_filter, extra_flags=None):
        flags = [common_test_flags]
        if native_sanitizers_enabled:
            flags.extend(
                [
                    "-fsanitize=address,undefined",
                    "-fno-omit-frame-pointer",
                    "-fno-sanitize-recover=all",
                ]
            )
        return {
            "platform": "native",
            "test_framework": "unity",
            "test_filter": test_filter,
            "test_build_src": "yes",
            "build_src_filter": build_src_filter,
            "build_flags": "\n".join([*flags, *(extra_flags or [])]),
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
    pio_config["env:native_test_matrix_diagnostics"] = native_test_env(
        "test_matrix",
        "+<matrix.c>",
        [
            "-DMATRIX_DETAILED_SCAN_DIAGNOSTICS=1",
        ],
    )
    pio_config["env:native_test_matrix_fast_path_margin"] = native_test_env(
        "test_matrix",
        "+<matrix.c>",
        ["-DMATRIX_IDLE_RAW_FAST_PATH_MARGIN=8"],
    )
    pio_config["env:native_test_matrix_kalman_fast"] = native_test_env(
        "test_matrix",
        "+<matrix.c>",
        [
            "-DMATRIX_KALMAN_POSITION_GAIN=0.50f",
            "-DMATRIX_KALMAN_VELOCITY_GAIN=0.10f",
            "-DMATRIX_KALMAN_VELOCITY_DAMPING=0.85f",
            "-DMATRIX_RT_DOWN_MIN_VELOCITY=0.5f",
            "-DMATRIX_RT_UP_MIN_VELOCITY=0.5f",
        ],
    )
    pio_config["env:native_test_matrix_kalman_slow"] = native_test_env(
        "test_matrix",
        "+<matrix.c>",
        [
            "-DMATRIX_KALMAN_POSITION_GAIN=0.20f",
            "-DMATRIX_KALMAN_VELOCITY_GAIN=0.02f",
            "-DMATRIX_KALMAN_VELOCITY_DAMPING=0.95f",
            "-DMATRIX_RT_DOWN_MIN_VELOCITY=0.1f",
            "-DMATRIX_RT_UP_MIN_VELOCITY=0.1f",
        ],
    )
    pio_config["env:native_test_matrix_kalman_no_arm"] = native_test_env(
        "test_matrix",
        "+<matrix.c>",
        [
            "-DMATRIX_INNOVATION_EVENT_THRESHOLD=1000.0f",
        ],
    )
    pio_config["env:native_test_matrix_kalman_deadzone"] = native_test_env(
        "test_matrix",
        "+<matrix.c>",
        [
            "-DMATRIX_NOISE_DEADZONE=5",
        ],
    )
    pio_config["env:native_test_analog_scan"] = native_test_env(
        "test_analog_scan",
        "+<analog_scan.c>",
        [
            "-DHMK_DIAG_CHANNEL_IDENTITY=1",
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
    pio_config["env:native_test_commands"] = native_test_env(
        "test_commands",
        "+<commands.c>",
        [
            "-I test/test_commands",
            "-DCFG_TUSB_MCU=0",
            "-DBOARD_USB_FS=1",
            "-DRGB_ENABLED=1",
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
        "+<joystick.c> +<joystick_math.c>",
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
    rgb_test_flags = [
        "-I include",
        "-include test/test_rgb/test_rgb_config.h",
        "-DRGB_ENABLED=1",
        "-DNUM_LEDS=40",
    ]
    if native_sanitizers_enabled:
        rgb_test_flags.extend(
            [
                "-fsanitize=address,undefined",
                "-fno-omit-frame-pointer",
                "-fno-sanitize-recover=all",
            ]
        )
    pio_config["env:native_test_rgb"] = {
        "platform": "native",
        "test_framework": "unity",
        "test_filter": "test_rgb",
        "test_build_src": "yes",
        "build_src_filter": "+<rgb.c>",
        "build_flags": "\n".join(rgb_test_flags),
    }
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
    pio_config["env:native_test_encoder_inverted"] = native_test_env(
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
            "-DENCODER_INVERT_DIRECTIONS='{1}'",
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
