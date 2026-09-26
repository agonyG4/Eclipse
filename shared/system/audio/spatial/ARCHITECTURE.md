# Astrea spatial audio engine

## Ownership and threads

`ShellRuntime` owns one `SpatialAudioEngine`. That facade owns the Rust engine handle and its
dedicated `pw_filter` host. The existing `AudioService` and `PipeWireAudioBackend` remain device
control and observation components.

The Shell control thread loads built-in WAVs from `/usr/share/astrea/hrtf`, validates them, prepares
FFT plans and immutable kernel banks, then starts the PipeWire host. The PipeWire realtime callback
uses only the native planar block bridge and Rust process API. It does not call Qt or enter the
device-control backend. The host disconnects and destroys the filter under the PipeWire thread-loop
lock. PipeWire removes the realtime callback from its data loop before filter destruction returns;
only then does the host clear its port and handle pointers, stop its control loop, and allow the Rust
handle and prepared memory to be destroyed off the realtime thread.

## HRTF representation and convolution

The source WAV layout has 14 HeSuVi channels. Preparation maps each source into the engine's
canonical `8 sources x 2 ears` matrix in 7.1 order: FL, FR, FC, LFE, RL, RR, SL, SR. LFE explicitly
uses the FC left/right pair. The realtime engine sees only this canonical matrix.

All prepared banks in an engine share one partition size, FFT size, and global input-history
capacity. Each bank stores only the kernel partitions through its last exactly non-zero
coefficient across the canonical HRTF channels. An all-zero valid response keeps one effective
partition. Trailing exact-zero frames may be omitted during kernel preparation; no amplitude
threshold is used, so tiny non-zero values remain part of the response. Each input block is
transformed once for each of the eight sources, and those spectra are stored in one source-indexed
ring. Both output ears and both banks in a transition reuse that same ring. Kernel memory lives in
fixed registry slots for the engine lifetime; the realtime callback changes slot identity and
never releases a bank.

Long impulse responses use uniform partitioned FFT convolution. Eight forward transforms feed two
ear accumulations and two inverse transforms per output render. The target bank's overlap state is
seeded from existing input spectra when a switch starts, so its response can include samples that
arrived before the request.

## Switching and realtime rules

The state machine is `Stable(active)`, `Transition(active, target)`, then `Stable(target)`. Requests
carry increasing generations. Before a transition starts, an atomic latest-generation mailbox
replaces older pending requests. The realtime consumer observes a fresh request, applies it to
realtime state, publishes that state and generation, then acknowledges the exact encoded mailbox
value with compare-exchange. The request therefore does not disappear from both the mailbox and
published state during handoff, and acknowledgement cannot erase a newer generation. Stale
requests are cleared only when the exact stale value is still present. While a transition is
active, one queued target is retained and newer requests replace it. Requests older than the
applied generation are ignored.

The default transition lasts 480 output frames at 48 kHz. Progress is measured in frames, so graph
quantum changes do not change its duration. Old and new stereo renders use complementary raised
cosine gains, with exact old-only and new-only endpoints. Invalid or unavailable preset requests
return a bounded status to the caller and leave current playback state untouched.

The processing path uses preallocated buffers and FFT scratch, a non-blocking atomic single-callback
gate, and atomic request/status handoff. It performs no allocation, deallocation, file access, mutex
acquisition, waiting, logging, Qt dispatch, HRTF parsing, or FFT-plan construction. If the callback
cannot obtain valid buffers or the Rust processor rejects a block, the native bridge writes silence
to valid output buffers.

## Preset duration and pipeline latency

An impulse response's duration describes its convolution tail. End-to-end latency also depends on
the PipeWire graph quantum, the engine's partition buffering, graph scheduling, and device buffers.
A short preset therefore does not imply a matching end-to-end latency; this foundation does not
select or promise a latency mode.

The production WAV assets are external fixtures. No captured Apple or Dolby audio is stored in this
repository; tests use deterministic synthetic responses.
