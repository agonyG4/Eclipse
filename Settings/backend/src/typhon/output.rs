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

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum OutputRequest {
    Get,
}

#[derive(Clone, Debug, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct ModeSnapshot {
    pub width: u32,
    pub height: u32,
    pub refresh_millihz: u32,
}

#[derive(Clone, Debug, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct OutputModeSnapshot {
    pub width: u32,
    pub height: u32,
    pub refresh_millihz: u32,
    pub preferred: bool,
    pub interlaced: bool,
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
#[serde(rename_all = "snake_case", deny_unknown_fields)]
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
    pub scale_milli: u32,
    pub transform: String,
    pub position: PositionSnapshot,
    pub focused: bool,
    pub backend: String,
    pub vrr: FeatureStateSnapshot,
    pub direct_scanout: FeatureStateSnapshot,
    pub modes: Vec<OutputModeSnapshot>,
    pub modes_truncated: bool,
}

#[derive(Clone, Debug, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct OutputListSnapshot {
    pub outputs: Vec<OutputSnapshot>,
    pub total: u32,
    pub truncated: bool,
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
            if output.id.is_empty()
                || output.name.is_empty()
                || output.scale_milli == 0
                || !ids.insert(output.id.as_str())
                || output.modes.len() > MAX_CONTROL_OUTPUT_MODES
                || output.current_mode.as_ref().is_some_and(|mode| {
                    mode.width == 0 || mode.height == 0 || mode.refresh_millihz == 0
                })
                || output
                    .modes
                    .iter()
                    .any(|mode| mode.width == 0 || mode.height == 0 || mode.refresh_millihz == 0)
                || output
                    .physical_size_mm
                    .as_ref()
                    .is_some_and(|size| size.width_mm == 0 || size.height_mm == 0)
            {
                return Err(OutputSnapshotError::InvalidStructure);
            }
        }
        Ok(())
    }
}
