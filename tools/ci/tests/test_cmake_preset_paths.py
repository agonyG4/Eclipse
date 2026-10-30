import importlib.util
import json
import os
import subprocess
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[3]
MODULE_PATH = PROJECT_ROOT / "tools/ci/preset-binary-dir.py"
SPEC = importlib.util.spec_from_file_location("preset_binary_dir", MODULE_PATH)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class CMakePresetPathTest(unittest.TestCase):
    def test_base_preset_maps_cargo_home_from_shared_gate_environment(self):
        presets = json.loads((PROJECT_ROOT / "CMakePresets.json").read_text())
        base = next(
            preset for preset in presets["configurePresets"] if preset["name"] == "base"
        )
        self.assertEqual(
            base["environment"]["CARGO_HOME"], "$penv{ASTREA_CARGO_HOME}"
        )

    def test_debug_uses_stable_aether_default(self):
        os.environ.pop("ASTREA_CMAKE_BUILD_ROOT", None)
        self.assertEqual(
            MODULE.resolve_binary_dir(PROJECT_ROOT, "debug"),
            Path("/mnt/Aether/Desktop/GitHub/Eclipse-build"),
        )

    def test_host_override_keeps_preset_suffix(self):
        previous = os.environ.get("ASTREA_CMAKE_BUILD_ROOT")
        try:
            os.environ["ASTREA_CMAKE_BUILD_ROOT"] = "/runner/temp"
            self.assertEqual(
                MODULE.resolve_binary_dir(PROJECT_ROOT, "no-layer-shell"),
                Path("/runner/temp/Eclipse-no-layer-shell-build"),
            )
        finally:
            if previous is None:
                os.environ.pop("ASTREA_CMAKE_BUILD_ROOT", None)
            else:
                os.environ["ASTREA_CMAKE_BUILD_ROOT"] = previous

    def test_shared_gate_environment_defaults_to_aether(self):
        environment = os.environ.copy()
        for name in (
            "ASTREA_CMAKE_BUILD_ROOT",
            "ASTREA_CARGO_TARGET_DIR",
            "ASTREA_CARGO_HOME",
            "ASTREA_TMPDIR",
            "ASTREA_XDG_CACHE_HOME",
        ):
            environment.pop(name, None)
        result = subprocess.run(
            ["bash", "-c", 'source "$1"; printf "%s\\n%s\\n%s\\n%s\\n" "$CARGO_TARGET_DIR" "$CARGO_HOME" "$TMPDIR" "$XDG_CACHE_HOME"', "test", str(PROJECT_ROOT / "tools/ci/set-build-output-env.sh")],
            check=True,
            capture_output=True,
            text=True,
            env=environment,
        )
        self.assertEqual(
            result.stdout.splitlines(),
            [
                "/mnt/Aether/Desktop/GitHub/Eclipse-target",
                "/mnt/Aether/Desktop/GitHub/cargo-home",
                "/mnt/Aether/Desktop/GitHub/tmp",
                "/mnt/Aether/Desktop/GitHub/cache",
            ],
        )

    def test_shared_gate_environment_rejects_non_root_cargo_target(self):
        environment = os.environ.copy()
        environment["ASTREA_CARGO_TARGET_DIR"] = "/home/agony/GitHub/Eclipse/target"
        result = subprocess.run(
            ["bash", "-c", 'source "$1"', "test", str(PROJECT_ROOT / "tools/ci/set-build-output-env.sh")],
            capture_output=True,
            text=True,
            env=environment,
        )
        self.assertEqual(result.returncode, 2)
        self.assertIn("outside ASTREA_CMAKE_BUILD_ROOT", result.stderr)

    def test_shared_gate_environment_rejects_non_aether_local_root(self):
        environment = os.environ.copy()
        environment["ASTREA_CMAKE_BUILD_ROOT"] = "/tmp/eclipse-build"
        environment.pop("RUNNER_TEMP", None)
        result = subprocess.run(
            [
                "bash",
                "-c",
                'source "$1"',
                "test",
                str(PROJECT_ROOT / "tools/ci/set-build-output-env.sh"),
            ],
            capture_output=True,
            text=True,
            env=environment,
        )
        self.assertEqual(result.returncode, 2)
        self.assertIn("must use Aether locally", result.stderr)


if __name__ == "__main__":
    unittest.main()
