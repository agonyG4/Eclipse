"""Deterministic tests for Eclipse's explicit LayerShellQt CMake contract."""

from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[3]
CONTRACT_MODULE = REPOSITORY_ROOT / "cmake" / "AstreaLayerShell.cmake"


class LayerShellContractTests(unittest.TestCase):
    def configure_fixture(self, *, enabled: bool, package_available: bool) -> subprocess.CompletedProcess[str]:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            source = root / "source"
            build = root / "build"
            prefix = root / "prefix"
            source.mkdir()
            if package_available:
                package = prefix / "lib" / "cmake" / "LayerShellQt"
                package.mkdir(parents=True)
                (package / "LayerShellQtConfig.cmake").write_text(
                    "set(LayerShellQt_FOUND TRUE)\n"
                    "add_library(LayerShellQt::Interface INTERFACE IMPORTED)\n",
                    encoding="utf-8",
                )

            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.24)\n"
                "project(layer-shell-contract NONE)\n"
                f"set(ASTREA_ENABLE_LAYER_SHELL {'ON' if enabled else 'OFF'})\n"
                f"include(\"{CONTRACT_MODULE}\")\n"
                "astrea_configure_layer_shell()\n"
                "if(ASTREA_ENABLE_LAYER_SHELL AND NOT TARGET LayerShellQt::Interface)\n"
                "  message(FATAL_ERROR \"enabled contract did not expose LayerShellQt::Interface\")\n"
                "endif()\n"
                "if(NOT ASTREA_ENABLE_LAYER_SHELL AND TARGET LayerShellQt::Interface)\n"
                "  message(FATAL_ERROR \"disabled contract unexpectedly discovered LayerShellQt\")\n"
                "endif()\n",
                encoding="utf-8",
            )
            command = ["cmake", "-S", str(source), "-B", str(build)]
            if package_available:
                command.append(f"-DCMAKE_PREFIX_PATH={prefix}")
            return subprocess.run(command, check=False, capture_output=True, text=True)

    def test_enabled_contract_succeeds_with_interface_package(self) -> None:
        result = self.configure_fixture(enabled=True, package_available=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_enabled_contract_fails_without_package(self) -> None:
        result = self.configure_fixture(enabled=True, package_available=False)
        self.assertNotEqual(result.returncode, 0)
        output = result.stdout + result.stderr
        self.assertIn("LayerShellQt is required by astrea-shell", output)
        self.assertIn("ASTREA_ENABLE_LAYER_SHELL=OFF", output)

    def test_explicit_disabled_contract_succeeds_without_package(self) -> None:
        result = self.configure_fixture(enabled=False, package_available=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_source_has_no_normal_window_fallback(self) -> None:
        sources = (
            REPOSITORY_ROOT / "Shell" / "app" / "AstreaShellApplication.cpp",
            REPOSITORY_ROOT / "Dock" / "platform" / "wayland" / "DockLayerShellSurface.cpp",
            REPOSITORY_ROOT / "shared" / "platform" / "wayland" / "LayerShellHelper.cpp",
        )
        for source in sources:
            self.assertNotIn("using a normal window", source.read_text(encoding="utf-8"))

    def test_startup_uses_shared_preparation_seam(self) -> None:
        helper = (REPOSITORY_ROOT / "shared" / "platform" / "wayland" / "LayerShellHelper.cpp").read_text(
            encoding="utf-8"
        )
        bootstrap = (REPOSITORY_ROOT / "Shell" / "app" / "main.cpp").read_text(encoding="utf-8")
        self.assertIn("LayerShellQt::Shell::useLayerShell", helper)
        self.assertIn("AstreaLayerShellHelper::prepare", bootstrap)


if __name__ == "__main__":
    unittest.main()
