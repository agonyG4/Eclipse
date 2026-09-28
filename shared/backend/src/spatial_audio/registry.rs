use std::path::Path;
use std::sync::Arc;

use rustfft::{Fft, FftPlanner};

use super::convolution::{KernelBank, PartitionGeometry};
use super::hrtf::{HrtfError, HrtfImpulseResponse, HrtfPresetId};

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum PrepareError {
    MissingRequiredPreset,
    InvalidPartitionGeometry,
    DuplicatePreset,
    KernelPreparationFailed,
}

pub struct PreparedBankRegistry {
    slots: [Option<Box<KernelBank>>; 8],
    geometry: PartitionGeometry,
    forward: Arc<dyn Fft<f32>>,
    inverse: Arc<dyn Fft<f32>>,
    failed_asset_mask: u8,
}

impl PreparedBankRegistry {
    pub fn from_responses(
        banks: Vec<(HrtfPresetId, HrtfImpulseResponse)>,
        partition_frames: usize,
    ) -> Result<Self, PrepareError> {
        let mut seen = [false; 8];
        let mut maximum_ir_frames = 1;
        for (preset, response) in &banks {
            if seen[preset.index()] {
                return Err(PrepareError::DuplicatePreset);
            }
            seen[preset.index()] = true;
            maximum_ir_frames = maximum_ir_frames.max(response.effective_length());
        }
        if !seen[HrtfPresetId::Astrea.index()] {
            return Err(PrepareError::MissingRequiredPreset);
        }
        if partition_frames == 0 || partition_frames > u16::MAX as usize {
            return Err(PrepareError::InvalidPartitionGeometry);
        }

        let partition_count = maximum_ir_frames.div_ceil(partition_frames);
        let fft_size = partition_frames * 2;
        let geometry = PartitionGeometry {
            partition_frames,
            input_history_partitions: partition_count,
            fft_size,
            spectrum_bins: partition_frames + 1,
        };

        let mut planner = FftPlanner::<f32>::new();
        let forward = planner.plan_fft_forward(fft_size);
        let inverse = planner.plan_fft_inverse(fft_size);
        let mut slots: [Option<Box<KernelBank>>; 8] = std::array::from_fn(|_| None);

        for (preset, response) in banks {
            let bank = KernelBank::prepare(&response, geometry, forward.as_ref())
                .map_err(|_| PrepareError::KernelPreparationFailed)?;
            slots[preset.index()] = Some(Box::new(bank));
        }

        Ok(Self {
            slots,
            geometry,
            forward,
            inverse,
            failed_asset_mask: 0,
        })
    }

    pub fn from_asset_directory(
        directory: &Path,
        partition_frames: usize,
    ) -> Result<Self, PrepareError> {
        let mut responses = Vec::with_capacity(HrtfPresetId::ALL.len());
        let mut failed_asset_mask = 0;
        let required_path = directory.join(format!("{}.wav", HrtfPresetId::Astrea.as_str()));
        let required_response = HrtfImpulseResponse::from_wav(&required_path)
            .map_err(|_| PrepareError::MissingRequiredPreset)?;
        responses.push((HrtfPresetId::Astrea, required_response));

        for preset in HrtfPresetId::ALL {
            if preset == HrtfPresetId::Astrea {
                continue;
            }
            let path = directory.join(format!("{}.wav", preset.as_str()));
            match HrtfImpulseResponse::from_wav(&path) {
                Ok(response) => responses.push((preset, response)),
                Err(_) => failed_asset_mask |= 1 << preset.index(),
            }
        }

        let mut registry = Self::from_responses(responses, partition_frames)?;
        registry.failed_asset_mask = failed_asset_mask;
        Ok(registry)
    }

    pub fn contains(&self, preset: HrtfPresetId) -> bool {
        self.slots[preset.index()].is_some()
    }

    pub fn geometry(&self) -> PartitionGeometry {
        self.geometry
    }

    pub fn bank_geometry(&self, _preset: HrtfPresetId) -> PartitionGeometry {
        self.geometry
    }

    pub fn failed_asset_mask(&self) -> u8 {
        self.failed_asset_mask
    }

    pub(crate) fn bank(&self, preset: HrtfPresetId) -> Option<&KernelBank> {
        self.slots[preset.index()].as_deref()
    }

    pub(crate) fn forward(&self) -> &dyn Fft<f32> {
        self.forward.as_ref()
    }

    pub(crate) fn inverse(&self) -> &dyn Fft<f32> {
        self.inverse.as_ref()
    }
}

impl From<HrtfError> for PrepareError {
    fn from(_: HrtfError) -> Self {
        Self::KernelPreparationFailed
    }
}
