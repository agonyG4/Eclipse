"""Check generated bundles and host registration without touching real profiles."""

import base64
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

tools, service = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
with tempfile.TemporaryDirectory(prefix="pkg-", dir=service.parent) as directory:
    root = Path(directory)
    generated = root / "generated"
    subprocess.run([sys.executable, str(tools / "prepare-browser-bridge.py"),
                    "--output", str(generated)], check=True)
    (generated / "astrea-downloads").symlink_to(service)
    assert os.access(generated / "astrea-downloads-native-host", os.X_OK)
    brave = json.loads((generated / "browser/brave/manifest.json").read_text())
    zen = json.loads((generated / "browser/zen/manifest.json").read_text())
    assert brave["manifest_version"] == 3
    assert zen["manifest_version"] == 2
    assert brave["permissions"] == zen["permissions"] == ["downloads", "nativeMessaging", "storage"]
    assert brave["incognito"] == zen["incognito"] == "not_allowed"
    digest = hashlib.sha256(base64.b64decode(brave["key"], validate=True)).hexdigest()[:32]
    extension_id = "".join(chr(ord("a") + int(value, 16)) for value in digest)
    home = root / "user"
    command = [sys.executable, str(tools / "install-browser-bridge.py"),
               "--build-dir", str(generated), "--home", str(home)]
    subprocess.run(command, check=True, stdout=subprocess.DEVNULL)
    subprocess.run(command, check=True, stdout=subprocess.DEVNULL)
    mozilla = home / ".mozilla/native-messaging-hosts/org.astrea.downloads.json"
    chromium = home / ".config/BraveSoftware/Brave-Browser/NativeMessagingHosts/org.astrea.downloads.json"
    assert json.loads(mozilla.read_text())["allowed_extensions"] == ["downloads@astrea.local"]
    assert json.loads(chromium.read_text())["allowed_origins"] == [f"chrome-extension://{extension_id}/"]
    before = mozilla.read_bytes()
    chromium.write_text('{"name":"foreign"}')
    result = subprocess.run(command, capture_output=True)
    assert result.returncode != 0
    assert mozilla.read_bytes() == before
    chromium.unlink()
    subprocess.run(command + ["--uninstall"], check=True, stdout=subprocess.DEVNULL)
    assert not mozilla.exists() and not chromium.exists()
print("Browser packaging and installation checks passed")
