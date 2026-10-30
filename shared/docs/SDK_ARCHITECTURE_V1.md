# Astrea SDK Architecture V1 (M0)

## Scope and interface classes

The installable SDK package in M0 is intentionally limited to `AstreaSDK::Core`:
the Qt Core/Gui/Qml/Quick dependency surface, the existing `Astrea.Shared` QML
module, icon rendering and theme discovery, and the C++ engine initializer.
`AstreaAppIcon`, `ShellMaterialTheme`, and the `image://astrea-icon/` provider
remain the existing implementation; M0 does not replace their rendering or
theme code.

The C++ public header is `AstreaSDK/Core.hpp`. Call
`AstreaSDK::Core::initializeQmlEngine(engine, &error)` once before loading QML.
It registers the `astrea-icon` provider and publishes it as the
`AstreaIconProvider` QML context property used by `AstreaAppIcon` for theme
revision updates. Repeating the call is safe. A conflicting image-provider ID
is an initialization error. An already registered Astrea provider is reused.

`Astrea.Shared` is the only module in this package intended as a public M0 QML
surface. Existing `Astrea.System` and `Astrea.Effects` URIs are preserved for
Eclipse's internal applications, but they are not part of `AstreaSDK::Core`.
They are experimental in-tree interfaces without an independent SDK support
promise. System services and Wayland effects are future, separately qualified
modules.
Shell composition, Settings controllers, generated protocol objects, Typhon
clients and authentication, system backends, and all other Shared headers are
internal implementation details. No private Shared headers are installed.

## Ownership and lifetime

The application creates and owns its `QGuiApplication` and `QQmlEngine`. The
SDK allocates `AstreaIconProvider` only when the engine has no provider under
`astrea-icon`; `QQmlEngine::addImageProvider` transfers ownership to that
engine. The initializer also places the same engine-owned object in the root
QML context as `AstreaIconProvider`. Keep the engine alive while QML objects
that use `Astrea.Shared` are alive, and do not manually delete the provider.
Destroying the engine destroys its registered image provider.

The icon provider retains the existing process-wide Qt icon-theme integration
and filesystem watchers. Its theme revision changes invalidate QML icon URLs;
the SDK does not create another theme service or watcher. Theme-dependent
presentation properties in `AstreaAppIcon` can consume an application's
optional `ThemeController` context property. The SDK initializer does not
create or own that application-specific controller.

## Package and dependency graph

`AstreaSDK::Core` is the installed shared facade. It links the existing static
`astrea-shared-core` implementation and statically imports its generated QML
plugin. The plugin packages `Astrea.Shared` QML resources, including the
rounded-icon shader. Keeping Qt's generated resource object targets inside the
SDK shared library avoids exporting build-tree-only target names and means
consumers link only the facade; the plugin remains statically registered with
Qt when the SDK library loads.

The installed package resolves Qt 6.8 or later's Core, Gui, Qml, and Quick
targets. Qt ShaderTools is needed by Eclipse at build time to compile the
existing shader; it is not part of the public link interface. SDK Core does
not link PipeWire, BlueZ, Rust/CXX-Qt, LayerShellQt, Wayland, or Typhon.
Installation uses ordinary CMake archive, include, and package locations
under the caller's `CMAKE_INSTALL_PREFIX`. Exported target locations are
relative to the package prefix, so the installation can be moved.

## Installation and independent use

Configure Eclipse in SDK-only mode with a desired install prefix, then build
and install the standalone SDK. SDK-only mode configures Shared Core and the
SDK without probing LayerShellQt, Wayland, PipeWire, or CXX-Qt/Cargo. For
example:

```sh
export CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-target
export TMPDIR=/mnt/Aether/Desktop/GitHub/tmp
export XDG_CACHE_HOME=/mnt/Aether/Desktop/GitHub/cache
cmake -S /home/agony/GitHub/Eclipse \
  -B /mnt/Aether/Desktop/GitHub/Eclipse-build \
  -DASTREA_BUILD_SDK_ONLY=ON \
  -DCMAKE_INSTALL_PREFIX=/opt/astrea
cmake --build /mnt/Aether/Desktop/GitHub/Eclipse-build \
  --target astrea-sdk-core
cmake --install /mnt/Aether/Desktop/GitHub/Eclipse-build \
  --component AstreaSDK
```

An independent consumer can use standard package discovery and Qt targets:

```cmake
find_package(AstreaSDK CONFIG REQUIRED)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick)

target_link_libraries(my-app PRIVATE
    AstreaSDK::Core
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
)
```

Create the application's `QGuiApplication` and `QQmlEngine`, call
`AstreaSDK::Core::initializeQmlEngine(engine, &error)`, and then load QML that
imports `Astrea.Shared`. Add Qt QML runtime modules to the deployed application
according to the application's Qt deployment process. `shared/examples/sdk-consumer`
is the package-install integration consumer and does not refer to Eclipse
source-tree paths.

## Versioning and compatibility

The package currently reports version 0.1.0 and is pre-1.0. Its C++ and QML
interfaces may change between M0 and a 1.0 release. The generated CMake version
file advertises only an exact version match; consumers that specify a package
version must select the version they built against. Before 1.0, source and ABI
compatibility across releases is not promised. A future 1.x policy should
version the public API deliberately and preserve compatible changes within a
major release.

## Cross-process ownership and capability boundaries

M0 SDK Core is in-process Qt/QML and icon/theme functionality. It creates no
daemon and does not connect to Typhon. Eclipse Shell, Settings, and other
services retain their existing process ownership and IPC responsibilities.
The Typhon tree contains public read-oriented compositor protocol paths as
well as authenticated action and capability protocols; authentication is
performed by internal clients against the compositor connection. Neither
those clients, generated Wayland objects, authentication helpers, nor
privileged operations are exported by Core. This package grants no compositor
capabilities and exposes no generated Wayland handles.

System services, Wayland effects, and narrowly scoped compositor integration
remain future modules. Their migration requires explicit public APIs,
dependency and capability reviews, and independent-consumer tests; it must not
expand Core's dependency surface implicitly.

For M1, first separate the `Astrea.System` QML registration from the current
backend aggregate, then publish only service models whose process owner,
transport, and runtime dependencies are explicit. Evaluate `Astrea.Effects`
as a separate optional module that describes compositor capabilities without
exporting generated Wayland objects or authentication helpers. Keep both URIs
unchanged while introducing those packages; consumers can migrate by adding
the appropriate package target rather than by gaining the modules through
`AstreaSDK::Core`.

## Verification and qualification

The focused `astrea-sdk-install-consumer-test` installs only the SDK component
to a build-tree prefix, independently configures and links the example using
`find_package(AstreaSDK CONFIG REQUIRED)`, imports `Astrea.Shared`, renders a
fixture icon through the registered provider, checks the unknown-icon provider
result and QML fallback, checks idempotent initialization and engine ownership,
then relocates the install prefix and repeats configure/build/run with
`QT_QPA_PLATFORM=offscreen`.

The existing `astrea-icon-provider-test` and `astrea-app-icon-qml-test` cover
the current implementation's icon resolution and QML rendering in the Eclipse
build. The SDK integration test qualifies the installed static-link package
with the configured local Qt toolchain and offscreen platform. It does not
qualify every Qt distribution, platform plugin, packaging format, production
compositor, or runtime deployment layout. Existing Shell and Settings tests
remain the regression checks for their consumers.

SDK-only configuration also registers `astrea-sdk-only-configure-test`. It
configures a nested SDK-only build with PkgConfig, LayerShellQt, and CXX-Qt
package discovery disabled and FetchContent disconnected. The probe build is
placed under the active CMake binary directory. SDK-only mode requires Qt
Core, Gui, Qml, Quick, and ShaderTools; Test is required only when focused
tests are enabled.
