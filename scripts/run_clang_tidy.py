#!/usr/bin/env python3

import argparse
import json
import subprocess
import sys
from pathlib import Path


def resolve_entry_file(entry: dict[str, str]) -> Path:
    file_path = Path(entry["file"])
    if file_path.is_absolute():
        return file_path.resolve()
    return (Path(entry["directory"]) / file_path).resolve()


def collect_project_files(database_dir: Path, repo_root: Path) -> list[Path]:
    with (database_dir / "compile_commands.json").open("r", encoding="utf-8") as handle:
        entries = json.load(handle)

    files: list[Path] = []
    seen: set[Path] = set()
    for entry in entries:
        file_path = resolve_entry_file(entry)
        try:
            relative_path = file_path.relative_to(repo_root)
        except ValueError:
            continue

        if relative_path.parts[0] == ".pio" or file_path.suffix != ".c":
            continue
        if relative_path.parts[0] != "src":
            continue
        if file_path in seen:
            continue

        seen.add(file_path)
        files.append(file_path)

    return sorted(files)


def run_clang_tidy(clang_tidy: str, database_dir: Path, repo_root: Path) -> int:
    files = collect_project_files(database_dir, repo_root)
    if not files:
        print(
            f"[clang-tidy] ERROR: no project source files found in {database_dir}",
            file=sys.stderr,
        )
        return 1

    print(
        f"[clang-tidy] {database_dir.name}: checking {len(files)} source files "
        f"with {clang_tidy}"
    )

    cmd = [clang_tidy, f"-p={database_dir}", "--quiet"]
    cmd.extend(str(file_path) for file_path in files)

    return subprocess.run(cmd, cwd=repo_root, check=False).returncode


def main() -> int:
    parser = argparse.ArgumentParser(description="Run clang-tidy on project sources")
    parser.add_argument(
        "--database-dir",
        action="append",
        required=True,
        help="Directory containing compile_commands.json",
    )
    parser.add_argument("--clang-tidy", default="clang-tidy")
    parser.add_argument(
        "--repo-root",
        default=Path(__file__).resolve().parents[1],
        type=Path,
    )
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()

    for database_dir_arg in args.database_dir:
        database_dir = Path(database_dir_arg).resolve()
        result = run_clang_tidy(
            args.clang_tidy,
            database_dir,
            repo_root,
        )
        if result != 0:
            return result

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
