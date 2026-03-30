#!/usr/bin/env python3

import argparse
import os
import sys
from pathlib import Path

sys.path.append("scripts")

import utils


def parse_stack_usage_line(line: str) -> dict[str, str | int] | None:
    parts = line.rstrip().split("\t")
    if len(parts) < 2 or not parts[1].isdigit():
        return None

    location = parts[0]
    qualifiers = parts[2] if len(parts) > 2 else ""
    location_parts = location.rsplit(":", 3)
    if len(location_parts) == 4:
        source_path, line_no, column_no, function = location_parts
        location_suffix = f"{line_no}:{column_no}"
    else:
        source_path = location
        function = "<unknown>"
        location_suffix = "?"

    return {
        "source_path": source_path,
        "location": location_suffix,
        "function": function,
        "size": int(parts[1]),
        "qualifiers": qualifiers,
    }


def collect_stack_usage(build_dir: str) -> list[dict[str, str | int]]:
    entries: list[dict[str, str | int]] = []
    for su_path in Path(build_dir).rglob("*.su"):
        with su_path.open("r", encoding="utf-8") as handle:
            for line in handle:
                entry = parse_stack_usage_line(line)
                if entry is not None:
                    entries.append(entry)
    return entries


def main() -> int:
    parser = argparse.ArgumentParser(description="Summarize GCC stack usage data")
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--keyboard")
    parser.add_argument("--limit", type=int, default=20)
    parser.add_argument("--max-stack-frame", type=int)
    parser.add_argument("--output")
    args = parser.parse_args()

    max_stack_frame = args.max_stack_frame
    if args.keyboard and max_stack_frame is None:
        kb_json = utils.get_kb_json(args.keyboard)
        max_stack_frame = kb_json.get("memory_budget", {}).get("max_stack_frame")

    entries = collect_stack_usage(args.build_dir)
    if not entries:
        print(f"[stack] ERROR: no .su files found in {args.build_dir}", file=sys.stderr)
        return 1

    entries.sort(key=lambda entry: (-int(entry["size"]), str(entry["function"])))

    lines: list[str] = []
    label = args.keyboard or os.path.basename(os.path.abspath(args.build_dir))
    lines.append(f"[stack] {label}: top {min(args.limit, len(entries))} stack frames")
    for entry in entries[: args.limit]:
        qualifiers = f" [{entry['qualifiers']}]" if entry["qualifiers"] else ""
        source_path = os.path.relpath(str(entry["source_path"]), os.getcwd())
        lines.append(
            f"[stack] {int(entry['size']):>5}B {entry['function']} "
            f"({source_path}:{entry['location']}){qualifiers}"
        )

    failures = [
        entry
        for entry in entries
        if max_stack_frame is not None and int(entry["size"]) > max_stack_frame
    ]
    if max_stack_frame is not None:
        lines.append(f"[stack] limit: {max_stack_frame}B")

    output = "\n".join(lines)
    print(output)

    if args.output:
        with open(args.output, "w", encoding="utf-8") as handle:
            handle.write(output + "\n")

    if failures:
        largest = failures[0]
        print(
            f"[stack] ERROR: {largest['function']} uses {largest['size']} bytes, "
            f"above the {max_stack_frame}-byte limit",
            file=sys.stderr,
        )
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
