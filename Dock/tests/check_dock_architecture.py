#!/usr/bin/env python3
"""Check Dock's resident build ownership and component boundaries."""

from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[2]
DOCK_CMAKE = (ROOT / "Dock/CMakeLists.txt").read_text(encoding="utf-8")
SHELL_CMAKE = (ROOT / "Shell/CMakeLists.txt").read_text(encoding="utf-8")
SHARED_CMAKE = (ROOT / "shared/CMakeLists.txt").read_text(encoding="utf-8")

RUNTIME_SOURCES = (
    "core/runtime/DockRuntimeBackend.cpp",
    "core/runtime/DockAppMatcher.cpp",
    "core/runtime/DockRuntimeTaskIdentityTracker.cpp",
    "core/runtime/DockApplicationStateProjector.cpp",
    "core/DockAppInfo.cpp",
    "core/DockAppModel.cpp",
    "core/DockController.cpp",
    "core/DockSurfaceGeometry.cpp",
    "services/DockConfigPersistence.cpp",
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
    "dock-app-matcher-test": "astrea-dock-runtime",
    "dock-application-state-projector-test": "astrea-dock-runtime",
    "dock-config-watcher-test": "astrea-dock-runtime",
    "dock-config-persistence-test": "astrea-dock-runtime",
    "dock-runtime-paths-test": "astrea-dock-runtime",
    "dock-controller-test": "astrea-dock-runtime",
    "dock-typhon-runtime-integration-test": "astrea-dock-typhon",
    "dock-layer-shell-surface-test": "astrea-dock-wayland",
    "dock-input-region-test": "astrea-dock-wayland",
    "dock-hover-qml-test": "astrea-dock-wayland",
    "dock-surface-geometry-test": "astrea-dock-runtime",
}


def command_blocks(text: str, command: str) -> list[tuple[str, str]]:
    """Extract simple line-formatted CMake commands without parsing CMake."""
    pattern = re.compile(
        rf"(?ms)^[ \t]*{re.escape(command)}\(\s*([A-Za-z0-9_.:+-]+)(.*?)^[ \t]*\)"
    )
    return [(match.group(1), match.group(0)) for match in pattern.finditer(text)]


def target_blocks(text: str, command: str, target: str) -> list[str]:
    return [block for name, block in command_blocks(text, command) if name == target]


def has_dependency(blocks: list[str], dependency: str) -> bool:
    return any(re.search(rf"\b{re.escape(dependency)}\b", block) for block in blocks)


def has_scoped_dependency(blocks: list[str], scope: str, dependency: str) -> bool:
    for block in blocks:
        scopes = list(re.finditer(r"\b(PUBLIC|PRIVATE|INTERFACE)\b", block))
        for index, match in enumerate(scopes):
            if match.group(1) != scope:
                continue
            end = scopes[index + 1].start() if index + 1 < len(scopes) else len(block)
            if re.search(rf"\b{re.escape(dependency)}\b", block[match.end():end]):
                return True
    return False


def without_cpp_comments(text: str) -> str:
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.DOTALL)


def main() -> int:
    errors: list[str] = []

    runtime_blocks = target_blocks(DOCK_CMAKE, "add_library", "astrea-dock-runtime")
    if runtime_blocks and "core/DockPinStore.hpp" not in "\n".join(runtime_blocks):
        errors.append("astrea-dock-runtime must list core/DockPinStore.hpp")
    if "DockConfigValidation" in DOCK_CMAKE:
        errors.append("Dock/CMakeLists.txt must not reference DockConfigValidation")

    pin_store = ROOT / "Dock/core/DockPinStore.hpp"
    controller_header = ROOT / "Dock/core/DockController.hpp"
    if not pin_store.is_file():
        errors.append("Dock core must define Dock/core/DockPinStore.hpp")
    else:
        port = without_cpp_comments(pin_store.read_text(encoding="utf-8"))
        port_class = re.search(r"\bclass\s+DockPinStore\s*\{(?P<body>.*?)\};", port, re.DOTALL)
        if not port_class:
            errors.append("DockPinStore.hpp must define DockPinStore")
        else:
            body = re.sub(r"\s+", " ", port_class.group("body")).strip()
            expected_body = (
                "public: virtual ~DockPinStore() = default; "
                "virtual bool writePins(const QStringList &pins, QString *errorOut = nullptr) = 0;"
            )
            if body != expected_body:
                errors.append("DockPinStore must contain only its virtual destructor and writePins")
        port_includes = re.findall(r"(?m)^\s*#\s*include\s*[<\"]([^>\"]+)[>\"]", port)
        if port_includes != ["QString", "QStringList"]:
            errors.append("DockPinStore may include only the Qt QString value types it needs")

    controller = without_cpp_comments(controller_header.read_text(encoding="utf-8"))
    if '#include "core/DockPinStore.hpp"' not in controller:
        errors.append("DockController.hpp must include the core DockPinStore contract")
    if not re.search(r"DockController\s*\([^;]*DockPinStore\s*\*", controller, re.DOTALL):
        errors.append("DockController must consume DockPinStore")
    if not re.search(r"DockPinStore\s*\*\s*m_pinStore\b", controller):
        errors.append("DockController must store its pin persistence dependency as m_pinStore")
    controller_source = without_cpp_comments(
        (ROOT / "Dock/core/DockController.cpp").read_text(encoding="utf-8")
    )
    if not re.search(r"DockConfigCodec::validDesktopFileName\s*\(", controller_source):
        errors.append("DockController must validate desktop filenames with DockConfigCodec")

    for path in sorted((ROOT / "Dock/core").rglob("*")):
        if path.suffix not in {".cpp", ".hpp"}:
            continue
        source = without_cpp_comments(path.read_text(encoding="utf-8"))
        if re.search(
            r"Dock/services/|\bservices/|\bDockConfigPersistence\b|"
            r"\bDockConfigWatcher\b|\bDockConfigValidation\b",
            source,
        ):
            errors.append(f"Dock core must not depend on Dock services: {path.relative_to(ROOT)}")

    persistence_header = ROOT / "Dock/services/DockConfigPersistence.hpp"
    persistence = without_cpp_comments(persistence_header.read_text(encoding="utf-8"))
    if not re.search(r"class\s+DockConfigPersistence\s+final\s*:\s*public\s+DockPinStore", persistence):
        errors.append("DockConfigPersistence must implement DockPinStore as a final service")

    for path in sorted((ROOT / "Dock").rglob("*")):
        if path.suffix not in {".cpp", ".hpp"} or "tests/" in path.relative_to(ROOT / "Dock").as_posix():
            continue
        if re.search(r"\bDockConfigValidation\b", without_cpp_comments(path.read_text(encoding="utf-8"))):
            errors.append(f"production source must not reference DockConfigValidation: {path.relative_to(ROOT)}")

    shell_runtime_header = without_cpp_comments(
        (ROOT / "Shell/runtime/ShellRuntime.hpp").read_text(encoding="utf-8")
    )
    persistence_member = re.search(
        r"std::unique_ptr\s*<\s*DockConfigPersistence\s*>\s+m_dockPersistence\s*;",
        shell_runtime_header,
    )
    controller_member = re.search(
        r"std::unique_ptr\s*<\s*DockController\s*>\s+m_dockController\s*;",
        shell_runtime_header,
    )
    if not persistence_member or not controller_member or persistence_member.start() > controller_member.start():
        errors.append("ShellRuntime must declare DockConfigPersistence before its borrowing DockController")

    for target, expected_sources in (
        ("astrea-dock-runtime", RUNTIME_SOURCES),
        ("astrea-dock-wayland", WAYLAND_SOURCES),
        ("astrea-dock-typhon", (
            "platform/typhon/TyphonDockRuntimeBackend.cpp",
        )),
    ):
        blocks = target_blocks(DOCK_CMAKE, "add_library", target)
        if not blocks:
            errors.append(f"Dock/CMakeLists.txt must define {target}")
            continue
        block = "\n".join(blocks)
        for source in expected_sources:
            if source not in block:
                errors.append(f"{target} must own {source}")

    required_links = (
        ("astrea-shell-runtime", "astrea-dock-runtime", SHELL_CMAKE),
        ("astrea-shell-runtime", "astrea-dock-typhon", SHELL_CMAKE),
        ("astrea-shell", "astrea-dock-wayland", SHELL_CMAKE),
        ("astrea-dock-wayland", "astrea-dock-runtime", DOCK_CMAKE),
        ("astrea-dock-typhon", "astrea-dock-runtime", DOCK_CMAKE),
        ("astrea-dock-typhon", "astrea-shared-typhon", DOCK_CMAKE),
    )
    for target, dependency, cmake in required_links:
        blocks = target_blocks(cmake, "target_link_libraries", target)
        if not has_dependency(blocks, dependency):
            errors.append(f"{target} must link {dependency}")

    runtime_links = target_blocks(DOCK_CMAKE, "target_link_libraries", "astrea-dock-runtime")
    if not runtime_links:
        errors.append("astrea-dock-runtime must declare its link dependencies")
    for dependency in ("astrea-shared-typhon", "astrea-dock-typhon"):
        if has_dependency(runtime_links, dependency):
            errors.append(f"astrea-dock-runtime must not link {dependency}")
    if not has_scoped_dependency(runtime_links, "PUBLIC", "astrea-shared-appidentity"):
        errors.append("astrea-dock-runtime must link astrea-shared-appidentity PUBLIC")

    wayland_links = target_blocks(DOCK_CMAKE, "target_link_libraries", "astrea-dock-wayland")
    for dependency in ("astrea-dock-typhon", "astrea-shared-typhon"):
        if has_dependency(wayland_links, dependency):
            errors.append(f"astrea-dock-wayland must remain independent of {dependency}")

    for target, dependencies in {
        "astrea-dock-runtime": (
            "Qt6::Core", "astrea-shared-core", "astrea-shared-appidentity",
            "astrea-shared-dock",
        ),
        "astrea-dock-wayland": (
            "Qt6::Core", "Qt6::Gui", "Qt6::Quick", "astrea-shared-dock",
            "astrea-shared-layer-shell",
        ),
    }.items():
        blocks = target_blocks(DOCK_CMAKE, "target_link_libraries", target)
        for dependency in dependencies:
            if not has_dependency(blocks, dependency):
                errors.append(f"{target} must declare its direct dependency on {dependency}")

    for source in RESIDENT_SOURCES:
        dock_count = sum(block.count(source) for block in target_blocks(
            DOCK_CMAKE, "add_library", "astrea-dock-runtime"
        ) + target_blocks(DOCK_CMAKE, "add_library", "astrea-dock-wayland"))
        if source.startswith("platform/wayland/"):
            shell_count = SHELL_CMAKE.count(f"../Dock/{source}")
        else:
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
        blocks = target_blocks(DOCK_CMAKE, "add_executable", target)
        if not blocks:
            errors.append(f"Dock/CMakeLists.txt must define {target}")
            continue
        link_blocks = target_blocks(DOCK_CMAKE, "target_link_libraries", target)
        if not has_dependency(link_blocks, dependency):
            errors.append(f"{target} must link {dependency}")
        for block in blocks:
            for source in RESIDENT_SOURCES:
                if source in block:
                    errors.append(f"{target} must not compile a private copy of {source}")

    for target, block in command_blocks(DOCK_CMAKE, "add_executable"):
        if not target.startswith("dock-"):
            continue
        for source in RESIDENT_SOURCES:
            if source in block:
                errors.append(f"{target} must not compile a private copy of {source}")

    geometry_definitions = sum(
        1 for target, _ in command_blocks(DOCK_CMAKE + SHELL_CMAKE, "add_executable")
        if target == "dock-surface-geometry-test"
    )
    if geometry_definitions != 1 or not target_blocks(
        DOCK_CMAKE, "add_executable", "dock-surface-geometry-test"
    ):
        errors.append("dock-surface-geometry-test must be registered exactly once by Dock")
    if not re.search(r"(?m)^\s*add_test\(NAME dock-surface-geometry-test\b", DOCK_CMAKE):
        errors.append("Dock/CMakeLists.txt must register dock-surface-geometry-test")
    if not re.search(r"(?m)^\s*add_test\(NAME dock-architecture-guard\b", DOCK_CMAKE):
        errors.append("Dock/CMakeLists.txt must register dock-architecture-guard")

    compatibility = target_blocks(DOCK_CMAKE, "qt_add_executable", "astrea-dock")
    if not compatibility:
        compatibility = target_blocks(DOCK_CMAKE, "add_executable", "astrea-dock")
    if not compatibility:
        errors.append("Dock/CMakeLists.txt must define the astrea-dock compatibility client")
    compatibility_links = target_blocks(DOCK_CMAKE, "target_link_libraries", "astrea-dock")
    for block in (*compatibility, *compatibility_links):
        if re.search(r"\bastrea-dock-(?:runtime|wayland|typhon)\b", block):
            errors.append("astrea-dock must remain independent of resident Dock libraries")

    # Dock core/services/runtime code depends only on neutral Dock runtime types.
    forbidden_runtime_tokens = re.compile(
        r"(?:platform/typhon/|TyphonToplevelConnection|TyphonProtocolTypes|Astrea::Typhon)"
    )
    for directory in ("core", "services", "platform/runtime"):
        for path in sorted((ROOT / "Dock" / directory).rglob("*")):
            if path.suffix not in {".cpp", ".hpp"}:
                continue
            if forbidden_runtime_tokens.search(path.read_text(encoding="utf-8")):
                errors.append(
                    "Dock runtime production source must use neutral runtime types: "
                    f"{path.relative_to(ROOT)}"
                )

    # The Shell composition root owns the connection and adapter lifetime.
    shell_runtime = (ROOT / "Shell/runtime/ShellRuntime.cpp").read_text(encoding="utf-8")
    if "TyphonDockRuntimeBackend" not in shell_runtime or "attachRuntimeBackend" not in shell_runtime:
        errors.append("ShellRuntime must attach a TyphonDockRuntimeBackend to DockController")
    if "attachTyphonConnection" in shell_runtime:
        errors.append("ShellRuntime must not pass TyphonToplevelConnection directly to DockController")
    for path in (ROOT / "Dock").rglob("*.cpp"):
        relative = path.relative_to(ROOT / "Dock").as_posix()
        if "tests/" in relative:
            continue
        if "platform/typhon" not in relative and \
                "TyphonToplevelConnection" in path.read_text(encoding="utf-8"):
            errors.append(
                "TyphonToplevelConnection adaptation must stay in Dock/platform/typhon: "
                f"{path.relative_to(ROOT)}"
            )
    for component in (ROOT / "Dock", ROOT / "Shell"):
        for path in component.rglob("*.cpp"):
            if "tests/" in path.relative_to(component).as_posix():
                continue
            if "attachTyphonConnection" in path.read_text(encoding="utf-8"):
                errors.append(f"production call to attachTyphonConnection remains in {path.relative_to(ROOT)}")

    for name in (
        "DockApplicationStateProjector.cpp",
        "RuntimeTaskIdentityTracker.cpp",
        "TyphonAppMatcher.cpp",
        "DockApplicationStateProjectorTest.cpp",
        "TyphonAppMatcherTest.cpp",
    ):
        if name in SHARED_CMAKE:
            errors.append(f"shared/CMakeLists.txt must not own Dock-specific {name}")

    sibling_include = re.compile(
        r"^\s*#\s*include\s*[<\"](?:\.\./)*(?:Shell|Bar|Spotlight|AltTab|Paper)/",
        re.MULTILINE,
    )
    for directory in ("core", "services", "platform/runtime", "platform/wayland"):
        for path in sorted((ROOT / "Dock" / directory).rglob("*")):
            if path.suffix not in {".cpp", ".hpp"}:
                continue
            if sibling_include.search(path.read_text(encoding="utf-8")):
                errors.append(
                    "resident Dock C++ must not include a sibling component from "
                    f"{path.relative_to(ROOT)}"
                )

    sibling_target = re.compile(
        r"\bastrea-(?:shell|bar|spotlight|alttab|alt-tab|paper)(?:-|\b)"
    )
    for target in ("astrea-dock-runtime", "astrea-dock-wayland", "astrea-dock-typhon"):
        for block in target_blocks(DOCK_CMAKE, "target_link_libraries", target):
            if sibling_target.search(block):
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
