//! Realtime-safe prepared HRTF selection and partitioned spatial convolution.

mod convolution;
mod ffi;
mod hrtf;
mod registry;
mod switching;

pub use convolution::{KernelPrepareError, PartitionGeometry};
pub use hrtf::{
    BUILTIN_HRTF_ASSET_DIR, ENGINE_SAMPLE_RATE, Ear, HESUVI_CHANNEL_COUNT, HrtfError,
    HrtfImpulseResponse, HrtfPresetId, SourceChannel,
};
pub use registry::{PrepareError, PreparedBankRegistry};
pub use switching::{
    DEFAULT_TRANSITION_FRAMES, HrtfSwitchState, ProcessError, RealtimeSpatialEngine,
    SelectionError, SelectionResult, SwitchPhase, crossfade_gains,
};
