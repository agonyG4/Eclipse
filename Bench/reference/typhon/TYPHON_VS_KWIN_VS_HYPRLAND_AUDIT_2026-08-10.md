# Typhon vs KWin vs Hyprland — Exhaustive Architecture, Features, and Optimization Audit

**Audit date:** 2026-08-10  
**Typhon snapshot:** `Typhon(20260810-120657).zip`  
**Typhon HEAD:** `d307e09715bef585fa34b2051b8f999b2e96469c` — `fix(compositor): stabilize native window geometry`  
**KWin snapshot:** `kwin-master(1)(5).zip`  
**Hyprland snapshot:** `Hyprland-main(1)(5).zip`

## 1. Scope and methodology

This report compares the provided source snapshots as implementations, not as marketing feature lists.

The audit inspected the native output path, KMS architecture, frame scheduling, buffering, rendering, explicit synchronization, Direct Scanout, hardware planes, XWayland/XWM, input, protocol globals, window management, process supervision, session lifecycle, observability, test structure, and current feature gates.

Important methodology rules:

- A protocol or feature is counted as **active only when it is actually advertised/wired in the product path**, not merely because generated bindings, enums, tests, or foundation code exist.
- A feature marked **experimental/gated** is implemented but intentionally disabled by default or restricted to a qualification mode.
- A feature marked **foundation only** has model/adapter/protocol code but is explicitly not connected end to end.
- For KWin, the provided tree contains the DRM/KMS implementation directly, so low-level display behavior can be inspected in detail.
- Hyprland delegates a large part of low-level DRM/KMS/output ownership to **Aquamarine**. Aquamarine is not part of the provided archive. Therefore, some Hyprland backend details can only be classified as **delegated**, not fairly compared line by line with Typhon/KWin.
- No runtime FPS, latency, power, VRR, or hardware benchmark was executed. Statements such as “better” refer to architecture, completeness, explicitness, or source-level maturity unless explicitly stated otherwise.
- The current environment does not have the Rust/Cargo toolchain, so the exact Typhon HEAD could not be independently compiled or have its Cargo test suite executed here.
- `bin/check-source-layout` passes for the current Typhon source.
- The Typhon archive contains Git metadata. The KWin and Hyprland archives do not contain usable Git history, so exact upstream commits cannot be identified from the provided files alone.

## 2. Executive verdict

Typhon is no longer comparable to the July snapshot as a small compositor with mostly future-facing scaffolding. The current source has become a large native compositor platform with a real XWayland/XWM service, an explicit atomic output transaction model, KMS commit worker, adaptive triple-buffering model, explicit synchronization machinery, native Direct Scanout, strong process/session ownership, a strict control plane, and unusually dense deterministic testing.

However, Typhon is still much narrower than KWin and Hyprland as a complete desktop compositor.

The important distinction is:

- **Typhon is strong in correctness-oriented architecture around a narrow native path.**
- **KWin is strongest in complete display-system breadth and production hardware maturity.**
- **Hyprland is strongest in modern desktop/tiling behavior and broad protocol coverage while delegating much of the native backend to Aquamarine.**

Typhon's current largest gaps are not “basic compositor mechanics.” They are now:

1. real multi-output and hotplug architecture;
2. active VRR and tearing;
3. complete XWayland clipboard/PRIMARY/XDND/RandR/cursor integration;
4. broad Wayland protocol coverage for a modern desktop;
5. color management/HDR;
6. touch/gesture/tablet/IME/virtual-input coverage;
7. advanced output management, capture, session lock, DRM leasing, and multi-GPU;
8. generalized hardware overlay/underlay plane composition;
9. restoring continuous integration and cleaning up documentation drift;
10. completing real hardware qualification for the sophisticated paths already implemented.

For a **single-monitor, Wayland-first, gaming-oriented AstreaOS compositor**, Typhon is much closer to replacing Hyprland than its protocol-count gap initially suggests. For a **general-purpose KDE-class compositor**, KWin remains several architectural generations ahead in output breadth, color, multi-GPU, hardware planes, and compatibility.

---

# 3. Side-by-side capability matrix

Legend:

- **Active** — source shows an end-to-end product path.
- **Deep/active** — mature and broad implementation in the supplied source.
- **Experimental** — real path exists but is deliberately gated or not production-qualified.
- **Implemented, gated** — implementation exists but current capability construction does not advertise/activate it.
- **Foundation only** — explicit source/docs say the path is not connected end to end.
- **Delegated** — primarily implemented by an external backend not included in the archive.
- **Not found** — no working implementation was found in the supplied snapshot.

| Area | KWin | Hyprland | Typhon | Comparative result |
|---|---|---|---|---|
| Native DRM/KMS ownership | Deep/active | Delegated to Aquamarine | Deep/active, owned in-tree | KWin broadest; Typhon most directly comparable |
| Atomic KMS | Deep/active | Delegated | Active | KWin maturity lead |
| Legacy KMS fallback | Active | Delegated | Active | KWin/Typhon |
| KMS commit worker/thread | Active production commit thread | Delegated | Experimental/default-off worker with explicit phase model | KWin production maturity; Typhon diagnostics/modeling strength |
| Output transaction modeling | Deep | Backend-driven | Very explicit typed transaction/ownership model | Typhon is unusually explicit |
| Double/triple buffering | Adaptive, production | New third-frame scheduler exists, default-off | Adaptive `auto` by default when capability allows | KWin maturity; Typhon predictor depth |
| Triple-buffer risk predictor | Render journal + safety margin + hysteresis | Primarily missed-frame trigger | EWMA + deviation + p90/p95 wake/submit/worker/ioctl/slip metrics | Typhon strongest instrumentation |
| Presentation feedback | Deep/active | Active | Deep/active | All strong |
| Explicit sync (`linux-drm-syncobj`) | Deep/active | Active if backend supports it | Deep/active with detailed commit ownership | KWin mature; Typhon unusually formal |
| Buffer release ownership | Mature | Active | Explicit commit/supersede/release modeling | Typhon architecture strength |
| Damage tracking | Deep/active | Active | Deep/active | KWin breadth; Typhon diagnostics |
| Buffer age / partial repaint | Deep/active | Active/backend-assisted | Active | All |
| Direct Scanout | Deep/active | Active | Experimental, default-off | KWin maturity; Typhon conservative validation |
| Direct Scanout TEST_ONLY validation | Active DRM validation | Tests output path | Explicit conservative key/cache contract | Typhon design strength |
| Direct Scanout device identity proof | Multi-GPU-aware | Visible source contains multi-GPU FIXME | Explicit same-physical-GPU proof | Typhon stronger than visible Hypr path |
| Hardware primary plane | Active | Delegated | Active | All |
| Hardware cursor plane | Active | Delegated | Active with software fallback | All |
| Hardware overlay planes | Active | Backend-dependent/not visible in compositor | Not assigned | KWin clear lead |
| Hardware underlay planes | Active | Not visible | Not assigned | KWin clear lead |
| Plane `zpos` composition | Active | Backend-dependent | Not active for overlays | KWin |
| VRR / Adaptive Sync | Active | Active via monitor/backend policy | Detection/planning only | KWin/Hyprland |
| Tearing / async pageflip | Active | Active | Not found | KWin/Hyprland |
| `wp_tearing_control_v1` | Active | Active | Not found | KWin/Hyprland |
| Multi-output | Deep/active | Active | Structurally single-output | Major Typhon gap |
| DRM hotplug | Deep/active | Active/delegated | Not implemented | Major Typhon gap |
| Multi-GPU | Deep/active | Backend support; DS has visible caveat | No cross-GPU architecture | KWin lead |
| Cross-GPU copy paths | Active | Delegated | Not found | KWin |
| Mode setting | Deep/active | Active/delegated | Active | All |
| Per-output layout | Deep/active | Deep/active | No real multi-output layout | KWin/Hyprland |
| Per-output scale | Deep/active | Deep/active | Single-output protocol mechanics exist | KWin/Hyprland |
| Per-output transform | Deep/active | Deep/active | Render mechanics exist; no full output policy | KWin/Hyprland |
| Color management protocol | Deep/active | Active when enabled | Implementation skeleton, product cap disabled | Major Typhon gap |
| ICC profiles | Deep/active | Partial/active color stack | Unsupported in current Typhon protocol path | KWin |
| HDR / PQ / BT.2020 | Deep/active | Active color/HDR stack | Not end-to-end | KWin/Hyprland |
| KMS color pipeline | Deep/active | Active/backend-integrated | Not comparable/end-to-end absent | KWin |
| Gamma/output color controls | Active | Active (`gamma-control`) | Not found | KWin/Hyprland |
| XWayland process lifecycle | Deep/active | Active | Managed generation-bound service, default-off | All technically real |
| Managed XWM | Deep/active | Deep/active | Active when managed XWayland mode is enabled | KWin/Hypr broader |
| ICCCM focus/configure | Deep | Deep | Implemented | All |
| EWMH | Deep | Deep | Significant subset implemented | KWin/Hypr breadth |
| Override-redirect | Active | Active | Active | All |
| X11 transient handling | Active | Active | Active | All |
| X11 Shape | Active | Active | Optional/version-gated | All |
| X11 Sync resize | Active | Active | Optional/version-gated | All |
| X11 clipboard bridge | Active | Active | Foundation only | Major Typhon XWayland gap |
| X11 PRIMARY bridge | Active | Active | Foundation only | Major Typhon XWayland gap |
| XDND bridge | Active | Active | Foundation only | Major Typhon XWayland gap |
| Runtime RandR publication | Active | Active | Foundation only | Typhon gap |
| X11 cursor ownership integration | Active | Active | Foundation only | Typhon gap |
| Wayland clipboard (`wl_data_device`) | Active | Active | Active | All |
| Primary selection | Active | Active | Implemented but current native cap disabled | Low-hanging Typhon gap |
| Data-control | Active | Active | Implemented but current native cap disabled | Low-hanging Typhon gap |
| Relative pointer | Active | Active | Active | All |
| Pointer constraints | Active | Active | Active | All |
| Pointer warp | Active/current protocol support | Active | Active | All |
| Keyboard shortcuts inhibit protocol | Active | Active | Internal behavior exists, protocol not advertised | Typhon gap |
| Idle inhibit | Active | Active | Implemented but current native cap disabled | Low-hanging Typhon gap |
| Touch input | Deep/active | Active | Physical event model not found | Major laptop/touch gap |
| Pointer gestures | Active | Active | Not found | Typhon gap |
| Tablet protocol/input | Deep/active | Active | Not found | Typhon gap |
| Text input | Deep/active | v1 + v3 | Not found | Typhon gap |
| Input method | Deep/active | v2 | Not found | Typhon gap |
| Virtual keyboard | Active | Active | Not found | Typhon gap |
| Virtual pointer | Active | Active | Not found | Typhon gap |
| Session lock | Active | Active | Not found | Major desktop/security gap |
| Security context | Active | Active | Not found | Typhon gap |
| Screencopy | Mature screenshot/screencast stack | Active | No compositor capture protocol found | Major Typhon gap |
| Image-copy-capture | Modern capture stack | Active | Not found | Typhon gap |
| DRM lease | Active | Active | Not found | VR/headset gap |
| Output management | Deep/active | Active | Not found | Multi-output prerequisite gap |
| Output power management | Active | Active | Not found | Typhon gap |
| XDG output | Active | Active | Not found | Typhon gap |
| FIFO protocol | Active | Active | Explicitly hidden/not implemented | Gaming/frame pacing gap |
| Commit timing protocol | Not found in provided KWin snapshot | Active, default enabled | Explicitly hidden/not implemented | Hyprland lead |
| Content type protocol | Active | Active | Not found | Typhon gap |
| Cursor shape protocol | Active | Active | Theme/cursor ownership exists, protocol not found | Typhon gap |
| Layer shell | Active | Active | Active | All |
| Fractional scale | Active | Active | Active | All |
| Viewporter | Active | Active | Active | All |
| Presentation protocol | Active | Active | Active | All |
| XDG activation | Active | Active | Active | All |
| XDG decoration | Active | Active | Active | All |
| Linux dmabuf | Deep/active | Active/backend-integrated | Capability-gated active | All; Typhon conservative |
| dmabuf feedback/device proof | Deep multi-GPU | Active/backend-driven | Explicit proof-gated promotion | Typhon correctness strength |
| Floating window geometry | Deep | Deep | Active and heavily tested | All |
| Move/resize interactions | Deep | Deep | Active and heavily tested | All |
| Maximize/fullscreen | Deep | Deep | Active | All |
| Minimize/restore | Deep | Active | Active | All |
| Workspaces/virtual desktops | Deep | Core strength | No comparable general subsystem found | Major Typhon desktop gap |
| Tiling | Deep tile manager | Core strength | No general tiling engine found | KWin/Hyprland |
| Window rules | Extensive | Extensive | No comparable broad rule engine found | KWin/Hyprland |
| Effects/animations | Extensive plugin/effects stack | Core strength | No comparable subsystem found | KWin/Hyprland |
| Shell-specific control | KDE/DBus ecosystem | Hyprland IPC | Strong Astrea-specific protocols | Different goals |
| Runtime CLI/control | DBus/debug tooling | `hyprctl` + event socket | `astreactl`, strict typed local socket | Typhon smaller but very robust |
| Event subscription IPC | DBus/signals | Rich event socket | No comparable broad subscription stream | KWin/Hyprland |
| Child/process supervision | Mature app/process ecosystem | Specialized runtime paths | Generic session-owned supervisor + process groups + cleanup | Typhon standout architecture |
| Session suspend/resume | Deep/active | Active/backend-driven | Explicit native recovery state | Strong Typhon area |
| KMS state restoration | Deep | Backend | Explicit atomic/legacy restore-on-drop | KWin/Typhon |
| Resource quotas | Mature systemic controls | Mixed | Many explicit local bounds, no unified client budget | Typhon good but incomplete |
| Deterministic unit/model tests | Very mature | Smaller visible in-tree unit footprint | Extremely dense test investment | Typhon standout for project age |
| Continuous integration | Active KDE CI files | Active GitHub CI | **No current workflow** | Current Typhon regression |
| Source layout enforcement | Project conventions | Project conventions | Explicit line/layout gate | Typhon strong |
| Hardware qualification | Production ecosystem | Production ecosystem | Explicitly incomplete in docs | Major Typhon readiness gap |
| Software rendering/fallback | QPainter + GL paths | Primarily GL/backend | CPU GBM/dumb scanout fallbacks | KWin broadest |
| Portal backend | KDE portal ecosystem | external portal ecosystem | Small local Settings/Notification/Access backend | Typhon portal is not capture-capable |

---

# 4. Deep comparison: rendering, scheduling, and triple buffering

## 4.1 KWin

KWin's render loop is production-oriented and comparatively compact.

Relevant sources:

- `src/core/renderloop.cpp`
- `src/backends/drm/drm_output.cpp`
- `src/core/renderjournal.*`
- DRM EGL layer/swapchain sources

For atomic DRM outputs, KWin sets the maximum pending frame count to two unless `KWIN_DRM_DISABLE_TRIPLE_BUFFERING` disables it:

- `src/backends/drm/drm_output.cpp:41-52`

Its scheduling estimate is based on:

- render journal result;
- presentation safety margin;
- an additional 1 ms scheduler/timer margin;
- a two-vblank upper bound.

The loop calculates `pageflipsInAdvance`, enters triple buffering immediately when needed, then uses hysteresis before returning to double buffering:

- `src/core/renderloop.cpp:50-84`

When VRR or tearing is active, KWin reduces the effective pending-frame count to one:

- `src/core/renderloop.cpp:253-270`

This is a major maturity point: KWin's buffering behavior is not isolated from presentation mode. Triple buffering, VRR, tearing, window timing, and output state are already one system.

### KWin strengths

- production-default integration;
- direct relationship to actual presentation modes;
- multi-output context;
- real-world driver exposure;
- simpler policy that has accumulated years of tuning.

### KWin weakness relative to Typhon's current design

KWin's source-level predictor is less diagnostically rich. It does not expose the same long list of separately modeled worker/ioctl/wake/target-slip risk terms that Typhon does.

That does **not** imply Typhon has lower latency. It means Typhon has a more explicit experimental model and more dimensions available for diagnosis.

## 4.2 Hyprland

Relevant source:

- `src/output/MonitorFrameScheduler.cpp`
- `src/config/values/ConfigValues.cpp`

Hyprland has a newer render scheduling path controlled by:

`render:new_render_scheduling`

The provided snapshot defaults it to `false`:

- `src/config/values/ConfigValues.cpp:568`

It requires explicit sync support and disables itself during Direct Scanout:

- `src/output/MonitorFrameScheduler.cpp:14-18`

The current strategy is easy to understand:

1. render normally;
2. when the render sync fires, compare elapsed render time against one refresh interval;
3. if the frame was too slow, immediately begin rendering a third frame without committing it;
4. when the previous frame presents, commit the pending third frame.

That is a practical third-buffer rescue strategy, but it is less predictive than Typhon's model. The supplied source also contains a candid lifetime FIXME around `renderMonitor()` potentially destroying the scheduler:

- `src/output/MonitorFrameScheduler.cpp:43-45`
- `src/output/MonitorFrameScheduler.cpp:118-120`

This is a specific source-level architectural rough edge, not a claim that Hyprland's scheduler is broadly unreliable.

## 4.3 Typhon

Relevant sources:

- `src/native/adaptive_buffering.rs`
- `src/native/scheduler.rs`
- `src/native/scheduler/pipeline.rs`
- `src/native_output/scanout/output_swapchain.rs`
- `src/native_output/runtime/presentation.rs`
- `src/native_output/presentation/*`

Typhon defaults `OBLIVION_ONE_TRIPLE_BUFFERING` to `auto`:

- `src/native_output/runtime/bootstrap.rs`

The adaptive capability model explicitly blocks triple buffering for conditions such as:

- non-atomic KMS;
- unavailable explicit swapchain;
- slot-capacity mismatch;
- missing primary in-fence;
- missing render-fence export;
- unhealthy submission transport;
- inactive session;
- unstable output generation;
- unsupported presentation mode;
- poisoned swapchain;
- visible software cursor.

The prediction model tracks:

- EWMA render time;
- upper positive deviation;
- p90 recent render time;
- p95 wake lateness;
- p95 atomic submit duration;
- p95 worker queue residency;
- p95 worker submit wake lateness;
- p95 atomic ioctl duration;
- submission budget;
- p95 target slip;
- safety margin;
- total predicted cost;
- idle wake guard.

Sources:

- `src/native/adaptive_buffering.rs:152-208`

Typhon also distinguishes why it entered triple buffering:

- predicted deadline pressure;
- proven render/readiness miss;
- proven submit miss;
- proven presentation miss;
- forced validation.

The explicit atomic swapchain is modeled around exactly three slots and tracks state such as current, queued, pending, ready, rendering, suspended/quarantined ownership, and fence proof.

### Verdict on triple buffering

**Best production maturity:** KWin.  
**Best source-level instrumentation and formal prediction model:** Typhon.  
**Simplest experimental third-frame strategy:** Hyprland.

For Typhon, the next work should **not** be to make the predictor more complicated. It should be to prove that the existing predictor produces better frame pacing on real hardware and to simplify any metrics that do not materially improve decisions.

The required benchmark is per-output:

- frame-time distribution;
- missed vblanks;
- input-to-present latency;
- double↔triple transition count;
- GPU busy/idle behavior;
- worker queue residency;
- CPU cost;
- power impact.

Without that, Typhon has a sophisticated hypothesis, not a proven win.

---

# 5. KMS architecture and commit submission

## 5.1 KWin commit thread

Relevant source:

- `src/backends/drm/drm_commit_thread.cpp`
- `src/backends/drm/drm_pipeline.*`
- `src/backends/drm/drm_atomic_commit.*`

KWin has a normal production commit thread tightly integrated with the DRM pipeline.

It can:

- work with atomic commits;
- test state;
- merge/reorder appropriate commits;
- preserve plane ordering;
- coordinate VRR/tearing;
- operate inside a multi-output, multi-plane environment.

This is the strongest implementation in terms of deployed breadth.

## 5.2 Hyprland

The compositor-level source exposes monitor scheduling and output commits, but the actual DRM submission machinery belongs primarily to Aquamarine.

Therefore, a fair audit would need the exact Aquamarine revision used by this Hyprland snapshot.

Any claim that Hyprland's low-level DRM worker is “simpler” or “worse” than Typhon/KWin based only on this archive would be unjustified.

## 5.3 Typhon KMS worker

Relevant directory:

- `src/native_output/kms_worker/`

The policy supports:

- `off`
- `auto`
- `force`

Current default: **off**.

Source:

- `src/native_output/kms_worker/policy.rs:82-94`

Typhon's worker uses a highly explicit phase/state model around:

- idle;
- dequeue/predecessor wait;
- sidecar collection;
- frozen validation state;
- TEST_ONLY;
- submit ioctl;
- kernel in-flight;
- quiescing;
- shutdown quiescing;
- shutdown abandonment;
- stopped/fatal paths.

Its metrics cover, among other things:

- queue depth/state;
- queue residency;
- submit wake lateness;
- busy retries;
- ioctl timing;
- watchdog behavior;
- identity mismatch;
- quiesce/join;
- input fences;
- cursor sidecars;
- kernel in-flight state.

### Verdict

KWin wins on production integration and breadth.

Typhon's worker is one of the project's strongest architectural pieces because it makes ownership and failure stages explicit. It should be treated as a **high-value subsystem to qualify**, not rewritten.

The concern is default policy: both the KMS worker and Direct Scanout are currently off by default. This means the most advanced output pipeline is not yet the ordinary path.

---

# 6. Direct Scanout

## 6.1 KWin

KWin has the most complete Direct Scanout architecture of the three supplied source trees.

Relevant areas:

- `src/compositor.cpp`
- `src/backends/drm/drm_egl_layer.cpp`
- `src/backends/drm/drm_pipeline.cpp`
- `src/backends/drm/drm_plane.*`
- `src/scene/workspacescene.cpp`

KWin's Direct Scanout path participates in:

- color pipeline decisions;
- transform/scaling constraints;
- multi-GPU/cross-GPU behavior;
- mode changes;
- plane assignment;
- shadow buffers/fallback;
- generalized output composition.

The major advantage is that Direct Scanout is not a standalone “fullscreen optimization.” It exists inside a mature hardware-plane architecture.

## 6.2 Hyprland

Relevant source:

- `src/output/Monitor.cpp`

Hyprland's visible compositor path checks conditions such as:

- Direct Scanout configuration;
- mirroring;
- recording;
- software cursor;
- solitary candidate;
- exact geometry/transform;
- dmabuf suitability;
- color/HDR constraints.

It then sets the output buffer/presentation mode and commits through the backend.

Two source observations matter:

1. the path is practical and already integrated;
2. the provided source still contains a FIXME around validating the scanout dmabuf against the exact device/formats, explicitly warning that the path may fail badly on multi-GPU.

That makes Typhon's stricter device-identity proof a real design advantage over the visible Hyprland compositor layer.

## 6.3 Typhon

Relevant sources:

- `src/compositor/fullscreen.rs`
- `src/native_output/runtime/direct_plan.rs`
- `src/native_output/scanout/direct.rs`
- `src/native_output/scanout/atomic_direct.rs`
- `src/native_output/scanout/direct_validation.rs`
- `src/native_output/scanout/direct_lease.rs`
- `src/native_output/scanout/atomic_egl_gbm/direct.rs`
- `src/native_output/scanout/feedback_policy.rs`

Typhon's policy defaults to:

`OBLIVION_ONE_DIRECT_SCANOUT=off`

The only real opt-in mode is:

`experimental-auto`

Source:

- `src/native_output/scanout/direct_policy.rs`

The current known-issues contract explicitly restricts the candidate to a single opaque solitary fullscreen surface on one output, requiring:

- XRGB dmabuf content;
- selected DRM device match;
- exact primary-plane format/modifier support;
- identity geometry;
- identity scaling;
- identity transform;
- supported explicit synchronization;
- hardware cursor only if the complete primary+cursor assignment validates.

It explicitly excludes:

- overlay planes;
- VRR;
- tearing;
- multi-output;
- hotplug;
- scaling;
- transforms;
- color conversion;
- HDR;
- cross-device/multi-GPU;
- primary-plane blending.

Source:

- `docs/KNOWN_ISSUES.md:3-22`

The validation key binds multiple properties, including output generation, CRTC, primary plane, dimensions, format/modifier, layout identity, cursor assignment, and synchronization contract. Successful TEST_ONLY validation can be cached under the exact key.

### Verdict

**KWin:** best complete implementation.  
**Hyprland:** practical production path, but visible device-proof edge cases remain.  
**Typhon:** best conservative validation philosophy, but still an experimental narrow path.

The important optimization lesson for Typhon is **not** “make Direct Scanout accept more things immediately.” KWin shows that the real end-state is a generalized hardware-plane composition system. Typhon should expand only after multi-output, color, VRR, and synchronization invariants are per-output and stable.

---

# 7. Hardware planes

This is one of the clearest KWin wins.

## KWin

KWin supports real hardware composition involving:

- primary planes;
- cursor planes;
- overlay planes;
- underlay candidates;
- plane ordering / `zpos`.

Relevant sources:

- `src/backends/drm/drm_plane.h`
- `src/backends/drm/drm_pipeline.cpp`
- DRM plane composition paths

This allows KWin to avoid full-scene composition in scenarios that are more complex than one fullscreen surface.

## Hyprland

The visible Hyprland compositor source does not expose a comparable generalized overlay allocator. Because Aquamarine owns the native backend, this archive alone cannot establish the full backend plane capability.

## Typhon

Typhon discovers plane types, including overlays, but the current output transaction rejects unsupported overlay assignment rather than scheduling them.

In practical terms, Typhon currently has:

- primary plane;
- cursor plane;
- composition;
- conservative primary Direct Scanout.

It does **not** yet have KWin-style generalized overlay/underlay composition.

### Recommendation

Do not implement overlay scheduling before:

1. real per-output architecture;
2. hotplug-safe output generations;
3. active color pipeline;
4. VRR/tearing presentation modes;
5. cross-device policy;
6. explicit sync and release ownership are proven on hardware.

Otherwise overlay planes multiply every existing synchronization and color correctness problem.

---

# 8. VRR and tearing

This is currently one of Typhon's most important gaming gaps.

## KWin

KWin has real presentation modes that include:

- VSync;
- Adaptive Sync;
- Async;
- Adaptive Async.

The render loop changes scheduling behavior depending on VRR/tearing, and the DRM backend programs the corresponding output state.

It also has real async pageflip behavior and presentation-mode fallback.

## Hyprland

Hyprland creates the tearing-control protocol:

- `src/managers/ProtocolManager.cpp:170`

It tracks active tearing state in the monitor scheduler and uses backend presentation modes.

VRR policy is integrated into monitor/output behavior through Hyprland + Aquamarine.

## Typhon

Typhon discovers:

- connector `vrr_capable`;
- CRTC `VRR_ENABLED` property.

Relevant sources:

- `src/native_output/output/sysfs.rs`
- `src/native/kms/properties.rs`
- `src/native_output/runtime/bootstrap.rs`

No source path was found that actually writes/activates the CRTC `VRR_ENABLED` property.

No async pageflip/tearing-control implementation was found.

Therefore the accurate status is:

> **Typhon can detect and reason about VRR capability, but VRR is not currently activated. Tearing is not implemented.**

### Recommendation

Typhon should implement presentation modes as a first-class per-output state, not a boolean VRR switch.

Suggested model:

- `VSync`
- `AdaptiveSync`
- `Async`
- `AdaptiveAsync`

Then integrate that mode into:

- scheduler pending depth;
- adaptive triple buffering;
- KMS worker transaction;
- Direct Scanout eligibility;
- `wp_tearing_control_v1`;
- presentation feedback;
- frame target calculation;
- fullscreen policy.

This follows the architectural lesson from KWin: buffering and presentation mode must be one system.

---

# 9. Damage, buffer age, and partial repaint

All three compositors have serious damage systems.

## KWin

KWin's damage history and buffer-age behavior is integrated with:

- multi-output;
- EGL layers;
- cross-GPU paths;
- screencast;
- shadow buffers;
- output layers.

This breadth gives it the maturity lead.

## Hyprland

Hyprland has monitor damage rings and backend/swapchain damage behavior, with lower-level details partly delegated.

## Typhon

Relevant source:

- `src/egl_renderer/damage.rs`
- `src/compositor/render.rs`

Typhon has:

- buffer-age-aware history;
- partial repaint decisions;
- swap-with-damage/scissor handling;
- clipping/coalescing;
- explicit rejection/fallback reasons;
- software-present age handling;
- substantial testing.

This is a strong subsystem.

### Recommendation

The next performance work should be measurement-driven:

- percentage of frames promoted to full repaint;
- damage region rectangle count;
- copied bytes;
- actual GPU render duration;
- surface-tree rebuild duration;
- resize-specific damage inflation;
- scanout buffer age distribution.

Typhon already has enough policy. The risk now is adding optimization branches before proving which cost dominates.

---

# 10. Explicit synchronization

Typhon is unusually ambitious here.

## KWin

KWin has a mature `linux-drm-syncobj` implementation, including validation of acquire/release rules, buffer lifecycle integration, and output-side synchronization.

This is the production-maturity baseline.

## Hyprland

Hyprland advertises explicit sync when the backend supports it and integrates synchronization with rendering and the monitor scheduler.

Some low-level details live in Aquamarine.

## Typhon

Relevant areas:

- `src/compositor/explicit_sync.rs`
- `src/protocols/syncobj.rs`
- `src/native/explicit_sync.rs`
- `src/syncobj.rs`
- surface commit/publication state
- KMS worker input-fence paths

Typhon models:

- acquire/release points per commit;
- surface-tree dependencies;
- teardown cancellation;
- supersede/merge ownership;
- callback ownership;
- eventfd-backed waits;
- fallback polling;
- session suspend parking/rearm;
- release-point ownership;
- KMS input fences;
- explicit commit identity/publication disposition.

This is one of the areas where Typhon's source is more explicit than most small compositors.

### Risk

It is also one of the highest-complexity areas in the entire codebase.

The failure modes are serious:

- premature release;
- callback starvation;
- unpublished ready commit;
- stuck acquire;
- stale eventfd;
- buffer reuse before proof;
- merge/supersede ownership loss;
- client hang.

### Recommendation

Before adding more rendering features, build model/property tests around randomized commit sequences:

- commit A/B/C with arbitrary callbacks;
- supersede/merge combinations;
- subsurface synchronized/desynchronized trees;
- acquire pending → client disconnect;
- minimize/unmap/destroy during wait;
- VT suspend during acquire;
- pageflip generation change;
- direct-scanout fallback after TEST_ONLY failure;
- worker queue cancellation;
- buffer reuse after release.

Typhon's deterministic test culture makes it well suited to this.

---

# 11. XWayland and XWM

This area changed dramatically relative to older Typhon snapshots.

## 11.1 KWin

KWin has a complete, mature XWayland data bridge and X11 window integration.

The supplied source includes dedicated modules:

- `src/xwayland/clipboard.*`
- `src/xwayland/primary.*`
- `src/xwayland/selection.*`
- `src/xwayland/transfer.*`
- `src/xwayland/dnd.*`
- `src/xwayland/drag_x.*`
- `src/xwayland/drag_wl.*`
- `src/xwayland/databridge.*`

It handles XFixes selection ownership, transfers in both directions, XDND, focus, window management, and deep X11 integration.

This is the compatibility gold standard in this comparison.

## 11.2 Hyprland

Relevant source:

- `src/xwayland/XWM.cpp`
- `src/xwayland/Dnd.*`
- `src/xwayland/XDataSource.*`
- `src/xwayland/XSurface.*`
- `src/xwayland/Server.*`

The XWM creates and manages CLIPBOARD, PRIMARY, and XDND selection windows and connects them to the Wayland seat selection model.

This is a real end-to-end bridge, not just metadata.

## 11.3 Typhon

Authoritative current documentation:

- `docs/XWAYLAND.md`

Typhon now runs XWayland as a managed, generation-bound child.

The native reactor owns:

- displayfd;
- private Wayland socket;
- XWM socket;
- stderr ring;
- retirement tokens.

The XWM uses nonblocking `x11rb` and supports a serious set of behavior:

- normal managed windows;
- override-redirect windows;
- ICCCM input focus;
- `WM_TAKE_FOCUS`;
- configure masks;
- transient validation;
- stacking;
- `WM_STATE`;
- implemented EWMH properties;
- required Composite;
- optional/version-gated XFixes;
- Shape;
- RandR;
- Sync;
- global X11 DPI policy;
- xwayland-shell/surface association;
- generation cleanup/restart behavior.

However, `docs/XWAYLAND.md:23-35` explicitly says these are **not connected end to end**:

- XFixes CLIPBOARD bridge;
- PRIMARY bridge;
- XDND ClientMessage bridge;
- runtime RandR publication;
- X11 cursor ownership integration.

The default XWayland mode is also `off`:

- `src/xwayland/config.rs:27-38`

Managed modes exist:

- `lazy`
- `eager`

### Verdict

Typhon's **XWM core is now credible**.

The compatibility gap is the bridge layer, not basic X11 window management.

This should be one of the highest-priority daily-driver tasks because Steam/Proton/X11 applications expose these gaps immediately.

### Documentation bug

`docs/KNOWN_ISSUES.md:39-43` still says:

> X11 compatibility is not enabled and XWayland remains an architectural boundary.

That is stale relative to the current `docs/XWAYLAND.md` and current source.

This is a real documentation-consistency defect and should be fixed immediately.

---

# 12. Wayland protocol breadth

This is where simple `grep` comparisons are misleading.

Typhon contains implementation code for several protocols that the current native capability profile deliberately does **not** advertise.

Current product capability construction:

- `src/main.rs`
- `src/compositor/plan.rs`

Native input profile:

- relative pointer: enabled;
- pointer constraints: enabled;
- pointer warp: enabled;
- keyboard shortcuts inhibit: disabled;
- idle inhibit: disabled.

Native selection profile:

- clipboard: enabled;
- primary selection: disabled;
- data-control: disabled.

Renderer capability:

- color management: disabled.

Tests also explicitly assert that frame-pacing staging protocols are hidden:

- `wp_fifo_manager_v1`
- `wp_commit_timing_manager_v1`

Source:

- `src/compositor/tests/plan.rs`

## 12.1 Hyprland protocol breadth

`src/managers/ProtocolManager.cpp:161-245` creates a very broad protocol surface, including:

- seat/data/compositor/subcompositor/shm;
- viewporter;
- tearing control;
- fractional scale;
- XDG output;
- cursor shape;
- idle inhibit;
- relative pointer;
- XDG decoration;
- alpha modifier;
- gamma;
- foreign toplevel;
- pointer gestures;
- keyboard shortcuts inhibit;
- text input v1/v3;
- pointer constraints;
- output power;
- XDG activation;
- idle notifier;
- session lock;
- input method v2;
- virtual keyboard;
- virtual pointer;
- output management;
- tablet v2;
- layer shell;
- presentation;
- XDG shell;
- data control;
- primary selection;
- XWayland shell;
- global shortcuts;
- XDG dialog;
- single pixel buffer;
- security context;
- content type;
- workspace;
- pointer warp;
- FIFO;
- XDG foreign;
- capture/screencopy protocols;
- conditional commit timing;
- conditional color management;
- DRM lease.

Hyprland's commit timing protocol is enabled by default in the provided snapshot:

- `src/config/values/ConfigValues.cpp:571`

## 12.2 KWin protocol breadth

KWin's Wayland server has similarly broad coverage around:

- pointer/gesture/input protocols;
- primary/data control;
- output management;
- session/idle protocols;
- activation;
- content type;
- tearing;
- FIFO;
- color;
- DRM leasing;
- desktop/window integration;
- modern output/client features.

No `wp_commit_timing` implementation was found in the provided KWin snapshot.

## 12.3 Typhon protocol priority

Typhon should split missing protocols into three classes.

### Class A — implementation already exists, product cap disabled

These are cheap/high-value if the implementation is truly complete:

- idle inhibit;
- primary selection;
- ext data control.

They should be activated only after end-to-end tests prove the real native path.

### Class B — gaming/frame pacing

High priority for Typhon's target:

- `wp_fifo_v1`;
- `wp_commit_timing_v1`;
- `wp_tearing_control_v1`;
- content type.

These integrate directly with the scheduler and presentation model.

### Class C — full desktop compatibility

- touch;
- pointer gestures;
- tablet;
- text-input/input-method;
- virtual keyboard/pointer;
- session lock;
- security context;
- cursor shape;
- XDG output;
- output management/power;
- capture;
- DRM lease;
- gamma/color controls.

These are necessary before calling Typhon a general desktop compositor.

---

# 13. Input architecture

## KWin

KWin has the deepest input ecosystem:

- libinput device management;
- keyboard/mouse;
- touch;
- tablet;
- gestures;
- advanced device configuration;
- protocol integration;
- virtual input;
- shortcuts/inhibition;
- desktop/security integration.

## Hyprland

Hyprland also has broad physical and protocol input support, including touch, gesture, tablet, text-input, input method, virtual devices, and session-lock integration.

## Typhon

The native hardware event model found in the current source centers on:

- key;
- pointer button;
- relative/absolute pointer motion;
- pointer axis.

Typhon is strong inside this narrower model:

- exact pointer press ownership;
- implicit grab behavior;
- compositor move/resize capture;
- pointer constraints;
- lock/confine;
- relative motion;
- pointer warp;
- hardware/software cursor;
- modifier handling;
- shell shortcut routing;
- extensive regression testing.

### Verdict

Typhon's mouse/keyboard path is not primitive. It is narrow.

For a desktop PC with keyboard and mouse, that can be enough today. For laptops, drawing tablets, accessibility, touchscreens, IMEs, or remote-control software, it is a major compatibility gap.

---

# 14. Multi-output, hotplug, and multi-GPU

This is Typhon's largest architectural blocker.

## 14.1 Typhon

The current documentation explicitly states that Typhon remains single-output:

- `docs/wayland/PROTOCOL_SOURCE_MANIFEST.md:184-185`

The source has a `PhysicalOutputId`, but currently uses a single fixed identity:

- `src/compositor/state/output_membership.rs`
- `NATIVE_PHYSICAL_OUTPUT = PhysicalOutputId(0)`

This is a placeholder for deterministic membership, not a real multi-output architecture.

The runtime still fundamentally owns one:

- KMS target;
- mode;
- scanout pipeline;
- scheduler;
- presentation state;
- cursor pipeline;
- output scale/layout context.

There is no complete DRM connector hotplug model.

There is no cross-GPU composition/migration architecture.

## 14.2 Hyprland

Hyprland has a real monitor model with dynamic monitor add/remove and per-output state. The low-level connector/hotplug/GPU mechanics are largely Aquamarine-owned.

## 14.3 KWin

KWin is in another class here:

- multiple DRM GPUs;
- GPU manager;
- udev/hotplug;
- output creation/removal;
- per-output render loops;
- cross-GPU paths;
- per-output color;
- per-output scale/transform;
- output configuration;
- connector topology.

### Recommended Typhon architecture

Do not “add a second connector” to the current singleton runtime.

Introduce a real structure such as:

```text
OutputId
OutputRuntime {
    generation
    connector/crtc/plane identity
    mode
    scheduler
    adaptive buffering controller
    scanout/swapchain
    KMS worker channel/state
    cursor
    damage history
    presentation ledger
    VRR/tearing mode
    scale/transform
    color pipeline
}
DesktopLayout {
    outputs
    logical geometry
    primary output policy
}
```

Then move surface output membership from a fixed physical-output constant to a set of real `OutputId`s.

Only after that should hotplug migrate surfaces and destroy/recreate pipelines.

This refactor should happen **before** large output-specific features such as HDR, generalized overlays, or complex VRR policy, otherwise those features will have to be rewritten from singleton state later.

---

# 15. Color management and HDR

## KWin

KWin is the clear leader.

Relevant source includes:

- `src/core/colorpipeline.*`
- ICC profile handling;
- DRM color pipeline;
- HDR metadata;
- BT.2020;
- ST2084/PQ;
- output luminance metadata;
- color-aware scene rendering;
- per-output color descriptions.

Color is integrated across protocol, scene, and KMS.

## Hyprland

The provided snapshot has a substantial modern color-management path.

Examples:

- `render:cm_enabled` default true;
- auto HDR policy;
- color management protocol;
- HDR transfer-function handling;
- optional VCGT handling;
- FP16/internal rendering controls.

It is less mature/broad than KWin but substantially ahead of Typhon.

## Typhon

Typhon has color-management protocol/model code, but the production renderer capability is constructed with:

`RendererProtocolCapabilities::unsupported()`

Therefore the color global is not part of the ordinary product path.

The implementation itself is also not equivalent to KWin's complete pipeline; ICC and advanced HDR behavior are not end-to-end.

### Recommendation

Color should be implemented as a renderer/output contract, not as a protocol-first task.

Required order:

1. internal color description;
2. source surface color metadata;
3. linear/render space choice;
4. compositing transform;
5. output transform;
6. KMS properties/LUT/metadata where available;
7. protocol advertisement;
8. Direct Scanout color-equivalence test;
9. HDR fallback rules;
10. per-output policy.

---

# 16. Window management and desktop behavior

Typhon has improved substantially in ordinary floating-window behavior.

It has:

- persistent absolute geometry;
- focus/raise;
- move;
- resize;
- maximize;
- fullscreen;
- minimize/restore;
- stacking;
- X11 transient/popup logic;
- Astrea toplevel publication/control;
- detailed pointer interaction state.

Recent source/test investment around window geometry is significant.

However, KWin and Hyprland are still much broader as window managers.

## KWin

- virtual desktops;
- custom/quick tiling;
- placement;
- window rules;
- activities;
- scripts;
- effects;
- plugins;
- advanced focus/stacking policy.

## Hyprland

- dynamic tiling;
- floating;
- workspaces;
- groups;
- rules;
- animation system;
- monitor/workspace policy;
- special workspaces;
- rich dispatch/control.

## Typhon

No comparable general workspace/tiling/rules/effects architecture was found.

That may be intentional if AstreaOS keeps policy in separate shell components. If so, Typhon still needs the compositor primitives to support that policy cleanly:

- workspace identity;
- window assignment;
- output/workspace topology;
- atomic workspace changes;
- focus history;
- shell-authorized window operations;
- animation transaction hooks.

---

# 17. Process supervision and compositor lifecycle

This is a current Typhon strength and directly fixes major older weaknesses.

Relevant source:

- `src/process.rs`

Current Typhon supports:

- process classification;
- session-owned flag;
- restart policy;
- crash-loop logic;
- SIGCHLD integration;
- dedicated process groups for session-owned processes;
- `SIGTERM` orderly stop;
- `SIGKILL` escalation;
- process-group signaling;
- immediate session-owned cleanup;
- bootstrap cleanup guard;
- `Drop` safety net;
- tests for dedicated groups and startup failure.

Source examples:

- process group setup: `src/process.rs:445-467`;
- TERM/KILL shutdown: `src/process.rs:657-721`;
- bootstrap guard: around `src/process.rs:745`;
- process-group signaling: `src/process.rs:880-914`;
- `Drop` safety: `src/process.rs:918+`.

### Verdict

For its size, Typhon's process ownership model is exceptionally explicit.

This is one of the narrow areas where Typhon's architecture can be considered **cleaner and easier to reason about** than a collection of subsystem-specific child launchers.

The lesson is to preserve this generic ownership model when XWayland and future shell components grow.

---

# 18. Session suspend/resume and shutdown

Typhon has invested heavily in seat/session correctness.

The native pipeline includes behavior for:

- session inactive state;
- scheduler abandonment;
- explicit-sync watch parking/rearm;
- swapchain suspended ownership;
- late pageflip rejection by generation;
- atomic/legacy KMS restoration;
- shutdown state sequencing;
- worker quiescence;
- cursor/scanout cleanup.

The KMS backends keep restore-on-drop safety.

This is a strong foundation for TTY/SDDM use.

KWin remains the production reference because it has been exercised across a much larger driver/hardware ecosystem.

Hyprland delegates substantial session/DRM mechanics to Aquamarine.

### Typhon recommendation

The architecture is sufficient. The missing piece is a published real-hardware matrix and repeated fault injection:

- VT switch during render;
- VT switch during KMS worker TEST_ONLY;
- VT switch during acquire wait;
- compositor kill during pending pageflip;
- XWayland restart during seat transition;
- DRM master revoke/reacquire;
- SIGTERM during queued frame;
- GPU reset/error simulation where possible.

---

# 19. GPU capability advertisement

Typhon has a strong correctness-oriented design here.

Rather than blindly advertising every generated protocol, it builds GPU protocol capabilities after native probing.

It can gate:

- linux-dmabuf behavior;
- dmabuf feedback;
- modifier support;
- main device identity;
- explicit syncobj;
- `wl_drm`.

The native bootstrap dynamically registers GPU globals after the relevant proof exists.

The NVIDIA compatibility path also explicitly reasons about render-node/scanout-node physical GPU identity.

### Why this matters

A compositor that advertises a capability it cannot safely execute creates client-side failures that are much harder to diagnose than simply hiding the global.

This “prove capability, then advertise” philosophy is one of Typhon's best design choices and should be expanded to:

- color management;
- VRR;
- FIFO/commit timing interactions;
- capture;
- DRM leases;
- advanced XWayland bridges.

KWin already has mature capability negotiation across its hardware stack. Hyprland similarly gates many features, but exact native-device proof is split with Aquamarine.

---

# 20. Control plane and observability

## KWin

KWin has a broad ecosystem around:

- DBus;
- debug console;
- support information;
- internal interfaces;
- scripting/plugins;
- desktop configuration.

It wins on breadth.

## Hyprland

`hyprctl` and the event socket provide:

- mutable runtime control;
- window/workspace operations;
- monitor operations;
- configuration;
- diagnostics;
- event subscriptions.

This is excellent for power users and shell integration.

## Typhon

Typhon's `astreactl` surface is smaller, but the local control server is unusually strict.

The current control architecture includes:

- per-instance Unix socket;
- owner-only expectations;
- peer credential checks;
- path/symlink validation;
- bounded request size;
- bounded response size;
- client-count limits;
- per-cycle operation limits;
- deadlines/timeouts;
- typed schemas;
- structured doctor/status output.

The doctor surface includes concepts around:

- KMS;
- renderer;
- cursor;
- XWayland;
- shutdown;
- KMS worker;
- Direct Scanout;
- triple buffering;
- VRR capability state.

### Verdict

Typhon is **not richer** than `hyprctl` or KWin DBus.

It is arguably **more deliberately bounded and defensive** than typical early-project IPC.

### Recommendation

Add a separate subscription/event channel instead of weakening the one-request/one-response control socket.

Events worth exposing:

- output add/remove/change;
- window add/remove/focus/geometry;
- presentation mode;
- Direct Scanout state change;
- triple-buffer mode transition;
- KMS worker degraded/recovered;
- XWayland generation restart;
- session active/inactive.

---

# 21. Resource limits and security

Typhon already has explicit bounds in multiple subsystems, including areas such as:

- control-plane sizes/client counts;
- MIME count/length;
- recent input serial history;
- Astrea manager/toplevel/action counts;
- synchronization/worker queue bounds.

This is better than leaving all client-controlled collections unbounded.

However, there is no single compositor-wide `ClientResourceBudget` that clearly owns quotas for all generic Wayland resources.

Potential budget categories:

- surfaces;
- subsurfaces;
- frame callbacks;
- presentation feedback;
- activation tokens;
- data offers;
- pending sync points;
- manager resources;
- shortcut registrations.

KWin benefits from much more mature framework/system behavior, while Hyprland has its own resource policies.

Typhon should make resource budgeting a first-class per-`ClientId` object so the limits are observable and centrally auditable.

---

# 22. Unsafe/FFI boundary

Typhon necessarily uses unsafe Rust around:

- DRM;
- EGL;
- GBM;
- libinput;
- mmap;
- eventfd/signalfd;
- syncobj;
- raw FDs;
- X11 interop.

A rough textual scan of production Rust finds:

- about **462** `unsafe` tokens;
- about **137** `SAFETY:` comments.

This count is not equal to “462 unsafe blocks,” but it is enough to show that the unsafe boundary is significant.

No crate-wide `unsafe_op_in_unsafe_fn` deny policy was found.

### Recommendation

Adopt:

```rust
#![deny(unsafe_op_in_unsafe_fn)]
```

and require a local `// SAFETY:` justification for every unsafe block that describes:

- lifetime/ownership;
- FD validity;
- thread/concurrency requirements;
- pointer alignment/nullability;
- external API guarantees;
- cleanup responsibility.

Repeated FFI ownership patterns should be wrapped behind small safe RAII types.

This matters more as KMS worker and XWayland concurrency grows.

---

# 23. Tests, source size, and engineering quality

Current Typhon snapshot, approximate static counts:

- **406 Rust files** under `src` + `tests`;
- **216,603 Rust lines** under `src` + `tests`;
- **2,510 `#[test]`-style test attributes**;
- about **101 test-oriented Rust files**.

These raw counts are not directly comparable to KWin/Hyprland because their test frameworks and project layouts differ.

What is meaningful is the type of Typhon tests:

- lifecycle teardown;
- pointer ownership;
- window geometry;
- explicit sync;
- pageflip generations;
- session suspend/recovery;
- KMS transaction ownership;
- Direct Scanout validation;
- worker state;
- XWayland/XWM;
- process supervision;
- control-plane boundaries.

That is a major project strength.

## 23.1 Current CI regression

Typhon currently has **no `.github/workflows` workflow** in the supplied HEAD.

Git history in the archive shows:

- CI added in `43da07f`;
- modified in `f20f046`;
- removed in `ea47a21` (`wip: checkpoint presentation stabilization`).

The deleted workflow previously ran:

- `cargo fmt --check`;
- `cargo check --all-targets`;
- `cargo clippy --all-targets -- -D warnings`;
- `cargo test`;
- source-layout gate.

This is a regression and should be fixed immediately.

The Rust toolchain is still pinned to Rust `1.94.0` with `rustfmt` and `clippy`.

### Recommended CI

```text
cargo fmt --check
cargo check --locked --all-targets
cargo clippy --locked --all-targets -- -D warnings
cargo test --locked
./bin/check-source-layout
```

Then add separate opt-in hardware qualification jobs/scripts rather than mixing hardware assumptions into deterministic CI.

---

# 24. Module size / architecture pressure

Typhon's explicit source-layout gate is useful, but many active production modules are now sitting immediately below the configured limit.

Examples around the current snapshot include:

- `src/xwayland/service.rs` ~1498 lines;
- `src/native_output/runtime/presentation.rs` ~1497;
- `src/compositor/state/surfaces.rs` ~1492;
- `src/xwayland/xwm/window.rs` ~1491;
- `src/process.rs` ~1491;
- `src/native_output/kms_worker/thread.rs` ~1491;
- `src/native/scheduler.rs` ~1490;
- `src/compositor/toplevel_publication.rs` ~1487;
- `src/egl_renderer/damage.rs` ~1481;
- `src/native_output/runtime/bootstrap.rs` ~1480.

Two explicitly large modules are much bigger:

- `src/compositor/render.rs` ~4409;
- `src/egl_renderer.rs` ~2972.

### Risk

A line-limit gate can accidentally encourage arbitrary extraction instead of real architecture if developers repeatedly split at 1499 lines.

### Recommendation

Split by ownership/state-machine boundary, not by size.

Examples:

**XWayland service**
- generation bootstrap;
- socket/display lock;
- process lifecycle;
- reactor IO;
- retirement/restart.

**Presentation runtime**
- target planning;
- KMS submission;
- completion;
- callback/feedback settlement;
- adaptive buffering observations.

**Scheduler**
- target calculation;
- render-ahead policy;
- pageflip ownership;
- buffering mode.

**Renderer**
- scene construction;
- buffer import;
- damage;
- draw execution;
- color transform;
- presentation/export.

The line gate should remain a warning mechanism, not the architecture.

---

# 25. Documentation quality

Typhon documentation is unusually detailed for a project at this stage.

The source contains:

- architecture notes;
- protocol manifests;
- qualification plans;
- research reports;
- known issues;
- XWayland contract documentation;
- superpowers design/spec files.

That is a real strength.

However, documentation drift is already visible.

Most obvious example:

- `docs/XWAYLAND.md` documents a real managed XWayland/XWM implementation.
- `docs/KNOWN_ISSUES.md:39-43` still says X11 compatibility is not enabled and XWayland is only an architectural boundary.

This should be fixed because stale limitations are just as harmful as overclaiming support.

Recommendation: generate a small checked feature-status document from capability constants/tests, or at minimum add CI assertions that key docs and feature defaults agree.

---

# 26. Portal/capture status

Typhon has a local portal backend in `src/portal.rs`.

It implements portal interfaces for:

- Settings;
- Notification;
- Access.

It does **not** provide a ScreenCast/Screenshot compositor capture stack.

Therefore it should not be counted as a replacement for KWin/Hyprland's screen sharing/capture ecosystem.

For modern desktop compatibility Typhon still needs:

- compositor capture protocol;
- PipeWire integration;
- portal ScreenCast;
- portal Screenshot;
- selection UI/shell integration;
- secure session-lock/capture interactions;
- per-output/window source selection.

---

# 27. What Typhon is genuinely better at

This section deliberately avoids saying “Typhon is faster,” because no cross-compositor benchmark was run.

## 27.1 Explicit ownership modeling

Typhon makes many ownership transitions first-class types/state machines:

- output transactions;
- pageflip tokens/generations;
- swapchain slot ownership;
- worker queued/pending/ready/rendering roles;
- direct leases;
- explicit sync acquire/release ownership;
- commit supersede/merge disposition;
- callback ownership.

This makes subtle lifetime bugs easier to test.

In this narrow architectural sense, Typhon is often easier to audit than Hyprland's more organically evolved hot paths.

KWin is also rigorous, but its maturity and C++ framework architecture spread some ownership across broader object graphs.

## 27.2 Adaptive buffering observability

Typhon's adaptive buffering controller observes more separate timing dimensions than the visible KWin and Hyprland policy code.

That makes it the best of the three for asking:

> “Why did the compositor decide to render ahead?”

Again, this is an observability advantage, not yet proof of better latency.

## 27.3 Conservative Direct Scanout qualification

Typhon's:

- exact TEST_ONLY validation contract;
- validation key;
- same-physical-GPU/device proof;
- explicit blocker taxonomy;
- lease lifetime;

form a very safe design.

This is particularly notable next to the visible Hyprland Direct Scanout FIXME around exact device/multi-GPU validation.

## 27.4 Process-tree ownership

Typhon's generic `ChildSupervisor` has:

- process groups;
- restart policy;
- crash-loop behavior;
- session-owned classification;
- bootstrap rollback;
- TERM→KILL;
- drop cleanup.

That is excellent infrastructure for XWayland and the Astrea shell.

## 27.5 Capability-gated GPU protocol advertisement

Typhon's policy of proving native capability before advertising dmabuf/syncobj/wl_drm is exactly the right direction.

## 27.6 Strict local control plane

`astreactl` has a small API but strong boundaries and clear feature-state reporting.

## 27.7 Deterministic test culture

For a much younger compositor, the amount of lifecycle/state-machine regression testing is one of Typhon's largest advantages.

The caveat is that this advantage is partially wasted while CI is absent.

---

# 28. What KWin does substantially better

KWin is the reference implementation in this comparison for broad desktop/output architecture.

Typhon should study KWin for:

1. **multi-output runtime decomposition**;
2. **DRM hotplug lifecycle**;
3. **multi-GPU ownership and cross-GPU copy**;
4. **hardware overlay/underlay planes**;
5. **VRR + tearing integration**;
6. **color pipeline and HDR**;
7. **Direct Scanout inside generalized plane composition**;
8. **complete XWayland selection/DND**;
9. **touch/tablet/IME input stack**;
10. **output management/power/gamma/DRM leasing**;
11. **screen capture/screencast**;
12. **software rendering fallback**;
13. **desktop rules/effects/workspaces**;
14. **hardware/driver error handling accumulated through production use**.

The most important architectural lesson is that **output state is per-output and presentation mode is part of the output transaction**.

---

# 29. What Hyprland does substantially better

Typhon should study Hyprland for:

1. dynamic tiling/workspace UX;
2. low-overhead shell/window-control workflows;
3. broad modern Wayland protocol support;
4. practical XWayland selection/DND integration;
5. VRR/tearing gaming behavior;
6. FIFO + commit timing protocols;
7. session lock;
8. capture protocols;
9. virtual input;
10. text input/IME;
11. tablet/gesture support;
12. output control;
13. runtime IPC/event workflow;
14. fast compositor-policy iteration.

Hyprland is especially relevant to Typhon because both target a lower-level, performance-sensitive desktop experience rather than KWin's full KDE platform scope.

The main source-level caution is that some Hyprland compositor hot paths have more implicit lifetime/backend assumptions than Typhon's typed state-machine approach. Typhon should copy Hyprland's **feature/product breadth**, not necessarily its exact ownership structure.

---

# 30. What Typhon should not copy

## From KWin

Do not copy KWin's total scope all at once.

Typhon does not need to become KDE to be a successful AstreaOS compositor. Importing every effect, plugin, desktop abstraction, and compatibility layer would destroy the project's current ability to reason about its hot path.

Use KWin primarily as the reference for:

- output architecture;
- KMS correctness;
- planes;
- color;
- multi-GPU;
- XWayland edge cases.

## From Hyprland

Do not copy feature behavior by bypassing Typhon's ownership model.

If a Hyprland feature depends on Aquamarine/backend assumptions, re-express it in Typhon's transaction/scheduler abstractions rather than adding side effects directly to the render loop.

In particular:

- VRR;
- tearing;
- Direct Scanout;
- FIFO;
- commit timing;

should become formal Typhon presentation-policy inputs.

---

# 31. Priority roadmap for Typhon

There are two valid priorities: **daily-driver replacement** and **long-term compositor architecture**. They should run in parallel.

## P0 — Engineering gate, immediately

### 1. Restore CI

Recreate the Rust workflow and use `--locked`.

Acceptance:

- fmt;
- check all targets;
- clippy `-D warnings`;
- full deterministic tests;
- source-layout;
- clean diff check where appropriate.

### 2. Fix documentation drift

At minimum:

- XWayland current support;
- Direct Scanout current gates;
- KMS worker defaults;
- protocol capability status;
- hardware qualification status.

### 3. Create a machine-readable feature-status registry

Avoid three sources of truth between code, tests, and docs.

---

# 32. P1 — Immediate single-monitor daily-driver track

This is the fastest path to replacing Hyprland on the known desktop.

## 32.1 Finish XWayland compatibility

Implement end-to-end:

1. XFixes CLIPBOARD;
2. PRIMARY;
3. XDND;
4. live RandR publication;
5. X11 cursor ownership;
6. restart cleanup for all bridge state.

Acceptance matrix:

- Steam;
- Proton game launcher;
- X11 Qt;
- X11 GTK;
- Wine;
- clipboard both directions;
- PRIMARY both directions;
- drag Wayland→X11;
- drag X11→Wayland;
- override-redirect menus;
- fullscreen;
- DPI behavior.

## 32.2 Activate already-implemented Wayland protocols after validation

- idle inhibit;
- primary selection;
- ext data control.

These are low-cost compatibility wins.

## 32.3 Implement gaming frame protocols

- FIFO;
- commit timing;
- content type.

Integrate them with existing commit identity/supersede logic rather than adding independent queues.

## 32.4 Implement real VRR and tearing

Use first-class presentation modes.

Acceptance:

- VRR on/off policy;
- fullscreen/solitary app policy;
- cursor/overlay interaction;
- Direct Scanout interaction;
- KMS worker interaction;
- triple buffer forced off/depth-one where required;
- fallback if async/adaptive commit rejected.

## 32.5 Qualify the existing advanced pipeline

Current sophisticated features must stop being permanent experiments.

Qualification combinations:

- KMS worker off/auto/force;
- triple off/auto/force;
- Direct Scanout off/experimental-auto;
- hardware cursor/auto/software;
- explicit sync on supported clients;
- high refresh;
- Firefox/Zen;
- Kitty;
- Steam;
- Proton/Vulkan games;
- repeated fullscreen/windowed transitions;
- resize stress.

Only after evidence should default policies change.

---

# 33. P1 — Structural output architecture track

Start this early even if the first product remains one monitor.

## 33.1 Introduce real `OutputId`

Replace the fixed single physical output identity.

## 33.2 Create per-output runtime state

Move into `OutputRuntime`:

- mode;
- KMS identity/generation;
- scheduler;
- adaptive buffering;
- swapchain;
- worker state;
- cursor;
- damage;
- presentation;
- VRR/tearing;
- color;
- scale/transform.

## 33.3 Add desktop output layout

- geometry;
- scale;
- transform;
- primary output;
- surface membership;
- enter/leave.

## 33.4 Add hotplug

- connector added;
- connector removed;
- mode list changed;
- CRTC/plane reassignment;
- safe migration;
- output global lifecycle.

## 33.5 Add output management protocols

Only after the model exists.

This track prevents every later display feature from being rewritten.

---

# 34. P2 — Full desktop compatibility

After the output foundation and gaming path:

- touch;
- pointer gestures;
- tablet;
- text input;
- input method;
- virtual keyboard;
- virtual pointer;
- keyboard shortcuts inhibit protocol;
- session lock;
- security context;
- cursor shape;
- XDG output;
- output power;
- output management;
- gamma control;
- DRM lease;
- content type if not done earlier.

---

# 35. P2 — Capture and portals

Implement:

- compositor capture abstraction;
- image-copy-capture;
- screencopy compatibility if needed;
- PipeWire stream;
- portal ScreenCast;
- portal Screenshot;
- secure source picker;
- window/output capture;
- cursor modes;
- damage-based incremental capture.

This also creates pressure to formalize output IDs, so it should follow the multi-output foundation.

---

# 36. P2/P3 — Color and HDR

Implement end-to-end before advertising the protocol:

1. color description model;
2. SDR baseline;
3. output ICC/profile;
4. shader color conversion;
5. KMS color pipeline;
6. HDR metadata;
7. scRGB/PQ/HLG policy;
8. per-surface color protocol;
9. Direct Scanout equivalence;
10. screenshot/capture color semantics.

Use KWin as the primary architecture reference.

---

# 37. P3 — Generalized hardware plane composition

Only after the previous output/color/sync foundations:

- overlay candidates;
- underlay candidates;
- zpos;
- plane damage;
- scaling limits;
- rotation;
- color encoding/range;
- alpha;
- fence compatibility;
- TEST_ONLY;
- fallback;
- per-plane rejection telemetry.

KWin is the clear model here.

---

# 38. P3 — Multi-GPU

Required pieces:

- GPU identity graph;
- render GPU vs scanout GPU;
- cross-GPU import;
- copy fallback;
- modifier negotiation;
- dmabuf feedback tranches per output/device;
- Direct Scanout device equivalence;
- explicit sync across devices;
- hot-unplug behavior.

Again, KWin is the main reference.

---

# 39. Hardening work that should happen continuously

## Unified client resource budget

Centralize per-client limits.

## Unsafe audit

Add `unsafe_op_in_unsafe_fn` deny and local safety comments.

## Module decomposition

Refactor ownership boundaries before files repeatedly hit 1490+ lines.

## Stress/property testing

Especially for:

- explicit sync;
- commit supersede;
- pageflip generation;
- XWayland restart;
- multi-output once introduced.

## Performance baselines

Maintain reproducible metrics for:

- 60 Hz;
- 120/144/165+ Hz;
- idle;
- resize;
- browser scroll;
- game fullscreen;
- Direct Scanout;
- VRR;
- CPU fallback.

---

# 40. Suggested feature ordering by user-visible impact

For the current AstreaOS goal, the most efficient order is:

1. **restore CI**
2. **XWayland clipboard/PRIMARY/XDND**
3. **enable idle inhibit + primary + data-control after qualification**
4. **FIFO + commit timing**
5. **VRR + tearing**
6. **qualify KMS worker/triple buffering/Direct Scanout and move safe modes toward default**
7. **real OutputId/per-output runtime**
8. **hotplug + multi-output**
9. **session lock + capture**
10. **IME/text input + virtual input + gestures/tablet**
11. **color/HDR**
12. **multi-GPU**
13. **overlay/underlay planes**
14. **larger workspace/rules/effects policy only where AstreaOS actually needs it**

This order gives the known single-monitor gaming desktop visible wins without postponing the output refactor indefinitely.

---

# 41. Relative ranking by category

These rankings are deliberately narrow.

## Display-system completeness

1. **KWin**
2. **Hyprland + Aquamarine**
3. **Typhon**

## Multi-output / multi-GPU

1. **KWin**
2. **Hyprland + Aquamarine**
3. **Typhon**

## Color/HDR

1. **KWin**
2. **Hyprland**
3. **Typhon**

## XWayland compatibility

1. **KWin**
2. **Hyprland**
3. **Typhon**

Typhon's XWM itself is much closer than this ranking suggests; the bridge layer is what lowers the result.

## Modern Wayland protocol breadth

1. **Hyprland / KWin**
2. **Typhon**

Hyprland has an edge in this supplied snapshot on FIFO + commit timing together.

## Window-management feature breadth

1. **KWin / Hyprland**, depending desktop model
2. **Typhon**

## Triple-buffer predictor/diagnostics

1. **Typhon**
2. **KWin**
3. **Hyprland new scheduler**

This is about source-model richness, not proven frame pacing.

## Triple-buffer production maturity

1. **KWin**
2. **Hyprland traditional production path / experimental new scheduler**
3. **Typhon**, until hardware qualification and advanced worker path becomes ordinary

## Direct Scanout breadth

1. **KWin**
2. **Hyprland**
3. **Typhon**

## Direct Scanout conservatism/auditability

1. **Typhon**
2. **KWin**
3. **Hyprland visible compositor layer**

## Explicit ownership/state-machine clarity

1. **Typhon**
2. **KWin**
3. **Hyprland**

This is subjective but strongly supported by the supplied source organization.

## Process supervision architecture

1. **Typhon**
2. **KWin**
3. **Hyprland**

Again, this is the generic compositor-owned child/process-tree model, not application-launch ecosystem breadth.

## Runtime control breadth

1. **Hyprland**
2. **KWin**
3. **Typhon**

## Runtime control strictness/bounds

1. **Typhon**
2. **KWin/Hyprland**

## Test density relative to project age/scope

**Typhon is exceptional.**

The current missing CI prevents this from delivering its full value.

---

# 42. The most important design decisions to preserve in Typhon

While adding KWin/Hyprland features, do not lose these Typhon properties:

1. **Capability before advertisement.**
2. **Exact ownership for buffers, callbacks, fences, and children.**
3. **Output transactions are typed, not side-effect soup.**
4. **Pageflip completion is generation/token validated.**
5. **Every experimental optimization has a safe composition fallback.**
6. **Direct Scanout needs TEST_ONLY proof.**
7. **Session loss invalidates unsafe native ownership explicitly.**
8. **Control endpoints are bounded and owner-checked.**
9. **State-machine transitions have deterministic tests.**
10. **Metrics explain rejection and fallback reasons.**

These are the parts that can make Typhon technically distinctive rather than merely “another wlroots-like compositor.”

---

# 43. The biggest risks in the current Typhon direction

## 43.1 Sophistication before qualification

The KMS worker, triple-buffer predictor, explicit sync, and Direct Scanout models are already complex.

If more policy is added before hardware qualification, Typhon risks optimizing abstractions rather than the real bottleneck.

## 43.2 Single-output state hardening into the architecture

Every new output-dependent feature added before `OutputId`/`OutputRuntime` increases future rewrite cost.

## 43.3 Protocol implementation existing but remaining disabled indefinitely

Idle inhibit, primary selection, and data-control are examples where implementation and product capability can drift apart.

Every gated protocol needs an explicit “why not active” test/qualification issue.

## 43.4 CI removal

With 216k+ Rust lines and 2.5k tests, no automatic gate is unacceptable for the current project size.

## 43.5 Documentation drift

Current XWayland docs already contradict one another.

## 43.6 1490-line module pattern

The layout gate is being approached from below by many active modules.

Refactor by responsibility before adding another large wave of output/XWayland work.

---

# 44. Final assessment

Typhon has crossed an important threshold.

It is no longer mainly a feature-gap project. Its core native architecture now contains several serious systems that deserve comparison with established compositors:

- explicit KMS transaction ownership;
- worker-driven atomic submission;
- adaptive triple buffering;
- detailed presentation accounting;
- explicit synchronization;
- conservative Direct Scanout;
- strong process supervision;
- session recovery;
- strict local control;
- dense state-machine testing.

In a few narrow areas, Typhon's source architecture is arguably cleaner or more observable than the equivalent visible path in Hyprland and, in some cases, more explicitly modeled than KWin.

But KWin and Hyprland still win decisively in **product completeness**.

The shortest description is:

> **KWin is the best reference for display correctness and complete output architecture. Hyprland is the best reference for modern compositor UX/protocol breadth and gaming-oriented desktop behavior. Typhon's opportunity is to combine those capabilities with its own unusually explicit ownership, validation, and observability model.**

The most important Typhon improvements are therefore not a rewrite of its hot path.

They are:

- finish compatibility around the hot path;
- make output state truly per-output;
- activate modern presentation modes;
- close XWayland bridges;
- expand input/protocol breadth;
- implement color/capture;
- qualify the advanced native pipeline;
- restore CI.

If those are done without weakening the current transaction/ownership discipline, Typhon can become meaningfully different from both KWin and Hyprland rather than just a smaller copy of either one.
