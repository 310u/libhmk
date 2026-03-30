#!/usr/bin/env python3

import argparse
import os
import re
import subprocess
import sys

sys.path.append("scripts")

import utils

FLASH_BASE = 0x08000000
RAM_BASE = 0x20000000
DEFAULT_MIN_RAM_HEADROOM = 8192
DEFAULT_MIN_FLASH_HEADROOM = 16384


def parse_size_expression(expr: str) -> int:
    total = 0
    for sign, number, unit in re.findall(r"([+-]?)\s*(\d+)\s*([KkMm]?)", expr):
        value = int(number)
        if unit in ("K", "k"):
            value *= 1024
        elif unit in ("M", "m"):
            value *= 1024 * 1024
        total += -value if sign == "-" else value
    return total


def linker_ram_length(ldscript_path: str) -> int:
    with open(ldscript_path, "r", encoding="utf-8") as handle:
        for line in handle:
            if "RAM" not in line or "LENGTH" not in line:
                continue
            match = re.search(r"LENGTH\s*=\s*([^/\n]+)", line)
            if match:
                return parse_size_expression(match.group(1).strip())
    raise ValueError(f"RAM LENGTH not found in {ldscript_path}")


def load_sections(elf_path: str) -> dict[str, dict[str, int]]:
    output = subprocess.check_output(
        ["objdump", "-h", elf_path], text=True, encoding="utf-8"
    )
    sections: dict[str, dict[str, int]] = {}
    pattern = re.compile(
        r"^\s*\d+\s+(\S+)\s+([0-9A-Fa-f]+)\s+([0-9A-Fa-f]+)\s+([0-9A-Fa-f]+)\s+"
    )
    for line in output.splitlines():
        match = pattern.match(line)
        if not match:
            continue
        name, size_hex, vma_hex, lma_hex = match.groups()
        sections[name] = {
            "size": int(size_hex, 16),
            "vma": int(vma_hex, 16),
            "lma": int(lma_hex, 16),
        }
    return sections


def main() -> int:
    parser = argparse.ArgumentParser(description="Check firmware memory headroom")
    parser.add_argument("--keyboard", required=True)
    parser.add_argument("--elf", required=True)
    parser.add_argument("--min-ram-headroom", type=int)
    parser.add_argument("--min-flash-headroom", type=int)
    args = parser.parse_args()

    keyboard = args.keyboard
    kb_json = utils.get_kb_json(keyboard)
    driver = utils.get_driver(keyboard)
    memory_budget = kb_json.get("memory_budget", {})
    min_ram_headroom = (
        args.min_ram_headroom
        if args.min_ram_headroom is not None
        else memory_budget.get("min_ram_headroom", DEFAULT_MIN_RAM_HEADROOM)
    )
    min_flash_headroom = (
        args.min_flash_headroom
        if args.min_flash_headroom is not None
        else memory_budget.get("min_flash_headroom", DEFAULT_MIN_FLASH_HEADROOM)
    )

    wl_cfg = kb_json.get("wear_leveling", {})
    wl_virtual_size = wl_cfg.get("virtual_size", 8192)
    wl_write_log_size = wl_cfg.get("write_log_size", 65536)
    reserved_flash = driver.metadata.flash.round_up_to_flash_sectors(
        wl_virtual_size + wl_write_log_size
    )
    flash_limit = driver.metadata.flash.get_flash_size() - reserved_flash

    ldscript_path = os.path.join("linker", driver.platformio.ldscript)
    ram_length = linker_ram_length(ldscript_path)

    sections = load_sections(args.elf)
    heap_stack_reserved = sections.get("._user_heap_stack", {}).get("size", 0)

    used_ram = sum(
        section["size"]
        for name, section in sections.items()
        if RAM_BASE <= section["vma"] < RAM_BASE + ram_length
        and name != "._user_heap_stack"
    )
    used_flash = sum(
        section["size"]
        for name, section in sections.items()
        if FLASH_BASE <= section["lma"] < FLASH_BASE + flash_limit
    )

    data_ram_limit = ram_length - heap_stack_reserved
    ram_headroom = data_ram_limit - used_ram
    flash_headroom = flash_limit - used_flash

    print(
        f"[size] {keyboard}: flash_used={used_flash}/{flash_limit} "
        f"headroom={flash_headroom}"
    )
    print(
        f"[size] {keyboard}: ram_used={used_ram}/{data_ram_limit} "
        f"headroom={ram_headroom} reserved_heap_stack={heap_stack_reserved}"
    )

    failures: list[str] = []
    if flash_headroom < min_flash_headroom:
        failures.append(
            f"flash headroom {flash_headroom} is below "
            f"{min_flash_headroom} bytes"
        )
    if ram_headroom < min_ram_headroom:
        failures.append(
            f"RAM headroom {ram_headroom} is below "
            f"{min_ram_headroom} bytes"
        )

    if failures:
        for failure in failures:
            print(f"[size] ERROR: {failure}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
