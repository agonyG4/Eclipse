# Astrea Paper Wallpaper API v1 — Completion Report

**Date:** 2026-08-19  
**Scope:** Eclipse Paper service, Eclipse Settings/renderer consumers, and the Typhon `astreactl` client adapter.

## 1. Outcome

Paper is now the authoritative wallpaper state boundary for the native Eclipse shell path. It owns typed descriptors, validation, configured/default/effective state, persistence, fallback, generation, async latest-request-wins transitions, and the secure runtime control API. Eclipse renders the effective descriptor; Settings and `astreactl` consume Paper state without owning it.

The required design and implementation plan are available at:

- `docs/superpowers/specs/2026-08-19-paper-wallpaper-api-v1-design.md`
- `docs/superpowers/plans/2026-08-19-paper-wallpaper-api-v1.md`

## 2. Baseline and working-tree safety

| Repository | Baseline HEAD | Role |
| --- | --- | --- |
| Eclipse | `35d484decb287220775acf9cd02e52352144780d` (`35d484d`) | Native Qt shell, Settings, and Paper implementation |
| Typhon | `0ef9f7b99fa38d0fc04bf5ffa8f494db5a6eade6` (`0ef9f7b`) | Existing compositor and `astreactl` client |
| AstreaOS | `c70bb69b3c2089ce792b7ca6869109dc8418c22a` (`c70bb69`) | Quickshell/Python compatibility implementation |

All three baselines were dirty. Existing Bar, Bench, Shell, Spotlight, compositor, native-output, AstreaOS wallpaper, QML, autostart, and report changes were preserved. No reset, restore, checkout, clean, or stash operation was used. No commit was created.

## 3. Current implementation and legacy inventory

| Observed component | Classification | v1 treatment |
| --- | --- | --- |
| AstreaOS `wallpaper_manager.py` | CURRENT in the Quickshell session; LEGACY for native Eclipse | Left intact as compatibility code; it is not called by native Paper. |
| AstreaOS wallpaper Settings QML and `awww` restore path | CURRENT/LEGACY compatibility | Left intact; native Eclipse Settings uses the Paper client. |
| AstreaOS lockscreen wallpaper path and blur/thumb files | LEGACY/COMPATIBILITY | No longer part of the native Paper authority; no native lockscreen surface exists in Eclipse yet. |
| Historical Paper library/state symlinks and `wallpaper.json` | LEGACY/DEAD evidence | Read-only one-time migration support exists for the known legacy symlink; v1 never creates or repairs one. |
| Eclipse native wallpaper service/renderer | NOT IMPLEMENTED at baseline | Added under `Eclipse/Paper`. |
| Existing `/tmp/astrea-shell-v1` shell IPC | CURRENT for existing shell features | Kept unchanged; Paper uses a separate secure endpoint. |
| Typhon `astreactl` | CURRENT compositor client | Extended with a Paper adapter; Typhon remains stateless. |

The native route no longer has a competing filesystem/symlink authority: Wallpaper QML does not mutate links, write state files, spawn Python, or launch shell commands. Legacy AstreaOS compatibility files were intentionally not deleted because they remain active in a separate Quickshell session.

## 4. External architecture references

- [Hyprpaper](https://github.com/hyprwm/hyprpaper) informed the small runtime control surface, explicit fit vocabulary, and future output-targeting boundary. Astrea does not depend on a separate wallpaper daemon or expose Hyprpaper-style resource management as its product API.
- [KDE Plasma’s image wallpaper QML](https://github.com/KDE/plasma-workspace/blob/master/wallpapers/image/imagepackage/contents/ui/main.qml) informed the separation between wallpaper source/configuration and presentation/loading. Astrea does not copy Plasma’s containment/plugin framework.

## 5. Final Paper architecture

The new native domain is implemented in `Eclipse/Paper`:

- `WallpaperDescriptor`: typed `Image/Dynamic/Video/Slideshow` kind vocabulary, `SystemResource/ExternalFile` source kind, `Cover/Contain/Stretch/Center/Tile` fit, and `Global/Output` scope. v1 accepts only `Image + Global`.
- `WallpaperResolver`: expands `~`, handles file URIs, relative paths at the API boundary, Unicode/spaces, symlinked files, regular-file checks, `QImageReader` validation, and typed failures.
- `WallpaperPersistence`: replaceable interface with an XDG atomic file adapter.
- `WallpaperService`: the single state authority and QML/native API.
- `WallpaperValidationWorker`: one bounded worker pipeline for off-thread validation/decode preparation.
- `WallpaperSurfaceManager`/`WallpaperSurfaceBundle`: presentation only; one background surface per current screen in the available LayerShell build.
- `WallpaperControlServer`: secure local Paper endpoint.

The service snapshot contains `configured`, `factoryDefault`, `effective`, `state`, `fallback`, `generation`, and typed error information. Consumers do not reconstruct this state independently.

## 6. State semantics and default resolution

The state rule is:

```text
valid ConfiguredWallpaper -> EffectiveWallpaper = ConfiguredWallpaper
invalid/unavailable ConfiguredWallpaper -> EffectiveWallpaper = FactoryDefaultWallpaper
no configured override -> EffectiveWallpaper = FactoryDefaultWallpaper
```

An unavailable configured source is retained and reported as fallback; it is not erased. `reset` clears the configured override and resolves the factory descriptor instead of writing the factory path into user state. `generation` advances only when the effective descriptor changes.

The factory lookup chain is:

1. Explicit resolver factory source, when supplied by the host/test.
2. `ASTREA_WALLPAPER_DEFAULT`.
3. `ASTREA_ROOT/Features/Paper/library/landscapes/Sequoia/wallpaper.jpg`.
4. `ASTREA_ROOT/src/Features/Paper/library/landscapes/Sequoia/wallpaper.jpg`.
5. `AstreaOS/wallpapers/Sequoia/wallpaper.jpg` under the generic data location.
6. Embedded `:/qt/qml/Astrea/Paper/assets/emergency.svg` as the final emergency renderable.

The normal product identity is `astrea://wallpaper/default`; the emergency identity is distinct. Missing packaged artwork therefore cannot produce a permanently blank/black wallpaper, but the installed product artwork path remains a deployment qualification item.

## 7. Persistence, Regulus, and migration

No production Regulus implementation was found in Eclipse, Typhon, or the inspected AstreaOS source. v1 therefore uses `WallpaperPersistence` with an XDG-compatible `AstreaOS/paper.ini` adapter. It stores typed configured intent, uses percent encoding for paths/IDs, writes with `QSaveFile`, and applies owner-only permissions. Persistence is centralized in Paper rather than Settings or the renderer.

When the new record is absent, the adapter may import a valid target from the documented legacy `.../user/paper/wallpaper/wallpaper.jpg` symlink. The target is canonicalized and validated; stale links are ignored. The migration is bounded, idempotent, and one-way. It never creates, repairs, or continuously synchronizes a legacy link.

## 8. Renderer and Settings migration

The Paper renderer consumes only the service’s effective descriptor. It creates one transparent-input background surface per current `QScreen`, anchors it to the screen, and mirrors the global v1 descriptor. QML uses asynchronous double-buffered image slots: the current image remains visible until the replacement reaches `Ready`, so a failed load does not create an intentional blank frame.

The native Settings route is `Settings/qml/pages/appearance/Wallpaper.qml`, backed by `SettingsWallpaperController` and exposed through `SettingsController`. It displays authoritative configured/effective/fallback state and uses native socket requests for `get`, `set`, `reset`, and `default`. It does not import process APIs, mutate symlinks, write Paper files, or launch shell/Python commands.

Eclipse has no native lockscreen surface at this baseline. The service publishes the effective descriptor/generation contract required for a future lockscreen consumer, but no unsupported native lockscreen migration or qualification is claimed. The existing AstreaOS Quickshell lockscreen remains explicitly compatibility-only.

## 9. External control and `astreactl`

Paper listens on `$XDG_RUNTIME_DIR/astrea-shell/wallpaper.sock`, with a `0700` parent directory and `0600` socket. Requests are bounded, line-oriented JSON actions:

```text
wallpaper get {}
wallpaper set {"source":"...","kind":"image","fit":"cover","scope":"global"}
wallpaper reset {}
wallpaper default {}
```

Malformed, unknown, unsupported, oversized, and injection-shaped requests are rejected without partial mutation. The endpoint is transport-specific; `WallpaperService` remains transport-independent.

Typhon adds a thin direct socket client and routes:

```text
astreactl wallpaper get
astreactl wallpaper set SOURCE [--fit cover|contain|stretch|center|tile]
astreactl wallpaper reset
astreactl wallpaper default
```

`astreactl` does not discover or connect to the Typhon compositor socket for wallpaper, does not persist state, and rejects Typhon-only `--instance`/`--socket` options for this namespace.

## 10. Concurrency, restart, and error behavior

Validation uses one active request plus one replaceable pending request. Request tokens make late completions harmless. Rapid `set(A)`, `set(B)`, `set(C)` sequences commit only the newest valid request, with bounded work. Failed validation leaves the prior configured/effective state intact; reset invalidates in-flight work.

Covered service semantics include:

- valid configured source and persistence round-trip;
- valid restart restoration from persisted state;
- reset clearing the override;
- missing configured source retained while effective state falls back;
- missing factory source using the emergency fallback;
- invalid/missing/unsupported image inputs;
- Unicode, spaces, URI/path expansion, symlink, and directory handling;
- latest-request-wins and bounded validation work;
- reset invalidating an in-flight request;
- secure IPC, malformed JSON, unknown action, oversized request, Unicode, spaces, and injection-shaped source.

Paper has no periodic wallpaper polling or per-query process spawning. Idle completion leaves the service and worker without pending work; a change performs finite validation/decode preparation.

## 11. Validation results

Using the existing `Eclipse/build/no-layer-shell` build directory:

```text
cmake --build build/no-layer-shell --target astrea-shell astrea-settings -j2        PASS
QT_QPA_PLATFORM=offscreen ctest --test-dir build/no-layer-shell \
  -R 'paper-|settings-(controller|wallpaper|qml|structure)|shell-(runtime|ipc)-test' \
  --output-on-failure                                                               PASS: 12/12
cmake --build build/no-layer-shell --target paper-service-test -j2                  PASS
QT_QPA_PLATFORM=offscreen ctest --test-dir build/no-layer-shell \
  -R 'paper-service-test' --output-on-failure                                       PASS: 1/1
git diff --check (Eclipse)                                                         PASS
```

Typhon results:

```text
cargo fmt --check                                                                  PASS
cargo test --lib astreactl::wallpaper --no-default-features                       PASS: 1/1
cargo test --bin astreactl --no-default-features                                    PASS: 2/2
git diff --check (Typhon)                                                          PASS
```

The broader unchanged `cargo test --lib astreactl --no-default-features` run reached 18 passing tests and two failures in existing `astreactl::discovery` tests. Both fail before Paper code with `path must be shorter than SUN_LEN`, caused by the test-generated Unix socket path in this workspace. `src/astreactl/discovery.rs` was not changed by this task; the focused Paper library and CLI tests pass.

## 12. Native and visual qualification status

No real Typhon/Eclipse Wayland session was exercised in this run. The available verification build uses `ASTREA_ENABLE_LAYER_SHELL=OFF` and offscreen QML tests, so the following are not claimed: live LayerShell presentation, native multi-output behavior, real Settings-to-desktop visual change, native lockscreen/unlock behavior, production factory-artwork packaging, or live `astreactl` socket operation.

## 13. Remaining blockers and future path

- Add a production Regulus adapter when Regulus exists, without changing Paper semantics.
- Integrate the native lockscreen when Eclipse gains that surface; it must consume `effectiveWallpaper` and must not create a second authority.
- Qualify the installed Sequoia/default artwork and live LayerShell output lifecycle in a real session.
- Decide the deployment handoff from the legacy Quickshell/Python session; v1 deliberately does not perform continuous two-way synchronization.
- Add stable `OutputId` scope only when Eclipse has a suitable identity; `Output` is modeled but rejected in v1.
- Add dynamic/video/slideshow providers only as explicit future descriptor kinds; none are partially implemented now.

## 14. Final working-tree status

`git status --short` was run for Eclipse and Typhon at completion. The wallpaper-scope changes are:

```text
Eclipse:
 M CMakeLists.txt
 M Settings/CMakeLists.txt
 M Settings/app/SettingsApplication.cpp
 M Settings/core/CMakeLists.txt
 M Settings/core/SettingsController.cpp
 M Settings/core/SettingsController.hpp
 M Settings/core/navigation/SettingsNavigationCatalog.cpp
 M Settings/qml/CMakeLists.txt
 M Settings/tests/CMakeLists.txt
 M Settings/tests/integration/SettingsQmlSmokeTest.cpp
 M Settings/tests/static/SettingsStructureTest.cmake
 M Settings/tests/unit/SettingsControllerTest.cpp
 M Shell/CMakeLists.txt
 M Shell/app/AstreaShellApplication.cpp
 M Shell/app/AstreaShellApplication.hpp
 M Shell/runtime/ShellRuntime.cpp
 M Shell/runtime/ShellRuntime.hpp
?? Paper/
?? Settings/qml/pages/appearance/
?? Settings/services/wallpaper/
?? Settings/tests/unit/SettingsWallpaperControllerTest.cpp
?? docs/superpowers/plans/2026-08-19-paper-wallpaper-api-v1.md
?? docs/superpowers/specs/2026-08-19-paper-wallpaper-api-v1-design.md
?? REPORT-2026-08-19-paper-wallpaper-api-v1.md

Typhon:
 M src/astreactl/mod.rs
 M src/astreactl/output.rs
 M src/bin/astreactl.rs
 M src/control_snapshots.rs
?? src/astreactl/wallpaper.rs
```

The complete status also contains the pre-existing dirty Bar/Bench/Shell/Spotlight and Typhon compositor/native-output/report files listed in the baseline inventory. They were retained and not used as Paper authorities.
