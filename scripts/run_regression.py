#!/usr/bin/env python3

import argparse
import subprocess
import sys
from pathlib import Path


NATIVE_TEST_ENVS = [
    "native_test_advanced_keys",
    "native_test_analog_scan",
    "native_test_deferred_actions",
    "native_test_encoder",
    "native_test_event_pipeline",
    "native_test_hid",
    "native_test_hid_usbmon_diag",
    "native_test_joystick",
    "native_test_layout",
    "native_test_matrix",
    "native_test_migration",
    "native_test_rgb_animated",
    "native_test_stm32_rgb",
    "native_test_usb_runtime",
    "native_test_xinput",
]


def run_command(cmd, cwd):
    print("+", " ".join(cmd), flush=True)
    subprocess.run(cmd, cwd=cwd, check=True)


def main():
    parser = argparse.ArgumentParser(description="Run libhmk regression checks")
    parser.add_argument(
        "--keyboard",
        "-k",
        required=True,
        help="Keyboard environment to build, for example mochiko40he",
    )
    parser.add_argument(
        "--skip-setup",
        action="store_true",
        help="Do not regenerate platformio.ini before running checks",
    )
    parser.add_argument(
        "--skip-native",
        action="store_true",
        help="Skip native unit test environments",
    )
    parser.add_argument(
        "--skip-build",
        action="store_true",
        help="Skip firmware build environments",
    )
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parent.parent

    if not args.skip_setup:
        run_command([sys.executable, "setup.py", "-k", args.keyboard], repo_root)

    if not args.skip_native:
        for env in NATIVE_TEST_ENVS:
            run_command(["pio", "test", "-e", env], repo_root)

    if not args.skip_build:
        for env in [args.keyboard, f"{args.keyboard}_recovery"]:
            run_command(["pio", "run", "-e", env], repo_root)


if __name__ == "__main__":
    main()
