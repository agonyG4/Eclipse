# Legacy Astrea Wallpaper Settings Port

## Goal

Replace Eclipse's temporary generic wallpaper settings page with the approved AstreaOS Wallpaper page hierarchy and visual geometry while keeping native Eclipse/Paper ownership intact.

## Design

`Settings/qml/pages/appearance/Wallpaper.qml` will own only presentation and local interaction state. It will restore the legacy `ScrollPage` structure: the CURRENT section, a compact current-wallpaper card, a separate Transition card, the divider and WALLPAPER LIBRARY header, and one card containing independently collapsible Dynamic Wallpapers, User Wallpapers, and Landscapes sections. The page uses the existing Eclipse form primitives and legacy dimensions: 28 px page margins, a 180×112 rounded preview, 16 px card padding, 12 px card radius, 3-column grids, and the approved copy and transition option order.

The current preview and library tiles consume native controller data directly. The preview uses `effectiveSource`; tiles use descriptor metadata and `resolvedSource`, preserving stable `logicalId` values for Paper selection. The page never scans directories, derives names from paths, invokes a process, constructs environment-dependent paths, or persists QML state.

`SettingsWallpaperController` will add read-only presentation projections derived from the Paper response: `currentDisplayName`, `dynamicWallpapers`, `userWallpapers`, and `landscapeWallpapers`. Category membership is semantic: user origin maps to User Wallpapers, dynamic kind maps to Dynamic Wallpapers when not user-owned, and system image descriptors map to Landscapes. The current display name prefers the effective descriptor's `displayName`, then the matching catalog entry, then a stable native fallback; QML supplies the localized final fallback.

The Paper IPC contract remains unchanged. Selecting supported image/global descriptors continues through `selectWallpaper`; unsupported legacy-only features (transition renderer, blur mode, workspace scope, Screensaver, Lockscreen, and file picking) remain visually present with isolated no-op/future-integration signals and no fake persistence or legacy runtime dependencies.

## Testing

The controller unit test will prove semantic projections and display-name precedence using a fake Paper local socket response. The QML smoke test will assert the restored page object hierarchy and 28 px ScrollPage margin. The existing Settings wallpaper controller, QML smoke, and relevant Paper tests will continue to run through the normal Eclipse CMake/CTest configuration.

## Constraints

- Work directly on `main` in `/home/agony/GitHub/Eclipse`.
- Preserve unrelated existing worktree changes.
- Do not add Quickshell, QML processes, shell commands, filesystem scans, awww, Hyprland, Typhon, or QML-owned persistence.
- Do not extend Paper's renderer or IPC contract for this visual port.
