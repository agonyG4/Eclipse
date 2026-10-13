#!/usr/bin/env python3
"""Guard the Bar module's production and CMake dependency boundary."""

from pathlib import Path
import re
import sys


def target_links(cmake: str, target: str) -> str:
    match = re.search(
        rf"target_link_libraries\s*\(\s*{re.escape(target)}\s+([^)]+)\)",
        cmake,
        re.DOTALL,
    )
    return match.group(1) if match else ""


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: check_bar_architecture.py SOURCE_ROOT", file=sys.stderr)
        return 2

    source_root = Path(sys.argv[1])
    bar_root = source_root / "Bar"
    shell_cmake_path = source_root / "Shell" / "CMakeLists.txt"
    failures: list[str] = []

    for source_path in sorted((*bar_root.rglob("*.cpp"), *bar_root.rglob("*.hpp"))):
        if "tests" in source_path.relative_to(bar_root).parts:
            continue
        source = source_path.read_text(encoding="utf-8")
        relative_path = source_path.relative_to(source_root)
        if re.search(r"^\s*#\s*include\s*[<\"][^>\"]*(?:Shell|Spotlight)/", source, re.MULTILINE):
            failures.append(f"{relative_path}: includes a Shell or Spotlight path")
        for dependency in ("Astrea::Shell::ContextMenuController", "SpotlightController"):
            if dependency in source:
                failures.append(f"{relative_path}: references {dependency}")

    shell_cmake = shell_cmake_path.read_text(encoding="utf-8")
    if re.search(r"Bar/(?:core|platform/wayland)/[^\s\")]+\.cpp", shell_cmake):
        failures.append("Shell/CMakeLists.txt directly compiles a Bar production source")
    if "astrea-bar-core" not in target_links(shell_cmake, "astrea-shell-runtime"):
        failures.append("astrea-shell-runtime does not link astrea-bar-core")
    if "astrea-bar-wayland" not in target_links(shell_cmake, "astrea-shell"):
        failures.append("astrea-shell does not link astrea-bar-wayland")

    if failures:
        print("Bar architecture boundary violations:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1

    print("Bar architecture boundary is intact.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
