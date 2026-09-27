# Astrea spatial audio engine

## Ownership and threads

`ShellRuntime` owns one persistent `SpatialAudioEngine`. That facade owns the Rust engine handle and
one `PipeWireSpatialHost`. The existing `AudioService` and `PipeWireAudioBackend` remain device
control and observation components.

The host owns one PipeWire thread loop, context, and core connection for a paired set of streams:

```text
application playback
    -> Astrea Spatial Audio main stream (7.1 Audio/Sink)
    -> SpatialAudioBlockBridge and the existing Rust DSP
    -> Astrea playback stream (stereo Stream/Output/Audio)
    -> WirePlumber-selected physical Audio/Sink
```

The main stream is a virtual sink named `astrea_spatial_audio` and described as `Astrea Spatial
Audio`. Its negotiated format is planar F32 at 48 kHz, with exactly `FL, FR, FC, LFE, RL, RR, SL,
SR` channel order. The playback stream is planar F32 at 48 kHz with `FL, FR`; it is passive and
sets `stream.dont-remix=true`.

Both streams share one per-instance `node.group` and `node.link-group`. The main node is exposed as
the `astrea-spatial-audio` WirePlumber smart filter and is targetable by applications. WirePlumber
recognizes the link-group pair as one filter: clients can target the main virtual sink, and the
playback stream follows standard default-target policy unless a target is configured externally.
The host does not select a physical output or make Astrea the global default sink. The Astrea
virtual endpoint and the physical device receiving its stereo output remain separate nodes.

The Shell control thread loads built-in WAVs from `/usr/share/astrea/hrtf`, validates them, prepares
FFT plans and immutable kernel banks, then starts the paired PipeWire streams. Each stream reports
its negotiated `SPA_PARAM_Format`; the Rust processor is called only while both sides match the
fixed 48 kHz planar F32 boundary and canonical channel positions. PipeWire's graph or device rate
does not determine the Rust stream format; PipeWire adapters handle rate conversion outside the
Astrea stream pair.

Capture processing only triggers the playback stream. The playback process callback dequeues the
newest available capture buffer, recycles stale buffers, obtains one playback buffer, validates
each planar mapping and chunk range, calls the existing block bridge, then recycles or queues both
buffers. If output is available but capture data or the negotiated format is unusable, the valid
stereo F32P output range is filled with silence. An unknown or non-F32P playback layout is left
uninterpreted until a safe writable format is negotiated. A failed playback trigger drains and
recycles queued capture buffers. This pair uses PipeWire scheduling directly and adds neither an
Astrea ring buffer nor a second DSP thread.

Neither realtime process callback allocates or frees memory, locks or waits, accesses files, calls
Qt, or logs. The host propagates PipeWire latency and process-latency parameters using the
paired-stream behavior of PipeWire's filter-chain module. No DSP latency value or latency mode is
invented here.

Startup is transactional: a failure while creating the loop, context, core, streams, formats, or
connections tears down the resources already acquired. Shutdown closes realtime callback entry,
waits for active processing to leave the Rust handle, disconnects both streams before destroying
either, destroys both stream listeners and streams, disconnects the core and context, then stops
and destroys the loop. Only after the callback lifetime is closed can the Rust handle and prepared
memory be destroyed off the realtime thread.

Default-output policy remains deferred. This host does not make Astrea the global default sink,
select a backing device, or set a physical `filter.smart.target`; the playback stream uses normal
WirePlumber default-target behavior when no external target is configured.

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
