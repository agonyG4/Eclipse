#!/usr/bin/env python3
"""Repository-specific guard for Spotlight Phase 1 CMake ownership.

This checks only the small set of add_library/qt_add_executable and
target_link_libraries declarations used by Spotlight and Shell. It is not a
general CMake parser.

Phase 1 protects production source ownership and consumer links. It
intentionally does not enforce internal layering: SpotlightController still
depends on platform/rust, platform/runtime, services, ApplicationLauncher,
DesktopEntryCatalog, and QProcess weather orchestration. Those are deferred to
later phases and are not evidence that astrea-spotlight-runtime is a portable
core.

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
    "platform/runtime/SpotlightRuntimePaths.cpp",
    "platform/rust/RustSpotlightBackend.cpp",
}
RUNTIME_HEADERS = {
    "core/SpotlightController.hpp",
    "core/SpotlightResultsModel.hpp",
    "services/AstreaI18n.hpp",
    "services/GameModeMonitor.hpp",
    "services/SpotlightConfigWatcher.hpp",
    "platform/runtime/SpotlightRuntimePaths.hpp",
    "platform/rust/RustSpotlightBackend.hpp",
}
WAYLAND_CPP = {"platform/wayland/LayerShellSurface.cpp"}
WAYLAND_HEADERS = {"platform/wayland/LayerShellSurface.hpp"}


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

    print("Spotlight Phase 1 architecture invariants passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
