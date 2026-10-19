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

- [x] Write Qt tests for the contract and observe the missing implementation fail.
- [x] Implement store validation, derived progress and bounded retention.
- [x] Implement framed local IPC, watchers, exclusive startup and CLI consumers.
- [x] Build incrementally in the verified default Aether tree.
- [x] Run focused tests, CLI integration and review the diff.
- [x] Document usage and limitations, commit feature files on main.

## Execution notes

The design was approved through the user's instruction to implement following the
proposed API contract. Execute without an additional approval round. No model
delegation: the user's no-subagent rule applies; this session cannot change its
own model. Tests focus on behavior and actual local IPC.

## Validation and rulings

- Initial configuration failed on the intentionally missing implementation files.
- The first compilation was blocked by Aether exhaustion; source syntax checks
  passed without generating output. Space subsequently became available, and the
  same default build tree was reused. No existing build trees were removed.
- Ruling: bound retained serialized metadata to 1 MiB and outgoing queues to 2 MiB.
  A 256 KiB outgoing limit would reject valid full snapshots; the updated bounds
  allow a snapshot while still disconnecting slow watchers.
- Review found that stale-socket removal must reject regular files and symlinks.
  The regression test failed before the fix, then passed after `lstat` validation.
- Final build: `astrea-downloads` and `download-progress-test`, default Aether tree.
- Final check: `ctest --test-dir /mnt/Aether/Desktop/GitHub/Eclipse-build -R '^download-' --output-on-failure`:
  2/2 passed, including 8 Qt behavioral cases and the subprocess CLI integration.
- Reviewed the source directly: new Downloads files were not yet tracked by the
  graph. No independent agent review, per the user's no-subagent rule.
