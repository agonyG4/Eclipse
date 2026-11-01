#!/usr/bin/env python3
"""Generate runnable browser bundles and the native host in the build directory."""

import argparse
import json
from pathlib import Path
import shutil


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    source = Path(__file__).resolve().parents[1] / "browser"
    checkout = source.parents[1]
    release_output = (checkout / "build" / "release" / "Downloads").resolve()
    if output.is_relative_to(checkout) and output != release_output:
        parser.error("Generated bundles must be outside the source checkout or in build/release/Downloads")
    output.mkdir(parents=True, exist_ok=True)
    host = output / "astrea-downloads-native-host"
    shutil.copyfile(source / "native_host.py", host)
    host.chmod(0o755)
    for browser in ("zen", "brave"):
        bundle = output / "browser" / browser
        bundle.mkdir(parents=True, exist_ok=True)
        manifest = json.loads((source / f"{browser}.json").read_text())
        (bundle / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        shutil.copyfile(source / "background.js", bundle / "background.js")
        (bundle / "config.js").write_text(f'globalThis.ASTREA_BROWSER = "{browser}";\n')
        if browser == "brave":
            (bundle / "worker.js").write_text('importScripts("config.js", "background.js");\n')


if __name__ == "__main__":
    main()
