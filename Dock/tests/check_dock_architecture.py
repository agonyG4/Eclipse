#!/usr/bin/env python3
"""Check Dock's resident build ownership and component boundaries."""

from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[2]
DOCK_CMAKE = (ROOT / "Dock/CMakeLists.txt").read_text(encoding="utf-8")
SHELL_CMAKE = (ROOT / "Shell/CMakeLists.txt").read_text(encoding="utf-8")

RUNTIME_SOURCES = (
    "core/DockAppInfo.cpp",
    "core/DockAppModel.cpp",
    "core/DockController.cpp",
    "core/DockSurfaceGeometry.cpp",
    "services/DockConfigPersistence.cpp",
    "services/DockConfigValidation.cpp",
    "services/DockConfigWatcher.cpp",
    "platform/runtime/DockRuntimePaths.cpp",
)
WAYLAND_SOURCES = (
    "platform/wayland/DockLayerShellSurface.cpp",
    "platform/wayland/DockInputRegionBridge.cpp",
    "platform/wayland/DockInputRegionPolicy.cpp",
)
RESIDENT_SOURCES = (*RUNTIME_SOURCES, *WAYLAND_SOURCES)

TEST_CONSUMERS = {
    "dock-app-model-test": "astrea-dock-runtime",
    "dock-config-watcher-test": "astrea-dock-runtime",
    "dock-config-persistence-test": "astrea-dock-runtime",
    "dock-runtime-paths-test": "astrea-dock-runtime",
    "dock-controller-test": "astrea-dock-runtime",
    "dock-typhon-runtime-integration-test": "astrea-dock-runtime",
    "dock-layer-shell-surface-test": "astrea-dock-wayland",
    "dock-input-region-test": "astrea-dock-wayland",
    "dock-hover-qml-test": "astrea-dock-wayland",
    "dock-surface-geometry-test": "astrea-dock-runtime",
}


def target_block(text: str, command: str, target: str) -> str | None:
    """Extract a known line-formatted CMake command without parsing CMake."""
    match = re.search(
        rf"(?ms)^[ \t]*{re.escape(command)}\(\s*{re.escape(target)}(?![A-Za-z0-9_-])(.*?)^[ \t]*\)",
        text,
    )
    return match.group(0) if match else None


def main() -> int:
    errors: list[str] = []

    for target, expected_sources in (
        ("astrea-dock-runtime", RUNTIME_SOURCES),
        ("astrea-dock-wayland", WAYLAND_SOURCES),
    ):
        block = target_block(DOCK_CMAKE, "add_library", target)
        if block is None:
            errors.append(f"Dock/CMakeLists.txt must define {target}")
            continue
        for source in expected_sources:
            if source not in block:
                errors.append(f"{target} must own {source}")

    for target, dependency, cmake in (
        ("astrea-shell-runtime", "astrea-dock-runtime", SHELL_CMAKE),
        ("astrea-shell", "astrea-dock-wayland", SHELL_CMAKE),
        ("astrea-dock-wayland", "astrea-dock-runtime", DOCK_CMAKE),
    ):
        block = target_block(cmake, "target_link_libraries", target)
        if block is None or not re.search(rf"\b{re.escape(dependency)}\b", block):
            errors.append(f"{target} must link {dependency}")

    direct_dependencies = {
        "astrea-dock-runtime": (
            "Qt6::Core",
            "astrea-shared-core",
            "astrea-shared-appidentity",
            "astrea-shared-dock",
            "astrea-shared-typhon",
        ),
        "astrea-dock-wayland": (
            "Qt6::Core",
            "Qt6::Gui",
            "Qt6::Quick",
            "astrea-shared-dock",
            "astrea-shared-layer-shell",
        ),
    }
    for target, dependencies in direct_dependencies.items():
        block = target_block(DOCK_CMAKE, "target_link_libraries", target)
        for dependency in dependencies:
            if block is None or not re.search(rf"\b{re.escape(dependency)}\b", block):
                errors.append(f"{target} must declare its direct dependency on {dependency}")

    for source in RESIDENT_SOURCES:
        dock_count = len(
            re.findall(
                rf"(?<![A-Za-z0-9_./-]){re.escape(source)}(?![A-Za-z0-9_.-])",
                DOCK_CMAKE,
            )
        )
        shell_count = SHELL_CMAKE.count(f"../Dock/{source}")
        if dock_count != 1 or shell_count != 0:
            errors.append(
                f"{source} must have one Dock CMake owner and zero Shell owners "
                f"(found Dock={dock_count}, Shell={shell_count})"
            )

    resident_shell_source = re.compile(
        r"\bDock/(?:core|services|platform/runtime|platform/wayland)/[^\s)#\"']+\.cpp\b"
    )
    for line_number, line in enumerate(SHELL_CMAKE.splitlines(), start=1):
        if resident_shell_source.search(line):
            errors.append(
                f"Shell/CMakeLists.txt:{line_number} directly compiles {line.strip()}"
            )

    for target, dependency in TEST_CONSUMERS.items():
        block = target_block(DOCK_CMAKE, "add_executable", target)
        if block is None:
            errors.append(f"Dock/CMakeLists.txt must define {target}")
            continue
        link_block = target_block(DOCK_CMAKE, "target_link_libraries", target)
        if link_block is None or not re.search(rf"\b{re.escape(dependency)}\b", link_block):
            errors.append(f"{target} must link {dependency}")
        for source in RESIDENT_SOURCES:
            if source in block:
                errors.append(f"{target} must not compile a private copy of {source}")

    dock_test_targets = re.finditer(
        r"(?ms)^[ \t]*add_executable\(\s*(dock-[A-Za-z0-9_-]+)(.*?)^[ \t]*\)",
        DOCK_CMAKE,
    )
    for match in dock_test_targets:
        target, block = match.group(1), match.group(0)
        for source in RESIDENT_SOURCES:
            if source in block:
                errors.append(f"{target} must not compile a private copy of {source}")

    geometry_definitions = len(
        re.findall(
            r"(?m)^[ \t]*add_executable\(\s*dock-surface-geometry-test\b",
            DOCK_CMAKE + SHELL_CMAKE,
        )
    )
    if geometry_definitions != 1 or target_block(
        DOCK_CMAKE, "add_executable", "dock-surface-geometry-test"
    ) is None:
        errors.append("dock-surface-geometry-test must be registered exactly once by Dock")
    if not re.search(r"(?m)^\s*add_test\(NAME dock-surface-geometry-test\b", DOCK_CMAKE):
        errors.append("Dock/CMakeLists.txt must register dock-surface-geometry-test")

    if not re.search(r"(?m)^\s*add_test\(NAME dock-architecture-guard\b", DOCK_CMAKE):
        errors.append("Dock/CMakeLists.txt must register dock-architecture-guard")

    compatibility = target_block(DOCK_CMAKE, "qt_add_executable", "astrea-dock")
    if compatibility is None:
        compatibility = target_block(DOCK_CMAKE, "add_executable", "astrea-dock")
    if compatibility is None:
        errors.append("Dock/CMakeLists.txt must define the astrea-dock compatibility client")
    compatibility_links = target_block(DOCK_CMAKE, "target_link_libraries", "astrea-dock")
    if any(
        block and re.search(r"\bastrea-dock-(?:runtime|wayland)\b", block)
        for block in (compatibility, compatibility_links)
    ):
        errors.append("astrea-dock must remain independent of resident Dock libraries")

    # Phase 1 intentionally preserves Dock's direct shared/platform/typhon coupling.
    # Do not treat shared/platform/typhon or astrea-shared-typhon as sibling dependencies;
    # that runtime/core layering debt is reserved for Dock Runtime Boundary Phase 2.
    sibling_include = re.compile(
        r"^\s*#\s*include\s*[<\"](?:\.\./)*(?:Shell|Bar|Spotlight|AltTab)/",
        re.MULTILINE,
    )
    for directory in ("core", "services", "platform/runtime", "platform/wayland"):
        for path in sorted((ROOT / "Dock" / directory).glob("*")):
            if path.suffix not in {".cpp", ".hpp"}:
                continue
            if sibling_include.search(path.read_text(encoding="utf-8")):
                errors.append(
                    "resident Dock C++ must not include a sibling component from "
                    f"{path.relative_to(ROOT)}"
                )

    for target in ("astrea-dock-runtime", "astrea-dock-wayland"):
        block = target_block(DOCK_CMAKE, "target_link_libraries", target)
        if block and re.search(r"\bastrea-(?:shell|bar|spotlight|alt-tab)(?:-|\b)", block):
            errors.append(f"{target} must not link a sibling product component")

    if errors:
        print("Dock architecture guard failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print("Dock architecture guard passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
