#!/usr/bin/env python3
"""Guard the Bar module's production and CMake dependency boundary."""

from pathlib import Path
import re
import sys


def target_links(cmake: str, target: str) -> str:
    matches = re.findall(
        rf"target_link_libraries\s*\(\s*{re.escape(target)}\s+([^)]+)\)",
        cmake,
        re.DOTALL | re.IGNORECASE,
    )
    return "\n".join(matches)


def production_sources(directory: Path) -> list[Path]:
    if not directory.exists():
        return []
    return sorted((*directory.rglob("*.cpp"), *directory.rglob("*.hpp")))


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: check_bar_architecture.py SOURCE_ROOT", file=sys.stderr)
        return 2

    source_root = Path(sys.argv[1])
    bar_root = source_root / "Bar"
    shell_cmake_path = source_root / "Shell" / "CMakeLists.txt"
    failures: list[str] = []

    for source_path in production_sources(bar_root):
        relative_bar_path = source_path.relative_to(bar_root)
        if "tests" in relative_bar_path.parts:
            continue
        source = source_path.read_text(encoding="utf-8")
        relative_path = source_path.relative_to(source_root)
        if re.search(r"^\s*#\s*include\s*[<\"][^>\"]*(?:Shell|Spotlight)/", source, re.MULTILINE):
            failures.append(f"{relative_path}: includes a Shell or Spotlight path")
        for dependency in ("Astrea::Shell::ContextMenuController", "SpotlightController"):
            if dependency in source:
                failures.append(f"{relative_path}: references {dependency}")

        includes = re.findall(r"^\s*#\s*include\s*[<\"]([^>\"]+)", source, re.MULTILINE)
        if relative_bar_path.parts[0] == "core":
            if any(re.search(r"(?:^|/)platform/", include.replace("\\", "/"))
                   for include in includes):
                failures.append(f"{relative_path}: core includes a platform path")
        elif relative_bar_path.parts[0] == "services":
            def includes_bar_platform(include: str) -> bool:
                normalized = include.replace("\\", "/")
                return (normalized.startswith(("platform/", "./platform/", "../platform/",
                                               "Bar/platform/"))
                        or "/Bar/platform/" in normalized)

            if any(includes_bar_platform(include) for include in includes):
                failures.append(f"{relative_path}: service includes a Bar platform path")

        if "AstreaLayerShellConfig" in source or "AstreaLayerShellHelper" in source:
            if relative_bar_path.parts[:2] != ("platform", "wayland"):
                failures.append(f"{relative_path}: Layer Shell policy must stay in the Wayland layer")

    shell_cmake = shell_cmake_path.read_text(encoding="utf-8")
    if re.search(r"Bar/(?:core|services|platform/wayland)/[^\s\")]+\.cpp", shell_cmake):
        failures.append("Shell/CMakeLists.txt directly compiles a Bar production source")
    if "astrea-bar-core" not in target_links(shell_cmake, "astrea-shell-runtime"):
        failures.append("astrea-shell-runtime does not link astrea-bar-core")
    if "astrea-bar-wayland" not in target_links(shell_cmake, "astrea-shell"):
        failures.append("astrea-shell does not link astrea-bar-wayland")

    bar_cmake = (bar_root / "CMakeLists.txt").read_text(encoding="utf-8")
    core_links = target_links(bar_cmake, "astrea-bar-core")
    for dependency in ("astrea-shared-layer-shell", "astrea-bar-wayland"):
        if re.search(rf"\b{re.escape(dependency)}\b", core_links):
            failures.append(f"astrea-bar-core must not link {dependency}")
    if "astrea-bar-core" not in target_links(bar_cmake, "astrea-bar-wayland"):
        failures.append("astrea-bar-wayland does not link astrea-bar-core")

    if failures:
        print("Bar architecture boundary violations:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1

    print("Bar architecture boundary is intact.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
