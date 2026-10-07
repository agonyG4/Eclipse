use serde::{Deserialize, Serialize};
use std::collections::HashSet;

fn deserialize_required_option<'de, D, T>(deserializer: D) -> Result<Option<T>, D::Error>
where
    D: serde::Deserializer<'de>,
    T: Deserialize<'de>,
{
    Option::<T>::deserialize(deserializer)
}

pub const MAX_CONTROL_OUTPUTS: usize = 32;
pub const MAX_CONTROL_OUTPUT_MODES: usize = 128;
pub const OUTPUT_CONFIRMATION_TIMEOUT_MS: u32 = 15_000;

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum OutputRequest {
    Get,
    Configure(OutputConfigureRequest),
    Confirm(OutputTransactionRequest),
    Revert(OutputTransactionRequest),
}

#[derive(Clone, Debug, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct OutputConfigureRequest {
    pub version: u8,
    pub output_id: String,
    pub base_configuration_generation: u64,
    pub mode_id: u32,
    pub scale_milli: u32,
    pub transform: OutputTransform,
}

impl OutputConfigureRequest {
    pub fn validate(&self) -> Result<(), OutputSnapshotError> {
        if self.version != 1
            || self.output_id.is_empty()
            || self.base_configuration_generation == 0
            || self.mode_id == 0
            || self.scale_milli == 0
        {
            return Err(OutputSnapshotError::InvalidStructure);
        }
        Ok(())
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct OutputTransactionRequest {
    pub version: u8,
    pub transaction_id: u64,
}

impl OutputTransactionRequest {
    pub fn validate(&self) -> Result<(), OutputSnapshotError> {
        if self.version != 1 || self.transaction_id == 0 {
            return Err(OutputSnapshotError::InvalidStructure);
        }
        Ok(())
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq, Hash, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum OutputTransform {
    Normal,
    Rotate90,
    Rotate180,
    Rotate270,
    Flipped,
    Flipped90,
    Flipped180,
    Flipped270,
}

#[derive(Clone, Debug, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct ModeSnapshot {
    pub id: u32,
    pub width: u32,
    pub height: u32,
    pub refresh_millihz: u32,
}

#[derive(Clone, Debug, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct OutputModeSnapshot {
    pub id: u32,
    pub width: u32,
    pub height: u32,
    pub refresh_millihz: u32,
    pub preferred: bool,
    pub interlaced: bool,
}

#[derive(Clone, Debug, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct ScaleMutationCapability {
    pub supported: bool,
    #[serde(deserialize_with = "deserialize_required_option")]
    pub min_milli: Option<u32>,
    #[serde(deserialize_with = "deserialize_required_option")]
    pub max_milli: Option<u32>,
    pub values_milli: Vec<u32>,
}

impl ScaleMutationCapability {
    pub fn supports(&self, value: u32) -> bool {
        if !self.supported {
            return false;
        }
        if !self.values_milli.is_empty() {
            self.values_milli.contains(&value)
        } else {
            self.min_milli.is_some_and(|min| value >= min)
                && self.max_milli.is_some_and(|max| value <= max)
        }
    }

    fn validate(&self) -> bool {
        if !self.supported {
            return self.min_milli.is_none()
                && self.max_milli.is_none()
                && self.values_milli.is_empty();
        }
        let mut values = HashSet::with_capacity(self.values_milli.len());
        if self
            .values_milli
            .iter()
            .any(|value| *value == 0 || !values.insert(*value))
        {
            return false;
        }
        match (self.min_milli, self.max_milli, self.values_milli.is_empty()) {
            (Some(min), Some(max), true) => min != 0 && max >= min,
            (None, None, false) => true,
            _ => false,
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct TransformMutationCapability {
    pub supported: bool,
    pub transforms: Vec<OutputTransform>,
}

impl TransformMutationCapability {
    pub fn supports(&self, value: OutputTransform) -> bool {
        self.supported && self.transforms.contains(&value)
    }

    fn validate(&self) -> bool {
        let mut transforms = HashSet::with_capacity(self.transforms.len());
        let unique = self
            .transforms
            .iter()
            .all(|value| transforms.insert(*value));
        unique && self.supported == !self.transforms.is_empty()
    }
}

#[derive(Clone, Debug, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct OutputMutationCapabilities {
    pub mode_selection_supported: bool,
    pub scale: ScaleMutationCapability,
    pub transform: TransformMutationCapability,
    pub enable_disable_supported: bool,
    pub topology_supported: bool,
    pub vrr_mutation_supported: bool,
}

impl OutputMutationCapabilities {
    pub fn supports(&self, scale_milli: u32, transform: OutputTransform) -> bool {
        self.scale.supports(scale_milli) && self.transform.supports(transform)
    }

    fn validate(&self) -> bool {
        self.scale.validate() && self.transform.validate()
    }
}

#[derive(Clone, Debug, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct PhysicalSizeSnapshot {
    pub width_mm: u32,
    pub height_mm: u32,
}

#[derive(Clone, Debug, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct PositionSnapshot {
    pub x: i32,
    pub y: i32,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum FeatureState {
    Unavailable,
    Configured,
    Available,
    Active,
    Degraded,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct FeatureStateSnapshot {
    pub state: FeatureState,
}

#[derive(Clone, Debug, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct OutputSnapshot {
    pub id: String,
    pub name: String,
    #[serde(deserialize_with = "deserialize_required_option")]
    pub make: Option<String>,
    #[serde(deserialize_with = "deserialize_required_option")]
    pub model: Option<String>,
    #[serde(deserialize_with = "deserialize_required_option")]
    pub serial: Option<String>,
    pub enabled: bool,
    #[serde(deserialize_with = "deserialize_required_option")]
    pub current_mode: Option<ModeSnapshot>,
    #[serde(deserialize_with = "deserialize_required_option")]
    pub physical_size_mm: Option<PhysicalSizeSnapshot>,
    pub configuration_generation: u64,
    pub scale_milli: u32,
    pub transform: OutputTransform,
    pub mutation_capabilities: OutputMutationCapabilities,
    pub position: PositionSnapshot,
    pub focused: bool,
    pub backend: String,
    pub vrr: FeatureStateSnapshot,
    pub direct_scanout: FeatureStateSnapshot,
    pub modes: Vec<OutputModeSnapshot>,
    pub modes_truncated: bool,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum OutputTransactionState {
    PendingConfirmation,
    PersistencePending,
    RollingBack,
    RollbackFailed,
}

#[derive(Clone, Debug, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct OutputTransactionSnapshot {
    pub id: u64,
    pub output_id: String,
    pub state: OutputTransactionState,
    pub applied_configuration_generation: u64,
    pub remaining_ms: u32,
    #[serde(deserialize_with = "deserialize_required_option")]
    pub error_code: Option<String>,
}

#[derive(Clone, Debug, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct OutputListSnapshot {
    pub outputs: Vec<OutputSnapshot>,
    pub total: u32,
    pub truncated: bool,
    #[serde(deserialize_with = "deserialize_required_option")]
    pub transaction: Option<OutputTransactionSnapshot>,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum OutputSnapshotError {
    InvalidStructure,
}

impl OutputListSnapshot {
    pub fn validate(&self) -> Result<(), OutputSnapshotError> {
        if self.outputs.len() > MAX_CONTROL_OUTPUTS || self.total < self.outputs.len() as u32 {
            return Err(OutputSnapshotError::InvalidStructure);
        }
        let mut ids = HashSet::with_capacity(self.outputs.len());
        for output in &self.outputs {
            let mut mode_ids = HashSet::with_capacity(output.modes.len());
            if output.id.is_empty()
                || output.name.is_empty()
                || output.configuration_generation == 0
                || output.scale_milli == 0
                || !ids.insert(output.id.as_str())
                || output.modes.len() > MAX_CONTROL_OUTPUT_MODES
                || !output.mutation_capabilities.validate()
                || output.current_mode.as_ref().is_some_and(|mode| {
                    mode.id == 0 || mode.width == 0 || mode.height == 0 || mode.refresh_millihz == 0
                })
                || output.modes.iter().any(|mode| {
                    mode.id == 0
                        || mode.width == 0
                        || mode.height == 0
                        || mode.refresh_millihz == 0
                        || !mode_ids.insert(mode.id)
                })
                || output.current_mode.as_ref().is_some_and(|current| {
                    match output.modes.iter().find(|mode| mode.id == current.id) {
                        Some(mode) => {
                            mode.width != current.width
                                || mode.height != current.height
                                || mode.refresh_millihz != current.refresh_millihz
                        }
                        None => !output.modes_truncated,
                    }
                })
                || output
                    .physical_size_mm
                    .as_ref()
                    .is_some_and(|size| size.width_mm == 0 || size.height_mm == 0)
            {
                return Err(OutputSnapshotError::InvalidStructure);
            }
        }
        if let Some(transaction) = &self.transaction {
            if transaction.id == 0
                || transaction.output_id.is_empty()
                || transaction.applied_configuration_generation == 0
                || transaction.remaining_ms > OUTPUT_CONFIRMATION_TIMEOUT_MS
                || match transaction.state {
                    OutputTransactionState::PendingConfirmation
                    | OutputTransactionState::PersistencePending => {
                        transaction.error_code.is_some()
                    }
                    OutputTransactionState::RollingBack => {
                        transaction.remaining_ms != 0 || transaction.error_code.is_some()
                    }
                    OutputTransactionState::RollbackFailed => {
                        transaction.remaining_ms != 0
                            || transaction.error_code.as_deref().is_none_or(str::is_empty)
                    }
                }
                || (!self.truncated
                    && !self
                        .outputs
                        .iter()
                        .any(|output| output.id == transaction.output_id))
                || self
                    .outputs
                    .iter()
                    .find(|output| output.id == transaction.output_id)
                    .is_some_and(|output| {
                        output.configuration_generation
                            != transaction.applied_configuration_generation
                    })
            {
                return Err(OutputSnapshotError::InvalidStructure);
            }
        }
        Ok(())
    }
}
