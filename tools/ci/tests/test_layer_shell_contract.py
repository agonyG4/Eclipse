"""Deterministic tests for Eclipse's explicit LayerShellQt CMake contract."""

from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[3]
CONTRACT_MODULE = REPOSITORY_ROOT / "cmake" / "AstreaLayerShell.cmake"


class LayerShellContractTests(unittest.TestCase):
    def configure_fixture(
        self,
        *,
        enabled: bool,
        package_available: bool,
        package_version: str = "6.4.5",
        expected_activate_on_show: bool | None = None,
    ) -> subprocess.CompletedProcess[str]:
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
                (package / "LayerShellQtConfigVersion.cmake").write_text(
                    f"set(PACKAGE_VERSION \"{package_version}\")\n"
                    "if(PACKAGE_FIND_VERSION VERSION_GREATER PACKAGE_VERSION)\n"
                    "  set(PACKAGE_VERSION_COMPATIBLE FALSE)\n"
                    "else()\n"
                    "  set(PACKAGE_VERSION_COMPATIBLE TRUE)\n"
                    "endif()\n"
                    "if(PACKAGE_FIND_VERSION VERSION_EQUAL PACKAGE_VERSION)\n"
                    "  set(PACKAGE_VERSION_EXACT TRUE)\n"
                    "endif()\n",
                    encoding="utf-8",
                )

            capability_assertion = ""
            if expected_activate_on_show is not None:
                expected = "ON" if expected_activate_on_show else "OFF"
                capability_assertion = (
                    "if(NOT DEFINED ASTREA_LAYER_SHELL_HAS_ACTIVATE_ON_SHOW)\n"
                    "  message(FATAL_ERROR \"activation capability was not exported\")\n"
                    "endif()\n"
                    "if(ASTREA_LAYER_SHELL_HAS_ACTIVATE_ON_SHOW)\n"
                    "  set(actual_activate_on_show ON)\n"
                    "else()\n"
                    "  set(actual_activate_on_show OFF)\n"
                    "endif()\n"
                    f'if(NOT actual_activate_on_show STREQUAL "{expected}")\n'
                    f'  message(FATAL_ERROR "expected activate-on-show capability {expected}")\n'
                    "endif()\n"
                )

            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.24)\n"
                "project(layer-shell-contract NONE)\n"
                f"set(ASTREA_ENABLE_LAYER_SHELL {'ON' if enabled else 'OFF'})\n"
                f"include(\"{CONTRACT_MODULE}\")\n"
                "astrea_configure_layer_shell()\n"
                + capability_assertion
                + "if(ASTREA_ENABLE_LAYER_SHELL AND NOT TARGET LayerShellQt::Interface)\n"
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
        result = self.configure_fixture(
            enabled=True,
            package_available=True,
            expected_activate_on_show=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_activate_on_show_capability_starts_at_layer_shell_qt_6_4_90(self) -> None:
        development_release = self.configure_fixture(
            enabled=True,
            package_available=True,
            package_version="6.4.90",
            expected_activate_on_show=True,
        )
        self.assertEqual(
            development_release.returncode,
            0,
            development_release.stdout + development_release.stderr,
        )

        stable_release = self.configure_fixture(
            enabled=True,
            package_available=True,
            package_version="6.5.0",
            expected_activate_on_show=True,
        )
        self.assertEqual(
            stable_release.returncode,
            0,
            stable_release.stdout + stable_release.stderr,
        )

    def test_enabled_contract_fails_without_package(self) -> None:
        result = self.configure_fixture(enabled=True, package_available=False)
        self.assertNotEqual(result.returncode, 0)
        output = result.stdout + result.stderr
        self.assertIn("LayerShellQt >= 6.4.5 is required", output)
        self.assertIn("ASTREA_ENABLE_LAYER_SHELL=OFF", output)

    def test_enabled_contract_fails_with_too_old_package(self) -> None:
        result = self.configure_fixture(
            enabled=True, package_available=True, package_version="6.3.0"
        )
        self.assertNotEqual(result.returncode, 0)
        output = result.stdout + result.stderr
        self.assertIn("LayerShellQt >= 6.4.5 is required", output)
        self.assertIn("6.3.0", output)

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

    def test_startup_uses_protocol_preflight(self) -> None:
        helper = (REPOSITORY_ROOT / "shared" / "platform" / "wayland" / "LayerShellHelper.cpp").read_text(
            encoding="utf-8"
        )
        bootstrap = (REPOSITORY_ROOT / "Shell" / "app" / "main.cpp").read_text(encoding="utf-8")
        application = (REPOSITORY_ROOT / "Shell" / "app" / "AstreaShellApplication.cpp").read_text(
            encoding="utf-8"
        )
        self.assertNotIn("LayerShellQt::Shell::useLayerShell", helper)
        self.assertNotIn("AstreaLayerShellHelper::prepare", bootstrap)
        self.assertIn("AstreaLayerShellHelper::protocolAdvertised", application)
        self.assertIn("protocolAdvertised", application)
        self.assertIn("dockConfigurationRequested", application)
        self.assertNotIn("dockConfigured", application)

    def test_helper_uses_layer_shell_qt_6_4_5_compatible_screen_api(self) -> None:
        helper = (REPOSITORY_ROOT / "shared" / "platform" / "wayland" / "LayerShellHelper.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("window->setScreen(config.screen)", helper)
        self.assertNotIn("layerWindow->setScreen", helper)
        self.assertIn("ASTREA_LAYER_SHELL_HAS_ACTIVATE_ON_SHOW", helper)
        self.assertIn("layerWindow->setActivateOnShow(false)", helper)

    def test_surface_policies_remain_layer_shell_specific(self) -> None:
        dock = (REPOSITORY_ROOT / "Dock" / "platform" / "wayland" / "DockLayerShellSurface.cpp").read_text(
            encoding="utf-8"
        )
        alt_tab = (REPOSITORY_ROOT / "AltTab" / "platform" / "wayland" / "LayerShellSurface.cpp").read_text(
            encoding="utf-8"
        )
        spotlight = (REPOSITORY_ROOT / "Spotlight" / "platform" / "wayland" / "LayerShellSurface.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn('QStringLiteral("astrea-dock")', dock)
        self.assertIn("AstreaLayerShellConfig::Layer::Top", dock)
        self.assertIn("AstreaLayerShellConfig::KeyboardInteractivity::None", dock)
        self.assertIn("layerConfig.anchorBottom = true", dock)
        self.assertIn("layerWindow->setExclusiveZone(mapped ? qMax(0, window->height()) : 0)", dock)
        self.assertIn('QStringLiteral("astrea-alt-tab")', alt_tab)
        self.assertIn("AstreaLayerShellConfig::Layer::Overlay", alt_tab)
        self.assertIn("AstreaLayerShellConfig::KeyboardInteractivity::Exclusive", alt_tab)
        self.assertIn('QStringLiteral("astrea-spotlight")', spotlight)
        self.assertIn("AstreaLayerShellConfig::Layer::Overlay", spotlight)
        self.assertIn("AstreaLayerShellConfig::KeyboardInteractivity::Exclusive", spotlight)

    def test_wallpaper_policy_requests_full_output_background(self) -> None:
        policy = (
            REPOSITORY_ROOT / "Paper" / "platform" / "wayland" / "WallpaperSurfacePolicy.cpp"
        ).read_text(encoding="utf-8")
        bundle = (
            REPOSITORY_ROOT / "Paper" / "platform" / "wayland" / "WallpaperSurfaceBundle.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn('QStringLiteral("astrea-paper-wallpaper")', policy)
        self.assertIn("AstreaLayerShellConfig::Layer::Background", policy)
        self.assertIn("AstreaLayerShellConfig::KeyboardInteractivity::None", policy)
        self.assertIn("config.anchorTop = true", policy)
        self.assertIn("config.anchorBottom = true", policy)
        self.assertIn("config.anchorLeft = true", policy)
        self.assertIn("config.anchorRight = true", policy)
        self.assertIn("config.exclusiveZone = -1", policy)
        self.assertIn("config.margins = QMargins()", policy)
        self.assertIn("WallpaperSurfacePolicy::background", bundle)
        self.assertNotIn("config.exclusiveZone = 0", bundle)

    def test_dock_physical_width_is_not_animated(self) -> None:
        dock_panel = (REPOSITORY_ROOT / "Dock" / "qml" / "components" / "DockPanel.qml").read_text(
            encoding="utf-8"
        )
        self.assertNotIn("Behavior on width", dock_panel)
        self.assertNotIn("NumberAnimation { duration: 135", dock_panel)


if __name__ == "__main__":
    unittest.main()
