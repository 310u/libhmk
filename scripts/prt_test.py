#!/usr/bin/env python3
"""PRT (Predictive Acceleration Trigger) hardware verification tool.

Polls a specific key's filtered ADC value and travel distance via Raw HID
COMMAND_ANALOG_INFO, logging results as CSV with millisecond timestamps.

Usage:
    python scripts/prt_test.py --key 0 --duration 10 > prt_key0.csv
    python scripts/prt_test.py --key 0 --rt-down 10 --rt-up 10 --duration 5

The CSV columns are: timestamp_ms, adc_filtered, distance.

Raw HID interface:
  - report size: 64 bytes
  - no report ID
  - vendor usage page 0xFFAB, usage 0xAB
"""

import argparse
import struct
import sys
import time

RAW_HID_USAGE_PAGE = 0xFFAB
RAW_HID_USAGE = 0xAB
RAW_HID_EP_SIZE = 64

COMMAND_ANALOG_INFO = 5
COMMAND_SET_ACTUATION_MAP = 131


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


def set_actuation_map(device, key: int, actuation_point: int, rt_down: int, rt_up: int,
                      continuous: bool = False, profile: int = 0):
    """Configure Rapid Trigger parameters for a single key."""
    # command_id(1) + profile(1) + offset(1) + len(1) + actuation_t(4)
    payload = struct.pack(
        "<BBBB",
        profile,
        key,
        1,  # len = 1 key
        actuation_point,
    )
    payload += struct.pack(
        "<BBB",
        rt_down,
        rt_up,
        1 if continuous else 0,
    )
    send_command_and_read_response(device, COMMAND_SET_ACTUATION_MAP, payload)


def read_analog_info(device, key: int) -> tuple[int, int]:
    """Return (adc_filtered, distance) for the given key."""
    payload = struct.pack("<B", key)
    response = send_command_and_read_response(device, COMMAND_ANALOG_INFO, payload)
    # response[0] = command_id
    # response[1:4] = first entry: adc_value (2 bytes) + distance (1 byte)
    adc_value, distance = struct.unpack_from("<HB", response, 1)
    return int(adc_value), int(distance)


def main():
    parser = argparse.ArgumentParser(
        description="Poll a key's ADC/distance for PRT verification"
    )
    parser.add_argument(
        "--key",
        type=int,
        default=0,
        help="Key index to poll (default: 0)",
    )
    parser.add_argument(
        "--duration",
        type=float,
        default=5.0,
        help="Polling duration in seconds (default: 5.0)",
    )
    parser.add_argument(
        "--interval",
        type=float,
        default=0.001,
        help="Polling interval in seconds (default: 0.001 = 1ms)",
    )
    parser.add_argument(
        "--rt-down",
        type=int,
        default=None,
        help="Set rt_down for the key before polling (optional)",
    )
    parser.add_argument(
        "--rt-up",
        type=int,
        default=None,
        help="Set rt_up for the key before polling (optional)",
    )
    parser.add_argument(
        "--actuation-point",
        type=int,
        default=128,
        help="Set actuation_point for the key before polling (default: 128)",
    )
    parser.add_argument(
        "--continuous",
        action="store_true",
        help="Enable continuous RT mode for the key",
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
        f"({dev_info['vendor_id']:04X}:{dev_info['product_id']:04X})",
        file=sys.stderr,
    )

    if args.rt_down is not None or args.rt_up is not None:
        rt_down = args.rt_down if args.rt_down is not None else 10
        rt_up = args.rt_up if args.rt_up is not None else 10
        set_actuation_map(
            device,
            args.key,
            args.actuation_point,
            rt_down,
            rt_up,
            args.continuous,
        )
        print(
            f"Set actuation map for key {args.key}: "
            f"ap={args.actuation_point} rt_down={rt_down} rt_up={rt_up} "
            f"continuous={args.continuous}",
            file=sys.stderr,
        )

    print("timestamp_ms,adc_filtered,distance")
    start = time.perf_counter()
    next_sample = start
    while True:
        now = time.perf_counter()
        if now - start >= args.duration:
            break
        if now < next_sample:
            time.sleep(max(0.0, next_sample - now))
            continue
        next_sample += args.interval

        try:
            adc, distance = read_analog_info(device, args.key)
            timestamp_ms = (time.perf_counter() - start) * 1000.0
            print(f"{timestamp_ms:.3f},{adc},{distance}")
        except RuntimeError as e:
            print(f"# read error: {e}", file=sys.stderr)


if __name__ == "__main__":
    main()
