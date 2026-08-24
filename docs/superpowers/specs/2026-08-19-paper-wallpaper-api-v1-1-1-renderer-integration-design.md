# Astrea Paper Wallpaper API v1.1.1 — Renderer Integration Design

Date: 2026-08-19  
Status: DESIGN DECISION

Evidence labels used below: `CONFIRMED` is source/test evidence, `DESIGN DECISION` is the selected implementation, `NATIVE-PROVEN` is evidence from the rebuilt LayerShell shell and live Paper endpoint, `UNPROVEN` is a qualification gap, and `FUTURE` is outside this closure.

## CONFIRMED — Current v1.1 state

Paper already owns configured, factory, effective, fallback, generation, persistence, validation, source watching, and terminal mutation results. Eclipse renders the effective snapshot globally; Settings and `astreactl` are asynchronous clients; Typhon remains stateless for wallpaper. This closure preserves those boundaries and changes only integration correctness.

## Confirmed findings

### CONFIRMED — Same-path QML reload

`WallpaperSurface.qml` starts a load when `wallpaperGeneration` changes, but both `Image` slots use the authoritative URL directly and retain `cache: true`. Assigning the same URL after an atomic replacement is not a sufficient cache identity change in Qt Quick. The service generation can therefore advance while Qt Quick keeps the old decoded pixels.

### CONFIRMED — Settings fit authority

The Settings controller projects source, state, fallback, and generation, but not `configured.fit` or `effective.fit`. The page's local index starts at Cover and can overwrite a Paper-owned Contain/Stretch/Center/Tile selection.

### CONFIRMED — Cross-process factory default

The current resolver can select `:/qt/qml/Astrea/Paper/assets/default.jpg`. That resource is linked by the shell's Paper target, not by the independent Settings process. The logical identity is correct, but the resolved source is not consumer-independent.

### CONFIRMED — Timeout semantics

The Paper worker/server contract uses a five-second mutation deadline, while Settings uses a one-second request timer and `astreactl` defaults to two seconds. Transport setup and final operation completion are therefore conflated.

### CONFIRMED — Symlink retargeting

The resolver canonicalizes a symlink target and the watcher currently watches the resolved target and its parent. A retarget of the configured symlink entry can happen without a notification for the watched target.

## DESIGN DECISION — Design decisions

### Generation-specific renderer identity

The bundle continues to pass the physical source URL without changing the authoritative descriptor. QML derives a renderer-only cache key:

```text
physical-source + (? or &) + astreaGeneration=<generation>
```

The key is used only for `Image.source`, so spaces, Unicode, file URLs, and resource URLs remain encoded by the existing bundle conversion. `cache: true` remains enabled between unchanged generations.

Each slot records its requested source and generation. The inactive slot loads the newest generation; if it is already loading, the newest request is queued instead of replacing an in-flight decode. A Ready notification promotes only a slot whose recorded source and generation still equal the newest request. A stale completion triggers the newest load and cannot promote an older generation. The visible slot is never cleared before a replacement reaches Ready.

Disabled-by-default diagnostic properties expose load-start count, Ready/error counts, requested slot generations, and visible generation for deterministic QML tests. No per-frame logging is added.

### Fit projection and strict conversion

Paper remains the only fit authority. The Settings controller exposes typed string values `configuredFit` and `effectiveFit`, validates all five supported wire values (`cover`, `contain`, `stretch`, `center`, `tile`), and rejects unsupported values before sending a mutation. The QML selector chooses configured fit when present and effective fit otherwise; every authoritative snapshot refresh re-synchronizes the selector. The current selection is preserved when only the source changes.

Paper adds an explicit strict fit conversion for API boundaries while retaining the existing compatibility conversion for legacy configuration reads. IPC already rejects unknown fit strings; Settings and `astreactl` use the same five-value set.

### Shared physical factory artwork

`Paper/assets/default.jpg` is installed as `${CMAKE_INSTALL_DATADIR}/AstreaOS/wallpapers/default.jpg`. Resolver candidates are ordered as explicit test override, `ASTREA_WALLPAPER_DEFAULT`, installed XDG data, the known Eclipse source-tree asset, legacy `ASTREA_ROOT` locations, and finally the independent embedded emergency resource. The Paper build embeds the source-tree location as a development-only resolver fallback; the descriptor continues to publish logical ID `astrea://wallpaper/default` and the resolved physical path, never the source-tree path as configuration.

The default QML resource remains available as a packaged shell asset for compatibility, but it is not selected as the normal factory source because another process cannot consume it. Emergency remains a separate small embedded resource.

### Transport and operation deadlines

A shared C++ protocol header documents:

- transport setup/write deadline: 1000 ms;
- Paper operation deadline: 5000 ms;
- client completion margin: 1000 ms.

Paper owns the operation deadline and emits a terminal `timed-out` result if validation/persistence has not completed. Late worker results are ignored. The IPC server waits through the client completion deadline and returns the Paper terminal result. Settings uses the transport timer until the request is written, then waits for the operation deadline plus margin. `astreactl` keeps the existing user-supplied `--timeout`; when omitted for wallpaper mutations it uses the same operation deadline plus margin, while transport writes use the shorter transport deadline.

A client disconnect or local timeout never cancels Paper's operation. Paper's own deadline prevents unbounded work and prevents a late success after its terminal timeout result.

### Minimal symlink watch set

For a local configured source, the watcher deduplicates and maintains at most the configured entry, configured parent directory, resolved target, and resolved target parent. It rebuilds this set whenever the descriptor changes. Directory notifications cover symlink retargeting and atomic rename-overwrite; target notifications cover in-place writes. Resource and non-file sources remain unwatched.

## DESIGN DECISION — End-to-end flow

```text
same physical path changes
        ↓
configured entry/parent or target watcher notification
        ↓
WallpaperService validates the configured descriptor
        ↓
new effective generation
        ↓
QML derives a new generation-specific Image source
        ↓
inactive slot reaches Ready and matches newest generation
        ↓
slot promotion exposes the new pixels
```

Configured source and configured fit remain intact through fallback. Corrupt content shows the factory effective source; restoration of the same configured path validates again and returns to the configured source without restarting Settings or the shell.

## CONFIRMED — Test strategy and evidence

Tests are written before implementation changes. The QML renderer test uses one physical path, distinct image revisions, generation updates, load counters, per-slot state, and rapid revisions. Settings tests cover authoritative fit projection, all five fit values, fallback preservation, physical factory preview input, and a delayed final response beyond the old one-second timer. Resolver tests verify installed/source-tree physical selection and never select the shell-only resource as the normal cross-process default. Watcher/service tests cover symlink retarget, bounded watch count, atomic replacement, corrupt fallback, and restoration. Controlled fake socket servers exercise delayed completion for Settings and `astreactl`; a deterministic delayed validation-worker test remains `UNPROVEN` because the existing final resolver is concrete and normally completes well below the five-second service deadline.

Native qualification is attempted only after these deterministic checks and is reported separately from offscreen/unit evidence.

## NATIVE-PROVEN — Limited live evidence

The rebuilt LayerShell shell remained alive in the available Wayland session and created the secure Paper endpoint. A live `astreactl wallpaper get` returned the normal physical factory default path, and all five fit mutations completed through Paper and were restored to the original configured source. This proves live endpoint and client transaction behavior, not visual desktop or Settings UI behavior.

## UNPROVEN — Qualification gaps

Settings was not visually inspected in the live session, and live same-path pixel transitions were not measured. The offscreen renderer test proves a new generation-specific QML load and promotion, while the service test proves watcher-to-generation replacement behavior; a single native visual test spanning both remains unproven.

## FUTURE — Outside this closure

Pixel-grab automation for the compositor-mediated surface, injected delayed Paper validation, and full multi-output visual qualification can be added when a stable native harness is available.

## Rejected alternatives

- Disabling `Image` caching globally: rejected because unchanged large wallpapers should retain cache reuse.
- Renaming or copying user wallpaper files: rejected because Paper must preserve the configured logical source.
- Moving wallpaper state into Settings or Typhon: rejected because Paper is the authoritative owner.
- Periodic filesystem polling: rejected because event-driven watcher semantics already exist.
- Publishing a `qrc:` factory source to all consumers: rejected because resource bundles are process-local.
- Using one arbitrary timeout for connect, write, and operation completion: rejected because it produces false client failures while Paper is still validly executing.
