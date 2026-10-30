#!/usr/bin/env python3
"""Resolve one CMake configure preset's binaryDir using its environment macros."""

from __future__ import annotations

import json
import os
import re
import sys
from pathlib import Path


def resolve_binary_dir(project_root: Path, preset_name: str) -> Path:
    presets = json.loads((project_root / "CMakePresets.json").read_text())
    by_name = {
        preset["name"]: preset
        for preset in presets.get("configurePresets", [])
    }
    if preset_name not in by_name:
        raise ValueError(f"unknown configure preset: {preset_name}")

    def expanded(preset: dict[str, object], seen: set[str]) -> dict[str, object]:
        name = str(preset["name"])
        if name in seen:
            raise ValueError(f"configure preset inheritance cycle at {name}")
        result: dict[str, object] = {}
        inherits = preset.get("inherits", [])
        if isinstance(inherits, str):
            inherits = [inherits]
        for parent_name in inherits:
            if parent_name not in by_name:
                raise ValueError(f"missing inherited configure preset: {parent_name}")
            result.update(expanded(by_name[parent_name], seen | {name}))
        result.update(preset)
        return result

    preset = expanded(by_name[preset_name], set())
    binary_dir = preset.get("binaryDir")
    if not isinstance(binary_dir, str):
        raise ValueError(f"configure preset has no binaryDir: {preset_name}")

    def replace_macro(match: re.Match[str]) -> str:
        variable = match.group(1) or f"penv:{match.group(2)}"
        if variable == "sourceDir":
            return str(project_root)
        if variable == "presetName":
            return preset_name
        if variable.startswith("penv:"):
            env_name = variable.removeprefix("penv:")
            value = os.environ.get(env_name)
            if not value:
                raise ValueError(f"preset environment variable is unset: {env_name}")
            return value
        raise ValueError(f"unsupported CMake preset macro: {variable}")

    resolved = re.sub(r"\$\{([^}]+)\}|\$penv\{([^}]+)\}",
                      replace_macro, binary_dir)
    path = Path(resolved)
    if not path.is_absolute():
        path = project_root / path
    override_root = os.environ.get("ASTREA_CMAKE_BUILD_ROOT")
    if override_root:
        path = Path(override_root) / path.name
        if not path.is_absolute():
            raise ValueError("ASTREA_CMAKE_BUILD_ROOT must be an absolute path")
    return path.resolve()
def main() -> int:
    if len(sys.argv) != 3:
        print("usage: preset-binary-dir.py PROJECT_ROOT PRESET", file=sys.stderr)
        return 2
    try:
        print(resolve_binary_dir(Path(sys.argv[1]).resolve(), sys.argv[2]))
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(error, file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
