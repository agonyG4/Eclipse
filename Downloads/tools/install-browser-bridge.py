#!/usr/bin/env python3
"""Register Astrea Downloads Native Messaging hosts for this user's native browsers."""

import argparse
import base64
import hashlib
import json
import os
from pathlib import Path

HOST_NAME = "org.astrea.downloads"


def chrome_id(key):
    digest = hashlib.sha256(base64.b64decode(key, validate=True)).hexdigest()[:32]
    return "".join(chr(ord("a") + int(value, 16)) for value in digest)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path,
                        default=Path("/mnt/Aether/Desktop/GitHub/Eclipse-build/Downloads"))
    parser.add_argument("--home", type=Path, help="Explicit user root (for isolated installations)")
    parser.add_argument("--uninstall", action="store_true")
    args = parser.parse_args()
    build = args.build_dir.resolve()
    home = args.home.resolve() if args.home else Path.home()
    config = home / ".config" if args.home else Path(os.environ.get("XDG_CONFIG_HOME", home / ".config"))
    native_host = build / "astrea-downloads-native-host"
    service = build / "astrea-downloads"
    if not native_host.is_file() or not os.access(native_host, os.X_OK) or not service.is_file():
        parser.error("Build astrea-downloads and its browser assets before installing")
    zen = json.loads((build / "browser/zen/manifest.json").read_text())
    brave = json.loads((build / "browser/brave/manifest.json").read_text())
    extension_id = chrome_id(brave["key"])
    common = {"name": HOST_NAME, "description": "Astrea local download progress bridge",
              "path": str(native_host), "type": "stdio"}
    registrations = [
        (home / ".mozilla/native-messaging-hosts" / f"{HOST_NAME}.json",
         dict(common, allowed_extensions=[zen["browser_specific_settings"]["gecko"]["id"]])),
        (config / "BraveSoftware/Brave-Browser/NativeMessagingHosts" / f"{HOST_NAME}.json",
         dict(common, allowed_origins=[f"chrome-extension://{extension_id}/"]))
    ]
    # Preflight both destinations before changing either one.
    for path, expected in registrations:
        if path.is_symlink():
            parser.error(f"Refusing to replace a symbolic link: {path}")
        if path.exists():
            try:
                previous = json.loads(path.read_text())
            except (ValueError, OSError):
                parser.error(f"Existing manifest is not an Astrea registration: {path}")
            allowed_key = "allowed_extensions" if "allowed_extensions" in expected else "allowed_origins"
            if not isinstance(previous, dict) or previous.get("name") != HOST_NAME \
                    or previous.get("type") != "stdio" \
                    or previous.get(allowed_key) != expected[allowed_key]:
                parser.error(f"Refusing to replace another registration: {path}")
    for path, manifest in registrations:
        if args.uninstall:
            path.unlink(missing_ok=True)
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(json.dumps(manifest, indent=2) + "\n")
            path.chmod(0o600)
        print(("Removed " if args.uninstall else "Registered ") + str(path))
    if not args.uninstall:
        print("Brave extension ID:", extension_id)
        print("Load Brave unpacked extension:", build / "browser/brave")
        print("Load Zen temporary add-on:", build / "browser/zen/manifest.json")
        print("Browser activation is required; this installer does not enable extensions.")


if __name__ == "__main__":
    main()
