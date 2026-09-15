#!/usr/bin/env python3
"""Fail when the private Typhon and Eclipse capture contracts drift."""

from pathlib import Path
import sys


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: check_astrea_screen_capture_protocol.py ECLIPSE_TYPHON", file=sys.stderr)
        return 2
    left = Path(sys.argv[1]).read_bytes()
    right = Path(sys.argv[2]).read_bytes()
    if left != right:
        print("astrea-screen-capture-v1.xml differs between Eclipse and Typhon", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
