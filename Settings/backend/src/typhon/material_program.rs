use serde::{Deserialize, Deserializer, Serialize};
use std::collections::BTreeMap;

#[derive(Clone, Debug, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct MaterialProgramCatalogSnapshot {
    pub registry_generation: u64,
    pub rendering_available: bool,
    pub programs: Vec<MaterialProgramCatalogEntry>,
}

#[derive(Clone, Debug, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct MaterialProgramCatalogEntry {
    pub name: String,
    pub origin: MaterialProgramOrigin,
    pub schema_signature: u64,
    pub parameter_count: u16,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case", deny_unknown_fields)]
pub enum MaterialProgramOrigin {
    System,
    TrustedLocal,
}

#[derive(Clone, Debug, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct MaterialProgramConfiguration {
    pub version: u8,
    pub requested_program: String,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum MaterialProgramFallbackReason {
    Missing,
    Unqualified,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum MaterialProgramConfigSource {
    Default,
    Persisted,
    Runtime,
}

#[derive(Clone, Debug, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct MaterialProgramSelectionSnapshot {
    pub generation: u64,
    pub source: MaterialProgramConfigSource,
    pub configuration: MaterialProgramConfiguration,
    pub effective_program: String,
    pub registry_generation: u64,
    pub rendering_available: bool,
    #[serde(deserialize_with = "deserialize_required_option")]
    pub fallback_reason: Option<MaterialProgramFallbackReason>,
}

impl MaterialProgramSelectionSnapshot {
    pub fn validate(&self) -> Result<(), MaterialProgramWireError> {
        if self.configuration.version != 1 || self.effective_program.is_empty() {
            return Err(MaterialProgramWireError::InvalidSnapshot);
        }
        Ok(())
    }
}

#[derive(Clone, Debug, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct MaterialProgramStateSnapshot {
    pub catalog: MaterialProgramCatalogSnapshot,
    pub selection: MaterialProgramSelectionSnapshot,
}

impl MaterialProgramStateSnapshot {
    pub fn validate(&self) -> Result<(), MaterialProgramWireError> {
        self.selection.validate()?;
        if self.catalog.registry_generation != self.selection.registry_generation
            || self.catalog.rendering_available != self.selection.rendering_available
            || self.catalog.programs.is_empty()
        {
            return Err(MaterialProgramWireError::InvalidSnapshot);
        }
        let mut names = std::collections::HashSet::new();
        if self
            .catalog
            .programs
            .iter()
            .any(|program| program.name.is_empty() || !names.insert(program.name.as_str()))
        {
            return Err(MaterialProgramWireError::InvalidSnapshot);
        }
        if !self
            .catalog
            .programs
            .iter()
            .any(|program| program.name == self.selection.effective_program)
        {
            return Err(MaterialProgramWireError::InvalidSnapshot);
        }
        Ok(())
    }
}

#[derive(Clone, Debug, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct MaterialProgramNameArguments {
    pub name: String,
}

#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
#[serde(
    tag = "type",
    content = "value",
    rename_all = "snake_case",
    deny_unknown_fields
)]
pub enum MaterialProgramParameterValue {
    Float(f64),
    Vec2([f64; 2]),
    Vec3([f64; 3]),
    Vec4([f64; 4]),
    Int(i32),
}

impl MaterialProgramParameterValue {
    pub fn validate(self) -> Result<(), MaterialProgramWireError> {
        if self
            .components()
            .is_some_and(|values| values.iter().any(|value| !value.is_finite()))
        {
            return Err(MaterialProgramWireError::InvalidParameterValue);
        }
        Ok(())
    }

    pub fn parameter_type(self) -> MaterialProgramParameterType {
        match self {
            Self::Float(_) => MaterialProgramParameterType::Float,
            Self::Vec2(_) => MaterialProgramParameterType::Vec2,
            Self::Vec3(_) => MaterialProgramParameterType::Vec3,
            Self::Vec4(_) => MaterialProgramParameterType::Vec4,
            Self::Int(_) => MaterialProgramParameterType::Int,
        }
    }

    pub fn components(self) -> Option<Vec<f64>> {
        match self {
            Self::Float(value) => Some(vec![value]),
            Self::Vec2(values) => Some(values.to_vec()),
            Self::Vec3(values) => Some(values.to_vec()),
            Self::Vec4(values) => Some(values.to_vec()),
            Self::Int(_) => None,
        }
    }

    pub fn with_component(self, index: usize, value: f64) -> Option<Self> {
        match self {
            Self::Vec2(mut values) if index < 2 => {
                values[index] = value;
                Some(Self::Vec2(values))
            }
            Self::Vec3(mut values) if index < 3 => {
                values[index] = value;
                Some(Self::Vec3(values))
            }
            Self::Vec4(mut values) if index < 4 => {
                values[index] = value;
                Some(Self::Vec4(values))
            }
            _ => None,
        }
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum MaterialProgramParameterType {
    Float,
    Vec2,
    Vec3,
    Vec4,
    Int,
}

impl MaterialProgramParameterType {
    pub fn component_count(self) -> Option<usize> {
        match self {
            Self::Float | Self::Int => None,
            Self::Vec2 => Some(2),
            Self::Vec3 => Some(3),
            Self::Vec4 => Some(4),
        }
    }
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(tag = "type", rename_all = "snake_case", deny_unknown_fields)]
pub enum MaterialProgramParameterRange {
    Float {
        min: f64,
        max: f64,
    },
    FloatComponents {
        components: u8,
        min: Vec<f64>,
        max: Vec<f64>,
    },
    Int {
        min: i32,
        max: i32,
    },
}

impl MaterialProgramParameterRange {
    pub fn validate(&self) -> Result<(), MaterialProgramWireError> {
        match self {
            Self::Float { min, max } if min.is_finite() && max.is_finite() && min <= max => Ok(()),
            Self::FloatComponents {
                components,
                min,
                max,
            } if (2..=4).contains(components)
                && min.len() == usize::from(*components)
                && max.len() == usize::from(*components)
                && min.iter().zip(max).all(|(minimum, maximum)| {
                    minimum.is_finite() && maximum.is_finite() && minimum <= maximum
                }) =>
            {
                Ok(())
            }
            Self::Int { min, max } if min <= max => Ok(()),
            _ => Err(MaterialProgramWireError::InvalidParameterRange),
        }
    }

    pub fn accepts(&self, parameter_type: MaterialProgramParameterType) -> bool {
        match (self, parameter_type) {
            (Self::Float { .. }, MaterialProgramParameterType::Float)
            | (Self::Int { .. }, MaterialProgramParameterType::Int) => true,
            (
                Self::FloatComponents { components, .. },
                MaterialProgramParameterType::Vec2
                | MaterialProgramParameterType::Vec3
                | MaterialProgramParameterType::Vec4,
            ) => parameter_type.component_count() == Some(usize::from(*components)),
            _ => false,
        }
    }
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct MaterialProgramParameterDescriptor {
    pub name: String,
    pub parameter_type: MaterialProgramParameterType,
    pub range: Option<MaterialProgramParameterRange>,
    pub default: MaterialProgramParameterValue,
    pub effective: MaterialProgramParameterValue,
    pub overridden: bool,
}

impl MaterialProgramParameterDescriptor {
    pub fn validate(&self) -> Result<(), MaterialProgramWireError> {
        if self.name.is_empty()
            || self.default.parameter_type() != self.parameter_type
            || self.effective.parameter_type() != self.parameter_type
        {
            return Err(MaterialProgramWireError::InvalidParameterValue);
        }
        self.default.validate()?;
        self.effective.validate()?;
        if let Some(range) = &self.range {
            range.validate()?;
            if !range.accepts(self.parameter_type) {
                return Err(MaterialProgramWireError::InvalidParameterRange);
            }
        }
        Ok(())
    }
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct MaterialProgramDescriptionSnapshot {
    pub registry_generation: u64,
    pub parameter_generation: u64,
    pub name: String,
    pub origin: MaterialProgramOrigin,
    pub schema_signature: u64,
    pub parameters: Vec<MaterialProgramParameterDescriptor>,
}

impl MaterialProgramDescriptionSnapshot {
    pub fn validate(&self) -> Result<(), MaterialProgramWireError> {
        if self.name.is_empty() {
            return Err(MaterialProgramWireError::InvalidSnapshot);
        }
        let mut names = std::collections::HashSet::new();
        for parameter in &self.parameters {
            parameter.validate()?;
            if !names.insert(parameter.name.as_str()) {
                return Err(MaterialProgramWireError::InvalidSnapshot);
            }
        }
        Ok(())
    }
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct MaterialProgramParameterConfiguration {
    pub version: u8,
    pub program: String,
    pub schema_signature: u64,
    pub overrides: BTreeMap<String, MaterialProgramParameterValue>,
}

impl MaterialProgramParameterConfiguration {
    pub fn validate(&self) -> Result<(), MaterialProgramWireError> {
        if self.version != 1 || self.program.is_empty() {
            return Err(MaterialProgramWireError::InvalidSnapshot);
        }
        for value in self.overrides.values() {
            value.validate()?;
        }
        Ok(())
    }
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct MaterialProgramParameterConfigurationSnapshot {
    pub registry_generation: u64,
    pub parameter_generation: u64,
    pub configuration: MaterialProgramParameterConfiguration,
    #[serde(deserialize_with = "deserialize_required_option")]
    pub description: Option<MaterialProgramDescriptionSnapshot>,
}

impl MaterialProgramParameterConfigurationSnapshot {
    pub fn validate(&self) -> Result<(), MaterialProgramWireError> {
        self.configuration.validate()?;
        if let Some(description) = &self.description {
            description.validate()?;
            if description.registry_generation != self.registry_generation
                || description.parameter_generation != self.parameter_generation
                || description.name != self.configuration.program
                || description.schema_signature != self.configuration.schema_signature
            {
                return Err(MaterialProgramWireError::InvalidSnapshot);
            }
        }
        Ok(())
    }
}

fn deserialize_required_option<'de, D, T>(deserializer: D) -> Result<Option<T>, D::Error>
where
    D: Deserializer<'de>,
    T: Deserialize<'de>,
{
    Option::<T>::deserialize(deserializer)
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum MaterialProgramWireError {
    InvalidSnapshot,
    InvalidParameterValue,
    InvalidParameterRange,
}
