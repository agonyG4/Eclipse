# Unified Astrea icons

Approved in chat on 2026-11-02.

Use one SDK-owned Freedesktop resolver, preserving Explorer candidate ordering, inherited themes, split roots, directory size/scale selection, light/dark variants, symbolic icons and asset invalidation. Keep the SDK provider byte-budgeted positive cache and bounded negative cache. Preserve existing SDK theme-selection precedence and provider URLs during migration; Explorer file configuration accepts desktop_icon_theme with system_icon_theme as fallback.

Expose reusable IconThemeService and FreedesktopIconThemeCatalog in AstreaSDK. Move the Explorer image provider implementation into the SDK with explicit optional initialization; the application owns its service and must outlive the QML engine. Core remains Qt-only and never acquires thumbnail-generation dependencies.

Extract AstreaIcon presentation from AstreaAppIcon. Keep AstreaAppIcon as a compatible adapter. Add AstreaFileIcon with original-color, square defaults and optional thumbnail and emblem composition. Thumbnails remain independently generated; the view keeps its icon visible until a preview is ready. Preserve Explorer loading-cover and thumbnail scheduling behavior.

Orbit consumes the installed AstreaSDK package, retaining only compatibility aliases for its C++ icon types. Do not vendor the SDK or link to Eclipse source paths.

Validation: existing Explorer resolver/provider fixture tests after relocation; SDK provider and QML tests; installed/relocated SDK consumer; Explorer self-test and QML tests. All build output and caches in stable Aether build trees. Work on main, no agents. Existing unrelated changes must stay outside commits.
