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


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: check_bar_qml.py BAR_QML_DIR", file=sys.stderr)
        return 2

    qml_root = Path(sys.argv[1])
    bar_root = qml_root.parent if qml_root.name == "qml" else qml_root
    failures = []
    paths = list(qml_root.rglob("*.qml"))
    for directory in (bar_root / "core", bar_root / "platform"):
        paths.extend(directory.rglob("*.cpp"))
        paths.extend(directory.rglob("*.hpp"))
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
