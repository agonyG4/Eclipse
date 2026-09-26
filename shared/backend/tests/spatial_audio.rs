use std::alloc::{GlobalAlloc, Layout, System};
use std::cell::Cell;
use std::sync::atomic::{AtomicUsize, Ordering};

use astrea_system_backend::spatial_audio::{
    Ear, HrtfImpulseResponse, HrtfPresetId, RealtimeSpatialEngine, SourceChannel, SwitchPhase,
    crossfade_gains,
};

struct CountingAllocator;

thread_local! {
    static TRACK_ALLOCATIONS: Cell<bool> = const { Cell::new(false) };
}

static ALLOCATION_COUNT: AtomicUsize = AtomicUsize::new(0);

unsafe impl GlobalAlloc for CountingAllocator {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        let pointer = unsafe { System.alloc(layout) };
        if TRACK_ALLOCATIONS.try_with(Cell::get).unwrap_or(false) {
            ALLOCATION_COUNT.fetch_add(1, Ordering::Relaxed);
        }
        pointer
    }

    unsafe fn dealloc(&self, pointer: *mut u8, layout: Layout) {
        if TRACK_ALLOCATIONS.try_with(Cell::get).unwrap_or(false) {
            ALLOCATION_COUNT.fetch_add(1, Ordering::Relaxed);
        }
        unsafe { System.dealloc(pointer, layout) };
    }
}

#[global_allocator]
static ALLOCATOR: CountingAllocator = CountingAllocator;

const SOURCE_CHANNELS: usize = 8;
const PARTITION_FRAMES: usize = 128;

fn hesuvi_response(length: usize, coefficients: &[(usize, usize, f32)]) -> HrtfImpulseResponse {
    let mut interleaved = vec![0.0f32; length * 14];
    for &(wav_channel, frame, value) in coefficients {
        interleaved[frame * 14 + wav_channel] = value;
    }
    HrtfImpulseResponse::from_hesuvi_interleaved(&interleaved, 14, 48_000).unwrap()
}

fn all_presets_with(response: HrtfImpulseResponse) -> Vec<(HrtfPresetId, HrtfImpulseResponse)> {
    HrtfPresetId::ALL
        .into_iter()
        .map(|id| (id, response.clone()))
        .collect()
}

fn engine_with_responses(
    astrea: HrtfImpulseResponse,
    dolby: HrtfImpulseResponse,
    movie: HrtfImpulseResponse,
    transition_frames: usize,
) -> RealtimeSpatialEngine {
    let mut banks = all_presets_with(astrea.clone());
    banks[HrtfPresetId::AstreaDolby as usize] = (HrtfPresetId::AstreaDolby, dolby);
    banks[HrtfPresetId::AstreaMovie as usize] = (HrtfPresetId::AstreaMovie, movie);
    RealtimeSpatialEngine::with_prepared_banks(banks, PARTITION_FRAMES, transition_frames).unwrap()
}

#[test]
fn maps_hesuvi_14_channels_to_canonical_8_by_2_and_aliases_lfe_to_fc() {
    let response = hesuvi_response(
        1,
        &[
            (0, 0, 0.10),
            (1, 0, 0.11),
            (8, 0, 0.20),
            (7, 0, 0.21),
            (6, 0, 0.30),
            (13, 0, 0.31),
            (4, 0, 0.40),
            (5, 0, 0.41),
            (12, 0, 0.50),
            (11, 0, 0.51),
            (2, 0, 0.60),
            (3, 0, 0.61),
            (10, 0, 0.70),
            (9, 0, 0.71),
        ],
    );

    assert_eq!(
        response.coefficient(SourceChannel::FrontLeft, Ear::Left, 0),
        Some(0.10)
    );
    assert_eq!(
        response.coefficient(SourceChannel::FrontLeft, Ear::Right, 0),
        Some(0.11)
    );
    assert_eq!(
        response.coefficient(SourceChannel::FrontRight, Ear::Left, 0),
        Some(0.20)
    );
    assert_eq!(
        response.coefficient(SourceChannel::FrontRight, Ear::Right, 0),
        Some(0.21)
    );
    assert_eq!(
        response.coefficient(SourceChannel::FrontCenter, Ear::Left, 0),
        Some(0.30)
    );
    assert_eq!(
        response.coefficient(SourceChannel::FrontCenter, Ear::Right, 0),
        Some(0.31)
    );
    assert_eq!(
        response.coefficient(SourceChannel::Lfe, Ear::Left, 0),
        Some(0.30)
    );
    assert_eq!(
        response.coefficient(SourceChannel::Lfe, Ear::Right, 0),
        Some(0.31)
    );
    assert_eq!(
        response.coefficient(SourceChannel::RearLeft, Ear::Left, 0),
        Some(0.40)
    );
    assert_eq!(
        response.coefficient(SourceChannel::RearLeft, Ear::Right, 0),
        Some(0.41)
    );
    assert_eq!(
        response.coefficient(SourceChannel::RearRight, Ear::Left, 0),
        Some(0.50)
    );
    assert_eq!(
        response.coefficient(SourceChannel::RearRight, Ear::Right, 0),
        Some(0.51)
    );
    assert_eq!(
        response.coefficient(SourceChannel::SideLeft, Ear::Left, 0),
        Some(0.60)
    );
    assert_eq!(
        response.coefficient(SourceChannel::SideLeft, Ear::Right, 0),
        Some(0.61)
    );
    assert_eq!(
        response.coefficient(SourceChannel::SideRight, Ear::Left, 0),
        Some(0.70)
    );
    assert_eq!(
        response.coefficient(SourceChannel::SideRight, Ear::Right, 0),
        Some(0.71)
    );
}

#[test]
fn rejects_wrong_channel_count_and_sample_rate() {
    let samples = vec![0.0f32; 14];
    assert!(HrtfImpulseResponse::from_hesuvi_interleaved(&samples, 13, 48_000).is_err());
    assert!(HrtfImpulseResponse::from_hesuvi_interleaved(&samples, 14, 44_100).is_err());
}

#[test]
fn rejects_malformed_or_non_finite_coefficients() {
    assert!(HrtfImpulseResponse::from_hesuvi_interleaved(&[], 14, 48_000).is_err());
    assert!(HrtfImpulseResponse::from_hesuvi_interleaved(&[0.0f32; 13], 14, 48_000).is_err());
    for invalid in [f32::NAN, f32::INFINITY, f32::NEG_INFINITY] {
        let mut samples = vec![0.0f32; 14];
        samples[0] = invalid;
        assert!(HrtfImpulseResponse::from_hesuvi_interleaved(&samples, 14, 48_000).is_err());
    }
}

#[test]
fn preset_ids_are_stable_and_match_the_builtin_names() {
    let names = [
        "astrea",
        "astrea-movie",
        "astrea-half",
        "astrea-short",
        "astrea-latency",
        "astrea-dolby",
        "astrea-dolby-half",
        "astrea-dolby-latency",
    ];
    for (slot, name) in names.into_iter().enumerate() {
        assert_eq!(HrtfPresetId::ALL[slot].as_str(), name);
        assert_eq!(HrtfPresetId::from_name(name), Some(HrtfPresetId::ALL[slot]));
    }
}

#[test]
fn same_preset_request_while_stable_is_an_idempotent_noop() {
    let response = hesuvi_response(1, &[(6, 0, 1.0), (13, 0, 1.0)]);
    let engine = engine_with_responses(response.clone(), response.clone(), response, 480);

    let before = engine.latest_generation();
    let request = engine.select_hrtf(HrtfPresetId::Astrea).unwrap();

    assert!(request.is_noop());
    assert_eq!(engine.latest_generation(), before);
    assert_eq!(engine.active_hrtf(), Some(HrtfPresetId::Astrea));
}

#[test]
fn unavailable_preset_is_rejected_without_changing_active_bank() {
    let response = hesuvi_response(1, &[(6, 0, 1.0), (13, 0, 1.0)]);
    let engine = RealtimeSpatialEngine::with_prepared_banks(
        vec![(HrtfPresetId::Astrea, response)],
        PARTITION_FRAMES,
        480,
    )
    .unwrap();

    assert!(engine.select_hrtf(HrtfPresetId::AstreaDolby).is_err());
    assert_eq!(engine.active_hrtf(), Some(HrtfPresetId::Astrea));
    assert_eq!(engine.switch_state().target, None);
}

#[test]
fn one_preset_bank_switches_every_source_and_ear_together() {
    let old = hesuvi_response(1, &[(0, 0, 1.0), (1, 0, 1.0)]);
    let new = hesuvi_response(
        1,
        &[
            (0, 0, 0.10),
            (1, 0, 0.11),
            (8, 0, 0.20),
            (7, 0, 0.21),
            (6, 0, 0.30),
            (13, 0, 0.31),
            (4, 0, 0.40),
            (5, 0, 0.41),
            (12, 0, 0.50),
            (11, 0, 0.51),
            (2, 0, 0.60),
            (3, 0, 0.61),
            (10, 0, 0.70),
            (9, 0, 0.71),
        ],
    );
    let engine = engine_with_responses(old, new, hesuvi_response(1, &[]), 1);
    engine.select_hrtf(HrtfPresetId::AstreaDolby).unwrap();

    let mut input = vec![0.0f32; PARTITION_FRAMES * SOURCE_CHANNELS];
    for sample in &mut input[..SOURCE_CHANNELS] {
        *sample = 1.0;
    }
    let mut output = vec![0.0f32; PARTITION_FRAMES * 2];
    engine.process_interleaved(&input, &mut output).unwrap();
    engine
        .process_interleaved(&vec![0.0f32; input.len()], &mut output)
        .unwrap();
    assert_eq!(engine.active_hrtf(), Some(HrtfPresetId::AstreaDolby));

    let expected_left = [0.10, 0.20, 0.30, 0.30, 0.40, 0.50, 0.60, 0.70]
        .into_iter()
        .sum::<f32>();
    let expected_right = [0.11, 0.21, 0.31, 0.31, 0.41, 0.51, 0.61, 0.71]
        .into_iter()
        .sum::<f32>();
    assert!((output[0] - expected_left).abs() < 2.0e-5);
    assert!((output[1] - expected_right).abs() < 2.0e-5);
    assert_eq!(engine.active_hrtf(), Some(HrtfPresetId::AstreaDolby));
}

#[test]
fn latest_request_before_transition_starts_replaces_older_request() {
    let response = hesuvi_response(1, &[(6, 0, 1.0), (13, 0, 1.0)]);
    let engine = engine_with_responses(response.clone(), response.clone(), response, 8);
    engine.select_hrtf(HrtfPresetId::AstreaDolby).unwrap();
    engine.select_hrtf(HrtfPresetId::AstreaMovie).unwrap();

    let input = vec![0.0f32; PARTITION_FRAMES * SOURCE_CHANNELS];
    let mut output = vec![0.0f32; PARTITION_FRAMES * 2];
    engine.process_interleaved(&input, &mut output).unwrap();

    assert_eq!(
        engine.switch_state().target,
        Some(HrtfPresetId::AstreaMovie)
    );
    assert_eq!(engine.switch_state().generation, engine.latest_generation());
}

#[test]
fn one_latest_target_is_queued_during_a_transition() {
    let response = hesuvi_response(1, &[(6, 0, 1.0), (13, 0, 1.0)]);
    let engine = engine_with_responses(response.clone(), response.clone(), response, 480);
    let input = vec![0.0f32; PARTITION_FRAMES * SOURCE_CHANNELS];
    let mut output = vec![0.0f32; PARTITION_FRAMES * 2];
    engine.select_hrtf(HrtfPresetId::AstreaDolby).unwrap();
    engine.process_interleaved(&input, &mut output).unwrap();
    engine.select_hrtf(HrtfPresetId::AstreaMovie).unwrap();
    engine.select_hrtf(HrtfPresetId::AstreaHalf).unwrap();
    engine.process_interleaved(&input, &mut output).unwrap();

    assert_eq!(
        engine.switch_state().target,
        Some(HrtfPresetId::AstreaDolby)
    );
    assert_eq!(engine.switch_state().queued, Some(HrtfPresetId::AstreaHalf));
}

#[test]
fn transition_completion_promotes_target_then_starts_latest_queued_target() {
    let response = hesuvi_response(1, &[(6, 0, 1.0), (13, 0, 1.0)]);
    let engine = engine_with_responses(
        response.clone(),
        response.clone(),
        response,
        PARTITION_FRAMES * 2,
    );
    let input = vec![0.0; PARTITION_FRAMES * SOURCE_CHANNELS];
    let mut output = vec![0.0; PARTITION_FRAMES * 2];
    engine.select_hrtf(HrtfPresetId::AstreaDolby).unwrap();
    engine.process_interleaved(&input, &mut output).unwrap();
    engine.select_hrtf(HrtfPresetId::AstreaMovie).unwrap();
    engine.process_interleaved(&input, &mut output).unwrap();
    assert_eq!(
        engine.switch_state().queued,
        Some(HrtfPresetId::AstreaMovie)
    );

    engine.process_interleaved(&input, &mut output).unwrap();

    assert_eq!(engine.active_hrtf(), Some(HrtfPresetId::AstreaDolby));
    assert_eq!(
        engine.switch_state().target,
        Some(HrtfPresetId::AstreaMovie)
    );
}

#[test]
fn newest_request_at_transition_boundary_replaces_an_older_queued_target() {
    let response = hesuvi_response(1, &[(6, 0, 1.0), (13, 0, 1.0)]);
    let engine = engine_with_responses(
        response.clone(),
        response.clone(),
        response,
        PARTITION_FRAMES * 2,
    );
    let input = vec![0.0; PARTITION_FRAMES * SOURCE_CHANNELS];
    let mut output = vec![0.0; PARTITION_FRAMES * 2];

    engine.select_hrtf(HrtfPresetId::AstreaDolby).unwrap();
    engine.process_interleaved(&input, &mut output).unwrap();
    engine.select_hrtf(HrtfPresetId::AstreaMovie).unwrap();
    engine.process_interleaved(&input, &mut output).unwrap();
    assert_eq!(
        engine.switch_state().queued,
        Some(HrtfPresetId::AstreaMovie)
    );

    engine.select_hrtf(HrtfPresetId::AstreaHalf).unwrap();
    engine.process_interleaved(&input, &mut output).unwrap();
    assert_eq!(engine.active_hrtf(), Some(HrtfPresetId::AstreaDolby));
    assert_eq!(engine.switch_state().target, Some(HrtfPresetId::AstreaHalf));
    assert_eq!(engine.switch_state().queued, None);

    engine.process_interleaved(&input, &mut output).unwrap();
    engine.process_interleaved(&input, &mut output).unwrap();
    assert_eq!(engine.active_hrtf(), Some(HrtfPresetId::AstreaHalf));
    assert_eq!(engine.switch_state().phase, SwitchPhase::Stable);
}

#[test]
fn crossfade_gains_are_complementary_for_every_frame() {
    let total_frames = 480;
    for frame in 0..total_frames {
        let (old_gain, new_gain) = crossfade_gains(frame, total_frames);
        assert!((old_gain + new_gain - 1.0).abs() <= f32::EPSILON);
        assert!((0.0..=1.0).contains(&old_gain));
        assert!((0.0..=1.0).contains(&new_gain));
    }
}

#[test]
fn crossfade_endpoints_are_exact_old_only_and_new_only() {
    assert_eq!(crossfade_gains(0, 480), (1.0, 0.0));
    assert_eq!(crossfade_gains(479, 480), (0.0, 1.0));
}

#[test]
fn identical_old_and_new_kernels_have_no_transition_discontinuity() {
    let response = hesuvi_response(256, &[(6, 0, 0.75), (13, 0, 0.50), (8, 128, 0.25)]);
    let switching =
        engine_with_responses(response.clone(), response.clone(), response.clone(), 480);
    let stable = engine_with_responses(response.clone(), response.clone(), response, 480);
    let mut input = vec![0.0; PARTITION_FRAMES * SOURCE_CHANNELS];
    input[2] = 1.0;
    let mut switching_output = vec![0.0f32; PARTITION_FRAMES * 2];
    let mut stable_output = vec![0.0f32; PARTITION_FRAMES * 2];
    switching
        .process_interleaved(&input, &mut switching_output)
        .unwrap();
    stable
        .process_interleaved(&input, &mut stable_output)
        .unwrap();
    switching.select_hrtf(HrtfPresetId::AstreaDolby).unwrap();

    for _ in 0..5 {
        let silence = vec![0.0f32; input.len()];
        switching
            .process_interleaved(&silence, &mut switching_output)
            .unwrap();
        stable
            .process_interleaved(&silence, &mut stable_output)
            .unwrap();
        for (&actual, &expected) in switching_output.iter().zip(&stable_output) {
            assert!(actual.is_finite());
            assert!((actual - expected).abs() <= 2.0e-5);
        }
    }
}

#[test]
fn switching_reuses_input_history_that_arrived_before_selection() {
    let old = hesuvi_response(1, &[(6, 0, 1.0), (13, 0, 1.0)]);
    let new = hesuvi_response(PARTITION_FRAMES + 2, &[(6, PARTITION_FRAMES + 1, 0.8)]);
    let engine = engine_with_responses(old, new, hesuvi_response(1, &[]), 4);
    let mut input = vec![0.0f32; PARTITION_FRAMES * SOURCE_CHANNELS];
    input[2] = 1.0;
    let mut output = vec![0.0f32; PARTITION_FRAMES * 2];

    engine.process_interleaved(&input, &mut output).unwrap();
    engine.select_hrtf(HrtfPresetId::AstreaDolby).unwrap();
    engine
        .process_interleaved(&vec![0.0f32; input.len()], &mut output)
        .unwrap();
    engine
        .process_interleaved(&vec![0.0f32; input.len()], &mut output)
        .unwrap();

    assert!((output[2] - 0.2).abs() < 2.0e-5);
}

#[test]
fn new_bank_matches_expected_response_to_pre_switch_samples() {
    let old = hesuvi_response(1, &[]);
    let new = hesuvi_response(PARTITION_FRAMES * 2 + 1, &[(6, PARTITION_FRAMES * 2, 0.6)]);
    let engine = engine_with_responses(old, new, hesuvi_response(1, &[]), 2);
    let mut input = vec![0.0f32; PARTITION_FRAMES * SOURCE_CHANNELS];
    input[2] = 0.5;
    let mut output = vec![0.0f32; PARTITION_FRAMES * 2];
    engine.process_interleaved(&input, &mut output).unwrap();
    engine.select_hrtf(HrtfPresetId::AstreaDolby).unwrap();

    for _ in 0..3 {
        engine
            .process_interleaved(&vec![0.0f32; input.len()], &mut output)
            .unwrap();
    }

    assert!((output[0] - 0.3).abs() < 2.0e-5);
}

#[test]
fn long_and_short_banks_share_one_history_geometry() {
    let short = hesuvi_response(16, &[(6, 0, 1.0), (13, 0, 1.0)]);
    let long = hesuvi_response(PARTITION_FRAMES * 9, &[(6, PARTITION_FRAMES * 8, 0.5)]);
    let mut banks = all_presets_with(short.clone());
    banks[HrtfPresetId::AstreaDolby as usize] = (HrtfPresetId::AstreaDolby, long);
    let engine = RealtimeSpatialEngine::with_prepared_banks(banks, PARTITION_FRAMES, 480).unwrap();

    assert_eq!(engine.partition_geometry().input_history_partitions, 9);
    assert_eq!(
        engine.partition_geometry(),
        engine.bank_partition_geometry(HrtfPresetId::AstreaDolby)
    );
    assert_eq!(
        engine.bank_partition_geometry(HrtfPresetId::Astrea),
        engine.partition_geometry()
    );
}

#[test]
fn short_bank_renders_its_expected_convolution_output() {
    let response = hesuvi_response(16, &[(6, 0, 0.5), (13, 0, 0.25)]);
    let engine = engine_with_responses(response.clone(), response.clone(), response, 1);
    let mut input = vec![0.0f32; PARTITION_FRAMES * SOURCE_CHANNELS];
    input[SourceChannel::FrontCenter.index()] = 1.0;
    let silence = vec![0.0f32; input.len()];
    let mut output = vec![0.0f32; PARTITION_FRAMES * 2];

    engine.process_interleaved(&input, &mut output).unwrap();
    engine.process_interleaved(&silence, &mut output).unwrap();

    assert!((output[0] - 0.5).abs() < 2.0e-5);
    assert!((output[1] - 0.25).abs() < 2.0e-5);
}

#[test]
fn switching_from_long_active_bank_to_short_target_reuses_history_without_shrinking_it() {
    let long = hesuvi_response(PARTITION_FRAMES * 3, &[(6, PARTITION_FRAMES * 2, 0.5)]);
    let short = hesuvi_response(PARTITION_FRAMES, &[(6, 1, 0.25)]);
    let engine = engine_with_responses(long, short.clone(), short, 1);
    let geometry = engine.partition_geometry();
    assert_eq!(geometry.input_history_partitions, 3);

    let mut input = vec![0.0f32; PARTITION_FRAMES * SOURCE_CHANNELS];
    input[(PARTITION_FRAMES - 1) * SOURCE_CHANNELS + SourceChannel::FrontCenter.index()] = 1.0;
    let silence = vec![0.0f32; input.len()];
    let mut output = vec![0.0f32; PARTITION_FRAMES * 2];
    engine.process_interleaved(&input, &mut output).unwrap();
    engine.select_hrtf(HrtfPresetId::AstreaDolby).unwrap();
    engine.process_interleaved(&silence, &mut output).unwrap();
    engine.process_interleaved(&silence, &mut output).unwrap();

    assert_eq!(engine.active_hrtf(), Some(HrtfPresetId::AstreaDolby));
    assert_eq!(engine.partition_geometry(), geometry);
    assert!(
        (output[0] - 0.25).abs() < 2.0e-5,
        "first output frames: {:?}",
        &output[..16]
    );
}

#[test]
fn process_uses_no_heap_allocations_after_initialization() {
    let short = hesuvi_response(16, &[(6, 0, 1.0), (13, 0, 1.0)]);
    let long = hesuvi_response(PARTITION_FRAMES * 4, &[(6, PARTITION_FRAMES * 3, 0.5)]);
    let engine = engine_with_responses(short.clone(), long, short.clone(), 480);
    let input: [Vec<f32>; SOURCE_CHANNELS] =
        std::array::from_fn(|_| vec![0.0f32; PARTITION_FRAMES * 4]);
    let input_refs: [&[f32]; SOURCE_CHANNELS] =
        std::array::from_fn(|channel| input[channel].as_slice());
    let mut left = vec![0.0f32; PARTITION_FRAMES * 4];
    let mut right = vec![0.0f32; PARTITION_FRAMES * 4];
    let mut output_refs = [left.as_mut_slice(), right.as_mut_slice()];
    engine
        .process_planar(&input_refs, &mut output_refs)
        .unwrap();
    engine.select_hrtf(HrtfPresetId::AstreaDolby).unwrap();
    ALLOCATION_COUNT.store(0, Ordering::Relaxed);
    TRACK_ALLOCATIONS.with(|tracking| tracking.set(true));
    engine
        .process_planar(&input_refs, &mut output_refs)
        .unwrap();
    TRACK_ALLOCATIONS.with(|tracking| tracking.set(false));

    assert_eq!(ALLOCATION_COUNT.load(Ordering::Relaxed), 0);
}

#[test]
fn transition_duration_tracks_frames_across_varying_callback_sizes() {
    let response = hesuvi_response(1, &[(6, 0, 1.0), (13, 0, 1.0)]);
    let engine = engine_with_responses(response.clone(), response.clone(), response, 480);
    engine.select_hrtf(HrtfPresetId::AstreaDolby).unwrap();
    let callback_sizes = [73, 211, 7, 189, PARTITION_FRAMES];
    for frames in callback_sizes {
        let input = vec![0.0f32; frames * SOURCE_CHANNELS];
        let mut output = vec![0.0f32; frames * 2];
        engine.process_interleaved(&input, &mut output).unwrap();
    }
    assert_eq!(engine.active_hrtf(), Some(HrtfPresetId::AstreaDolby));
    assert_eq!(engine.switch_state().phase, SwitchPhase::Stable);
}

#[test]
fn samples_after_a_mid_partition_fade_end_stay_on_the_promoted_bank() {
    let old = hesuvi_response(1, &[(6, 0, 1.0), (13, 0, 1.0)]);
    let new = hesuvi_response(1, &[(6, 0, 2.0), (13, 0, 2.0)]);
    let engine = engine_with_responses(old, new, hesuvi_response(1, &[]), 480);
    engine.select_hrtf(HrtfPresetId::AstreaDolby).unwrap();

    let frames = PARTITION_FRAMES + 480 + 32;
    let mut input = vec![0.0f32; frames * SOURCE_CHANNELS];
    for frame in 0..frames {
        input[frame * SOURCE_CHANNELS + SourceChannel::FrontCenter.index()] = 1.0;
    }
    let mut output = vec![0.0f32; frames * 2];
    engine.process_interleaved(&input, &mut output).unwrap();

    assert_eq!(engine.active_hrtf(), Some(HrtfPresetId::AstreaDolby));
    assert!((output[(PARTITION_FRAMES + 480) * 2] - 2.0).abs() < 2.0e-5);
    assert!((output[(PARTITION_FRAMES + 481) * 2] - 2.0).abs() < 2.0e-5);
}
