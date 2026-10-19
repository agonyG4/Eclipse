#!/usr/bin/env python3
"""Run the extension-free Zen download prototype in an isolated Aether runtime."""
import argparse
from contextlib import contextmanager
import fcntl
import json
import os
from pathlib import Path
import shutil
import signal
import socket
import subprocess
import sys
import time
import uuid

AETHER = Path("/mnt/Aether/Desktop/GitHub")
DEFAULT_BUILD = AETHER / "Eclipse-build/Downloads"
ASSETS = Path(__file__).resolve().parents[1] / "zen"
MARKER = ".astrea-zen-prototype.json"
OWNER = {"owner":"astrea-zen-downloads-prototype", "version":1}


def local_path(root, relative):
    path = root / relative
    if not path.resolve().is_relative_to(root.resolve()):
        raise ValueError("Prototype path escapes its output directory: " + str(path))
    for part in [path, *path.parents]:
        if part == root.parent:
            break
        if part.is_symlink():
            raise ValueError("Refusing a symlink in prototype output: " + str(part))
    return path


def output_root(build, create=False):
    build = build.expanduser().absolute()
    if not build.resolve().is_relative_to(AETHER) or build.resolve() == AETHER:
        raise ValueError("Build output must be under /mnt/Aether/Desktop/GitHub")
    root = build / "zen-prototype"
    if root.is_symlink():
        raise ValueError("Prototype output must not be a symlink")
    marker = local_path(root, MARKER)
    if root.exists():
        if not marker.is_file() or json.loads(marker.read_text()) != OWNER:
            raise ValueError("Refusing a foreign prototype output directory")
    elif create:
        root.mkdir(parents=True, mode=0o700)
        marker.write_text(json.dumps(OWNER) + "\n")
    else:
        raise ValueError("Prototype is not prepared; run prepare or run first")
    return root


@contextmanager
def locked(root):
    path = local_path(root, ".launcher.lock")
    with path.open("a") as handle:
        try:
            fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise ValueError("Another prototype launcher/preparer is running") from error
        yield


def browser_in_use(profile):
    # Also detect an orphan browser after a launcher was killed unexpectedly.
    for entry in Path("/proc").iterdir():
        if not entry.name.isdigit():
            continue
        try:
            args = (entry / "cmdline").read_bytes().split(b"\0")
            if b"--profile" in args and os.fsencode(profile) in args:
                return True
        except OSError:
            pass
    return False


def prepare(root, zen_dir):
    if browser_in_use(root / "profile"):
        raise ValueError("Close the isolated prototype browser before preparing it")
    source = zen_dir.expanduser().resolve()
    if source.is_relative_to(root.resolve()) or root.resolve().is_relative_to(source):
        raise ValueError("Zen source and prototype output must not overlap")
    for name in ["zen-bin", "application.ini", "libxul.so", "omni.ja"]:
        if not (source / name).is_file():
            raise ValueError("Missing installed Zen runtime file: " + str(source / name))
    runtime = local_path(root, "runtime")
    stamp = local_path(root, "runtime-source.json")
    fingerprint = {"source":str(source), "files":{
        name:[(source / name).stat().st_size, (source / name).stat().st_mtime_ns]
        for name in ["application.ini", "platform.ini", "libxul.so", "omni.ja", "zen-bin"]}}
    current = json.loads(stamp.read_text()) if stamp.exists() else None
    if current != fingerprint or not (runtime / "zen-bin").is_file():
        if runtime.exists():
            shutil.rmtree(runtime)
        # Dereference installed runtime links so subsequent writes stay isolated.
        shutil.copytree(source, runtime, symlinks=False, ignore_dangling_symlinks=True)
        stamp.write_text(json.dumps(fingerprint) + "\n")
    module_dir = local_path(root, "runtime/astrea-downloads")
    module_dir.mkdir(exist_ok=True)
    for name in ["ZenDownloadPublisher.sys.mjs"]:
        shutil.copyfile(ASSETS / name, local_path(root, "runtime/astrea-downloads/" + name))
    shutil.copyfile(ASSETS / "astrea-downloads.cfg", local_path(root, "runtime/astrea-downloads.cfg"))
    pref_dir = local_path(root, "runtime/defaults/pref")
    pref_dir.mkdir(parents=True, exist_ok=True)
    local_path(root, "runtime/defaults/pref/astrea-downloads.js").write_text(
        'pref("general.config.filename", "astrea-downloads.cfg");\n'
        'pref("general.config.obscure_value", 0);\n'
        'pref("general.config.sandbox_enabled", false);\n')
    for name in ["profile", "downloads", "cache", "config", "data", "tmp"]:
        local_path(root, name).mkdir(exist_ok=True)
    prefs = {"browser.download.useDownloadDir":True,
             "browser.download.dir":str(root / "downloads"), "browser.download.folderList":2,
             "browser.download.alwaysOpenPanel":False, "browser.shell.checkDefaultBrowser":False,
             "browser.startup.homepage":"about:blank", "zen.welcome-screen.seen":True,
             "datareporting.policy.dataSubmissionEnabled":False}
    local_path(root, "profile/user.js").write_text("".join(
        f"user_pref({json.dumps(k)}, {json.dumps(v)});\n" for k,v in prefs.items()))
    return runtime / "zen-bin"


def endpoint(root):
    path = local_path(root, "api.sock")
    if len(os.fsencode(path)) > 100:
        raise ValueError("Prototype Unix socket path is too long")
    return str(path)


def api_request(path, message):
    with socket.socket(socket.AF_UNIX) as client:
        client.settimeout(1)
        client.connect(path)
        client.sendall(json.dumps(message).encode() + b"\n")
        with client.makefile("rb") as stream:
            line = stream.readline(65537)
        if len(line) > 65536 or not line.endswith(b"\n"):
            raise ValueError("Invalid prototype API reply")
        reply = json.loads(line)
        if reply.get("version") != 1 or not reply.get("ok"):
            raise ValueError("Prototype API rejected the request")
        return reply


def stop(process):
    if process is None or process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()


def run(root, binary, service, urls):
    if not os.environ.get("WAYLAND_DISPLAY"):
        raise ValueError("A native Wayland session is required")
    if not service.is_file() or not os.access(service, os.X_OK):
        raise ValueError("Build astrea-downloads first: " + str(service))
    path = endpoint(root)
    env = dict(os.environ, MOZ_ENABLE_WAYLAND="1", GDK_BACKEND="wayland",
               ASTREA_DOWNLOADS_SOCKET=path, ASTREA_DOWNLOADS_SOURCE="zen-prototype:" + str(uuid.uuid4()),
               XDG_CONFIG_HOME=str(root / "config"), XDG_CACHE_HOME=str(root / "cache"),
               XDG_DATA_HOME=str(root / "data"), TMPDIR=str(root / "tmp"))
    env.pop("DISPLAY",None)
    env.pop("MOZ_MARIONETTE",None)
    api = browser = None
    with local_path(root, "api.log").open("a") as api_log, local_path(root, "browser.log").open("a") as browser_log:
        def ensure_api():
            nonlocal api
            try:
                api_request(path,{"version":1,"op":"ping"})
                return
            except (OSError, ValueError):
                pass
            if api is not None and api.poll() is None:
                raise ValueError("Owned prototype API stopped responding")
            api = subprocess.Popen([str(service),"--socket",path,"serve"], env=env,
                                   stdin=subprocess.DEVNULL,stdout=api_log,stderr=api_log,
                                   start_new_session=True)
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                try:
                    api_request(path,{"version":1,"op":"ping"})
                    return
                except (OSError, ValueError):
                    if api.poll() is not None:
                        break
                    time.sleep(.05)
            raise ValueError("Cannot start prototype API; see " + str(root / "api.log"))
        try:
            ensure_api()
            browser = subprocess.Popen([str(binary),"--no-remote","--profile",str(root / "profile"),
                                        *(urls or ["about:blank"])],env=env,
                                       stdout=browser_log,stderr=browser_log,start_new_session=True)
            print("Zen do protótipo aberto em Wayland nativo.",flush=True)
            print("Downloads: " + str(root / "downloads"),flush=True)
            print("Em outro terminal: python3 " + str(Path(__file__).resolve()) + " watch",flush=True)
            while browser.poll() is None:
                if api is not None and api.poll() is not None:
                    ensure_api()
                time.sleep(.5)
            return browser.returncode
        finally:
            stop(browser)
            if api is None:
                # A reused API belongs to somebody else. Remove only this session.
                try:
                    for record in api_request(path,{"version":1,"op":"list"})["downloads"]:
                        if record["source"] == env["ASTREA_DOWNLOADS_SOURCE"]:
                            api_request(path,{"version":1,"op":"remove","source":record["source"],"id":record["id"]})
                except (OSError, ValueError):
                    pass
            stop(api)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir",type=Path,default=DEFAULT_BUILD,
                        help="Downloads output directory under Aether")
    parser.add_argument("--zen-dir",type=Path,default=Path("/opt/zen-browser-bin"))
    parser.add_argument("command",choices=["prepare","run","list","watch"])
    parser.add_argument("urls",nargs="*")
    args = parser.parse_args()
    def interrupted(_signal, _frame):
        raise KeyboardInterrupt
    signal.signal(signal.SIGTERM, interrupted)
    signal.signal(signal.SIGHUP, interrupted)
    try:
        root = output_root(args.build_dir,create=args.command in ("prepare","run"))
        service = args.build_dir.expanduser().absolute() / "astrea-downloads"
        if args.command in ("list","watch"):
            return subprocess.call([str(service),"--socket",endpoint(root),args.command])
        with locked(root):
            binary = prepare(root,args.zen_dir)
            if args.command == "prepare":
                print("Protótipo preparado em " + str(root))
                return 0
            return run(root,binary,service,args.urls)
    except KeyboardInterrupt:
        return 130
    except (OSError, ValueError) as error:
        print("Astrea Zen Downloads: " + str(error),file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
