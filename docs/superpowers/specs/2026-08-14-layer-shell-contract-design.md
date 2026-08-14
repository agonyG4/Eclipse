# Layer Shell Contract Design

## Goal

Make the unified `astrea-shell` require a functional Qt 6 LayerShellQt integration at build and startup time. Dock, AltTab, and Spotlight must never silently become ordinary Qt windows.

## Architecture

The root project owns an explicit `ASTREA_ENABLE_LAYER_SHELL` option that defaults to `ON`. A small CMake module performs standard `LayerShellQt` package discovery, verifies `LayerShellQt::Interface`, and emits an actionable configure error. `astrea-shared-layer-shell` remains the only Eclipse target that owns the dependency; `astrea-shared-core` and Settings remain LayerShellQt-free. The explicit `OFF` mode compiles the existing helper stubs for isolated development/tests but is not a runnable production shell configuration.

`AstreaLayerShellHelper` gains a `prepare()` seam. `Shell/app/main.cpp` invokes it before constructing `QGuiApplication`, so the supported `LayerShellQt::Shell::useLayerShell()` call is centralized at the existing shared boundary. `AstreaShellApplication` then requires a Wayland Qt platform and a successful Layer Shell configuration for all three surfaces. QML load failures and per-surface setup failures abort startup; no normal-window fallback remains.

## Diagnostics

Shell status gains a `layerShell` object reporting compiled capability, the production requirement, the Qt platform name, and the successful configuration state of Dock, AltTab, and Spotlight. Startup errors identify whether the build lacks LayerShellQt, Qt is running on a non-Wayland platform, QML failed to load, or a concrete surface failed to configure.

## Tests and CI

The shared target receives a QtTest covering the compile-time capability and preparation seam in both ON and OFF builds. A deterministic Python contract test configures a minimal CMake fixture against a generated package: package available succeeds, disabled mode succeeds, missing package fails with the actionable diagnostic, and the tracked shell sources contain no normal-window fallback. Existing Dock structural tests remain unchanged and continue to protect bottom anchoring, margins, and exclusive-zone behavior.

CI installs a reproducible Qt 6 LayerShellQt dependency for the production CMake matrix, while retaining an explicit `no-layer-shell` preset/job for the dependency-free boundary. Documentation describes the standard CMake search-path contract and the fact that OFF is only for non-production/test use.

## Scope constraints

- Do not modify Typhon, Quickshell, compositor rules, or QML output-coordinate placement.
- Preserve Dock's `astrea-dock` scope, Top layer, bottom anchor, keyboard interactivity, margin, content sizing, dynamic exclusive zone, and unmapping semantics.
- Preserve AltTab and Spotlight overlay policies.
- Do not move LayerShellQt into `astrea-shared-core` or Settings.
