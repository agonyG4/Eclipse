use std::path::Path;

use hound::{SampleFormat, WavReader};

pub const BUILTIN_HRTF_ASSET_DIR: &str = "/usr/share/astrea/hrtf";
pub const HESUVI_CHANNEL_COUNT: usize = 14;
pub const ENGINE_SAMPLE_RATE: u32 = 48_000;

#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
#[repr(u8)]
pub enum HrtfPresetId {
    Astrea = 0,
    AstreaMovie = 1,
    AstreaHalf = 2,
    AstreaShort = 3,
    AstreaLatency = 4,
    AstreaDolby = 5,
    AstreaDolbyHalf = 6,
    AstreaDolbyLatency = 7,
}

impl HrtfPresetId {
    pub const ALL: [Self; 8] = [
        Self::Astrea,
        Self::AstreaMovie,
        Self::AstreaHalf,
        Self::AstreaShort,
        Self::AstreaLatency,
        Self::AstreaDolby,
        Self::AstreaDolbyHalf,
        Self::AstreaDolbyLatency,
    ];

    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Astrea => "astrea",
            Self::AstreaMovie => "astrea-movie",
            Self::AstreaHalf => "astrea-half",
            Self::AstreaShort => "astrea-short",
            Self::AstreaLatency => "astrea-latency",
            Self::AstreaDolby => "astrea-dolby",
            Self::AstreaDolbyHalf => "astrea-dolby-half",
            Self::AstreaDolbyLatency => "astrea-dolby-latency",
        }
    }

    pub const fn index(self) -> usize {
        self as usize
    }

    pub const fn from_raw(value: u8) -> Option<Self> {
        match value {
            0 => Some(Self::Astrea),
            1 => Some(Self::AstreaMovie),
            2 => Some(Self::AstreaHalf),
            3 => Some(Self::AstreaShort),
            4 => Some(Self::AstreaLatency),
            5 => Some(Self::AstreaDolby),
            6 => Some(Self::AstreaDolbyHalf),
            7 => Some(Self::AstreaDolbyLatency),
            _ => None,
        }
    }

    pub fn from_name(value: &str) -> Option<Self> {
        Self::ALL
            .into_iter()
            .find(|preset| preset.as_str() == value)
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[repr(u8)]
pub enum SourceChannel {
    FrontLeft = 0,
    FrontRight = 1,
    FrontCenter = 2,
    Lfe = 3,
    RearLeft = 4,
    RearRight = 5,
    SideLeft = 6,
    SideRight = 7,
}

impl SourceChannel {
    pub const ALL: [Self; 8] = [
        Self::FrontLeft,
        Self::FrontRight,
        Self::FrontCenter,
        Self::Lfe,
        Self::RearLeft,
        Self::RearRight,
        Self::SideLeft,
        Self::SideRight,
    ];

    pub const fn index(self) -> usize {
        self as usize
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[repr(u8)]
pub enum Ear {
    Left = 0,
    Right = 1,
}

impl Ear {
    const fn index(self) -> usize {
        self as usize
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum HrtfError {
    InvalidChannelCount,
    UnsupportedSampleRate,
    EmptyImpulseResponse,
    MalformedInterleaving,
    NonFiniteCoefficient,
    InvalidPcmBitDepth,
    InvalidWaveData,
}

#[derive(Clone, Debug)]
pub struct HrtfImpulseResponse {
    channels: [Vec<f32>; 16],
    length: usize,
}

impl HrtfImpulseResponse {
    pub fn from_hesuvi_interleaved(
        samples: &[f32],
        channel_count: usize,
        sample_rate: u32,
    ) -> Result<Self, HrtfError> {
        if channel_count != HESUVI_CHANNEL_COUNT {
            return Err(HrtfError::InvalidChannelCount);
        }
        if sample_rate != ENGINE_SAMPLE_RATE {
            return Err(HrtfError::UnsupportedSampleRate);
        }
        if samples.is_empty() {
            return Err(HrtfError::EmptyImpulseResponse);
        }
        if !samples.len().is_multiple_of(HESUVI_CHANNEL_COUNT) {
            return Err(HrtfError::MalformedInterleaving);
        }
        if samples.iter().any(|sample| !sample.is_finite()) {
            return Err(HrtfError::NonFiniteCoefficient);
        }

        let length = samples.len() / HESUVI_CHANNEL_COUNT;
        let mut channels: [Vec<f32>; 16] = std::array::from_fn(|_| Vec::with_capacity(length));

        // HeSuVi stores the left-ear set first and places FC again at channel 14.
        // LFE deliberately aliases the same FC pair in the canonical engine matrix.
        let source_channel_pairs = [
            (0, 1),   // FL left, FL right
            (8, 7),   // FR left, FR right
            (6, 13),  // FC left, FC right
            (6, 13),  // LFE reuses FC
            (4, 5),   // RL left, RL right
            (12, 11), // RR left, RR right
            (2, 3),   // SL left, SL right
            (10, 9),  // SR left, SR right
        ];

        for frame in samples.chunks_exact(HESUVI_CHANNEL_COUNT) {
            for (source, (left_source, right_source)) in
                source_channel_pairs.into_iter().enumerate()
            {
                channels[source * 2].push(frame[left_source]);
                channels[source * 2 + 1].push(frame[right_source]);
            }
        }

        Ok(Self { channels, length })
    }

    pub fn from_wav(path: &Path) -> Result<Self, HrtfError> {
        let mut reader = WavReader::open(path).map_err(|_| HrtfError::InvalidWaveData)?;
        let spec = reader.spec();
        if usize::from(spec.channels) != HESUVI_CHANNEL_COUNT {
            return Err(HrtfError::InvalidChannelCount);
        }
        if spec.sample_rate != ENGINE_SAMPLE_RATE {
            return Err(HrtfError::UnsupportedSampleRate);
        }

        let samples = match spec.sample_format {
            SampleFormat::Float => reader
                .samples::<f32>()
                .collect::<Result<Vec<_>, _>>()
                .map_err(|_| HrtfError::InvalidWaveData)?,
            SampleFormat::Int => {
                if spec.bits_per_sample == 0 || spec.bits_per_sample > 32 {
                    return Err(HrtfError::InvalidPcmBitDepth);
                }
                let scale = 2.0_f32.powi(i32::from(spec.bits_per_sample) - 1);
                reader
                    .samples::<i32>()
                    .map(|sample| {
                        sample
                            .map(|value| value as f32 / scale)
                            .map_err(|_| HrtfError::InvalidWaveData)
                    })
                    .collect::<Result<Vec<_>, _>>()?
            }
        };

        Self::from_hesuvi_interleaved(&samples, HESUVI_CHANNEL_COUNT, spec.sample_rate)
    }

    pub fn coefficient(&self, source: SourceChannel, ear: Ear, frame: usize) -> Option<f32> {
        (frame < self.length).then(|| self.channels[source.index() * 2 + ear.index()][frame])
    }

    pub const fn length(&self) -> usize {
        self.length
    }

    /// Returns the last exactly non-zero canonical coefficient frame, or one for a valid
    /// all-zero response. Tiny finite values remain significant.
    pub fn effective_length(&self) -> usize {
        self.channels
            .iter()
            .filter_map(|channel| {
                channel
                    .iter()
                    .rposition(|coefficient| *coefficient != 0.0)
                    .map(|frame| frame + 1)
            })
            .max()
            .unwrap_or(1)
    }

    pub(crate) fn channel(&self, source: usize, ear: usize) -> &[f32] {
        &self.channels[source * 2 + ear]
    }
}
