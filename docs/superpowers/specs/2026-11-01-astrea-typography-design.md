# Astrea Typography System

Approved by the user on 2026-11-01; implementation proceeds inline on main without subagents.

## Contract
Borealis owns semantic Display, Title, Body, Label, Caption and Mono roles. Each resolves family, pixel size, Qt weight, letter spacing and proportional line height. Initial semantic sizes are 24, 18, 14, 13, 11 and 13 px. Existing size tokens keep their default metrics as compatibility aliases; migration chooses roles explicitly rather than resizing every control at once.

A shared resolver combines Borealis defaults, theme typography and user overrides. The primary family is inherited by Display and Body unless explicitly overridden. A user primary-family override also replaces theme-specific Display/Body families unless the user explicitly overrides those roles. Mono is independent. Fontconfig via Qt supplies platform fallback. Missing installed families remain stored so temporarily unavailable fonts do not erase preferences.

Typography scale is 75–200%, independent of icons, radii and spacing. Theme roles may customize family, size, weight, letter spacing and line height. Invalid values are ignored. Themes remain unmodified. Legacy tokens and font_preferences remain readable.

## Ownership and persistence
ThemeController publishes resolved tokens for all applications and watches changes. A focused shared typography module owns resolution and validation. User mutations use atomic, locked, per-field transactions in a sibling typography.json, preserving unrelated and concurrent keys. Legacy theme.json font_preferences are imported on the first typography write, and theme.json remains otherwise untouched. Reset writes an empty preferences object to suppress reimport. Failed writes report an error without publishing unpersisted state. Malformed documents are not overwritten.

## Presentation and migration
Shared semantic QML text components expose resolved fonts and line metrics. Settings Typography offers live preview, primary and mono family, scale and reset. Display/Body and individual semantic metrics are advanced options. Preserve the existing Settings visual language and navigation contracts. Migrate Settings text wrappers, text-only fixed sizes, Spotlight, Shell/Bar, Dock text, AltTab and other present application consumers. Preserve glyph/icon sizing and demo typography where intentional. Explorer is migrated only if a source application exists in this checkout.

## Validation
Tests cover theme/user precedence, inheritance, independent mono, scale and icon independence, invalid values, persistence/reset, unavailable fonts, concurrent controller edits, malformed/write failures, file watcher propagation and actual QML consumers. Build and test incrementally in /mnt/Aether/Desktop/GitHub/Eclipse-build with CARGO_TARGET_DIR explicitly on Aether. Do not build a local Release or alter user settings. Commit only typography work, preserving unrelated staged and unstaged changes.
