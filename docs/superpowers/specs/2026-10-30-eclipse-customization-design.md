# Eclipse customization and maintainability

The user approved implementing the architectural review and explicitly requested Luna implementers. Work on main, preserve all pre-existing working-tree changes, and commit only this work. No new branch or worktree.

## Outcomes

1. One validated theme token vocabulary supplies colors, typography, spacing, radii and animation to Settings and Shell visuals. Existing defaults and behavior remain when no package is selected. Existing schema-1 manifests keep working; invalid/unknown values are ignored. Packages remain data-only unless executable QML is explicitly opted into.
2. Bar, Dock, Spotlight, AltTab and context menus consume the common visual policy. Layout and compositor ownership remain in native controllers. Editing the selected manifest updates the running theme, including atomic replacement.
3. SDK-only configuration builds and installs AstreaSDK::Core without probing/fetching Rust/CXX-Qt, PipeWire, Wayland or LayerShellQt. Astrea.Shared contains the reusable visual theme API; internal service/capability APIs remain private. Validate installation and relocation with the independent consumer.
4. External QML customization uses an explicit opt-in, versioned component contract, validated package-local URLs and fallback to built-in content on load or contract failure. Keep native windows, input policy and controller ownership internal. Document executable QML as trusted application code, not a sandbox. Deliver a working sample and behavior tests.
5. Reduce concrete architectural pressure: move Typhon shortcut translation out of AltTab core, extract pure display calculations from the large Settings page, and extract screenshot-surface composition from the Shell application where it can preserve lifecycle behavior. Preserve the Settings navigation API and current visuals.
6. CMake presets use stable Aether output paths. Default Debug is /mnt/Aether/Desktop/GitHub/Eclipse-build; materially incompatible configurations have distinct stable trees.

## Verification

All compilation, tests, generated output, temporary files and caches use /mnt/Aether/Desktop/GitHub. Verify each effective output path before building; set TMPDIR and CARGO_TARGET_DIR explicitly. Reuse the existing /mnt/Aether/Desktop/GitHub/Eclipse-target Cargo output. No deployment, runtime theme activation or system service mutation. Run focused behavioral tests, relevant architecture guards, SDK consumer/relocation, and affected QML smoke tests. Report environment blockers precisely.

## Execution

Implement in bounded sequential tasks with Luna workers and fresh review gates. The parent owns integration decisions and final validation. Prioritize meaningful behavior tests over source-shape assertions. Changes to already dirty files must be staged as task-only patches or avoided.
