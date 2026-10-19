#!/usr/bin/env python3
"""Native Messaging to Astrea's local API. stdout is protocol data only."""

import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import time

MAX_FRAME = 64 * 1024


def failure(code, message):
    return {"version": 1, "ok": False, "error": code, "message": message}


def read_exact(stream, length, allow_eof=False):
    result = bytearray()
    while len(result) < length:
        block = stream.read(length - len(result))
        if not block:
            if allow_eof and not result:
                return None
            raise ValueError("Truncated native message")
        result.extend(block)
    return bytes(result)


def reject_constant(value):
    raise ValueError("Non-finite JSON number")


class ApiClient:
    def __init__(self):
        explicit = os.environ.get("ASTREA_DOWNLOADS_SOCKET")
        runtime = os.environ.get("XDG_RUNTIME_DIR")
        if not explicit and not runtime:
            raise ValueError("XDG_RUNTIME_DIR is missing; cannot locate the download API")
        self.path = explicit or str(Path(runtime) / "astrea-downloads.sock")
        if not Path(self.path).is_absolute():
            raise ValueError("Download socket path must be absolute")
        self.service = os.environ.get("ASTREA_DOWNLOADS_SERVICE") or str(
            Path(__file__).resolve().with_name("astrea-downloads"))
        self.connection = None
        self.child = None

    def close(self):
        if self.connection is not None:
            self.connection.close()
            self.connection = None

    def connect_once(self):
        connection = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        connection.settimeout(3)
        try:
            connection.connect(self.path)
        except OSError:
            connection.close()
            raise
        self.connection = connection

    def ensure_connected(self):
        if self.connection is not None:
            return False
        try:
            self.connect_once()
            return True
        except OSError:
            pass
        if self.child is None or self.child.poll() is not None:
            if not Path(self.service).is_file() or not os.access(self.service, os.X_OK):
                raise ValueError("Download API executable is missing or not executable")
            self.child = subprocess.Popen(
                [self.service, "--socket", self.path, "serve"], start_new_session=True,
                stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            try:
                self.connect_once()
                return True
            except OSError:
                time.sleep(0.05)
        raise OSError("Could not start or connect to the local download API")

    def request(self, request):
        if not isinstance(request, dict) or type(request.get("version")) is not int \
                or request["version"] != 1:
            return failure("unsupported_version", "Expected protocol version 1")
        if request.get("op") not in ("ping", "update", "remove"):
            return failure("unsupported_operation", "Native host accepts ping, update and remove")
        for attempt in range(2):
            try:
                reconnected = self.ensure_connected()
                payload = json.dumps(request, ensure_ascii=False, separators=(",", ":")).encode()
                if len(payload) > MAX_FRAME:
                    return failure("oversized_message", "Download request exceeds 64 KiB")
                self.connection.sendall(payload + b"\n")
                response = bytearray()
                while b"\n" not in response:
                    chunk = self.connection.recv(min(4096, MAX_FRAME + 1 - len(response)))
                    if not chunk:
                        raise OSError("Download API disconnected before replying")
                    response.extend(chunk)
                    if len(response) > MAX_FRAME:
                        raise ValueError("Download API reply exceeds 64 KiB")
                result = json.loads(response, parse_constant=reject_constant)
                if not isinstance(result, dict):
                    raise ValueError("Download API returned an invalid reply")
                # The first API release predates ping; its explicit error still
                # proves the service is alive and supports the existing mutations.
                if request["op"] == "ping" and result.get("error") == "unknown_operation":
                    result = {"version": 1, "ok": True}
                if reconnected:
                    result["reconnected"] = True
                return result
            except (OSError, ValueError):
                self.close()
                if attempt:
                    raise


def main():
    client = None
    try:
        while True:
            header = read_exact(sys.stdin.buffer, 4, allow_eof=True)
            if header is None:
                return 0
            length = struct.unpack("=I", header)[0]
            if length == 0 or length > MAX_FRAME:
                raise ValueError("Native message length must be between 1 and 65536 bytes")
            payload = read_exact(sys.stdin.buffer, length)
            try:
                request = json.loads(payload, parse_constant=reject_constant)
                if client is None:
                    client = ApiClient()
                response = client.request(request)
            except (OSError, ValueError) as exc:
                response = failure("bridge_error", str(exc))
            encoded = json.dumps(response, ensure_ascii=False, separators=(",", ":")).encode()
            sys.stdout.buffer.write(struct.pack("=I", len(encoded)) + encoded)
            sys.stdout.buffer.flush()
    except (OSError, ValueError) as exc:
        print("Astrea Downloads native host:", exc, file=sys.stderr)
        return 1
    finally:
        if client is not None:
            client.close()


if __name__ == "__main__":
    raise SystemExit(main())
