"""Exercise the real headless service and CLI; temporary files stay in the build tree."""

import json
import os
from pathlib import Path
import select
import stat
import subprocess
import sys
import tempfile


binary = str(Path(sys.argv[1]).resolve())


def line(process):
    assert select.select([process.stdout], [], [], 5)[0], "Timed out waiting for JSON"
    data = process.stdout.readline()
    assert data, "Process exited before replying"
    return json.loads(data)


with tempfile.TemporaryDirectory(prefix="dl-", dir=Path(binary).parent) as directory:
    endpoint = str(Path(directory) / "api.sock")
    base = [binary, "--socket", endpoint]

    def run(*args, success=True):
        result = subprocess.run(base + list(args), capture_output=True, text=True, timeout=5)
        assert (result.returncode == 0) == success, (result.returncode, result.stderr)
        return result

    for restart in range(2):
        service = subprocess.Popen(base + ["serve"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        watcher = None
        try:
            assert line(service)["event"] == "ready"
            assert stat.S_IMODE(os.stat(endpoint).st_mode) & 0o077 == 0
            assert json.loads(run("list").stdout)["downloads"] == []
            if restart:
                continue  # SIGTERM left a stale endpoint/lock; startup recovered it.
            run("serve", success=False)
            watcher = subprocess.Popen(base + ["watch"], stdout=subprocess.PIPE,
                                       stderr=subprocess.PIPE)
            assert line(watcher)["event"] == "snapshot"
            item = {"source": "zen:default", "id": "123", "filename": "arquivo.zip",
                    "state": "downloading", "receivedBytes": 62, "totalBytes": 100}
            assert json.loads(run("update", json.dumps(item)).stdout)["download"]["progress"] == 0.62
            assert line(watcher)["download"]["filename"] == "arquivo.zip"
            assert len(json.loads(run("list").stdout)["downloads"]) == 1
            item["receivedBytes"] = -1
            assert not json.loads(run("update", json.dumps(item), success=False).stdout)["ok"]
            run("update", "not-json", success=False)
            run("unknown", success=False)
            run("remove", "zen:default", "123")
            assert line(watcher)["event"] == "removed"
            assert json.loads(run("list").stdout)["downloads"] == []
            service.terminate()
            service.wait(timeout=5)
            assert watcher.wait(timeout=5) != 0
        finally:
            for process in (watcher, service):
                if process is not None:
                    if process.poll() is None:
                        process.terminate()
                    process.communicate(timeout=5)
    run("list", success=False)

print("CLI integration passed: progress, watch, validation, isolation and crash recovery")
