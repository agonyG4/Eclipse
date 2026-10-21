# Tray rendering and icon reordering study

Date: 2026-10-21. Status: rendering correction and shared status components
implemented; build and automated integration checks passed.

## Observed problem

The supplied screenshot shows jagged tray artwork while the adjacent topbar
indicators look normal. A read-only inspection of the running StatusNotifier
items found Claude providing a 64x64 pixmap and Steam naming
`steam_tray_mono`, whose local PNG is 48x48. These observations establish that
larger source artwork is available; they do not establish the final texture size
in the running shell or prove that its binary matches this checkout.

## Rendering path in Eclipse

- `Shell/app/AstreaShellApplication.cpp` registers `astrea-tray` with
  `StatusNotifierIconProvider` and the runtime's icon store.
- `Bar/qml/Tray.qml` displays a 16x16 Image with `sourceSize` 16x16,
  `PreserveAspectFit`, and `smooth: true`. It does not enable mipmaps.
- `StatusNotifierIconProvider::requestPixmap()` passes `requestedSize` to the
  store and returns its pixmap without resizing it.
- `StatusNotifierIconStore::selectPixmap()` chooses source artwork by size,
  preferring a source at least as large as requested. It returns the decoded
  source at its original dimensions.
- `fallbackImage()` selects normal/attention artwork, composes any overlay,
  and returns it without resizing to the requested dimensions.
- `image()` explicitly performs smooth resizing for auxiliary images, but
  returns the unresized `fallbackImage()` for the main tray artwork.
- Named icons are loaded eagerly with a nominal 32x32 request. Direct files
  and files found in the item's theme path are read as QImage at their source
  dimensions; the system theme fallback is rasterized through QIcon at 32x32.
- `Bar/qml/components/AppleIcon.qml` instead loads themed artwork directly
  through an Image whose source size follows its 16x16 presentation. SVG
  artwork can therefore be rasterized for the actual presentation size.

## Qt and KDE comparison

The installed Qt Quick reports 6.12.0. In the upstream Qt 6.12.0 source,
`QQuickImageBase::loadPixmap()` passes `sourceSize * devicePixelRatio` to the
loader, and `isScalableImageFormat()` treats image-provider URLs as scalable.
With the existing sourceSize, the provider already receives a physical-size
request. Adding another DPR multiplier in QML would risk applying it twice.

The pixmap-provider branches in `qquickpixmapcache.cpp` create a texture from
the returned pixmap without first resizing it to the requested dimensions.
`QQuickImage` uses linear texture filtering for `smooth: true`; mipmap filtering
is independently disabled by default. Qt documents mipmaps as improving
minification quality compared with smooth filtering alone.

Plasma's StatusNotifierItem uses Kirigami.Icon. Plasma preserves supplied
pixmap sizes in QIcon, and Kirigami requests a pixmap with its presentation
size and effective device pixel ratio. This is a useful rendering reference;
adopting KDE dependencies is not required for the local correction.

References:

- [Qt image loading](https://github.com/qt/qtdeclarative/blob/v6.12.0/src/quick/items/qquickimagebase.cpp)
- [Qt provider texture creation](https://github.com/qt/qtdeclarative/blob/v6.12.0/src/quick/util/qquickpixmapcache.cpp)
- [Qt image filtering](https://github.com/qt/qtdeclarative/blob/v6.12.0/src/quick/items/qquickimage.cpp)
- [Plasma tray item](https://github.com/KDE/plasma-workspace/blob/master/applets/systemtray/qml/StatusNotifierItem.qml)
- [Plasma icon source](https://github.com/KDE/plasma-workspace/blob/master/applets/systemtray/statusnotifieritemsource.cpp)
- [Kirigami icon rendering](https://github.com/KDE/kirigami/blob/master/src/primitives/icon.cpp)

## Hypothesis and implementation choices

The strongest code-supported hypothesis is minification of original-size tray
textures using linear filtering without prefiltering or mipmaps. The supplied
48/64-pixel sources contain more detail than can be represented at 16 logical
pixels. Visual confirmation remains necessary before calling this resolved.

1. Recommended: respect the physical requestedSize in the tray rendering path,
   select/render artwork for that size, and smoothly resize raster artwork
   before returning it. Preserve alpha, aspect ratio, attention state, and
   overlays. Avoid losing scalable named-icon information in a fixed 32x32
   cache. Keep existing geometry and input behavior.
2. Smaller alternative: enable mipmaps on the tray Image. This can improve
   minification, but leaves size-dependent loading and named-icon caching
   behavior unchanged.
3. Broader alternative: retain QIcon-based sources and resolve them on demand,
   following the KDE approach. This changes more of the store and cache API.

## Reordering investigation

`StatusSurface.qml` declares Tray, network, Bluetooth, volume, and clock in a
fixed sequence. Inside Tray, Row/Repeater presents `StatusNotifierItemModel`.
The model appends new items and updates existing rows in place; its current
public API has no move operation. Tray's MouseArea handles clicks and wheel
events, without a drag gesture. DockAppDelegate already demonstrates a local
DragHandler pattern, but its threshold-based gesture does not itself provide
the requested hold-to-drag behavior.

A proposed tray gesture is left-button hold followed by horizontal dragging,
with position feedback, click suppression after dragging, tooltip dismissal,
and cancellation if the item disappears. Preserve the source model and move
existing visual delegates, so updates do not recreate the interaction target.

Persisted order requires an application identity rather than only the current
DBus address: ItemAddress::key() contains service and object path, and a service
can be a connection-specific unique name. Define fallback and duplicate-ID
handling before implementing persistence.

The user subsequently requested that tray and topbar share the same component
to keep their appearance consistent. The implementation scope therefore
includes the common image presentation and hold-to-drag interaction for tray,
network, Bluetooth, volume, and clock. Implementation is delegated to Luna as
explicitly requested by the user.

## Approved implementation direction

First correct the store's main-image minification while preserving its existing
selection and composition behavior. A regression must exercise fine source
details, requested dimensions, and aspect ratio.

Then use a shared image component for tray and themed topbar artwork, and a
shared item interaction component for all status controls. Keep application
artwork and themed artwork as their respective sources. Preserve normal,
middle, and context clicks and volume/tray scrolling.

Provide one horizontal order across individual tray icons and the fixed status
controls. Hold the left button to enter rearrangement, show the insertion
position, and release to commit. Suppress activation after a hold/drag and
cancel safely if the item disappears. Popup anchors must follow the actual
item position. Save stable item identities rather than connection-specific
DBus addresses, and preserve the order of temporarily absent applications.
Share the order across the shell's output surfaces.

Keep independent commits for the rendering correction and shared components /
reordering. Add focused coverage for click-versus-drag behavior, moving an item
across the tray/system boundary, persistence, and item removal while dragging.

## Shared component implementation

`BarIcon.qml` now defines the shared image presentation used by both the tray
and themed topbar artwork. `IndicatorButton.qml` owns the common background,
pointer dispatch, and hold gesture; tray, fixed indicators, and the clock use
that base. A 500 ms left-button hold enters rearrangement, with an 8 logical
pixel movement threshold. Clicks and the scroll wheel retain their actions.

`StatusItemLayout.qml` positions existing delegates in one order, allowing an
individual tray icon to sit between system indicators. It previews position
changes, suppresses activation after holding, closes open popups, and cancels
when membership changes during dragging. Returning to the original position
does not write the configuration.

`BarController.statusOrderStore` shares one `BarStatusOrderStore` between output
surfaces. Committed order is atomically saved to
`$XDG_CONFIG_HOME/AstreaOS/ui/topbar-order.json` (normally
`~/.config/AstreaOS/ui/topbar-order.json`). Application IDs plus object paths
provide normal tray identities; items without an ID use the DBus address
fallback and therefore cannot promise identity across connection changes.
Duplicate IDs receive a service discriminator within the layout. Missing
applications retain their saved slots. Save failures preserve the old order.

The optional ID role preserves compatibility with existing standalone tray
models. Native model data is explicitly requested by the delegate, so stable
IDs remain available when other required QML properties are present.

## Validation and evidence limits

The complete `astrea-shell` target compiled successfully in the stable
`/mnt/Aether/Desktop/GitHub/Eclipse-build` tree. Compilation temporaries,
Cargo output, and QML caches were explicitly directed to that Aether tree.
The final CTest run passed all seven selected suites:

- StatusNotifier: 32 passed.
- Bar core: 18 passed.
- Bar QML smoke: 34 passed.
- Order persistence: 6 passed.
- Shared topbar interaction: 14 passed.
- Bar QML legacy guard: passed.
- Shell context-menu QML interaction: 14 passed.

The new interface cases exercise ordinary activation, middle/context clicks,
scrolling, hold versus click, hand jitter, moving to both ends, moving across
system indicators, cancellation on removal, no-op round trips, actual popup
anchors, multiple output surfaces, restored application identity, missing ID
roles, and session ordering without a persistence store. Tests ran with the
offscreen platform and software Qt Quick backend.

The initial investigation used source and read-only runtime inspection.
The rendering correction was subsequently implemented in commit `d1ae334`:
the store smoothly resizes composed main artwork to the requested dimensions.
Its regression failed before the correction (64x32 returned instead of 16x8)
and passed afterward, including intermediate color and alpha checks. The full
StatusNotifier suite passed 32/32. No live shell replacement was performed.
Compare the current and corrected rendering with the same Steam/Claude
artwork at the actual display scale,
then check fractional scaling, attention/overlay composition, and unchanged
click/menu behavior. Any compilation or test output must use the stable
`/mnt/Aether/Desktop/GitHub/Eclipse-build` directory.

Graph evidence used project `home-agony-GitHub-Eclipse`, generation
2026-10-16T19:37:04Z, Tier 2. All relied-on paths were checked for coverage.
Changed/untracked sources and reported partial ranges were read directly.
The provider call trace contained unrelated resolved symbols, so its inferred
edges were not used as evidence; the source calls above were inspected directly.
