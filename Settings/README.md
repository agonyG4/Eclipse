# Astrea Settings Foundation

`astrea-settings` is the native Qt 6 Settings shell for Eclipse. This delivery
provides the application lifecycle, model-owned navigation, the legacy visual
shell, static theme tokens, placeholder content, and reusable page-agnostic
controls.

## Build

From the Eclipse checkout:

```bash
cmake -S . -B build-settings-foundation -G Ninja \
  -DASTREA_BUILD_TESTS=ON \
  -DASTREA_SETTINGS_BUILD_TESTS=ON
cmake --build build-settings-foundation --target \
  astrea-settings settings-controller-test
ctest --test-dir build-settings-foundation --output-on-failure \
  -R '^settings-'
```

The installed executable is `bin/astrea-settings`; the desktop entry is
installed under `share/applications`.

## Scope

The foundation includes:

- a normal frameless Wayland toplevel with native window actions;
- C++ navigation selection and case-insensitive filtering;
- a profile header with an optional AccountsService avatar and initials fallback;
- the expanded and collapsed legacy sidebar composition;
- static dark/light palette values and stable visual tokens;
- an intentionally empty content region;
- reusable controls for future page designs.

The foundation intentionally excludes concrete settings pages, page routing,
persistence, system or service backends, compositor controls, shell commands,
Quickshell, LayerShellQt, and Typhon-private protocols.

See `docs/ARCHITECTURE.md` for the public controller boundary and
`docs/TESTING.md` for verification procedures.
