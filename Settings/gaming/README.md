# Native gaming settings

`astrea_gaming` contains the shared Rust domain used by Settings through CXX-Qt
and by the `astrea-gaming` executable. QML only presents state and requests changes.

The service preserves the legacy JSON files under
`$XDG_CONFIG_HOME/AstreaOS/{gaming,system}` (or `$HOME/.config`). Reads do not
create files, apply profiles, install wrappers, or modify MIME associations.
Writes preserve unknown fields, serialize updates with a file lock, and replace
files atomically. Invalid documents are reported without replacing their contents.

## Launching

CMake builds `astrea-gaming` alongside `astrea-settings` and installs both into
`bin`. Copy the exact Steam launch option shown on the Proton page:

```text
astrea-gaming %command%
```

The UI uses the absolute sibling executable when running from a build tree.
The launcher reads settings at each launch. It applies configured environment
variables and available GameMode, MangoHud and Gamescope wrappers without shell
execution. Arguments and process exit status are preserved. The global
Performance GameMode preference gates the per-game option.

For standalone Windows executables:

```text
astrea-gaming --compat /path/to/game.exe [arguments...]
astrea-gaming --dry-run --compat /path/to/game.exe
```

Compatibility uses Proton, Wine, or automatic selection (Proton first). Its
Proton-profile option inherits runtime wrappers, flags, custom environment and
prefix arguments. Disabling that option selects Compatibility's own options.
Prefixes are created only for actual compatibility launches under
`$XDG_DATA_HOME/AstreaOS/windows-prefixes/shared`, with the usual HOME fallback.
`--dry-run` prints a JSON launch plan without creating prefixes or starting games.
No desktop association is changed; use `--compat` explicitly.

## Performance

Profile and scheduler changes are explicit actions using `powerprofilesctl` and
`scxctl` through bounded argv-only commands. Preferences and observed host state
are separate. Missing helpers disable actions. Refreshing a page never applies
a saved profile automatically. Helper failures and persistence failures are
reported to the page, and blocking operations run on dedicated workers.

## Verification

Use the repository's verified Aether build tree for all output:

```sh
CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-build/cargo/build \
TMPDIR=/mnt/Aether/Desktop/GitHub/Eclipse-build/tmp \
cargo test --manifest-path Settings/gaming/Cargo.toml --offline
```

The domain tests isolate configuration and exercise invalid input, atomic
updates, concurrent writes, legacy schema migration, literal argv, wrapper
inheritance, timeout handling, and native launcher exit semantics. The
`settings-gaming-qml-test` CMake target verifies native state bindings,
persistence, helper capability gating, and error feedback with fake executables.
