#!/usr/bin/env python3
"""Repository-specific guard for Spotlight Phase 1, Phase 2, and Phase 3A boundaries.

This checks only the small set of add_library/qt_add_executable and
target_link_libraries declarations used by Spotlight and Shell. It is not a
general CMake parser.

Phase 1 protects production source ownership and consumer links. Phase 2
protects the backend and resolved-path dependency directions. Phase 3A protects
the Spotlight-local launcher/catalog ports and Shell-owned adapters. Weather
orchestration through QProcess remains explicit Phase 3B debt, so this guard
does not claim that Spotlight is fully internally cleaned up.

SpotlightIpcServer is an intentional legacy/test-only exception. The
compatibility executable uses ShellIpcClient and the resident ShellIpcServer;
this old server remains compiled only into spotlight-tests for its existing
parser/transport tests.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SPOTLIGHT_CMAKE = ROOT / "Spotlight" / "CMakeLists.txt"
SHELL_CMAKE = ROOT / "Shell" / "CMakeLists.txt"

RUNTIME_CPP = {
    "core/SpotlightController.cpp",
    "core/SpotlightResultsModel.cpp",
    "services/AstreaI18n.cpp",
    "services/GameModeMonitor.cpp",
    "services/SpotlightConfigWatcher.cpp",
    "services/SpotlightApplicationLauncherAdapter.cpp",
    "services/SpotlightDesktopCatalogAdapter.cpp",
    "platform/runtime/SpotlightRuntimePaths.cpp",
    "platform/rust/RustSpotlightBackend.cpp",
}
RUNTIME_HEADERS = {
    "core/SpotlightBackend.hpp",
    "core/SpotlightLauncher.hpp",
    "core/SpotlightCatalogSource.hpp",
    "core/SpotlightControllerPaths.hpp",
    "core/SpotlightController.hpp",
    "core/SpotlightResultsModel.hpp",
    "services/AstreaI18n.hpp",
    "services/GameModeMonitor.hpp",
    "services/SpotlightConfigWatcher.hpp",
    "services/SpotlightApplicationLauncherAdapter.hpp",
    "services/SpotlightDesktopCatalogAdapter.hpp",
    "platform/runtime/SpotlightRuntimePaths.hpp",
    "platform/rust/RustSpotlightBackend.hpp",
}
WAYLAND_CPP = {"platform/wayland/LayerShellSurface.cpp"}
WAYLAND_HEADERS = {"platform/wayland/LayerShellSurface.hpp"}
PHASE_3_DEBT = ("QProcess",)


def without_comments(source: str) -> str:
    return re.sub(r"#[^\n]*", "", source)


def selected_cmake_calls(source: str, command: str, target: str) -> list[str]:
    """Read simple declarations for one known command and target.

    These repository declarations contain no nested calls, so each target's
    arguments end at the first closing parenthesis. All target_link_libraries
    declarations are collected, including repeated declarations.
    """
    clean = without_comments(source)
    pattern = re.compile(
        rf"(?is)\b{re.escape(command)}\s*\(\s*{re.escape(target)}\b(.*?)\)"
    )
    return [match.group(1) for match in pattern.finditer(clean)]


def source_entries(source: str, command: str, target: str) -> list[str]:
    bodies = selected_cmake_calls(source, command, target)
    return [token.strip('"') for body in bodies for token in body.split()]


def source_paths(source: str, command: str, target: str, suffix: str) -> set[str]:
    return {
        entry.replace("\\", "/")
        for entry in source_entries(source, command, target)
        if entry.endswith(suffix)
    }


def target_links(source: str, target: str) -> list[tuple[str, set[str]]]:
    declarations = selected_cmake_calls(source, "target_link_libraries", target)
    links: list[tuple[str, set[str]]] = []
    for declaration in declarations:
        tokens = declaration.split()
        visibility = ""
        libraries_by_visibility: dict[str, set[str]] = {}
        for token in tokens:
            upper = token.upper()
            if upper in {"PUBLIC", "PRIVATE", "INTERFACE"}:
                visibility = upper
                libraries_by_visibility.setdefault(visibility, set())
            elif visibility:
                libraries_by_visibility[visibility].add(token)
        links.extend(libraries_by_visibility.items())
    return links


def has_link(source: str, target: str, library: str, visibility: str | None = None) -> bool:
    return any(
        library in libraries and (visibility is None or scope == visibility)
        for scope, libraries in target_links(source, target)
    )


def main() -> int:
    errors: list[str] = []
    spotlight = without_comments(SPOTLIGHT_CMAKE.read_text(encoding="utf-8"))
    shell = without_comments(SHELL_CMAKE.read_text(encoding="utf-8"))

    def require(condition: bool, message: str) -> None:
        if not condition:
            errors.append(message)

    runtime_cpp = source_paths(spotlight, "add_library", "astrea-spotlight-runtime", ".cpp")
    require(
        runtime_cpp == RUNTIME_CPP,
        "astrea-spotlight-runtime must own exactly the expected resident .cpp files; "
        f"missing={sorted(RUNTIME_CPP - runtime_cpp)}, extra={sorted(runtime_cpp - RUNTIME_CPP)}",
    )
    runtime_headers = source_paths(spotlight, "add_library", "astrea-spotlight-runtime", ".hpp")
    require(
        RUNTIME_HEADERS <= runtime_headers,
        "astrea-spotlight-runtime must list its QObject/public headers for AUTOMOC; "
        f"missing={sorted(RUNTIME_HEADERS - runtime_headers)}",
    )
    require(
        re.search(
            r"(?is)add_library\s*\(\s*astrea-spotlight-(?:core|services|launcher|catalog)\b",
            spotlight,
        ) is None,
        "Phase 3A must keep core and service code in astrea-spotlight-runtime",
    )

    wayland_cpp = source_paths(spotlight, "add_library", "astrea-spotlight-wayland", ".cpp")
    require(
        wayland_cpp == WAYLAND_CPP,
        "astrea-spotlight-wayland must own only LayerShellSurface.cpp; "
        f"found={sorted(wayland_cpp)}",
    )
    wayland_headers = source_paths(spotlight, "add_library", "astrea-spotlight-wayland", ".hpp")
    require(
        WAYLAND_HEADERS <= wayland_headers,
        "astrea-spotlight-wayland must list LayerShellSurface.hpp",
    )

    shell_spotlight_sources = re.findall(
        r"(?i)(?:\.\./)?Spotlight/(?:core|services|platform)/[^\s)\"]+\.cpp\b",
        shell,
    )
    require(
        not shell_spotlight_sources,
        "Shell/CMakeLists.txt must not compile Spotlight implementation files; "
        f"found={sorted(set(shell_spotlight_sources))}",
    )
    require(
        has_link(shell, "astrea-shell-runtime", "astrea-spotlight-runtime", "PUBLIC"),
        "astrea-shell-runtime must consume astrea-spotlight-runtime as a PUBLIC dependency",
    )
    require(
        has_link(shell, "astrea-shell", "astrea-spotlight-wayland"),
        "astrea-shell must consume astrea-spotlight-wayland",
    )
    require(
        "astrea_spotlight_backend" not in shell,
        "Shell must not compile or directly link astrea_spotlight_backend",
    )
    for shell_target in ("astrea-shell-runtime", "astrea-shell"):
        shell_target_links = set().union(
            *(libraries for _, libraries in target_links(shell, shell_target))
        ) if target_links(shell, shell_target) else set()
        require(
            not ({"dl", "pthread"} & shell_target_links),
            f"{shell_target} must not supply Spotlight Rust system link dependencies",
        )

    require(
        has_link(spotlight, "astrea-spotlight-runtime", "astrea_spotlight_backend", "PRIVATE"),
        "astrea-spotlight-runtime must privately own astrea_spotlight_backend",
    )
    require(
        not any(
            "astrea_spotlight_backend" in libraries
            for scope, libraries in target_links(spotlight, "astrea-spotlight-runtime")
            if scope in {"PUBLIC", "INTERFACE"}
        ),
        "the Rust C ABI target must not be exposed through astrea-spotlight-runtime",
    )
    rust_wrapper_header = (
        ROOT / "Spotlight" / "platform/rust/RustSpotlightBackend.hpp"
    ).read_text(encoding="utf-8")
    require(
        "astrea_spotlight_backend.h" not in rust_wrapper_header
        and "struct AstreaSpotlightBackend;" in rust_wrapper_header,
        "RustSpotlightBackend.hpp must keep the native C ABI header out of the public API",
    )

    spotlight_root = ROOT / "Spotlight"
    core_dir = spotlight_root / "core"
    core_sources = sorted((*core_dir.rglob("*.hpp"), *core_dir.rglob("*.cpp")))
    forbidden_core_dependencies = re.compile(
        r"platform/(?:rust|runtime)/|RustSpotlightBackend|SpotlightRuntimePaths"
    )
    for path in core_sources:
        source = path.read_text(encoding="utf-8")
        require(
            forbidden_core_dependencies.search(source) is None,
            f"Spotlight core must not depend on platform Spotlight code: {path.relative_to(ROOT)}",
        )
        require(
            not re.search(
                r"ApplicationLauncher|ApplicationLaunchRequest|launch/ApplicationLauncher\.hpp|"
                r"services/ApplicationLauncher\.hpp|DesktopEntryCatalog|apps/DesktopEntryCatalog\.hpp",
                source,
            ),
            f"Spotlight core must depend only on core launcher/catalog ports: {path.relative_to(ROOT)}",
        )

    backend_port_path = core_dir / "SpotlightBackend.hpp"
    launcher_port_path = core_dir / "SpotlightLauncher.hpp"
    catalog_port_path = core_dir / "SpotlightCatalogSource.hpp"
    controller_paths_path = core_dir / "SpotlightControllerPaths.hpp"
    controller_header = (core_dir / "SpotlightController.hpp").read_text(encoding="utf-8")
    controller_cpp = (core_dir / "SpotlightController.cpp").read_text(encoding="utf-8")
    backend_port = (
        backend_port_path.read_text(encoding="utf-8") if backend_port_path.is_file() else ""
    )
    controller_paths = (
        controller_paths_path.read_text(encoding="utf-8")
        if controller_paths_path.is_file()
        else ""
    )
    launcher_port = launcher_port_path.read_text(encoding="utf-8") if launcher_port_path.is_file() else ""
    catalog_port = catalog_port_path.read_text(encoding="utf-8") if catalog_port_path.is_file() else ""
    require(backend_port_path.is_file(), "Spotlight core must own SpotlightBackend.hpp")
    require(launcher_port_path.is_file(), "Spotlight core must own SpotlightLauncher.hpp")
    require(catalog_port_path.is_file(), "Spotlight core must own SpotlightCatalogSource.hpp")
    require(
        "SpotlightBackend &backend" in controller_header
        and "SpotlightBackend &m_backend" in controller_header
        and "SpotlightBackend *" not in controller_header
        and "RustSpotlightBackend" not in controller_header + controller_cpp,
        "SpotlightController must require and borrow the core SpotlightBackend port by reference",
    )
    require(
        all(method in backend_port for method in (
            "create(", "createWithCatalog(", "reload(", "setCatalog(", "search(",
            "recordLaunch(", "ensureConfig(", "watchedDirectories()",
        ))
        and "destroy(" not in backend_port
        and "isValid(" not in backend_port,
        "SpotlightBackend must expose only the controller-required operations",
    )
    require(
        "class RustSpotlightBackend : public SpotlightBackend" in rust_wrapper_header,
        "RustSpotlightBackend must implement the core SpotlightBackend port",
    )
    require(
        controller_paths_path.is_file()
        and all(field in controller_paths for field in (
            "astreaRoot", "weatherExecutable", "weatherAssetDirectory",
        ))
        and "launcherExecutable" not in controller_paths
        and "launcherExecutable" not in controller_header + controller_cpp
        and "SpotlightControllerPaths m_paths" in controller_header,
        "SpotlightController must consume the neutral core controller path values",
    )
    require(
        all(field in launcher_port for field in (
            "desktopId", "desktopFileName", "exec", "appName", "iconName", "desktopFilePath",
        ))
        and "virtual void launch(const SpotlightLaunchRequest &request) = 0" in launcher_port
        and "launchFailed" in launcher_port
        and "launchTimedOut" in launcher_port
        and "launchSucceeded" not in launcher_port
        and "ApplicationLauncher" not in launcher_port
        and "ApplicationLaunchRequest" not in launcher_port,
        "SpotlightLauncher must expose only the six-field request and required failure/timeout events",
    )
    require(
        "virtual QJsonArray snapshotJson() const = 0" in catalog_port
        and "virtual QStringList watchedDirectories() const = 0" in catalog_port
        and "void updated()" in catalog_port
        and "DesktopEntryCatalog" not in catalog_port
        and not re.search(
            r"DesktopEntrySnapshot|DesktopEntryRecord|findByDesktop|revision\(|initialize\(|"
            r"QFileSystemWatcher|QTimer",
            catalog_port,
        ),
        "SpotlightCatalogSource must expose only its snapshot, watched paths, and update event",
    )
    require(
        "SpotlightLauncher &launcher" in controller_header
        and "SpotlightLauncher &m_launcher" in controller_header
        and "SpotlightLauncher *" not in controller_header
        and "SpotlightCatalogSource *catalog = nullptr" in controller_header
        and "SpotlightCatalogSource *m_catalog = nullptr" in controller_header
        and "ApplicationLauncher" not in controller_header + controller_cpp
        and "ApplicationLaunchRequest" not in controller_header + controller_cpp
        and "DesktopEntryCatalog" not in controller_header + controller_cpp
        and "m_ownedLauncher" not in controller_header + controller_cpp
        and "onLaunchSucceeded" not in controller_header + controller_cpp
        and "launchSucceeded" not in controller_header + controller_cpp
        and "catalog() const" not in controller_header
        and "launcher() const" not in controller_header,
        "SpotlightController must require the core launcher port, keep the catalog optional, and avoid concrete getters",
    )

    application_adapter_header = (
        spotlight_root / "services/SpotlightApplicationLauncherAdapter.hpp"
    ).read_text(encoding="utf-8")
    application_adapter_cpp = (
        spotlight_root / "services/SpotlightApplicationLauncherAdapter.cpp"
    ).read_text(encoding="utf-8")
    catalog_adapter_header = (
        spotlight_root / "services/SpotlightDesktopCatalogAdapter.hpp"
    ).read_text(encoding="utf-8")
    catalog_adapter_cpp = (
        spotlight_root / "services/SpotlightDesktopCatalogAdapter.cpp"
    ).read_text(encoding="utf-8")
    require(
        "class SpotlightApplicationLauncherAdapter final : public SpotlightLauncher" in application_adapter_header
        and "ApplicationLauncher &launcher" in application_adapter_header
        and "ApplicationLauncher &m_launcher" in application_adapter_header
        and "launch/ApplicationLauncher.hpp" in application_adapter_cpp
        and "launchDesktop(ApplicationLaunchRequest{" in application_adapter_cpp
        and all(field in application_adapter_cpp for field in (
            "request.desktopId", "request.desktopFileName", "request.exec",
            "request.appName", "request.iconName", "request.desktopFilePath",
        ))
        and "ApplicationLauncher::launchFailed" in application_adapter_cpp
        and "ApplicationLauncher::launchTimedOut" in application_adapter_cpp
        and "ApplicationLauncher::launchSucceeded" not in application_adapter_cpp
        and "std::unique_ptr<ApplicationLauncher>" not in application_adapter_header
        and "make_unique<ApplicationLauncher>" not in application_adapter_cpp
        and not re.search(
            r"class SpotlightApplicationLauncherAdapter\s*:\s*public SpotlightLauncher\s*,",
            application_adapter_header,
        ),
        "SpotlightApplicationLauncherAdapter must borrow and map the shared launcher request/events",
    )
    require(
        "class SpotlightDesktopCatalogAdapter final : public SpotlightCatalogSource" in catalog_adapter_header
        and "DesktopEntryCatalog &catalog" in catalog_adapter_header
        and "DesktopEntryCatalog &m_catalog" in catalog_adapter_header
        and "apps/DesktopEntryCatalog.hpp" in catalog_adapter_cpp
        and "m_catalog.snapshotJson()" in catalog_adapter_cpp
        and "m_catalog.watchedDirectories()" in catalog_adapter_cpp
        and "DesktopEntryCatalog::indexUpdated" in catalog_adapter_cpp
        and "DesktopEntryCatalog::initialize" not in catalog_adapter_cpp
        and "std::unique_ptr<DesktopEntryCatalog>" not in catalog_adapter_header
        and "make_unique<DesktopEntryCatalog>" not in catalog_adapter_cpp
        and not re.search(
            r"class SpotlightDesktopCatalogAdapter\s*:\s*public SpotlightCatalogSource\s*,",
            catalog_adapter_header,
        ),
        "SpotlightDesktopCatalogAdapter must borrow the one shared catalog and forward its narrow API",
    )
    require(
        not (spotlight_root / "services/ApplicationLauncher.hpp").exists(),
        "the obsolete Spotlight forwarding launcher header must be removed",
    )
    runtime_paths_header = (
        spotlight_root / "platform/runtime/SpotlightRuntimePaths.hpp"
    ).read_text(encoding="utf-8")
    runtime_paths_cpp = (
        spotlight_root / "platform/runtime/SpotlightRuntimePaths.cpp"
    ).read_text(encoding="utf-8")
    require(
        "SpotlightControllerPaths controllerPaths() const" in runtime_paths_header
        and "SpotlightControllerPaths SpotlightRuntimePaths::controllerPaths() const" in runtime_paths_cpp
        and all(path_method in runtime_paths_cpp for path_method in (
            "astreaRoot()", "weatherCli()", "weatherAssetDir()",
        )),
        "SpotlightRuntimePaths must convert only controller-consumed values into controller paths",
    )

    shell_runtime_header = (ROOT / "Shell/runtime/ShellRuntime.hpp").read_text(encoding="utf-8")
    shell_runtime_cpp = (ROOT / "Shell/runtime/ShellRuntime.cpp").read_text(encoding="utf-8")
    require(
        "QString astreaLaunch() const" in runtime_paths_header
        and "astreaLaunch()" in shell_runtime_cpp,
        "SpotlightRuntimePaths must retain astreaLaunch for Shell's shared launcher composition",
    )
    backend_owner = "std::unique_ptr<RustSpotlightBackend> m_spotlightBackend"
    catalog_adapter_owner = (
        "std::unique_ptr<SpotlightDesktopCatalogAdapter> m_spotlightCatalogAdapter"
    )
    launcher_adapter_owner = (
        "std::unique_ptr<SpotlightApplicationLauncherAdapter> m_spotlightLauncherAdapter"
    )
    controller_owner = "std::unique_ptr<SpotlightController> m_spotlightController"
    require(
        backend_owner in shell_runtime_header
        and "std::unique_ptr<DesktopEntryCatalog> m_catalog" in shell_runtime_header
        and "std::unique_ptr<ApplicationLauncher> m_launcher" in shell_runtime_header
        and catalog_adapter_owner in shell_runtime_header
        and launcher_adapter_owner in shell_runtime_header
        and controller_owner in shell_runtime_header
        and shell_runtime_header.index("std::unique_ptr<DesktopEntryCatalog> m_catalog")
            < shell_runtime_header.index(catalog_adapter_owner)
        and shell_runtime_header.index("std::unique_ptr<ApplicationLauncher> m_launcher")
            < shell_runtime_header.index(launcher_adapter_owner)
        and shell_runtime_header.index(backend_owner) < shell_runtime_header.index(catalog_adapter_owner)
        and shell_runtime_header.index(backend_owner) < shell_runtime_header.index(launcher_adapter_owner)
        and shell_runtime_header.index(catalog_adapter_owner) < shell_runtime_header.index(controller_owner)
        and shell_runtime_header.index(launcher_adapter_owner) < shell_runtime_header.index(controller_owner),
        "ShellRuntime must declare shared owners and adapter/backend owners before the borrowing controller",
    )
    require(
        shell_runtime_cpp.count("make_unique<RustSpotlightBackend>") == 1
        and shell_runtime_cpp.count("make_unique<SpotlightDesktopCatalogAdapter>") == 1
        and shell_runtime_cpp.count("make_unique<SpotlightApplicationLauncherAdapter>") == 1
        and shell_runtime_cpp.count("make_unique<DesktopEntryCatalog>") == 1
        and shell_runtime_cpp.count("make_unique<ApplicationLauncher>") == 1
        and "spotlightPaths.controllerPaths()" in shell_runtime_cpp
        and re.search(
            r"make_unique<SpotlightController>\s*\(\s*spotlightPaths\.controllerPaths\(\)\s*,\s*"
            r"\*m_spotlightBackend\s*,\s*\*m_spotlightLauncherAdapter\s*,\s*"
            r"m_spotlightCatalogAdapter\.get\(\)",
            shell_runtime_cpp,
        ) is not None,
        "ShellRuntime must create one shared service of each kind, one adapter each, and inject them",
    )
    require(
        "SpotlightApplicationLauncherAdapter *" not in shell_runtime_header
        and "SpotlightDesktopCatalogAdapter *" not in shell_runtime_header,
        "ShellRuntime must not expose test-only public adapter getters",
    )

    shortcut_test = (
        ROOT / "Shell/tests/ShellShortcutDispatcherTest.cpp"
    ).read_text(encoding="utf-8")
    require(
        "Spotlight/platform/runtime/SpotlightRuntimePaths.hpp" not in shortcut_test
        and "Spotlight/platform/rust/RustSpotlightBackend.hpp" not in shortcut_test
        and "SpotlightControllerPaths" in shortcut_test
        and "FakeSpotlightBackend" in shortcut_test,
        "ShellShortcutDispatcherTest must toggle Spotlight through core-facing test types",
    )
    require(
        "ApplicationLauncher" not in shortcut_test
        and "DesktopEntryCatalog" not in shortcut_test
        and "SpotlightApplicationLauncherAdapter" not in shortcut_test
        and "SpotlightDesktopCatalogAdapter" not in shortcut_test
        and "FakeSpotlightLauncher" in shortcut_test,
        "ShellShortcutDispatcherTest must use only the narrow core launcher/catalog boundary",
    )
    spotlight_test = (spotlight_root / "tests/SpotlightBackendTest.cpp").read_text(encoding="utf-8")
    controller_test_start = spotlight_test.find(
        "void TestSpotlightBackend::testControllerBackendInitialization()"
    )
    controller_test_end = spotlight_test.find(
        "void TestSpotlightBackend::testApplicationLauncher()", controller_test_start
    )
    controller_test_source = (
        spotlight_test[controller_test_start:controller_test_end]
        if controller_test_start >= 0 and controller_test_end > controller_test_start
        else ""
    )
    require(
        bool(controller_test_source)
        and "FakeSpotlightBackend" in spotlight_test
        and "FakeSpotlightLauncher" in spotlight_test
        and "FakeSpotlightCatalog" in spotlight_test
        and not re.search(
            r"ApplicationLauncher|ApplicationLaunchRequest|DesktopEntryCatalog",
            controller_test_source,
        ),
        "Spotlight controller behavior tests must use only the core fakes",
    )
    shell_runtime_tests = (
        (ROOT / "Shell/tests/ShellRuntimeTest.cpp").read_text(encoding="utf-8")
        + (ROOT / "Shell/tests/ShellUnifiedRuntimeIntegrationTest.cpp").read_text(encoding="utf-8")
    )
    require(
        "spotlightController()->catalog()" not in shell_runtime_tests
        and "spotlightController()->launcher()" not in shell_runtime_tests,
        "Shell composition tests must not depend on concrete Spotlight controller getters",
    )
    controller_sources = controller_header + controller_cpp
    require(
        all(debt in controller_sources for debt in PHASE_3_DEBT)
        and "ApplicationLauncher" not in controller_sources
        and "DesktopEntryCatalog" not in controller_sources,
        "Phase 3B debt is limited to weather/QProcess orchestration",
    )
    require(
        has_link(spotlight, "astrea-spotlight-runtime", "Qt6::Core", "PUBLIC"),
        "astrea-spotlight-runtime must publish its Qt Core requirement",
    )
    require(
        has_link(spotlight, "astrea-spotlight-runtime", "astrea-shared-core", "PUBLIC"),
        "astrea-spotlight-runtime must publish its shared-core requirement",
    )
    runtime_private_links = set().union(
        *(libraries for scope, libraries in target_links(spotlight, "astrea-spotlight-runtime")
          if scope == "PRIVATE")
    ) if any(scope == "PRIVATE" for scope, _ in target_links(spotlight, "astrea-spotlight-runtime")) else set()
    require(
        {"dl", "pthread"} <= runtime_private_links,
        "Rust static-library system dependencies dl and pthread must belong to Spotlight runtime",
    )

    require(
        not has_link(spotlight, "astrea-spotlight-wayland", "astrea-spotlight-runtime"),
        "astrea-spotlight-wayland must not gain a fake dependency on astrea-spotlight-runtime",
    )
    require(
        has_link(spotlight, "astrea-spotlight-wayland", "astrea-shared-layer-shell"),
        "astrea-spotlight-wayland must declare astrea-shared-layer-shell",
    )
    require(
        not has_link(spotlight, "astrea-spotlight-runtime", "astrea-spotlight-wayland"),
        "astrea-spotlight-runtime must not depend on the Spotlight Wayland adapter",
    )
    wayland_links = set().union(
        *(libraries for _, libraries in target_links(spotlight, "astrea-spotlight-wayland"))
    ) if target_links(spotlight, "astrea-spotlight-wayland") else set()
    require(
        {"Qt6::Core", "Qt6::Gui", "Qt6::Quick"} <= wayland_links,
        "astrea-spotlight-wayland must declare its Qt Core/Gui/Quick dependencies",
    )

    test_cpp = source_paths(spotlight, "add_executable", "spotlight-tests", ".cpp")
    require(
        not (RUNTIME_CPP & test_cpp),
        "spotlight-tests must not compile private copies of resident runtime sources; "
        f"found={sorted(RUNTIME_CPP & test_cpp)}",
    )
    require(
        not (WAYLAND_CPP & test_cpp),
        "spotlight-tests must not compile a private copy of LayerShellSurface.cpp",
    )
    require(
        has_link(spotlight, "spotlight-tests", "astrea-spotlight-runtime"),
        "spotlight-tests must link astrea-spotlight-runtime",
    )
    test_links = set().union(
        *(libraries for _, libraries in target_links(spotlight, "spotlight-tests"))
    ) if target_links(spotlight, "spotlight-tests") else set()
    require(
        not ({"dl", "pthread"} & test_links),
        "spotlight-tests must receive Rust system link requirements through the runtime target",
    )
    require(
        "astrea-spotlight-wayland" not in test_links,
        "spotlight-tests must not link astrea-spotlight-wayland",
    )
    require(
        "platform/ipc/SpotlightIpcServer.cpp" in test_cpp,
        "spotlight-tests must retain the legacy SpotlightIpcServer test source",
    )
    require(
        "platform/ipc/SpotlightIpcServer.cpp" not in runtime_cpp | wayland_cpp,
        "SpotlightIpcServer.cpp must remain outside resident production targets",
    )
    require(
        "SpotlightIpcServer.cpp" not in shell,
        "Shell must not compile the legacy SpotlightIpcServer",
    )

    cli_sources = set(source_entries(spotlight, "qt_add_executable", "astrea-spotlight"))
    require(
        {"app/main.cpp", "app/CommandLine.cpp", "../Shell/platform/ipc/ShellIpcClient.cpp"}
        <= cli_sources,
        "astrea-spotlight must remain the CommandLine/ShellIpcClient compatibility executable",
    )
    cli_source_paths = {entry.replace("\\", "/") for entry in cli_sources}
    require(
        not (RUNTIME_CPP | WAYLAND_CPP) & cli_source_paths,
        "astrea-spotlight must not compile resident Spotlight implementation sources",
    )
    require(
        "platform/ipc/SpotlightIpcServer.cpp" not in cli_source_paths,
        "astrea-spotlight must not compile the legacy SpotlightIpcServer",
    )
    cli_links = set().union(
        *(libraries for _, libraries in target_links(spotlight, "astrea-spotlight"))
    ) if target_links(spotlight, "astrea-spotlight") else set()
    require(
        not ({"astrea-spotlight-runtime", "astrea-spotlight-wayland"} & cli_links),
        "astrea-spotlight must not link resident Spotlight libraries",
    )
    require(
        "ShellIpcClient" in (ROOT / "Spotlight" / "app" / "main.cpp").read_text(encoding="utf-8"),
        "astrea-spotlight main must continue to use ShellIpcClient",
    )

    production_sources = (RUNTIME_CPP | RUNTIME_HEADERS | WAYLAND_CPP | WAYLAND_HEADERS)
    sibling_include = re.compile(
        r"(?m)^\s*#\s*include\s*[<\"](?:Bar|Dock|AltTab|Paper|Shell)/"
    )
    for relative_path in sorted(production_sources):
        source = (ROOT / "Spotlight" / relative_path).read_text(encoding="utf-8")
        require(
            sibling_include.search(source) is None,
            f"Spotlight production source must not include a sibling component: {relative_path}",
        )

    if errors:
        print("Spotlight architecture guard failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print("Spotlight Phase 1, Phase 2, and Phase 3A architecture invariants passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
