#!/usr/bin/env python3
"""Measure Linux usbmon interrupt-IN completion cadence for a USB device.

Examples:
  python scripts/usbmon_polling_rate.py --list
  sudo python scripts/usbmon_polling_rate.py --vid 0108 --pid 0111 --duration 5
  sudo python scripts/usbmon_polling_rate.py --product Mochiko40HE --endpoint 0x83 --duration 8
  python scripts/usbmon_polling_rate.py --capture-file /tmp/mochiko-usbmon.txt --vid 0108 --pid 0111 --endpoint 0x83

Notes:
  - Live capture needs root because usbmon is exposed through debugfs.
  - usbmon is close to the bus, but it is not a hardware analyzer. A single
    key press often produces sparse completions, so prefer a continuously
    changing analog/mouse/gamepad input when you want to observe ~125 us gaps.
"""

from __future__ import annotations

import argparse
import os
import re
import select
import statistics
import subprocess
import sys
import threading
import time
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path


DEBUGFS_MOUNTPOINT = Path("/sys/kernel/debug")
UMON_MAGIC = b"UMON"
UMON_CONTROL_DISABLE = 0
UMON_CONTROL_ENABLE = 1
RAW_HID_PACKET_SIZE = 64
USBMON_LINE_RE = re.compile(
    r"^(?P<tag>\S+)\s+"
    r"(?P<timestamp>\d+)\s+"
    r"(?P<event>[A-Z])\s+"
    r"(?P<address>[A-Za-z][io]:\d+:\d+:\d+)"
)
USBMON_ADDRESS_RE = re.compile(
    r"^(?P<transfer>[A-Za-z])(?P<direction>[io]):"
    r"(?P<bus>\d+):(?P<device>\d+):(?P<endpoint>\d+)$"
)
USBDEVICES_BUS_RE = re.compile(r"Bus=(\d+)")
USBDEVICES_DEVICE_RE = re.compile(r"Dev#=\s*(\d+)")
USBDEVICES_SPEED_RE = re.compile(r"Spd=(\d+)")
USBDEVICES_VIDPID_RE = re.compile(r"Vendor=([0-9A-Fa-f]{4}) ProdID=([0-9A-Fa-f]{4})")
USBDEVICES_ENDPOINT_RE = re.compile(
    r"Ad=([0-9A-Fa-f]{2})\(([IO])\).*?Ivl=([0-9]+)([a-zA-Z]+)"
)


@dataclass
class UsbEndpoint:
    address: int
    direction: str
    interval_us: int | None = None
    interface_number: int | None = None

    @property
    def number(self) -> int:
        return self.address & 0x0F

    @property
    def label(self) -> str:
        return f"0x{self.address:02x}"


@dataclass
class UsbDevice:
    bus: int | None = None
    device: int | None = None
    speed_mbps: int | None = None
    vid: str | None = None
    pid: str | None = None
    manufacturer: str | None = None
    product: str | None = None
    serial: str | None = None
    endpoints: list[UsbEndpoint] = field(default_factory=list)

    @property
    def description(self) -> str:
        product = self.product or "(unknown product)"
        vidpid = f"{self.vid or '????'}:{self.pid or '????'}"
        bus = "?" if self.bus is None else str(self.bus)
        device = "?" if self.device is None else str(self.device)
        speed = "?" if self.speed_mbps is None else str(self.speed_mbps)
        return f"{product} [{vidpid}] bus={bus} dev={device} speed={speed}M"

    @property
    def in_endpoints(self) -> list[UsbEndpoint]:
        return [endpoint for endpoint in self.endpoints if endpoint.direction == "I"]


@dataclass
class EndpointCapture:
    endpoint: UsbEndpoint
    events: int = 0
    last_timestamp_us: int | None = None
    deltas_us: list[int] = field(default_factory=list)
    diagnostic_samples: list["DiagnosticSample"] = field(default_factory=list)

    def record(self, timestamp_us: int) -> None:
        self.events += 1
        if self.last_timestamp_us is not None:
            self.deltas_us.append(timestamp_us - self.last_timestamp_us)
        self.last_timestamp_us = timestamp_us


@dataclass
class UsbmonCompletion:
    bus: int
    device: int
    endpoint: int
    timestamp_us: int
    payload: bytes


@dataclass
class DiagnosticSample:
    sequence: int
    tick_ms: int
    completion_gap_cycles: int
    rearm_cycles: int
    send_interval_cycles: int
    cpu_hz: int
    send_cycle: int


@dataclass
class HidrawDrain:
    device_node: Path
    interface_number: int
    stop_event: threading.Event = field(default_factory=threading.Event)
    thread: threading.Thread | None = None
    file_descriptor: int | None = None
    diagnostic_samples: list["DiagnosticSample"] = field(default_factory=list)

    def start(self) -> None:
        if self.thread is not None:
            return
        self.file_descriptor = os.open(self.device_node, os.O_RDWR | os.O_NONBLOCK)
        self.thread = threading.Thread(target=self._drain_loop, daemon=True)
        self.thread.start()
        self._write_control_packet(True)

    def stop(self) -> None:
        if self.file_descriptor is not None:
            self._write_control_packet(False)
        self.stop_event.set()
        if self.thread is not None:
            self.thread.join(timeout=1.0)
            self.thread = None
        if self.file_descriptor is not None:
            os.close(self.file_descriptor)
            self.file_descriptor = None

    def _drain_loop(self) -> None:
        assert self.file_descriptor is not None
        while not self.stop_event.is_set():
            readable, _, _ = select.select([self.file_descriptor], [], [], 0.2)
            if not readable:
                continue
            try:
                chunk = os.read(self.file_descriptor, 65536)
            except BlockingIOError:
                continue
            except OSError:
                break
            if not chunk:
                time.sleep(0.01)
                continue
            diagnostic_sample = parse_diagnostic_sample(chunk)
            if diagnostic_sample is not None:
                self.diagnostic_samples.append(diagnostic_sample)

    def _write_control_packet(self, enabled: bool) -> None:
        if self.file_descriptor is None:
            return

        packet = bytearray(64)
        packet[:4] = UMON_MAGIC
        packet[4] = UMON_CONTROL_ENABLE if enabled else UMON_CONTROL_DISABLE
        try:
            os.write(self.file_descriptor, packet)
        except OSError as exc:
            action = "enable" if enabled else "disable"
            print(
                f"Warning: failed to {action} diagnostic stream via {self.device_node}: {exc}",
                file=sys.stderr,
            )


def parse_hex_id(value: str) -> str:
    normalized = value.lower()
    if normalized.startswith("0x"):
        normalized = normalized[2:]
    if len(normalized) != 4 or any(char not in "0123456789abcdef" for char in normalized):
        raise argparse.ArgumentTypeError(f"expected a 4-digit hex value, got {value!r}")
    return normalized


def parse_endpoint_address(value: str) -> int:
    normalized = value.lower()
    base = 16 if normalized.startswith("0x") or any(char in "abcdef" for char in normalized) else 10
    try:
        address = int(normalized, base)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"invalid endpoint address {value!r}") from exc
    if not 0 <= address <= 0xFF:
        raise argparse.ArgumentTypeError(f"endpoint address out of range: {value!r}")
    if address < 0x80:
        raise argparse.ArgumentTypeError(
            f"expected an IN endpoint like 0x83, got {value!r}"
        )
    return address


def interval_to_us(raw_value: str, unit: str) -> int | None:
    value = int(raw_value)
    unit_lower = unit.lower()
    if unit_lower == "us":
        return value
    if unit_lower == "ms":
        return value * 1000
    if unit_lower == "s":
        return value * 1_000_000
    return None


def load_usb_devices_output() -> str:
    completed = subprocess.run(
        ["usb-devices"], check=True, capture_output=True, text=True
    )
    return completed.stdout


def parse_usb_devices(text: str) -> list[UsbDevice]:
    devices: list[UsbDevice] = []
    current: UsbDevice | None = None
    current_interface_number: int | None = None

    for raw_line in text.splitlines():
        line = raw_line.rstrip()
        if not line:
            if current is not None:
                devices.append(current)
                current = None
            current_interface_number = None
            continue

        prefix = line[:2]
        body = line[2:].strip()

        if prefix == "T:":
            if current is not None:
                devices.append(current)
            current = UsbDevice()
            bus_match = USBDEVICES_BUS_RE.search(body)
            device_match = USBDEVICES_DEVICE_RE.search(body)
            speed_match = USBDEVICES_SPEED_RE.search(body)
            if bus_match is not None:
                current.bus = int(bus_match.group(1))
            if device_match is not None:
                current.device = int(device_match.group(1))
            if speed_match is not None:
                current.speed_mbps = int(speed_match.group(1))
            continue

        if current is None:
            continue

        if prefix == "P:":
            vidpid_match = USBDEVICES_VIDPID_RE.search(body)
            if vidpid_match is not None:
                current.vid = vidpid_match.group(1).lower()
                current.pid = vidpid_match.group(2).lower()
            continue

        if prefix == "S:":
            if body.startswith("Manufacturer="):
                current.manufacturer = body.partition("=")[2].strip()
            elif body.startswith("Product="):
                current.product = body.partition("=")[2].strip()
            elif body.startswith("SerialNumber="):
                current.serial = body.partition("=")[2].strip()
            continue

        if prefix == "I:":
            interface_match = re.search(r"If#=\s*(\d+)", body)
            current_interface_number = (
                int(interface_match.group(1)) if interface_match is not None else None
            )
            continue

        if prefix == "E:":
            endpoint_match = USBDEVICES_ENDPOINT_RE.search(body)
            if endpoint_match is None:
                continue
            address = int(endpoint_match.group(1), 16)
            direction = endpoint_match.group(2)
            interval_us = interval_to_us(
                endpoint_match.group(3), endpoint_match.group(4)
            )
            current.endpoints.append(
                UsbEndpoint(
                    address=address,
                    direction=direction,
                    interval_us=interval_us,
                    interface_number=current_interface_number,
                )
            )

    if current is not None:
        devices.append(current)

    return devices


def read_sysfs_text(path: Path) -> str | None:
    try:
        return path.read_text(encoding="utf-8").strip()
    except OSError:
        return None


def find_usb_sysfs_device(device: UsbDevice) -> Path | None:
    if device.bus is None or device.device is None:
        return None

    usb_root = Path("/sys/bus/usb/devices")
    for candidate in usb_root.iterdir():
        if ":" in candidate.name or not candidate.is_dir():
            continue
        busnum = read_sysfs_text(candidate / "busnum")
        devnum = read_sysfs_text(candidate / "devnum")
        if busnum is None or devnum is None:
            continue
        try:
            if int(busnum) != device.bus or int(devnum) != device.device:
                continue
        except ValueError:
            continue

        if device.vid is not None:
            vendor = read_sysfs_text(candidate / "idVendor")
            if vendor is None or vendor.casefold() != device.vid:
                continue
        if device.pid is not None:
            product = read_sysfs_text(candidate / "idProduct")
            if product is None or product.casefold() != device.pid:
                continue
        return candidate

    return None


def find_hidraw_nodes_for_interface(
    device: UsbDevice, interface_number: int
) -> list[Path]:
    usb_device_path = find_usb_sysfs_device(device)
    if usb_device_path is None:
        return []

    interface_name = f"{usb_device_path.name}:1.{interface_number}"
    hidraw_nodes: list[Path] = []
    for hidraw_class_path in sorted(Path("/sys/class/hidraw").glob("hidraw*")):
        try:
            resolved_device = hidraw_class_path.resolve(strict=True)
        except OSError:
            continue
        if any(parent.name == interface_name for parent in resolved_device.parents):
            hidraw_nodes.append(Path("/dev") / hidraw_class_path.name)
    return hidraw_nodes


def start_hidraw_drains(
    device: UsbDevice, capture_map: dict[int, EndpointCapture]
) -> list[HidrawDrain]:
    drains: list[HidrawDrain] = []
    seen_interfaces: set[int] = set()
    for capture in capture_map.values():
        endpoint = capture.endpoint
        if endpoint.address != 0x84 or endpoint.interface_number is None:
            continue
        if endpoint.interface_number in seen_interfaces:
            continue
        seen_interfaces.add(endpoint.interface_number)
        hidraw_nodes = find_hidraw_nodes_for_interface(device, endpoint.interface_number)
        for hidraw_node in hidraw_nodes:
            drain = HidrawDrain(
                device_node=hidraw_node, interface_number=endpoint.interface_number
            )
            try:
                drain.start()
            except OSError as exc:
                print(
                    f"Warning: failed to open {hidraw_node} for interface #{endpoint.interface_number}: {exc}",
                    file=sys.stderr,
                )
                continue
            drains.append(drain)
            print(
                f"Auto-draining {hidraw_node} for interface #{endpoint.interface_number}",
                file=sys.stderr,
            )
    return drains


def merge_hidraw_diagnostics(
    capture_map: dict[int, EndpointCapture], drains: list[HidrawDrain]
) -> None:
    for drain in drains:
        for capture in capture_map.values():
            endpoint = capture.endpoint
            if endpoint.address != 0x84:
                continue
            if endpoint.interface_number != drain.interface_number:
                continue
            capture.diagnostic_samples.extend(drain.diagnostic_samples)


def format_interval_us(interval_us: int | None) -> str:
    if interval_us is None:
        return "?"
    if interval_us % 1000 == 0:
        return f"{interval_us // 1000}ms"
    return f"{interval_us}us"


def print_device_list(devices: list[UsbDevice]) -> None:
    for device in devices:
        endpoints = ", ".join(
            f"{endpoint.label}({format_interval_us(endpoint.interval_us)})"
            for endpoint in device.in_endpoints
        ) or "-"
        print(f"{device.description} in_eps={endpoints}")


def filter_devices(devices: list[UsbDevice], args: argparse.Namespace) -> list[UsbDevice]:
    filtered = devices

    if args.bus is not None:
        filtered = [device for device in filtered if device.bus == args.bus]
    if args.device is not None:
        filtered = [device for device in filtered if device.device == args.device]
    if args.vid is not None:
        filtered = [device for device in filtered if device.vid == args.vid]
    if args.pid is not None:
        filtered = [device for device in filtered if device.pid == args.pid]
    if args.product is not None:
        product_fragment = args.product.casefold()
        filtered = [
            device
            for device in filtered
            if device.product is not None and product_fragment in device.product.casefold()
        ]

    return filtered


def select_device(devices: list[UsbDevice], args: argparse.Namespace) -> UsbDevice:
    filtered = filter_devices(devices, args)
    if not filtered:
        raise SystemExit(
            "No matching USB device found. Run with --list to inspect connected devices."
        )
    if len(filtered) > 1:
        print("Multiple devices matched. Narrow it down with --product or --bus/--device:")
        print_device_list(filtered)
        raise SystemExit(2)
    return filtered[0]


def percentile(sorted_values: list[int], q: float) -> float:
    if not sorted_values:
        raise ValueError("no values")
    if len(sorted_values) == 1:
        return float(sorted_values[0])
    position = (len(sorted_values) - 1) * q
    lower = int(position)
    upper = min(lower + 1, len(sorted_values) - 1)
    weight = position - lower
    return sorted_values[lower] * (1.0 - weight) + sorted_values[upper] * weight


def top_buckets(intervals_us: list[int], bucket_us: int = 5, limit: int = 5) -> str:
    buckets = Counter(round(value / bucket_us) * bucket_us for value in intervals_us)
    return ", ".join(
        f"{bucket}us x{count}" for bucket, count in buckets.most_common(limit)
    )


def parse_usbmon_payload(line: str) -> bytes:
    if "=" not in line:
        return b""
    _, payload_text = line.split("=", 1)
    byte_values = []
    for token in payload_text.strip().split():
        if len(token) != 2:
            continue
        try:
            byte_values.append(int(token, 16))
        except ValueError:
            continue
    return bytes(byte_values)


def unpack_u32_le(payload: bytes, offset: int) -> int:
    return int.from_bytes(payload[offset : offset + 4], byteorder="little")


def parse_diagnostic_sample(payload: bytes) -> DiagnosticSample | None:
    marker_offset = payload.find(UMON_MAGIC)
    if marker_offset < 0:
        return None
    payload = payload[marker_offset:]
    if len(payload) < 32:
        return None
    return DiagnosticSample(
        sequence=unpack_u32_le(payload, 4),
        tick_ms=unpack_u32_le(payload, 8),
        completion_gap_cycles=unpack_u32_le(payload, 12),
        rearm_cycles=unpack_u32_le(payload, 16),
        send_interval_cycles=unpack_u32_le(payload, 20),
        cpu_hz=unpack_u32_le(payload, 24),
        send_cycle=unpack_u32_le(payload, 28),
    )


def cycles_to_us(cycles: int, cpu_hz: int) -> float:
    if cpu_hz <= 0:
        return 0.0
    return (cycles * 1_000_000.0) / cpu_hz


def summarize_intervals(label: str, intervals_us: list[float]) -> None:
    if len(intervals_us) < 2:
        print(f"  {label}: not enough samples")
        return

    sorted_intervals = sorted(intervals_us)
    average = statistics.fmean(sorted_intervals)
    median = statistics.median(sorted_intervals)
    p90 = percentile(sorted_intervals, 0.90)
    minimum = sorted_intervals[0]
    maximum = sorted_intervals[-1]
    effective_hz = 1_000_000.0 / average if average else 0.0
    near_125 = sum(1 for value in sorted_intervals if 105.0 <= value <= 145.0)
    near_125_ratio = (near_125 / len(sorted_intervals)) * 100.0

    print(
        "  {} avg={:.1f}us median={:.1f}us p90={:.1f}us min={:.1f}us max={:.1f}us est={:.1f}Hz".format(
            label, average, median, p90, minimum, maximum, effective_hz
        )
    )
    print(
        "  {} near_125us={:.1f}% common={}".format(
            label, near_125_ratio, top_buckets([int(round(value)) for value in sorted_intervals])
        )
    )


def print_capture_summary(
    device: UsbDevice,
    capture_map: dict[int, EndpointCapture],
    duration_s: float | None,
    source_label: str,
) -> None:
    print(f"Device: {device.description}")
    endpoints = ", ".join(
        f"{entry.endpoint.label}({format_interval_us(entry.endpoint.interval_us)})"
        for entry in capture_map.values()
    )
    print(f"Endpoints: {endpoints}")
    if duration_s is not None:
        print(f"Source: {source_label} over {duration_s:.2f}s")
    else:
        print(f"Source: {source_label}")
    print()

    for endpoint_address in sorted(capture_map):
        capture = capture_map[endpoint_address]
        endpoint = capture.endpoint
        print(
            f"{endpoint.label}: completions={capture.events}, descriptor_ivl={format_interval_us(endpoint.interval_us)}"
        )
        if len(capture.deltas_us) < 2:
            print("  not enough completion samples")
            continue

        intervals = sorted(capture.deltas_us)
        average = statistics.fmean(intervals)
        median = statistics.median(intervals)
        p90 = percentile(intervals, 0.90)
        minimum = intervals[0]
        maximum = intervals[-1]
        effective_hz = 1_000_000.0 / average if average else 0.0
        near_125 = sum(1 for value in intervals if 105 <= value <= 145)
        near_125_ratio = (near_125 / len(intervals)) * 100.0

        print(
            "  avg={:.1f}us median={:.1f}us p90={:.1f}us min={}us max={}us est={:.1f}Hz".format(
                average, median, p90, minimum, maximum, effective_hz
            )
        )
        print(
            "  near_125us={:.1f}% common={}".format(
                near_125_ratio, top_buckets(intervals)
            )
        )

        if not capture.diagnostic_samples:
            continue

        cpu_hz_values = {sample.cpu_hz for sample in capture.diagnostic_samples if sample.cpu_hz}
        sequence_gaps = 0
        for previous, current in zip(capture.diagnostic_samples, capture.diagnostic_samples[1:]):
            if current.sequence > previous.sequence + 1:
                sequence_gaps += current.sequence - previous.sequence - 1

        print(
            "  diagnostic samples={} cpu_hz={} sequence_gaps={}".format(
                len(capture.diagnostic_samples),
                ",".join(str(value) for value in sorted(cpu_hz_values)) or "?",
                sequence_gaps,
            )
        )
        summarize_intervals(
            "device completion",
            [
                cycles_to_us(sample.completion_gap_cycles, sample.cpu_hz)
                for sample in capture.diagnostic_samples
                if sample.sequence != 0 and sample.cpu_hz > 0
            ],
        )
        summarize_intervals(
            "device rearm",
            [
                cycles_to_us(sample.rearm_cycles, sample.cpu_hz)
                for sample in capture.diagnostic_samples
                if sample.sequence != 0 and sample.cpu_hz > 0
            ],
        )
        summarize_intervals(
            "device send interval",
            [
                cycles_to_us(sample.send_interval_cycles, sample.cpu_hz)
                for sample in capture.diagnostic_samples
                if sample.sequence != 0 and sample.cpu_hz > 0
            ],
        )


def parse_usbmon_line(line: str) -> UsbmonCompletion | None:
    line_match = USBMON_LINE_RE.match(line)
    if line_match is None or line_match.group("event") != "C":
        return None

    address_match = USBMON_ADDRESS_RE.match(line_match.group("address"))
    if address_match is None:
        return None
    if address_match.group("transfer") != "I" or address_match.group("direction") != "i":
        return None

    return UsbmonCompletion(
        bus=int(address_match.group("bus")),
        device=int(address_match.group("device")),
        endpoint=int(address_match.group("endpoint")),
        timestamp_us=int(line_match.group("timestamp")),
        payload=parse_usbmon_payload(line),
    )


def build_capture_map(
    device: UsbDevice, endpoint_addresses: set[int]
) -> tuple[dict[int, EndpointCapture], dict[int, int]]:
    capture_map: dict[int, EndpointCapture] = {}
    endpoint_number_map: dict[int, int] = {}
    for endpoint in device.in_endpoints:
        if endpoint.address not in endpoint_addresses:
            continue
        capture_map[endpoint.address] = EndpointCapture(endpoint=endpoint)
        endpoint_number_map[endpoint.number] = endpoint.address
    return capture_map, endpoint_number_map


def ensure_debugfs_mounted() -> None:
    with Path("/proc/mounts").open("r", encoding="utf-8") as handle:
        mounts = handle.read().splitlines()

    for line in mounts:
        fields = line.split()
        if len(fields) >= 3 and fields[1] == str(DEBUGFS_MOUNTPOINT) and fields[2] == "debugfs":
            return

    subprocess.run(
        ["mount", "-t", "debugfs", "debugfs", str(DEBUGFS_MOUNTPOINT)],
        check=True,
        capture_output=True,
        text=True,
    )


def ensure_usbmon_ready(bus: int) -> Path:
    if os.geteuid() != 0:
        raise SystemExit("Live usbmon capture needs root. Re-run this script with sudo.")

    try:
        ensure_debugfs_mounted()
    except subprocess.CalledProcessError as exc:
        stderr = exc.stderr.strip() or exc.stdout.strip() or str(exc)
        raise SystemExit(f"Failed to mount debugfs at {DEBUGFS_MOUNTPOINT}: {stderr}") from exc

    try:
        subprocess.run(
            ["modprobe", "usbmon"], check=True, capture_output=True, text=True
        )
    except subprocess.CalledProcessError as exc:
        stderr = exc.stderr.strip() or exc.stdout.strip() or str(exc)
        raise SystemExit(f"Failed to load usbmon module: {stderr}") from exc

    usbmon_path = DEBUGFS_MOUNTPOINT / "usb" / "usbmon" / f"{bus}u"
    deadline = time.monotonic() + 1.0
    while time.monotonic() < deadline:
        if usbmon_path.exists():
            return usbmon_path
        time.sleep(0.05)

    raise SystemExit(
        f"{usbmon_path} does not exist even after loading usbmon. "
        "Check that the kernel has usbmon support enabled."
    )


def iter_live_usbmon_lines(path: Path, duration_s: float) -> list[str]:
    file_descriptor = os.open(path, os.O_RDONLY | os.O_NONBLOCK)
    deadline = time.monotonic() + duration_s
    lines: list[str] = []
    buffer = b""

    try:
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            readable, _, _ = select.select([file_descriptor], [], [], remaining)
            if not readable:
                continue
            try:
                chunk = os.read(file_descriptor, 65536)
            except BlockingIOError:
                # usbmon can briefly report readiness but still return EAGAIN
                # on the next non-blocking read. Just wait for the next poll.
                continue
            if not chunk:
                continue
            buffer += chunk
            while b"\n" in buffer:
                raw_line, buffer = buffer.split(b"\n", 1)
                lines.append(raw_line.decode("utf-8", errors="replace"))
    finally:
        os.close(file_descriptor)

    return lines


def iter_file_lines(path: Path) -> list[str]:
    with path.open("r", encoding="utf-8", errors="replace") as handle:
        return [line.rstrip("\n") for line in handle]


def collect_samples(
    lines: list[str],
    device: UsbDevice,
    capture_map: dict[int, EndpointCapture],
    endpoint_number_map: dict[int, int],
) -> None:
    for line in lines:
        parsed = parse_usbmon_line(line)
        if parsed is None:
            continue
        if parsed.bus != device.bus or parsed.device != device.device:
            continue
        endpoint_address = endpoint_number_map.get(parsed.endpoint)
        if endpoint_address is None:
            continue
        capture = capture_map[endpoint_address]
        capture.record(parsed.timestamp_us)
        diagnostic_sample = parse_diagnostic_sample(parsed.payload)
        if diagnostic_sample is not None:
            capture.diagnostic_samples.append(diagnostic_sample)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Measure usbmon interrupt-IN completion intervals"
    )
    parser.add_argument("--list", action="store_true", help="List connected USB devices")
    parser.add_argument("--vid", type=parse_hex_id, help="USB vendor ID, e.g. 0108")
    parser.add_argument("--pid", type=parse_hex_id, help="USB product ID, e.g. 0111")
    parser.add_argument(
        "--product",
        help="Case-insensitive product name substring, e.g. Mochiko40HE",
    )
    parser.add_argument("--bus", type=int, help="USB bus number, e.g. 1")
    parser.add_argument("--device", type=int, help="USB device number, e.g. 55")
    parser.add_argument(
        "--endpoint",
        action="append",
        default=[],
        type=parse_endpoint_address,
        help="IN endpoint address to inspect, e.g. 0x83. Can be repeated.",
    )
    parser.add_argument(
        "--duration",
        type=float,
        default=5.0,
        help="Live capture duration in seconds (default: 5)",
    )
    parser.add_argument(
        "--capture-file",
        type=Path,
        help="Parse a saved usbmon text capture instead of capturing live",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    devices = parse_usb_devices(load_usb_devices_output())

    if args.list:
        print_device_list(devices)
        return

    if args.pid is not None and args.vid is None:
        raise SystemExit("--pid requires --vid")

    device = select_device(devices, args)

    endpoint_addresses = set(args.endpoint)
    if not endpoint_addresses:
        endpoint_addresses = {endpoint.address for endpoint in device.in_endpoints}

    capture_map, endpoint_number_map = build_capture_map(device, endpoint_addresses)
    if not capture_map:
        raise SystemExit("No matching IN endpoints found on the selected device.")

    if args.capture_file is not None:
        lines = iter_file_lines(args.capture_file)
        source_label = str(args.capture_file)
        duration_s = None
    else:
        usbmon_path = ensure_usbmon_ready(device.bus)
        drains: list[HidrawDrain] = []
        drains = start_hidraw_drains(device, capture_map)
        if any(
            capture.endpoint.address == 0x84 and capture.endpoint.interface_number is not None
            for capture in capture_map.values()
        ) and not drains:
            print(
                "Warning: no matching hidraw node found for the selected raw HID interface.",
                file=sys.stderr,
            )
        try:
            lines = iter_live_usbmon_lines(usbmon_path, args.duration)
        finally:
            for drain in drains:
                drain.stop()
        source_label = str(usbmon_path)
        duration_s = args.duration

    collect_samples(lines, device, capture_map, endpoint_number_map)
    if args.capture_file is None:
        merge_hidraw_diagnostics(capture_map, drains)
    print_capture_summary(device, capture_map, duration_s, source_label)

    if all(len(capture.deltas_us) < 2 for capture in capture_map.values()):
        print()
        print(
            "Hint: completions were sparse. Use a continuously changing input such as an analog/gamepad"
        )
        print("      axis or mouse movement instead of a single key press when you want to observe ~125us gaps.")


if __name__ == "__main__":
    main()
