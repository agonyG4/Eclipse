# Download Progress API Implementation Plan

> Execute inline using superpowers:executing-plans. The user prohibits subagents
> and branches and has authorized implementation of the proposed API contract.

**Goal:** Receive and inspect download progress without graphical UI.

**Architecture:** A reusable Qt Core store validates complete snapshots. A Qt
Network JSON Lines server exposes the store; a headless CLI serves and consumes
the protocol. This is a separate Downloads module, available for later integration.

**Tech Stack:** C++20, Qt6 Core/Network, Qt Test, CMake.

**Spec:** `docs/superpowers/specs/2026-10-19-download-progress-api-design.md`

## Global constraints

Work on main, no subagents. Preserve unrelated changes. Reuse the verified
`/mnt/Aether/Desktop/GitHub/Eclipse-build` tree for all generated output.
The first release is API/terminal only; browser capture is not implied.

## Review focus

- Unknown totals must not fabricate a percentage.
- Invalid updates must not destroy a previous valid record.
- Source identities must prevent browser/profile ID collisions.
- Concurrent service startup must not replace the active socket.
- Slow watchers and oversized frames must have bounded memory use.

## Task 1: Store, protocol and terminal executable

Files: new `Downloads/{CMakeLists.txt,core/DownloadStore.*,platform/DownloadServer.*,
app/main.cpp,tests/DownloadProgressTest.cpp}`, root `CMakeLists.txt`.

Interfaces: `DownloadStore::update(QJsonObject) -> QJsonObject`,
`list() -> QJsonArray`, `remove(QString, QString) -> QJsonObject`;
`DownloadServer::listen(QString, QString*) -> bool`.

- [ ] Write Qt tests for the contract and observe the missing implementation fail.
- [ ] Implement store validation, derived progress and bounded retention.
- [ ] Implement framed local IPC, watchers, exclusive startup and CLI consumers.
- [ ] Build incrementally in the verified default Aether tree.
- [ ] Run focused tests, CLI integration and review the diff.
- [ ] Document usage and limitations, commit feature files on main.

## Execution notes

The design was approved through the user's instruction to implement following the
proposed API contract. Execute without an additional approval round. No model
delegation: the user's no-subagent rule applies; this session cannot change its
own model. Tests focus on behavior and actual local IPC.
