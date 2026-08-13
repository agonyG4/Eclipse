# Eclipse M7-F.1 Hosted CI Correctness and Security Closure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close the hosted-CI Qt, action-pinning, workflow-policy, checkout-credential, and commit-range whitespace gaps while preserving M7-F’s deterministic quality-gate architecture.

**Architecture:** Keep `CMakePresets.json` and `tools/ci/*` as the native quality-gate sources of truth. Replace the workflow’s ad-hoc security regex with a standard-library policy checker, replace empty-worktree whitespace checking with a range-aware script, and keep GitHub Actions as thin orchestration.

**Tech Stack:** Bash, Python 3 standard library, Git, GitHub Actions YAML, CMake presets, existing Rust/QML/CMake gates.

## Global Constraints

- Work only in Eclipse and preserve the current `main` history; do not reset, rebase, amend, clean, force-push, or discard unrelated changes.
- `CMakePresets.json` remains the native matrix source of truth.
- `QT_VERSION` remains `6.8.3`.
- `jurplel/install-qt-action` remains v4.3.1 at `48d3ad6db93f3627c8ee7a0454bc6f3744f7e730`.
- `actions/checkout` remains v7.0.1 at `3d3c42e5aac5ba805825da76410c181273ba90b1` and every checkout sets `persist-credentials: false`.
- `actions/upload-artifact` becomes v7.0.1 at `043fb46d1a93c77aae656e7c1c64a875d1fc6a0a`.
- Qt installer `modules:` contains exactly `qt5compat qtshadertools`.
- No PyYAML, network-dependent tests, CI caches, production feature changes, Typhon source changes, or M7-G work.
- M7-F’s serial CTest, sanitizer, Typhon/no-Typhon, Rust, QML, JUnit, runtime-isolation, and `CI / required` contracts remain unchanged.

---

### Task 1: Record the approved hosted-CI closure design

**Files:**
- Create: `docs/superpowers/specs/2026-08-13-m7-f1-hosted-ci-closure-design.md`
- Create: `docs/superpowers/plans/2026-08-13-m7-f1-hosted-ci-closure.md`

**Interfaces:**
- Produces: the scoped design and the executable task breakdown used by the remaining tasks.

- [ ] **Step 1: Document the preserved M7-F architecture and four corrections**

  State the exact Qt module inputs, immutable action versions, policy-checker rules, range-aware whitespace modes, and hosted-evidence boundary. Do not rewrite M7-F as invalid or introduce a second build matrix.

- [ ] **Step 2: Self-review the documents**

  Confirm the documents contain no placeholder markers, unresolved decision, duplicate source of truth, or production scope expansion.

### Task 2: Implement the workflow policy checker with TDD

**Files:**
- Create: `tools/ci/tests/test_check_workflow_policy.py`
- Create: `tools/ci/check-workflow-policy.py`

**Interfaces:**
- Consumes: a workflow path from `check-workflow-policy.py WORKFLOW`.
- Produces: exit 0 for valid policy, exit 1 for policy/malformed relevant structure, exit 2 for CLI usage errors.

- [ ] **Step 1: Write tests before the checker**

  Use `tempfile.TemporaryDirectory()` and temporary workflow files. Cover the intended policy, active `pull_request_target`, `contents: write`, `write-all`, job-level permissions, `@main`, `@master`, `@latest`, `@v7`, semantic-version refs, short SHAs, secrets expressions, full 40-character SHAs, comment-only forbidden text, and local `./` actions.

- [ ] **Step 2: Run the focused tests and observe the expected red failure**

  Run `python3 -m unittest tools/ci/tests/test_check_workflow_policy.py`. It must fail because `tools/ci/check-workflow-policy.py` does not yet exist.

- [ ] **Step 3: Implement the smallest constrained parser**

  Strip comments without treating comment text as active YAML, identify the active top-level `on` block and reject `pull_request_target`, require exactly one top-level `permissions` mapping with only `contents: read`, reject nested permission blocks, reject active `secrets.` references, accept `uses: ./...`, and require external `uses:` refs to match a 40-hex SHA.

- [ ] **Step 4: Run the focused tests until green**

  Run the same unittest command and confirm all policy cases pass with clear violation messages.

- [ ] **Step 5: Run the complete helper suite**

  Run `python3 -m unittest discover -s tools/ci/tests -p 'test_*.py'` and confirm both existing JUnit tests and policy tests pass.

### Task 3: Implement and test range-aware whitespace validation

**Files:**
- Create: `tools/ci/check-git-whitespace.sh`
- Create: `tools/ci/tests/test_check_git_whitespace.py`

**Interfaces:**
- Consumes: `pr BASE HEAD`, `push BEFORE HEAD`, or `commit COMMIT`.
- Produces: exit 0 for clean committed changes, exit 1 for Git whitespace errors, exit 2 for invalid mode/arguments.

- [ ] **Step 1: Write isolated temporary-repository tests**

  Initialize temporary repositories, configure temporary `user.name` and `user.email`, create clean and trailing-whitespace commits, test clean ranges, failing ranges, PR three-dot merge-base behavior, all-zero push fallback, commit mode, and invalid CLI usage.

- [ ] **Step 2: Run the focused tests and observe the expected red failure**

  Run `python3 -m unittest tools/ci/tests/test_check_git_whitespace.py`. It must fail because the shell checker does not yet exist.

- [ ] **Step 3: Implement strict Git range dispatch**

  Use `set -euo pipefail`, validate exact argument counts, call `git diff --check BASE...HEAD` for PRs, `git diff --check BEFORE..HEAD` for normal pushes, use commit checking for all-zero pushes and commit mode, and return 2 for all invalid invocations.

- [ ] **Step 4: Run the focused tests until green**

  Confirm each temporary-repository scenario exercises the intended range and exit status.

### Task 4: Close workflow bootstrap and policy integration

**Files:**
- Modify: `.github/workflows/ci.yml`
- Modify: `docs/CI.md`

**Interfaces:**
- Consumes: `check-workflow-policy.py` and `check-git-whitespace.sh` from Tasks 2–3.
- Produces: a thin, read-only workflow that runs the same M7-F jobs and validates actual committed ranges.

- [ ] **Step 1: Harden every checkout**

  Add `persist-credentials: false` alongside `fetch-depth: 0` in the policy checkout and alongside the existing checkout steps in Rust, QML, and CMake jobs.

- [ ] **Step 2: Correct Qt installer modules and post-install checks**

  Replace `modules: qtdeclarative qt5compat qtshadertools qttools` with `modules: qt5compat qtshadertools` in both Qt installation sites. Preserve Qt 6.8.3 and the pinned Qt action. Extend version verification to `command -v qmllint`.

- [ ] **Step 3: Upgrade diagnostics upload**

  Replace the v4.6.2 upload action with `# actions/upload-artifact v7.0.1` and `actions/upload-artifact@043fb46d1a93c77aae656e7c1c64a875d1fc6a0a`, preserving failure-only conditions, diagnostic paths, five-day retention, and no successful binary artifacts.

- [ ] **Step 4: Replace inline policy and empty diff checks**

  Remove the shell regex block and invoke `python3 tools/ci/check-workflow-policy.py .github/workflows/ci.yml`. Add policy-job environment values for the event and commit context, then dispatch `pr`, `push`, or `commit` to `tools/ci/check-git-whitespace.sh` without duplicating the range logic in YAML.

- [ ] **Step 5: Update operational documentation**

  Document the corrected Qt base-plus-add-ons contract, policy checker tests, range-aware whitespace modes, current artifact action, and disabled checkout credential persistence. Keep hosted status claims conditional on an actual hosted run.

### Task 5: Qualify, review, and commit repository-side changes

**Files:**
- Create: `docs/superpowers/qualifications/2026-08-13-m7-f1-hosted-ci-closure.md`

**Interfaces:**
- Consumes: all repository changes and local/hosted verification evidence.
- Produces: an English qualification record with actual results and blockers.

- [ ] **Step 1: Run static and local checks**

  Run `bash -n tools/ci/*.sh`, the complete helper suite, `cmake --list-presets=all`, the policy checker on the real workflow, actionlint if available, `git diff --check`, and the existing Rust/QML/native gates when the environment supports them.

- [ ] **Step 2: Run the range checker against the repository**

  After the implementation commit exists, run PR-style `tools/ci/check-git-whitespace.sh pr 8e56432 HEAD` or the current reviewed base range, plus commit mode for `HEAD`, and record the exact result.

- [ ] **Step 3: Review scope and history**

  Confirm no Typhon source, production behavior, CMake preset, or M7-F gate contract changed. Run `git diff --check`, `git log --check 8e56432..HEAD`, inspect `git status --short`, and review the full diff.

- [ ] **Step 4: Commit coherent changes**

  Use focused commits such as `test(ci): specify hosted workflow policy`, `fix(ci): close hosted quality gate gaps`, and `docs(ci): qualify M7-F.1`; do not leave intended changes uncommitted.

### Task 6: Attempt hosted verification through the normal repository flow

**Files:**
- No additional repository files; observe the pushed workflow.

**Interfaces:**
- Consumes: the committed workflow and configured `origin` remote.
- Produces: actual GitHub job/run evidence or a concrete authentication/branch-policy blocker.

- [ ] **Step 1: Check authentication without mutation**

  Inspect `gh auth status` or the configured SSH authentication and the remote before pushing.

- [ ] **Step 2: Push normally if authorized**

  Push through the repository’s configured normal flow without bypassing branch protection or force-pushing.

- [ ] **Step 3: Observe the first CI run**

  Confirm policy, Rust, QML, all six CMake matrix members, and `required` execute; diagnose deterministic failures before any rerun.

- [ ] **Step 4: Record hosted outcome**

  Record run ID/URL and every job result if hosted execution occurs. If credentials or branch policy prevents it, record the exact blocker and end with `M7-F.1 PARTIAL`.
