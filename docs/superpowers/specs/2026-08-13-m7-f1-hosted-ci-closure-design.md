# Eclipse M7-F.1 Hosted CI Correctness and Security Closure Design

## Status

Approved by the M7-F.1 request. This document records the mechanical hosted-CI corrections and is not a new design gate.

## Goal

Close the four post-M7-F hosted-CI findings without changing the established quality-gate architecture or Eclipse production behavior.

## Preserved architecture

M7-F remains the source of truth for the native matrix and executable gates:

```text
CMakePresets.json -> tools/ci/* -> .github/workflows/ci.yml -> CI / required
```

M7-F.1 changes only workflow bootstrap, workflow policy validation, commit-range whitespace validation, and their documentation/tests. The CMake presets, CTest serialism, Typhon concrete-target proof, no-Typhon skip contract, Rust gate, QML gate, sanitizer gates, runtime isolation, and required aggregator remain intact.

## Hosted corrections

### Qt installer inputs

The pinned `jurplel/install-qt-action` v4.3.1 remains at commit `48d3ad6db93f3627c8ee7a0454bc6f3744f7e730` and Qt remains explicitly pinned to 6.8.3. Both Qt installation sites request only the additional modules `qt5compat qtshadertools`; base Qt archives provide the standard Qt modules and tools used by Eclipse. Each site verifies the installed version and checks that `qmllint` is on `PATH`.

### Artifact action and checkout credentials

Diagnostics continue to upload only on failed CMake matrix jobs with five-day retention and the existing LastTest.log, JUnit, and CMakeCache paths. `actions/upload-artifact` is upgraded to v7.0.1 at immutable commit `043fb46d1a93c77aae656e7c1c64a875d1fc6a0a`. Every checkout keeps v7.0.1 at `3d3c42e5aac5ba805825da76410c181273ba90b1` and sets `persist-credentials: false`.

### Dedicated workflow policy checker

`tools/ci/check-workflow-policy.py` uses only the Python standard library and a constrained line-oriented parser for the relevant workflow structure. It ignores comments, requires exactly one top-level `permissions` mapping containing only `contents: read`, rejects job-level permission overrides, rejects an active `pull_request_target` event, rejects active `secrets.` references, requires every external `uses:` reference to end in a full 40-character hexadecimal commit SHA, and allows local `./` actions. It returns 0 for valid policy, 1 for a policy or relevant-structure violation, and 2 for CLI usage errors.

The policy job invokes the checker directly. No duplicate shell regex policy remains in workflow YAML.

### Range-aware whitespace checker

`tools/ci/check-git-whitespace.sh` validates committed changes rather than an empty post-checkout worktree:

- `pr BASE HEAD` runs `git diff --check BASE...HEAD`.
- `push BEFORE HEAD` runs `git diff --check BEFORE..HEAD`, or checks `HEAD` itself when `BEFORE` is GitHub's all-zero new-ref SHA.
- `commit COMMIT` runs Git commit whitespace checking for the selected commit.

Invalid modes and argument counts return 2. The policy job passes event context through environment variables and dispatches only the appropriate one of these three modes.

## Testing and evidence

The workflow policy checker has standard-library unit tests covering the valid workflow, forbidden event, permission forms, job overrides, mutable and immutable action refs, secrets, comments, and local actions. The whitespace checker has isolated temporary-repository tests covering clean and trailing-whitespace ranges, PR merge-base behavior, zero-before push fallback, commit mode, and invalid CLI usage.

Local validation covers the helper suite, shell syntax, policy checker, range checker, CMake preset listing, actionlint when available, and the existing M7-F Rust/QML/native gates when the toolchain is available. Hosted status is recorded separately and is not inferred from local simulation.
