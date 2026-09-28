use std::cell::UnsafeCell;
use std::sync::atomic::{AtomicBool, AtomicU32, AtomicU64, AtomicUsize, Ordering};

use super::convolution::{PartitionGeometry, PartitionedConvolver};
use super::hrtf::{HrtfImpulseResponse, HrtfPresetId};
use super::registry::{PrepareError, PreparedBankRegistry};

pub const DEFAULT_TRANSITION_FRAMES: usize = 480;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum SelectionError {
    UnavailablePreset,
    GenerationExhausted,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct SelectionResult {
    pub generation: u64,
    pub no_op: bool,
}

impl SelectionResult {
    pub const fn is_noop(self) -> bool {
        self.no_op
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ProcessError {
    InvalidBufferLength,
    ConcurrentProcessCall,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum SwitchPhase {
    Stable,
    Transitioning,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct HrtfSwitchState {
    pub active: Option<HrtfPresetId>,
    pub target: Option<HrtfPresetId>,
    pub queued: Option<HrtfPresetId>,
    pub phase: SwitchPhase,
    pub generation: u64,
    pub progress_frames: usize,
    pub transition_frames: usize,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct DecodedRequest {
    pub generation: u64,
    pub preset: HrtfPresetId,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
struct PendingRequest {
    encoded: u64,
    request: DecodedRequest,
}

fn encode_request(generation: u64, preset: HrtfPresetId) -> u64 {
    (generation << 8) | (preset.index() as u64 + 1)
}

fn decode_request(encoded: u64) -> Option<DecodedRequest> {
    if encoded == 0 {
        return None;
    }
    Some(DecodedRequest {
        generation: encoded >> 8,
        preset: HrtfPresetId::from_raw((encoded & 0xff) as u8 - 1)?,
    })
}

fn store_latest_request(mailbox: &AtomicU64, generation: u64, preset: HrtfPresetId) {
    mailbox.fetch_max(encode_request(generation, preset), Ordering::Release);
}

fn acknowledge_request(mailbox: &AtomicU64, encoded: u64) -> bool {
    mailbox
        .compare_exchange(encoded, 0, Ordering::AcqRel, Ordering::Acquire)
        .is_ok()
}

fn peek_fresh_request(mailbox: &AtomicU64, applied_generation: u64) -> Option<PendingRequest> {
    let encoded = mailbox.load(Ordering::Acquire);
    let request = decode_request(encoded)?;
    if request.generation <= applied_generation {
        acknowledge_request(mailbox, encoded);
        return None;
    }
    Some(PendingRequest { encoded, request })
}

pub fn crossfade_gains(frame: usize, total_frames: usize) -> (f32, f32) {
    if total_frames <= 1 {
        return (0.0, 1.0);
    }
    if frame == 0 {
        return (1.0, 0.0);
    }
    if frame >= total_frames - 1 {
        return (0.0, 1.0);
    }
    let t = frame as f32 / (total_frames - 1) as f32;
    let new_gain = 0.5 - 0.5 * (std::f32::consts::PI * t).cos();
    (1.0 - new_gain, new_gain)
}

struct RtSwitchState {
    active: HrtfPresetId,
    target: Option<HrtfPresetId>,
    queued: Option<DecodedRequest>,
    progress_frames: usize,
    transition_frames: usize,
    applied_generation: u64,
    convolver: PartitionedConvolver,
}

pub struct RealtimeSpatialEngine {
    registry: PreparedBankRegistry,
    state: UnsafeCell<RtSwitchState>,
    process_busy: AtomicBool,
    next_generation: AtomicU64,
    latest_request: AtomicU64,
    published_status: AtomicU32,
    published_generation: AtomicU64,
    published_progress: AtomicUsize,
    transition_frames: usize,
}

// The PipeWire host serializes process calls with a non-blocking atomic gate. The registry is
// immutable after construction, while control calls touch only the atomic request mailbox.
unsafe impl Sync for RealtimeSpatialEngine {}

impl RealtimeSpatialEngine {
    pub fn with_prepared_banks(
        banks: Vec<(HrtfPresetId, HrtfImpulseResponse)>,
        partition_frames: usize,
        transition_frames: usize,
    ) -> Result<Self, PrepareError> {
        let registry = PreparedBankRegistry::from_responses(banks, partition_frames)?;
        Self::from_registry(registry, transition_frames)
    }

    pub fn from_asset_directory(
        directory: &std::path::Path,
        partition_frames: usize,
    ) -> Result<Self, PrepareError> {
        Self::from_registry(
            PreparedBankRegistry::from_asset_directory(directory, partition_frames)?,
            DEFAULT_TRANSITION_FRAMES,
        )
    }

    pub fn from_registry(
        registry: PreparedBankRegistry,
        transition_frames: usize,
    ) -> Result<Self, PrepareError> {
        if !registry.contains(HrtfPresetId::Astrea) {
            return Err(PrepareError::MissingRequiredPreset);
        }
        if transition_frames == 0 {
            return Err(PrepareError::InvalidPartitionGeometry);
        }
        let state = RtSwitchState {
            active: HrtfPresetId::Astrea,
            target: None,
            queued: None,
            progress_frames: 0,
            transition_frames,
            applied_generation: 0,
            convolver: PartitionedConvolver::new(
                registry.geometry(),
                registry.forward().get_inplace_scratch_len(),
                registry.inverse().get_inplace_scratch_len(),
            ),
        };
        Ok(Self {
            registry,
            state: UnsafeCell::new(state),
            process_busy: AtomicBool::new(false),
            next_generation: AtomicU64::new(0),
            latest_request: AtomicU64::new(0),
            published_status: AtomicU32::new(pack_status(HrtfPresetId::Astrea, None, None)),
            published_generation: AtomicU64::new(0),
            published_progress: AtomicUsize::new(0),
            transition_frames,
        })
    }

    pub fn select_hrtf(&self, preset: HrtfPresetId) -> Result<SelectionResult, SelectionError> {
        if !self.registry.contains(preset) {
            return Err(SelectionError::UnavailablePreset);
        }
        // The consumer publishes state before acknowledging its request. Read the mailbox first
        // so an acquired empty value cannot be paired with the pre-publication state.
        let pending_request = self.latest_request.load(Ordering::Acquire);
        let published = self.published_status.load(Ordering::Acquire);
        let (active, target, _) = unpack_status(published);
        if active == Some(preset) && target.is_none() && pending_request == 0 {
            return Ok(SelectionResult {
                generation: self.next_generation.load(Ordering::Acquire),
                no_op: true,
            });
        }

        let generation = self.next_generation.fetch_add(1, Ordering::AcqRel) + 1;
        if generation >= (u64::MAX >> 8) {
            self.next_generation.store(u64::MAX >> 8, Ordering::Release);
            return Err(SelectionError::GenerationExhausted);
        }
        store_latest_request(&self.latest_request, generation, preset);
        Ok(SelectionResult {
            generation,
            no_op: false,
        })
    }

    pub fn active_hrtf(&self) -> Option<HrtfPresetId> {
        unpack_status(self.published_status.load(Ordering::Acquire)).0
    }

    pub fn switch_state(&self) -> HrtfSwitchState {
        let status = self.published_status.load(Ordering::Acquire);
        let (active, target, queued) = unpack_status(status);
        HrtfSwitchState {
            active,
            target,
            queued,
            phase: if target.is_some() {
                SwitchPhase::Transitioning
            } else {
                SwitchPhase::Stable
            },
            generation: self.published_generation.load(Ordering::Acquire),
            progress_frames: self.published_progress.load(Ordering::Acquire),
            transition_frames: self.transition_frames,
        }
    }

    pub fn latest_generation(&self) -> u64 {
        self.next_generation.load(Ordering::Acquire)
    }

    pub fn partition_geometry(&self) -> PartitionGeometry {
        self.registry.geometry()
    }

    pub fn bank_partition_geometry(&self, preset: HrtfPresetId) -> PartitionGeometry {
        self.registry.bank_geometry(preset)
    }

    pub fn process_interleaved(
        &self,
        input: &[f32],
        output: &mut [f32],
    ) -> Result<(), ProcessError> {
        if !input.len().is_multiple_of(8)
            || !output.len().is_multiple_of(2)
            || input.len() / 8 != output.len() / 2
        {
            return Err(ProcessError::InvalidBufferLength);
        }
        self.process_frames(
            input.len() / 8,
            |frame, source| source.copy_from_slice(&input[frame * 8..frame * 8 + 8]),
            |frame, ears| {
                output[frame * 2] = ears[0];
                output[frame * 2 + 1] = ears[1];
            },
        )
    }

    pub fn process_planar(
        &self,
        inputs: &[&[f32]; 8],
        outputs: &mut [&mut [f32]; 2],
    ) -> Result<(), ProcessError> {
        let frames = inputs[0].len();
        if inputs.iter().any(|channel| channel.len() != frames)
            || outputs.iter().any(|channel| channel.len() != frames)
        {
            return Err(ProcessError::InvalidBufferLength);
        }
        self.process_frames(
            frames,
            |frame, source| {
                for channel in 0..8 {
                    source[channel] = inputs[channel][frame];
                }
            },
            |frame, ears| {
                outputs[0][frame] = ears[0];
                outputs[1][frame] = ears[1];
            },
        )
    }

    fn process_frames(
        &self,
        frame_count: usize,
        mut read_input: impl FnMut(usize, &mut [f32; 8]),
        mut write_output: impl FnMut(usize, [f32; 2]),
    ) -> Result<(), ProcessError> {
        if self
            .process_busy
            .compare_exchange(false, true, Ordering::Acquire, Ordering::Relaxed)
            .is_err()
        {
            return Err(ProcessError::ConcurrentProcessCall);
        }
        let _busy = ProcessBusyGuard(&self.process_busy);

        // SAFETY: process_busy grants this call exclusive access to the realtime state. Control
        // callers never access it directly; they exchange requests and read published atomics.
        let state = unsafe { &mut *self.state.get() };
        for frame_index in 0..frame_count {
            let mut source_frame = [0.0f32; 8];
            read_input(frame_index, &mut source_frame);
            let mut output_frame = [0.0f32; 2];
            if let Some((old_frame, new_frame)) = state.convolver.pop_output_frame() {
                let (old_gain, new_gain) = match state.target {
                    Some(_) => crossfade_gains(state.progress_frames, state.transition_frames),
                    None if state.convolver.output_is_transition() => (0.0, 1.0),
                    None => (1.0, 0.0),
                };
                output_frame[0] = old_frame[0] * old_gain + new_frame[0] * new_gain;
                output_frame[1] = old_frame[1] * old_gain + new_frame[1] * new_gain;
                if state.target.is_some() {
                    state.progress_frames += 1;
                    if state.progress_frames >= state.transition_frames
                        && let Some(target) = state.target.take()
                    {
                        state.active = target;
                        state.convolver.swap_target_to_active();
                        state.progress_frames = 0;
                        self.publish_state(state);
                    }
                }
                state.convolver.finish_output_frame();
            }
            write_output(frame_index, output_frame);

            if state.convolver.is_last_input_frame() {
                self.apply_latest_request(state);
                if state.target.is_none()
                    && let Some(queued) = state.queued.take()
                {
                    if queued.preset == state.active {
                        state.applied_generation = queued.generation;
                        self.published_generation
                            .store(queued.generation, Ordering::Release);
                    } else {
                        self.start_transition(state, queued);
                    }
                }
            }
            state
                .convolver
                .ingest_frame(&source_frame, state.active, state.target, &self.registry);
        }
        self.publish_state(state);
        Ok(())
    }

    fn apply_latest_request(&self, state: &mut RtSwitchState) {
        let Some(pending) = peek_fresh_request(&self.latest_request, state.applied_generation)
        else {
            return;
        };
        let request = pending.request;
        state.applied_generation = request.generation;
        self.published_generation
            .store(request.generation, Ordering::Release);
        if state.target.is_some() {
            state.queued = Some(request);
            self.publish_state(state);
        } else {
            state.queued = None;
            if request.preset != state.active {
                self.start_transition(state, request);
            } else {
                self.publish_state(state);
            }
        }
        acknowledge_request(&self.latest_request, pending.encoded);
    }

    fn start_transition(&self, state: &mut RtSwitchState, request: DecodedRequest) {
        state.convolver.begin_target(request.preset, &self.registry);
        state.target = Some(request.preset);
        state.progress_frames = 0;
        state.applied_generation = state.applied_generation.max(request.generation);
        self.published_generation
            .store(state.applied_generation, Ordering::Release);
        self.publish_state(state);
    }

    fn publish_state(&self, state: &RtSwitchState) {
        self.published_status.store(
            pack_status(
                state.active,
                state.target,
                state.queued.map(|request| request.preset),
            ),
            Ordering::Release,
        );
        self.published_progress
            .store(state.progress_frames, Ordering::Release);
    }
}

struct ProcessBusyGuard<'a>(&'a AtomicBool);

impl Drop for ProcessBusyGuard<'_> {
    fn drop(&mut self) {
        self.0.store(false, Ordering::Release);
    }
}

fn pack_status(
    active: HrtfPresetId,
    target: Option<HrtfPresetId>,
    queued: Option<HrtfPresetId>,
) -> u32 {
    let target = target.map_or(15, |preset| preset as u32);
    let queued = queued.map_or(15, |preset| preset as u32);
    active as u32 | (target << 4) | (queued << 8)
}

fn unpack_status(
    status: u32,
) -> (
    Option<HrtfPresetId>,
    Option<HrtfPresetId>,
    Option<HrtfPresetId>,
) {
    let decode = |value| {
        if value == 15 {
            None
        } else {
            HrtfPresetId::from_raw(value as u8)
        }
    };
    (
        decode(status & 15),
        decode((status >> 4) & 15),
        decode((status >> 8) & 15),
    )
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::AtomicU64;

    fn response() -> HrtfImpulseResponse {
        HrtfImpulseResponse::from_hesuvi_interleaved(&[0.0; 14], 14, 48_000).unwrap()
    }

    #[test]
    fn late_older_request_cannot_replace_a_newer_generation() {
        let mailbox = AtomicU64::new(0);
        store_latest_request(&mailbox, 12, HrtfPresetId::AstreaHalf);
        store_latest_request(&mailbox, 11, HrtfPresetId::AstreaDolby);
        assert_eq!(
            decode_request(mailbox.load(Ordering::Acquire)),
            Some(DecodedRequest {
                generation: 12,
                preset: HrtfPresetId::AstreaHalf
            })
        );

        let pending = peek_fresh_request(&mailbox, 0).unwrap();
        assert_eq!(pending.request.generation, 12);
        assert_eq!(
            decode_request(mailbox.load(Ordering::Acquire)),
            Some(pending.request)
        );
        assert!(acknowledge_request(&mailbox, pending.encoded));
        assert_eq!(mailbox.load(Ordering::Acquire), 0);

        // Simulate an older producer publishing after generation 12 was acknowledged.
        store_latest_request(&mailbox, 11, HrtfPresetId::AstreaDolby);
        assert_eq!(peek_fresh_request(&mailbox, 12), None);
        assert_eq!(mailbox.load(Ordering::Acquire), 0);
    }

    #[test]
    fn acknowledgement_of_an_observed_request_cannot_clear_a_newer_request() {
        let mailbox = AtomicU64::new(0);
        store_latest_request(&mailbox, 11, HrtfPresetId::AstreaDolby);
        let observed = mailbox.load(Ordering::Acquire);

        store_latest_request(&mailbox, 12, HrtfPresetId::AstreaHalf);
        assert!(!acknowledge_request(&mailbox, observed));
        assert_eq!(
            decode_request(mailbox.load(Ordering::Acquire)),
            Some(DecodedRequest {
                generation: 12,
                preset: HrtfPresetId::AstreaHalf,
            })
        );
    }

    #[test]
    fn stale_generation_cannot_replace_the_active_preset() {
        let registry = PreparedBankRegistry::from_responses(
            vec![
                (HrtfPresetId::Astrea, response()),
                (HrtfPresetId::AstreaDolby, response()),
                (HrtfPresetId::AstreaMovie, response()),
            ],
            16,
        )
        .unwrap();
        let engine = RealtimeSpatialEngine::from_registry(registry, 1).unwrap();
        store_latest_request(&engine.latest_request, 12, HrtfPresetId::AstreaDolby);
        let input = vec![0.0; 16 * 8];
        let mut output = vec![0.0; 16 * 2];
        engine.process_interleaved(&input, &mut output).unwrap();
        assert_eq!(
            engine.switch_state().target,
            Some(HrtfPresetId::AstreaDolby)
        );

        store_latest_request(&engine.latest_request, 11, HrtfPresetId::AstreaMovie);
        engine.process_interleaved(&input, &mut output).unwrap();
        assert_eq!(engine.active_hrtf(), Some(HrtfPresetId::AstreaDolby));
        assert_eq!(engine.switch_state().target, None);
    }

    #[test]
    fn selecting_active_preset_after_observing_an_unacknowledged_request_is_not_a_noop() {
        let registry = PreparedBankRegistry::from_responses(
            vec![
                (HrtfPresetId::Astrea, response()),
                (HrtfPresetId::AstreaDolby, response()),
                (HrtfPresetId::AstreaMovie, response()),
            ],
            16,
        )
        .unwrap();
        let engine = RealtimeSpatialEngine::from_registry(registry, 1).unwrap();

        let dolby = engine.select_hrtf(HrtfPresetId::AstreaDolby).unwrap();
        let observed = peek_fresh_request(&engine.latest_request, 0).unwrap();
        assert_eq!(observed.request.generation, dolby.generation);
        assert_eq!(
            engine.latest_request.load(Ordering::Acquire),
            observed.encoded
        );

        let astrea = engine.select_hrtf(HrtfPresetId::Astrea).unwrap();
        assert!(!astrea.no_op);
        assert_eq!(
            decode_request(engine.latest_request.load(Ordering::Acquire)),
            Some(DecodedRequest {
                generation: astrea.generation,
                preset: HrtfPresetId::Astrea,
            })
        );

        let input = vec![0.0f32; 16 * 8];
        let mut output = vec![0.0f32; 16 * 2];
        engine.process_interleaved(&input, &mut output).unwrap();

        assert_eq!(engine.active_hrtf(), Some(HrtfPresetId::Astrea));
        assert_eq!(engine.switch_state().target, None);
        assert_eq!(engine.switch_state().generation, astrea.generation);
    }
}
