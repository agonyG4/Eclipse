use std::path::Path;
use std::slice;

use super::hrtf::{BUILTIN_HRTF_ASSET_DIR, HrtfPresetId};
use super::switching::RealtimeSpatialEngine;

const RESULT_OK: i32 = 0;
const RESULT_INVALID_ARGUMENT: i32 = 1;
const RESULT_UNAVAILABLE_PRESET: i32 = 2;
const RESULT_PROCESS_REJECTED: i32 = 3;
const RESULT_PREPARATION_FAILED: i32 = 4;
const NONE_PRESET: u8 = u8::MAX;

#[repr(C)]
pub struct SpatialAudioHandle {
    engine: RealtimeSpatialEngine,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SpatialAudioState {
    pub active_preset: u8,
    pub target_preset: u8,
    pub queued_preset: u8,
    pub transitioning: u8,
    pub generation: u64,
    pub progress_frames: u64,
    pub transition_frames: u64,
}

/// Builds the engine and prepares the externally installed HRTF assets.
///
/// # Safety
/// If non-null, `error_code` must point to writable storage for one `i32`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn astrea_spatial_audio_create_default(
    error_code: *mut i32,
) -> *mut SpatialAudioHandle {
    if !error_code.is_null() {
        // SAFETY: the optional result pointer is supplied by the C++ control thread.
        unsafe { error_code.write(RESULT_OK) };
    }
    let engine =
        match RealtimeSpatialEngine::from_asset_directory(Path::new(BUILTIN_HRTF_ASSET_DIR), 128) {
            Ok(engine) => engine,
            Err(_) => {
                if !error_code.is_null() {
                    // SAFETY: the optional result pointer is supplied by the C++ control thread.
                    unsafe { error_code.write(RESULT_PREPARATION_FAILED) };
                }
                return std::ptr::null_mut();
            }
        };
    Box::into_raw(Box::new(SpatialAudioHandle { engine }))
}

/// # Safety
/// `handle` must be null or a live handle returned by `astrea_spatial_audio_create_default`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn astrea_spatial_audio_destroy(handle: *mut SpatialAudioHandle) {
    if !handle.is_null() {
        // SAFETY: the caller guarantees exclusive ownership after the PipeWire host has stopped.
        unsafe { drop(Box::from_raw(handle)) };
    }
}

/// Returns 0 on success, 1 for an invalid preset id, 2 for an unavailable prepared preset.
///
/// # Safety
/// `handle` must be null or point to a live handle returned by
/// `astrea_spatial_audio_create_default`. If non-null, `generation_out` must point to writable
/// storage for one `u64`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn astrea_spatial_audio_select(
    handle: *mut SpatialAudioHandle,
    preset_id: u8,
    generation_out: *mut u64,
) -> i32 {
    let Some(handle) = (unsafe { handle.as_ref() }) else {
        return RESULT_INVALID_ARGUMENT;
    };
    let Some(preset) = HrtfPresetId::from_raw(preset_id) else {
        return RESULT_INVALID_ARGUMENT;
    };
    match handle.engine.select_hrtf(preset) {
        Ok(result) => {
            if !generation_out.is_null() {
                // SAFETY: the optional result pointer is provided by the control caller.
                unsafe { generation_out.write(result.generation) };
            }
            RESULT_OK
        }
        Err(_) => RESULT_UNAVAILABLE_PRESET,
    }
}

/// # Safety
/// `handle` must be null or point to a live handle returned by
/// `astrea_spatial_audio_create_default`. If non-null, `state_out` must be writable for one
/// `SpatialAudioState`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn astrea_spatial_audio_read_state(
    handle: *mut SpatialAudioHandle,
    state_out: *mut SpatialAudioState,
) -> i32 {
    let Some(handle) = (unsafe { handle.as_ref() }) else {
        return RESULT_INVALID_ARGUMENT;
    };
    if state_out.is_null() {
        return RESULT_INVALID_ARGUMENT;
    }
    let state = handle.engine.switch_state();
    let ffi_state = SpatialAudioState {
        active_preset: state.active.map_or(NONE_PRESET, |preset| preset as u8),
        target_preset: state.target.map_or(NONE_PRESET, |preset| preset as u8),
        queued_preset: state.queued.map_or(NONE_PRESET, |preset| preset as u8),
        transitioning: u8::from(state.target.is_some()),
        generation: state.generation,
        progress_frames: state.progress_frames as u64,
        transition_frames: state.transition_frames as u64,
    };
    // SAFETY: the output pointer was checked and is provided by the non-realtime caller.
    unsafe { state_out.write(ffi_state) };
    RESULT_OK
}

/// Processes planar 8-channel input and planar stereo output. Error results must silence outputs.
///
/// # Safety
/// `handle` must be null or point to a live handle returned by
/// `astrea_spatial_audio_create_default`. For non-zero `frames`, each pointer array must contain
/// the declared number of valid, disjoint channel buffers. The buffers must remain valid for the
/// duration of this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn astrea_spatial_audio_process_planar(
    handle: *mut SpatialAudioHandle,
    input_channels: *const *const f32,
    output_channels: *const *mut f32,
    frames: usize,
) -> i32 {
    let Some(handle) = (unsafe { handle.as_ref() }) else {
        return RESULT_INVALID_ARGUMENT;
    };
    if frames == 0 {
        return RESULT_OK;
    }
    if input_channels.is_null() || output_channels.is_null() {
        return RESULT_INVALID_ARGUMENT;
    }
    // SAFETY: both pointer arrays must contain at least 8 and 2 elements respectively.
    let has_null_channel = unsafe {
        (0..8).any(|index| (*input_channels.add(index)).is_null())
            || (0..2).any(|index| (*output_channels.add(index)).is_null())
    };
    if has_null_channel {
        return RESULT_INVALID_ARGUMENT;
    }
    // SAFETY: pointer validity and channel buffer extents are part of this function's contract.
    let inputs: [&[f32]; 8] = unsafe {
        std::array::from_fn(|index| {
            let pointer = *input_channels.add(index);
            slice::from_raw_parts(pointer, frames)
        })
    };
    // SAFETY: output pointers must address writable, pairwise-disjoint buffers as documented.
    let mut outputs: [&mut [f32]; 2] = unsafe {
        std::array::from_fn(|index| {
            let pointer = *output_channels.add(index);
            slice::from_raw_parts_mut(pointer, frames)
        })
    };
    match handle.engine.process_planar(&inputs, &mut outputs) {
        Ok(()) => RESULT_OK,
        Err(_) => RESULT_PROCESS_REJECTED,
    }
}
