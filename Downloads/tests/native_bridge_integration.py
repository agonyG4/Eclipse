"""Native Messaging framing and forwarding against the real local API."""

import json
import os
from pathlib import Path
import select
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import unittest

host, service = map(lambda value: str(Path(value).resolve()), sys.argv[1:3])
sys.argv = sys.argv[:1]


def frame(message):
    data = json.dumps(message).encode()
    return struct.pack("=I", len(data)) + data


def receive(process):
    assert select.select([process.stdout], [], [], 5)[0], "Native response timed out"
    length = process.stdout.read(4)
    assert len(length) == 4, process.stderr.read().decode()
    return json.loads(process.stdout.read(struct.unpack("=I", length)[0]))


class NativeBridgeTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="nm-", dir=Path(service).parent)
        self.root = Path(self.directory.name)
        self.endpoint = str(self.root / "api.sock")
        self.pidfile = self.root / "api.pid"
        wrapper = self.root / "service-wrapper"
        wrapper.write_text("#!" + sys.executable + "\nimport os,sys\nfrom pathlib import Path\n"
                           + f"Path({str(self.pidfile)!r}).write_text(str(os.getpid()))\n"
                           + f"os.execv({service!r}, [{service!r}] + sys.argv[1:])\n")
        wrapper.chmod(0o700)
        self.env = dict(os.environ, ASTREA_DOWNLOADS_SOCKET=self.endpoint,
                        ASTREA_DOWNLOADS_SERVICE=str(wrapper), PYTHONDONTWRITEBYTECODE="1")
        self.process = subprocess.Popen([sys.executable, host], env=self.env,
                                        stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE)

    def tearDown(self):
        if self.process.poll() is None:
            self.process.terminate()
        self.process.communicate(timeout=5)
        if self.pidfile.exists():
            try:
                os.kill(int(self.pidfile.read_text()), signal.SIGTERM)
            except ProcessLookupError:
                pass
        self.directory.cleanup()

    def request(self, value):
        self.process.stdin.write(frame(value))
        self.process.stdin.flush()
        return receive(self.process)

    def test_autostart_progress_and_reconnect(self):
        first = self.request({"version": 1, "op": "ping"})
        self.assertTrue(first["ok"])
        self.assertTrue(first["reconnected"])
        update = {"version": 1, "op": "update", "download": {
            "source": "zen:test-profile", "id": "1", "filename": "arquivo.zip",
            "state": "downloading", "receivedBytes": 62, "totalBytes": 100}}
        result = self.request(update)
        self.assertEqual(result["download"]["progress"], 0.62)
        listed = subprocess.run([service, "--socket", self.endpoint, "list"],
                                capture_output=True, text=True, check=True, timeout=5)
        self.assertEqual(len(json.loads(listed.stdout)["downloads"]), 1)
        os.kill(int(self.pidfile.read_text()), signal.SIGKILL)
        result = self.request(update)
        self.assertTrue(result["ok"])
        self.assertTrue(result["reconnected"])
        self.assertEqual(result["download"]["progress"], 0.62)

    def test_fragmented_frames_and_rejected_operations(self):
        data = frame({"version": 1, "op": "ping"})
        self.process.stdin.write(data[:2])
        self.process.stdin.flush()
        self.process.stdin.write(data[2:7])
        self.process.stdin.flush()
        self.process.stdin.write(data[7:])
        self.process.stdin.flush()
        self.assertTrue(receive(self.process)["ok"])
        self.assertFalse(self.request({"version": 1, "op": "watch"})["ok"])
        self.assertFalse(self.request({"version": 2, "op": "ping"})["ok"])
        self.assertFalse(self.request({"version": 1, "op": "update", "download": {}})["ok"])
        self.assertTrue(self.request({"version": 1, "op": "ping"})["ok"])

    def test_running_first_release_remains_compatible(self):
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as server:
            server.bind(self.endpoint)
            server.listen(1)
            def legacy_reply():
                connection, _ = server.accept()
                with connection, connection.makefile("rb") as stream:
                    self.assertEqual(json.loads(stream.readline())["op"], "ping")
                    connection.sendall(b'{"version":1,"ok":false,"error":"unknown_operation"}\n')
            worker = threading.Thread(target=legacy_reply, daemon=True)
            worker.start()
            self.assertTrue(self.request({"version": 1, "op": "ping"})["ok"])
            worker.join(timeout=3)
            self.assertFalse(worker.is_alive())

    def test_oversized_frames(self):
        self.assertTrue(self.request({"version": 1, "op": "ping"})["ok"])
        self.process.stdin.write(struct.pack("=I", 65537))
        self.process.stdin.flush()
        self.process.stdin.close()
        self.process.stdin = None
        self.assertNotEqual(self.process.wait(timeout=5), 0)

    def test_truncated_frames(self):
        self.assertTrue(self.request({"version": 1, "op": "ping"})["ok"])
        self.process.stdin.write(struct.pack("=I", 20) + b"{}")
        self.process.stdin.flush()
        self.process.stdin.close()
        self.process.stdin = None
        self.assertNotEqual(self.process.wait(timeout=5), 0)


if __name__ == "__main__":
    unittest.main()
