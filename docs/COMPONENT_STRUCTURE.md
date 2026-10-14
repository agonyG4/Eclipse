# Eclipse Component Structure

Eclipse components may use the directories that fit their responsibilities. This is a guideline for keeping ownership clear, not a required framework.

> A component does not need every directory.

Create a directory when the component has work that belongs there. Do not add empty directories for symmetry.

## Responsibilities

| Directory | Responsibility |
| --- | --- |
| `app/` | Optional standalone process entrypoint and top-level process composition. A component hosted by another process may not need it. |
| `core/` | Feature-owned models, controllers, state machines, pure policies, and platform-independent calculations. Core must not depend outward on platform implementations. |
| `services/` | Long-lived feature services and watchers that are not tied to one platform backend. Services may depend on core and shared foundations. Keep arbitrary helper classes out of this directory. |
| `platform/<backend>/` | Adapters for operating systems, compositors, protocols, IPC transports, native APIs, Rust bridges, Wayland, Hyprland, Typhon, and similar environments. Adapters may depend inward on core and services; core must not depend back on them. |
| `qml/` | Presentation and reusable declarative UI components. QML consumes the feature-facing QObject and model surface supplied by the appropriate runtime or composition layer. |
| `tests/` | Tests arranged around production ownership boundaries. Core tests should not require a platform target unless they are explicitly integration tests. |
| `cmake/` | Optional component-specific CMake helpers. |
| `assets/` | Optional component-owned images, fonts, and other assets. |
| `docs/` | Optional component documentation. |
| `packaging/` | Optional installation and distribution files. |
| `backend/` | Optional backend implementation, such as a Rust crate or another separately organized runtime. |

The preferred dependency direction is:

```text
core and shared foundations
            ↑
         services
            ↑
   platform adapters
            ↑
app / Shell composition root
```

Each layer may depend on the layer below it. The composition root wires feature components and platform adapters together. QML receives the feature-facing objects through that runtime composition.

Filesystem layers and CMake targets are related, but they do not need to map one-to-one. For example, a small `services/` directory may reasonably compile into the component's core library instead of creating a one-file library.

Sibling product components should not reach into one another's concrete controllers just because their folders are nearby. Compose cross-component behavior in the owning composition root or define a deliberately shared contract when one is needed. Avoid global service locators and global event buses.

## Bar: first cleaned-up example

Bar is the first component organized around these boundaries:

```text
Bar/
├── core/                 # controllers, models, layout metrics and pure layout policy
├── services/             # BarClockService
├── platform/wayland/     # Layer Shell policy, surfaces, bundles and lifecycle manager
├── qml/                  # root surfaces and reusable components
└── tests/                # core, Wayland, QML and architecture checks
```

`astrea-bar-core` owns Bar core and service sources. `astrea-bar-wayland` owns the Wayland adapter and depends on `astrea-bar-core`. This is an example of the intended dependency direction, not a claim that other Eclipse components already follow the same structure.

## Scope

Use the structure to make existing ownership and dependency direction visible. It does not require splitting large files, creating a library for every directory, or adding abstractions without a real boundary to protect.
