#!/usr/bin/env python3
"""Reject legacy shell integrations from production Eclipse Bar sources."""

from pathlib import Path
import sys


FORBIDDEN = (
    "import Quickshell",
    "import Quickshell.Io",
    "import Quickshell.Hyprland",
    "import Quickshell.Services.SystemTray",
    "Process {",
    "FileView {",
    "IpcHandler {",
    "wpctl",
    "nmcli",
    "bluetoothctl",
    "hyprctl",
    "ddcutil",
    "systemctl",
    "python",
    "python3",
    "astrea-statusd",
    "quickshell",
    "ASTREA_ROOT",
)


def production_paths(root: Path):
    if root.name == "qml":
        yield from root.rglob("*.qml")
        source_root = root.parent
        for directory in (source_root / "core", source_root / "platform"):
            yield from directory.rglob("*.cpp")
            yield from directory.rglob("*.hpp")
        return
    for suffix in ("*.qml", "*.cpp", "*.hpp", "*.h", "*.cc", "*.cxx"):
        yield from root.rglob(suffix)


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: check_bar_qml.py PRODUCTION_ROOT ...", file=sys.stderr)
        return 2

    failures = []
    paths = {path for argument in sys.argv[1:] for path in production_paths(Path(argument))}
    for path in sorted(paths):
        text = path.read_text(encoding="utf-8")
        for token in FORBIDDEN:
            if token in text:
                failures.append(f"{path}: prohibited token {token!r}")

    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
