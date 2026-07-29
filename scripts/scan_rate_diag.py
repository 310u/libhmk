#!/usr/bin/env python3
"""Query matrix and analog scan diagnostics from a libhmk keyboard over Raw HID.

Raw HID interface:
  - report size: 64 bytes
  - no report ID
  - vendor usage page 0xFFAB, usage 0xAB

Example:
  python scripts/scan_rate_diag.py
  python scripts/scan_rate_diag.py --reset
  python scripts/scan_rate_diag.py --bootloader
"""

import argparse
import struct
import sys
import time

RAW_HID_USAGE_PAGE = 0xFFAB
RAW_HID_USAGE = 0xAB
RAW_HID_EP_SIZE = 64

COMMAND_BOOTLOADER = 2
COMMAND_GET_MATRIX_SCAN_DIAGNOSTICS = 149
COMMAND_RESET_MATRIX_SCAN_DIAGNOSTICS = 150
COMMAND_GET_ANALOG_SCAN_DIAGNOSTICS = 151
COMMAND_RESET_ANALOG_SCAN_DIAGNOSTICS = 152


def find_raw_hid_device(vid: int | None, pid: int | None):
    import hid

    candidates = []
    for dev in hid.enumerate():
        if vid is not None and dev["vendor_id"] != vid:
            continue
        if pid is not None and dev["product_id"] != pid:
            continue
        if dev.get("usage_page") != RAW_HID_USAGE_PAGE:
            continue
        if dev.get("usage") != RAW_HID_USAGE:
            continue
        candidates.append(dev)

    if not candidates:
        return None
    if len(candidates) > 1:
        print("Multiple Raw HID interfaces found; using the first one.", file=sys.stderr)
    return candidates[0]


def open_device(vid: int | None, pid: int | None):
    import hid

    dev_info = find_raw_hid_device(vid, pid)
    if dev_info is None:
        raise RuntimeError(
            "Raw HID interface not found. "
            "Is the keyboard connected and enumerated?"
        )

    device = hid.device()
    device.open_path(dev_info["path"])
    device.set_nonblocking(False)
    return device, dev_info


def send_command(device, command_id: int, payload: bytes = b""):
    report = bytes([command_id]) + payload
    report = report.ljust(RAW_HID_EP_SIZE, b"\x00")
    if len(report) != RAW_HID_EP_SIZE:
        raise ValueError(f"Report length {len(report)} != {RAW_HID_EP_SIZE}")
    # Prepend report ID 0 for Windows hidapi. TinyUSB Raw HID uses report ID 0
    # but the descriptor does not declare a report ID, so the report ID byte
    # must be included in the write call and is stripped by the OS on read.
    written = device.write(bytes([0]) + report)
    if written != 1 + RAW_HID_EP_SIZE:
        raise RuntimeError(
            f"Incomplete write: {written} bytes (expected {1 + RAW_HID_EP_SIZE})"
        )


def read_response(device, expected_command_id: int, timeout_ms: int = 1000) -> bytes:
    data = device.read(RAW_HID_EP_SIZE, timeout_ms)
    if not data:
        raise RuntimeError("No response from device (timeout)")
    if len(data) < RAW_HID_EP_SIZE:
        raise RuntimeError(f"Short response: {len(data)} bytes")
    response = bytes(data)
    if response[0] != expected_command_id:
        raise RuntimeError(
            f"Unexpected response command id: {response[0]} (expected {expected_command_id})"
        )
    return response


def send_command_and_read_response(
    device, command_id: int, payload: bytes = b"", timeout_ms: int = 1000
) -> bytes:
    send_command(device, command_id, payload)
    return read_response(device, command_id, timeout_ms)


def parse_matrix_diagnostics(response: bytes):
    # command_id (1) + payload (63)
    if response[0] != COMMAND_GET_MATRIX_SCAN_DIAGNOSTICS:
        raise RuntimeError(f"Unexpected response command id: {response[0]}")

    payload = response[1:]
    fmt = "<" + "I" * 15 + "H" + "B"  # 15 uint32 + 1 uint16 + 1 uint8 = 63 bytes
    fields = struct.unpack(fmt, payload[: struct.calcsize(fmt)])
    return {
        "scan_count": fields[0],
        "matrix_scan_hz": fields[1],
        "last_matrix_scan_us": fields[2],
        "max_matrix_scan_us": fields[3],
        "raw_scan_hz": fields[4],
        "last_raw_scan_us": fields[5],
        "max_raw_scan_us": fields[6],
        "full_scan_generation": fields[7],
        "missed_generation_count": fields[8],
        "matrix_processing_divider": fields[9],
        "intentional_skip_count": fields[10],
        "coalesced_generation_count": fields[11],
        "overload_missed_generation_count": fields[12],
        "scheduler_budget_exhausted_count": fields[13],
        "matrix_catchup_scan_count": fields[14],
        "expected_matrix_scan_hz": fields[15],
        "matrix_fast_overrun_count": fields[16],
    }


def parse_analog_diagnostics(response: bytes):
    if response[0] != COMMAND_GET_ANALOG_SCAN_DIAGNOSTICS:
        raise RuntimeError(f"Unexpected response command id: {response[0]}")

    payload = response[1:]
    fmt = "<" + "H" * 2 + "I" * 11  # 2 uint16 + 11 uint32 = 48 bytes
    fields = struct.unpack(fmt, payload[: struct.calcsize(fmt)])
    return {
        "mux_sample_delay_us": fields[0],
        "mux_step_count": fields[1],
        "scan_count": fields[2],
        "last_scan_cycles": fields[3],
        "max_scan_cycles": fields[4],
        "last_scan_us": fields[5],
        "max_scan_us": fields[6],
        "estimated_scan_hz": fields[7],
        "bad_channel_id_count": fields[8],
        "dma_overrun_count": fields[9],
        "overrun_count": fields[10],
        "spi_error_count": fields[11],
        "missed_scan_count": fields[12],
    }


def print_matrix_diagnostics(diag: dict):
    divider = diag["matrix_processing_divider"]
    raw_hz = diag["raw_scan_hz"]
    expected = raw_hz // divider if divider else 0

    print("=== Matrix Scan Diagnostics ===")
    print(f"  scan_count                  : {diag['scan_count']}")
    print(f"  matrix_scan_hz              : {diag['matrix_scan_hz']}")
    print(f"  expected_matrix_scan_hz     : {diag['expected_matrix_scan_hz']}")
    print(f"  computed expected (raw/div) : {expected}")
    print(f"  raw_scan_hz                 : {raw_hz}")
    print(f"  matrix_processing_divider   : {divider}")
    print(f"  last_matrix_scan_us         : {diag['last_matrix_scan_us']}")
    print(f"  max_matrix_scan_us          : {diag['max_matrix_scan_us']}")
    print(f"  last_raw_scan_us            : {diag['last_raw_scan_us']}")
    print(f"  max_raw_scan_us             : {diag['max_raw_scan_us']}")
    print(f"  full_scan_generation        : {diag['full_scan_generation']}")
    print(f"  missed_generation_count     : {diag['missed_generation_count']}")
    print(f"  intentional_skip_count      : {diag['intentional_skip_count']}")
    print(f"  coalesced_generation_count  : {diag['coalesced_generation_count']}")
    print(f"  overload_missed_gen_count   : {diag['overload_missed_generation_count']}")
    print(f"  scheduler_budget_exhausted  : {diag['scheduler_budget_exhausted_count']}")
    print(f"  matrix_fast_overrun_count   : {diag['matrix_fast_overrun_count']}")

    if diag["scheduler_budget_exhausted_count"] or diag["overload_missed_generation_count"]:
        print("\n  WARNING: scheduler is falling behind. Increase task intervals or reduce divider.")
    else:
        print("\n  OK: no scheduler overruns detected.")


def print_analog_diagnostics(diag: dict):
    print("=== Analog Scan Diagnostics ===")
    print(f"  estimated_scan_hz     : {diag['estimated_scan_hz']}")
    print(f"  mux_sample_delay_us   : {diag['mux_sample_delay_us']}")
    print(f"  mux_step_count        : {diag['mux_step_count']}")
    print(f"  scan_count            : {diag['scan_count']}")
    print(f"  last_scan_us          : {diag['last_scan_us']}")
    print(f"  max_scan_us           : {diag['max_scan_us']}")
    print(f"  last_scan_cycles      : {diag['last_scan_cycles']}")
    print(f"  max_scan_cycles       : {diag['max_scan_cycles']}")
    print(f"  bad_channel_id_count  : {diag['bad_channel_id_count']}")
    print(f"  dma_overrun_count     : {diag['dma_overrun_count']}")
    print(f"  overrun_count         : {diag['overrun_count']}")
    print(f"  spi_error_count       : {diag['spi_error_count']}")
    print(f"  missed_scan_count     : {diag['missed_scan_count']}")


def main():
    parser = argparse.ArgumentParser(
        description="Query libhmk keyboard scan-rate diagnostics via Raw HID"
    )
    parser.add_argument(
        "--vid",
        type=lambda x: int(x, 0),
        default=0x0108,
        help="USB Vendor ID (default: 0x0108)",
    )
    parser.add_argument(
        "--pid",
        type=lambda x: int(x, 0),
        default=0x0111,
        help="USB Product ID (default: 0x0111)",
    )
    parser.add_argument(
        "--reset",
        action="store_true",
        help="Reset diagnostics counters before reading",
    )
    parser.add_argument(
        "--bootloader",
        action="store_true",
        help="Send bootloader command and exit (enter DFU mode)",
    )
    args = parser.parse_args()

    try:
        import hid
    except ImportError as exc:
        print(
            "Error: Python 'hidapi' package is required.\n"
            "Install with:  pip install hidapi",
            file=sys.stderr,
        )
        raise SystemExit(1) from exc

    device, dev_info = open_device(args.vid, args.pid)
    print(
        f"Opened {dev_info['manufacturer_string']} {dev_info['product_string']} "
        f"({dev_info['vendor_id']:04X}:{dev_info['product_id']:04X}) "
        f"path={dev_info['path']!r}"
    )

    if args.bootloader:
        send_command(device, COMMAND_BOOTLOADER)
        print("Bootloader command sent. Device should now be in DFU mode.")
        return

    if args.reset:
        send_command_and_read_response(device, COMMAND_RESET_MATRIX_SCAN_DIAGNOSTICS)
        send_command_and_read_response(device, COMMAND_RESET_ANALOG_SCAN_DIAGNOSTICS)
        print("Diagnostics counters reset.\n")
        time.sleep(0.2)

    matrix_response = send_command_and_read_response(
        device, COMMAND_GET_MATRIX_SCAN_DIAGNOSTICS
    )
    matrix_diag = parse_matrix_diagnostics(matrix_response)
    print_matrix_diagnostics(matrix_diag)

    print()

    analog_response = send_command_and_read_response(
        device, COMMAND_GET_ANALOG_SCAN_DIAGNOSTICS
    )
    analog_diag = parse_analog_diagnostics(analog_response)
    print_analog_diagnostics(analog_diag)


if __name__ == "__main__":
    main()
