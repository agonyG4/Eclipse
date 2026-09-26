use rustfft::Fft;
use rustfft::num_complex::Complex;

use super::hrtf::{HrtfImpulseResponse, SourceChannel};

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct PartitionGeometry {
    pub partition_frames: usize,
    pub input_history_partitions: usize,
    pub fft_size: usize,
    pub spectrum_bins: usize,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum KernelPrepareError {
    InvalidGeometry,
    InvalidResponse,
}

pub struct KernelBank {
    spectra: Vec<Complex<f32>>,
    geometry: PartitionGeometry,
}

impl KernelBank {
    pub(super) fn prepare(
        response: &HrtfImpulseResponse,
        geometry: PartitionGeometry,
        forward: &dyn Fft<f32>,
    ) -> Result<Self, KernelPrepareError> {
        let frames = geometry.partition_frames;
        if frames == 0
            || geometry.fft_size != frames * 2
            || geometry.spectrum_bins != frames + 1
            || geometry.input_history_partitions == 0
            || response.length() == 0
        {
            return Err(KernelPrepareError::InvalidGeometry);
        }

        let count = geometry.input_history_partitions;
        let bins = geometry.spectrum_bins;
        let mut spectra = vec![Complex::new(0.0, 0.0); 8 * 2 * count * bins];
        let mut scratch = vec![Complex::new(0.0, 0.0); geometry.fft_size];
        let mut plan_scratch = vec![Complex::new(0.0, 0.0); forward.get_inplace_scratch_len()];

        for source in SourceChannel::ALL {
            for ear in 0..2 {
                let coefficients = response.channel(source.index(), ear);
                for partition in 0..count {
                    scratch.fill(Complex::new(0.0, 0.0));
                    let start = partition * frames;
                    if let Some(partition_coefficients) =
                        coefficients.get(start..coefficients.len().min(start + frames))
                    {
                        for (index, coefficient) in partition_coefficients.iter().enumerate() {
                            if !coefficient.is_finite() {
                                return Err(KernelPrepareError::InvalidResponse);
                            }
                            scratch[index].re = *coefficient;
                        }
                    }
                    forward.process_with_scratch(&mut scratch, &mut plan_scratch);
                    let bank_start = ((source.index() * 2 + ear) * count + partition) * bins;
                    spectra[bank_start..bank_start + bins].copy_from_slice(&scratch[..bins]);
                }
            }
        }

        Ok(Self { spectra, geometry })
    }

    fn partition(&self, source: usize, ear: usize, partition: usize) -> &[Complex<f32>] {
        let bins = self.geometry.spectrum_bins;
        let start =
            ((source * 2 + ear) * self.geometry.input_history_partitions + partition) * bins;
        &self.spectra[start..start + bins]
    }
}

pub(super) struct PartitionedConvolver {
    geometry: PartitionGeometry,
    input_block: Vec<f32>,
    input_fill: usize,
    forward_scratch: Vec<Complex<f32>>,
    history: Vec<Complex<f32>>,
    history_write: usize,
    history_blocks: usize,
    accumulation: Vec<Complex<f32>>,
    inverse_scratch: Vec<Complex<f32>>,
    fft_plan_scratch: Vec<Complex<f32>>,
    overlap_active: Vec<f32>,
    overlap_target: Vec<f32>,
    render_active: Vec<f32>,
    render_target: Vec<f32>,
    output_cursor: usize,
    output_ready: bool,
    output_transition: bool,
}

impl PartitionedConvolver {
    pub(super) fn new(
        geometry: PartitionGeometry,
        forward_scratch_len: usize,
        inverse_scratch_len: usize,
    ) -> Self {
        let frames = geometry.partition_frames;
        let bins = geometry.spectrum_bins;
        Self {
            geometry,
            input_block: vec![0.0; 8 * frames],
            input_fill: 0,
            forward_scratch: vec![Complex::new(0.0, 0.0); 8 * geometry.fft_size],
            history: vec![Complex::new(0.0, 0.0); 8 * geometry.input_history_partitions * bins],
            history_write: 0,
            history_blocks: 0,
            accumulation: vec![Complex::new(0.0, 0.0); geometry.fft_size],
            inverse_scratch: vec![Complex::new(0.0, 0.0); geometry.fft_size],
            fft_plan_scratch: vec![
                Complex::new(0.0, 0.0);
                forward_scratch_len.max(inverse_scratch_len)
            ],
            overlap_active: vec![0.0; frames * 2],
            overlap_target: vec![0.0; frames * 2],
            render_active: vec![0.0; frames * 2],
            render_target: vec![0.0; frames * 2],
            output_cursor: 0,
            output_ready: false,
            output_transition: false,
        }
    }

    pub(super) fn begin_target(
        &mut self,
        preset: super::hrtf::HrtfPresetId,
        registry: &super::registry::PreparedBankRegistry,
    ) {
        self.overlap_target.fill(0.0);
        if self.history_blocks == 0 {
            return;
        }
        if let Some(bank) = registry.bank(preset) {
            self.render_bank(bank, true, registry.inverse());
        }
    }

    pub(super) fn swap_target_to_active(&mut self) {
        std::mem::swap(&mut self.overlap_active, &mut self.overlap_target);
    }

    pub(super) fn pop_output_frame(&self) -> Option<([f32; 2], [f32; 2])> {
        if !self.output_ready || self.output_cursor >= self.geometry.partition_frames {
            return None;
        }
        let index = self.output_cursor * 2;
        Some((
            [self.render_active[index], self.render_active[index + 1]],
            [self.render_target[index], self.render_target[index + 1]],
        ))
    }

    pub(super) fn is_last_input_frame(&self) -> bool {
        self.input_fill + 1 == self.geometry.partition_frames
    }

    pub(super) fn finish_output_frame(&mut self) {
        self.output_cursor += 1;
        if self.output_cursor == self.geometry.partition_frames {
            self.output_cursor = 0;
            self.output_ready = false;
        }
    }

    pub(super) fn output_is_transition(&self) -> bool {
        self.output_transition
    }

    pub(super) fn ingest_frame(
        &mut self,
        input: &[f32; 8],
        active: super::hrtf::HrtfPresetId,
        target: Option<super::hrtf::HrtfPresetId>,
        registry: &super::registry::PreparedBankRegistry,
    ) {
        let frame = self.input_fill;
        for (source, sample) in input.iter().enumerate() {
            self.input_block[source * self.geometry.partition_frames + frame] = *sample;
        }
        self.input_fill += 1;
        if self.input_fill != self.geometry.partition_frames {
            return;
        }

        self.transform_input(registry.forward());
        if let Some(bank) = registry.bank(active) {
            self.render_bank(bank, false, registry.inverse());
        } else {
            self.render_active.fill(0.0);
        }
        if let Some(target_id) = target {
            if let Some(bank) = registry.bank(target_id) {
                self.render_bank(bank, true, registry.inverse());
            } else {
                self.render_target.fill(0.0);
            }
        } else {
            self.render_target.fill(0.0);
        }
        self.input_fill = 0;
        self.output_cursor = 0;
        self.output_ready = true;
        self.output_transition = target.is_some();
    }

    fn transform_input(&mut self, forward: &dyn Fft<f32>) {
        let frames = self.geometry.partition_frames;
        let bins = self.geometry.spectrum_bins;
        for source in 0..8 {
            let scratch_start = source * self.geometry.fft_size;
            let scratch =
                &mut self.forward_scratch[scratch_start..scratch_start + self.geometry.fft_size];
            scratch.fill(Complex::new(0.0, 0.0));
            for (frame, bin) in scratch.iter_mut().take(frames).enumerate() {
                bin.re = self.input_block[source * frames + frame];
            }
            forward.process_with_scratch(scratch, &mut self.fft_plan_scratch);
            let history_start =
                (source * self.geometry.input_history_partitions + self.history_write) * bins;
            self.history[history_start..history_start + bins].copy_from_slice(&scratch[..bins]);
        }
        self.history_write = (self.history_write + 1) % self.geometry.input_history_partitions;
        self.history_blocks = (self.history_blocks + 1).min(self.geometry.input_history_partitions);
    }

    fn render_bank(&mut self, bank: &KernelBank, target: bool, inverse: &dyn Fft<f32>) {
        let geometry = self.geometry;
        let bins = geometry.spectrum_bins;
        let parts = geometry.input_history_partitions;
        let history_latest = if self.history_blocks == 0 {
            None
        } else {
            Some((self.history_write + parts - 1) % parts)
        };

        for ear in 0..2 {
            self.accumulation.fill(Complex::new(0.0, 0.0));
            if let Some(latest) = history_latest {
                for source in 0..8 {
                    for partition in 0..parts {
                        if partition >= self.history_blocks {
                            break;
                        }
                        let history_slot = (latest + parts - partition) % parts;
                        let history_start = (source * parts + history_slot) * bins;
                        let input_spectrum = &self.history[history_start..history_start + bins];
                        let kernel = bank.partition(source, ear, partition);
                        for bin in 0..bins {
                            self.accumulation[bin] += input_spectrum[bin] * kernel[bin];
                        }
                    }
                }
            }

            self.inverse_scratch.fill(Complex::new(0.0, 0.0));
            self.inverse_scratch[..bins].copy_from_slice(&self.accumulation[..bins]);
            for bin in 1..geometry.partition_frames {
                self.inverse_scratch[geometry.fft_size - bin] = self.accumulation[bin].conj();
            }
            inverse.process_with_scratch(&mut self.inverse_scratch, &mut self.fft_plan_scratch);
            let scale = 1.0 / geometry.fft_size as f32;
            for frame in 0..geometry.partition_frames {
                let output_index = frame * 2 + ear;
                let old_overlap = if target {
                    self.overlap_target[output_index]
                } else {
                    self.overlap_active[output_index]
                };
                let rendered = self.inverse_scratch[frame].re * scale + old_overlap;
                let tail = self.inverse_scratch[frame + geometry.partition_frames].re * scale;
                if target {
                    self.render_target[output_index] = rendered;
                    self.overlap_target[output_index] = tail;
                } else {
                    self.render_active[output_index] = rendered;
                    self.overlap_active[output_index] = tail;
                }
            }
        }
    }
}
