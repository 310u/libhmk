#!/usr/bin/env python3

import argparse
import json
import os
import sys


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(SCRIPT_DIR)
os.chdir(REPO_ROOT)
sys.path.append(SCRIPT_DIR)

import utils
from keyboard_metadata import build_keyboard_metadata


def load_args():
    parser = argparse.ArgumentParser(description="Export libhmk metadata as JSON")
    parser.add_argument(
        "--keyboard",
        help="Keyboard name under keyboards/<name>/keyboard.json",
    )
    parser.add_argument(
        "--keyboard-json",
        help="Path to a keyboard.json file to export",
    )
    args = parser.parse_args()

    if bool(args.keyboard) == bool(args.keyboard_json):
        parser.error("Specify exactly one of --keyboard or --keyboard-json")

    return args


def load_keyboard_json(args):
    if args.keyboard_json:
        with open(args.keyboard_json, "r") as f:
            kb_json = json.load(f)
        driver = utils.get_driver_by_name(kb_json["hardware"]["driver"])
        return kb_json, driver

    keyboard = args.keyboard
    kb_json = utils.get_kb_json(keyboard)
    driver = utils.get_driver(keyboard)
    return kb_json, driver


if __name__ == "__main__":
    args = load_args()
    kb_json, driver = load_keyboard_json(args)
    print(json.dumps(build_keyboard_metadata(kb_json, driver)))
