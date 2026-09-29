use crate::typhon::material_program as wire;
use crate::typhon::material_program::{
    MaterialProgramDescriptionSnapshot, MaterialProgramFallbackReason,
    MaterialProgramParameterConfiguration, MaterialProgramParameterConfigurationSnapshot,
    MaterialProgramParameterDescriptor, MaterialProgramParameterRange,
    MaterialProgramParameterType, MaterialProgramParameterValue, MaterialProgramSelectionSnapshot,
    MaterialProgramStateSnapshot,
};
use std::collections::{BTreeMap, HashMap};

#[derive(Default)]
pub struct MaterialProgramState {
    available: bool,
    snapshot: Option<MaterialProgramStateSnapshot>,
    description: Option<MaterialProgramDescriptionSnapshot>,
    pending_selection: Option<String>,
    parameter_identity: Option<(String, u64)>,
    parameter_overrides: Option<BTreeMap<String, MaterialProgramParameterValue>>,
    parameters_dirty: bool,
    observed_parameter_generations: HashMap<(u64, String, u64), u64>,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum MaterialProgramStateError {
    Unavailable,
    InvalidSnapshot,
    StaleSelectionGeneration,
    StaleParameterGeneration,
    WrongRegistryGeneration,
    WrongEffectiveProgram,
    WrongSchemaSignature,
    UnknownProgram,
    DescriptionUnavailable,
    UnknownParameter,
    ParameterTypeMismatch,
    ComponentOutOfRange,
    NonFiniteValue,
    ValueOutOfRange,
}

impl std::fmt::Display for MaterialProgramStateError {
    fn fmt(&self, formatter: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        formatter.write_str(match self {
            Self::Unavailable => "Typhon Material Program control is unavailable",
            Self::InvalidSnapshot => "Typhon returned an invalid Material Program snapshot",
            Self::StaleSelectionGeneration => "Typhon returned a stale Material Program selection",
            Self::StaleParameterGeneration => "Typhon returned stale Material Program parameters",
            Self::WrongRegistryGeneration => {
                "Material Program data belongs to another registry generation"
            }
            Self::WrongEffectiveProgram => {
                "Material Program data does not match the effective program"
            }
            Self::WrongSchemaSignature => "Material Program data does not match the current schema",
            Self::UnknownProgram => "Material Program is not in the qualified catalog",
            Self::DescriptionUnavailable => "Material Program parameter description is unavailable",
            Self::UnknownParameter => {
                "Material Program parameter is not in the current description"
            }
            Self::ParameterTypeMismatch => {
                "Material Program parameter type does not match the requested edit"
            }
            Self::ComponentOutOfRange => {
                "Material Program vector component is outside the parameter shape"
            }
            Self::NonFiniteValue => "Material Program parameter value must be finite",
            Self::ValueOutOfRange => {
                "Material Program parameter value is outside its declared range"
            }
        })
    }
}

impl std::error::Error for MaterialProgramStateError {}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum SelectionApply {
    Accepted,
    RefreshRequired,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ParameterApply {
    Accepted,
    RefreshRequired,
}

impl MaterialProgramState {
    pub fn is_available(&self) -> bool {
        self.available
    }

    pub fn snapshot(&self) -> Option<&MaterialProgramStateSnapshot> {
        self.snapshot.as_ref()
    }

    pub fn catalog(&self) -> Option<&wire::MaterialProgramCatalogSnapshot> {
        self.snapshot.as_ref().map(|snapshot| &snapshot.catalog)
    }

    pub fn selection(&self) -> Option<&MaterialProgramSelectionSnapshot> {
        self.snapshot.as_ref().map(|snapshot| &snapshot.selection)
    }

    pub fn description(&self) -> Option<&MaterialProgramDescriptionSnapshot> {
        self.description.as_ref()
    }

    pub fn has_current_description(&self) -> bool {
        self.snapshot
            .as_ref()
            .zip(self.description.as_ref())
            .is_some_and(|(snapshot, description)| description_matches(snapshot, description))
    }

    pub fn requested_program(&self) -> Option<&str> {
        self.selection()
            .map(|selection| selection.configuration.requested_program.as_str())
    }

    pub fn effective_program(&self) -> Option<&str> {
        self.selection()
            .map(|selection| selection.effective_program.as_str())
    }

    pub fn fallback_reason(&self) -> Option<MaterialProgramFallbackReason> {
        self.selection()
            .and_then(|selection| selection.fallback_reason)
    }

    pub fn rendering_available(&self) -> bool {
        self.catalog()
            .is_some_and(|catalog| catalog.rendering_available)
    }

    pub fn effective_origin(&self) -> Option<wire::MaterialProgramOrigin> {
        let name = self.effective_program()?;
        self.catalog()?
            .programs
            .iter()
            .find(|entry| entry.name == name)
            .map(|entry| entry.origin)
    }

    pub fn description_target(&self) -> Option<(u64, String, u64)> {
        let snapshot = self.snapshot.as_ref()?;
        let (program, signature) = schema_identity(snapshot)?;
        Some((snapshot.catalog.registry_generation, program, signature))
    }

    pub fn pending_selection(&self) -> Option<&str> {
        self.pending_selection.as_deref()
    }

    pub fn selection_generation(&self) -> Option<u64> {
        self.selection().map(|selection| selection.generation)
    }

    pub fn apply_snapshot(
        &mut self,
        snapshot: MaterialProgramStateSnapshot,
    ) -> Result<(), MaterialProgramStateError> {
        snapshot
            .validate()
            .map_err(|_| MaterialProgramStateError::InvalidSnapshot)?;
        if self.available
            && self.snapshot.as_ref().is_some_and(|current| {
                current.catalog.registry_generation == snapshot.catalog.registry_generation
                    && snapshot.selection.generation < current.selection.generation
            })
        {
            return Err(MaterialProgramStateError::StaleSelectionGeneration);
        }

        let previous_identity = self.current_schema_identity();
        let next_identity = schema_identity(&snapshot);
        let same_schema_identity = previous_identity == next_identity;
        let exact_description_identity = self
            .description
            .as_ref()
            .is_some_and(|description| description_matches(&snapshot, description));

        if !same_schema_identity {
            self.clear_parameter_edits();
        }
        if !self.available || !exact_description_identity {
            self.description = None;
        }
        if let Some((program, signature)) = &self.parameter_identity
            && next_identity.as_ref() != Some(&(program.clone(), *signature))
        {
            self.clear_parameter_edits();
        }
        self.snapshot = Some(snapshot);
        self.available = true;
        if self
            .pending_selection
            .as_deref()
            .is_some_and(|program| !self.catalog_contains(program))
        {
            self.pending_selection = None;
        }
        Ok(())
    }

    pub fn set_unavailable(&mut self) {
        self.available = false;
        self.description = None;
        self.observed_parameter_generations.clear();
    }

    pub fn apply_description(
        &mut self,
        description: MaterialProgramDescriptionSnapshot,
    ) -> Result<(), MaterialProgramStateError> {
        description
            .validate()
            .map_err(|_| MaterialProgramStateError::InvalidSnapshot)?;
        let snapshot = self
            .snapshot
            .as_ref()
            .ok_or(MaterialProgramStateError::Unavailable)?;
        if description.registry_generation != snapshot.catalog.registry_generation {
            return Err(MaterialProgramStateError::WrongRegistryGeneration);
        }
        if description.name != snapshot.selection.effective_program {
            return Err(MaterialProgramStateError::WrongEffectiveProgram);
        }
        let catalog_entry = snapshot
            .catalog
            .programs
            .iter()
            .find(|entry| entry.name == description.name)
            .ok_or(MaterialProgramStateError::WrongEffectiveProgram)?;
        if description.schema_signature != catalog_entry.schema_signature {
            return Err(MaterialProgramStateError::WrongSchemaSignature);
        }
        let identity = (
            description.registry_generation,
            description.name.clone(),
            description.schema_signature,
        );
        if self
            .observed_parameter_generations
            .get(&identity)
            .is_some_and(|generation| description.parameter_generation < *generation)
        {
            return Err(MaterialProgramStateError::StaleParameterGeneration);
        }

        let edit_identity = (description.name.clone(), description.schema_signature);
        let keep_pending = self.parameters_dirty
            && self.parameter_identity.as_ref() == Some(&edit_identity)
            && self
                .parameter_overrides
                .as_ref()
                .is_some_and(|overrides| overrides_valid_for_description(overrides, &description));
        if !keep_pending {
            self.parameter_overrides = Some(authoritative_overrides(&description));
            self.parameter_identity = Some(edit_identity);
            self.parameters_dirty = false;
        }
        self.observed_parameter_generations
            .insert(identity, description.parameter_generation);
        self.description = Some(description);
        Ok(())
    }

    pub fn select_program(&mut self, program: &str) -> Result<(), MaterialProgramStateError> {
        self.ensure_available()?;
        if !self.catalog_contains(program) {
            return Err(MaterialProgramStateError::UnknownProgram);
        }
        self.pending_selection = Some(program.to_owned());
        Ok(())
    }

    pub fn pending_selection_configuration(&self) -> Option<wire::MaterialProgramConfiguration> {
        self.pending_selection.as_ref().map(|requested_program| {
            wire::MaterialProgramConfiguration {
                version: 1,
                requested_program: requested_program.clone(),
            }
        })
    }

    pub fn reject_selection_if_current(&mut self, requested_program: &str) {
        if self.pending_selection.as_deref() == Some(requested_program) {
            self.pending_selection = None;
        }
    }

    pub fn apply_selection_result(
        &mut self,
        selection: MaterialProgramSelectionSnapshot,
        submitted: &wire::MaterialProgramConfiguration,
    ) -> Result<SelectionApply, MaterialProgramStateError> {
        selection
            .validate()
            .map_err(|_| MaterialProgramStateError::InvalidSnapshot)?;
        let Some(snapshot) = self.snapshot.as_ref() else {
            return Err(MaterialProgramStateError::Unavailable);
        };
        if selection.registry_generation != snapshot.catalog.registry_generation
            || selection.rendering_available != snapshot.catalog.rendering_available
            || !snapshot
                .catalog
                .programs
                .iter()
                .any(|entry| entry.name == selection.effective_program)
        {
            return Ok(SelectionApply::RefreshRequired);
        }
        if selection.generation < snapshot.selection.generation {
            return Err(MaterialProgramStateError::StaleSelectionGeneration);
        }

        let old_schema_identity = schema_identity(snapshot);
        let mut next_snapshot = snapshot.clone();
        next_snapshot.selection = selection.clone();
        let new_schema_identity = schema_identity(&next_snapshot);
        if old_schema_identity != new_schema_identity {
            self.description = None;
            self.clear_parameter_edits();
        }
        self.snapshot = Some(next_snapshot);
        if self.pending_selection.as_deref() == Some(submitted.requested_program.as_str()) {
            self.pending_selection = None;
        }
        Ok(SelectionApply::Accepted)
    }

    pub fn current_parameter_configuration(&self) -> Option<MaterialProgramParameterConfiguration> {
        let description = self.description.as_ref()?;
        if !description_matches(self.snapshot.as_ref()?, description) {
            return None;
        }
        let overrides = self.parameter_overrides.as_ref()?;
        Some(MaterialProgramParameterConfiguration {
            version: 1,
            program: description.name.clone(),
            schema_signature: description.schema_signature,
            overrides: overrides.clone(),
        })
    }

    pub fn pending_parameter_configuration(&self) -> Option<MaterialProgramParameterConfiguration> {
        self.parameters_dirty
            .then(|| self.current_parameter_configuration())
            .flatten()
    }

    pub fn mark_parameter_configuration_submitted(
        &mut self,
        submitted: &MaterialProgramParameterConfiguration,
    ) {
        if self.current_parameter_configuration().as_ref() == Some(submitted) {
            self.parameters_dirty = false;
        }
    }

    pub fn has_parameter_overrides(&self) -> bool {
        self.current_parameter_configuration()
            .is_some_and(|configuration| !configuration.overrides.is_empty())
    }

    pub fn displayed_parameter_value(&self, name: &str) -> Option<MaterialProgramParameterValue> {
        self.parameter_overrides
            .as_ref()?
            .get(name)
            .copied()
            .or_else(|| {
                self.description
                    .as_ref()?
                    .parameters
                    .iter()
                    .find(|parameter| parameter.name == name)
                    .map(|parameter| parameter.effective)
            })
    }

    pub fn displayed_parameter_overridden(&self, name: &str) -> bool {
        self.parameter_overrides
            .as_ref()
            .is_some_and(|overrides| overrides.contains_key(name))
    }

    pub fn set_float_parameter(
        &mut self,
        name: &str,
        value: f64,
    ) -> Result<(), MaterialProgramStateError> {
        self.ensure_editable()?;
        let descriptor = self.parameter_descriptor(name)?.clone();
        if descriptor.parameter_type != MaterialProgramParameterType::Float {
            return Err(MaterialProgramStateError::ParameterTypeMismatch);
        }
        validate_value_for_descriptor(&descriptor, MaterialProgramParameterValue::Float(value))?;
        self.store_parameter_value(name, MaterialProgramParameterValue::Float(value));
        Ok(())
    }

    pub fn set_int_parameter(
        &mut self,
        name: &str,
        value: i32,
    ) -> Result<(), MaterialProgramStateError> {
        self.ensure_editable()?;
        let descriptor = self.parameter_descriptor(name)?.clone();
        if descriptor.parameter_type != MaterialProgramParameterType::Int {
            return Err(MaterialProgramStateError::ParameterTypeMismatch);
        }
        validate_value_for_descriptor(&descriptor, MaterialProgramParameterValue::Int(value))?;
        self.store_parameter_value(name, MaterialProgramParameterValue::Int(value));
        Ok(())
    }

    pub fn set_vector_component(
        &mut self,
        name: &str,
        component: usize,
        value: f64,
    ) -> Result<(), MaterialProgramStateError> {
        self.ensure_editable()?;
        if !value.is_finite() {
            return Err(MaterialProgramStateError::NonFiniteValue);
        }
        let descriptor = self.parameter_descriptor(name)?.clone();
        if descriptor.parameter_type.component_count().is_none() {
            return Err(MaterialProgramStateError::ParameterTypeMismatch);
        }
        let current = self
            .parameter_overrides
            .as_ref()
            .and_then(|overrides| overrides.get(name).copied())
            .unwrap_or(descriptor.effective);
        let updated = current
            .with_component(component, value)
            .ok_or(MaterialProgramStateError::ComponentOutOfRange)?;
        validate_value_for_descriptor(&descriptor, updated)?;
        self.store_parameter_value(name, updated);
        Ok(())
    }

    pub fn clear_parameter(&mut self, name: &str) -> Result<(), MaterialProgramStateError> {
        self.ensure_editable()?;
        self.parameter_descriptor(name)?;
        self.parameter_overrides
            .as_mut()
            .ok_or(MaterialProgramStateError::DescriptionUnavailable)?
            .remove(name);
        self.update_dirty_flag();
        Ok(())
    }

    pub fn reset_parameters(&mut self) -> Result<(), MaterialProgramStateError> {
        self.ensure_editable()?;
        self.parameter_overrides
            .as_mut()
            .ok_or(MaterialProgramStateError::DescriptionUnavailable)?
            .clear();
        self.update_dirty_flag();
        Ok(())
    }

    pub fn discard_parameter_edits(&mut self) {
        self.clear_parameter_edits();
    }

    pub fn reject_parameter_configuration_if_current(
        &mut self,
        submitted: &MaterialProgramParameterConfiguration,
    ) {
        if self.current_parameter_configuration().as_ref() == Some(submitted)
            && let Some(description) = self.description.as_ref()
        {
            self.parameter_identity =
                Some((description.name.clone(), description.schema_signature));
            self.parameter_overrides = Some(authoritative_overrides(description));
            self.parameters_dirty = false;
        }
    }

    pub fn apply_parameter_result(
        &mut self,
        result: MaterialProgramParameterConfigurationSnapshot,
        submitted: &MaterialProgramParameterConfiguration,
    ) -> Result<ParameterApply, MaterialProgramStateError> {
        result
            .validate()
            .map_err(|_| MaterialProgramStateError::InvalidSnapshot)?;
        let Some(description) = result.description.as_ref() else {
            self.clear_parameter_edits();
            self.description = None;
            return Ok(ParameterApply::RefreshRequired);
        };
        let current = self
            .snapshot
            .as_ref()
            .ok_or(MaterialProgramStateError::Unavailable)?;
        if result.registry_generation != current.catalog.registry_generation
            || description.registry_generation != current.catalog.registry_generation
        {
            return Err(MaterialProgramStateError::WrongRegistryGeneration);
        }
        if description.name != current.selection.effective_program
            || result.configuration.program != current.selection.effective_program
            || submitted.program != current.selection.effective_program
        {
            return Err(MaterialProgramStateError::WrongEffectiveProgram);
        }
        let catalog_entry = current
            .catalog
            .programs
            .iter()
            .find(|entry| entry.name == description.name)
            .ok_or(MaterialProgramStateError::WrongEffectiveProgram)?;
        if description.schema_signature != catalog_entry.schema_signature
            || result.configuration.schema_signature != catalog_entry.schema_signature
            || submitted.schema_signature != catalog_entry.schema_signature
        {
            return Err(MaterialProgramStateError::WrongSchemaSignature);
        }
        if !description_matches(current, description) {
            return Err(MaterialProgramStateError::InvalidSnapshot);
        }
        let identity = (
            description.registry_generation,
            description.name.clone(),
            description.schema_signature,
        );
        if self
            .observed_parameter_generations
            .get(&identity)
            .is_some_and(|generation| result.parameter_generation < *generation)
        {
            return Err(MaterialProgramStateError::StaleParameterGeneration);
        }

        let keep_newer_local_edits = self.parameters_dirty
            && self.parameter_identity.as_ref()
                == Some(&(submitted.program.clone(), submitted.schema_signature))
            && self.parameter_overrides.as_ref().is_some_and(|overrides| {
                overrides != &submitted.overrides
                    && overrides_valid_for_description(overrides, description)
            });
        if keep_newer_local_edits {
            self.parameter_identity =
                Some((description.name.clone(), description.schema_signature));
        } else {
            self.parameter_identity =
                Some((description.name.clone(), description.schema_signature));
            self.parameter_overrides = Some(result.configuration.overrides.clone());
            self.parameters_dirty = false;
        }
        self.observed_parameter_generations
            .insert(identity, result.parameter_generation);
        self.description = Some(description.clone());
        Ok(ParameterApply::Accepted)
    }

    fn ensure_available(&self) -> Result<(), MaterialProgramStateError> {
        self.available
            .then_some(())
            .ok_or(MaterialProgramStateError::Unavailable)
    }

    fn ensure_editable(&self) -> Result<(), MaterialProgramStateError> {
        self.ensure_available()?;
        if self.description.is_none() {
            return Err(MaterialProgramStateError::DescriptionUnavailable);
        }
        Ok(())
    }

    fn catalog_contains(&self, program: &str) -> bool {
        self.catalog()
            .is_some_and(|catalog| catalog.programs.iter().any(|entry| entry.name == program))
    }

    fn parameter_descriptor(
        &self,
        name: &str,
    ) -> Result<&MaterialProgramParameterDescriptor, MaterialProgramStateError> {
        self.description
            .as_ref()
            .and_then(|description| description.parameters.iter().find(|item| item.name == name))
            .ok_or(MaterialProgramStateError::UnknownParameter)
    }

    fn store_parameter_value(&mut self, name: &str, value: MaterialProgramParameterValue) {
        self.parameter_overrides
            .get_or_insert_with(BTreeMap::new)
            .insert(name.to_owned(), value);
        self.update_dirty_flag();
    }

    fn update_dirty_flag(&mut self) {
        let authoritative = self
            .description
            .as_ref()
            .map(authoritative_overrides)
            .unwrap_or_default();
        self.parameters_dirty = self.parameter_overrides.as_ref() != Some(&authoritative);
    }

    fn clear_parameter_edits(&mut self) {
        self.parameter_identity = None;
        self.parameter_overrides = None;
        self.parameters_dirty = false;
    }

    fn current_schema_identity(&self) -> Option<(String, u64)> {
        self.snapshot.as_ref().and_then(schema_identity)
    }
}

fn schema_identity(snapshot: &MaterialProgramStateSnapshot) -> Option<(String, u64)> {
    let program = &snapshot.selection.effective_program;
    snapshot
        .catalog
        .programs
        .iter()
        .find(|entry| &entry.name == program)
        .map(|entry| (program.clone(), entry.schema_signature))
}

fn description_matches(
    snapshot: &MaterialProgramStateSnapshot,
    description: &MaterialProgramDescriptionSnapshot,
) -> bool {
    description.registry_generation == snapshot.catalog.registry_generation
        && description.name == snapshot.selection.effective_program
        && snapshot
            .catalog
            .programs
            .iter()
            .find(|entry| entry.name == description.name)
            .is_some_and(|entry| entry.schema_signature == description.schema_signature)
}

fn authoritative_overrides(
    description: &MaterialProgramDescriptionSnapshot,
) -> BTreeMap<String, MaterialProgramParameterValue> {
    description
        .parameters
        .iter()
        .filter(|parameter| parameter.overridden)
        .map(|parameter| (parameter.name.clone(), parameter.effective))
        .collect()
}

fn overrides_valid_for_description(
    overrides: &BTreeMap<String, MaterialProgramParameterValue>,
    description: &MaterialProgramDescriptionSnapshot,
) -> bool {
    overrides.iter().all(|(name, value)| {
        description
            .parameters
            .iter()
            .find(|parameter| &parameter.name == name)
            .is_some_and(|parameter| validate_value_for_descriptor(parameter, *value).is_ok())
    })
}

fn validate_value_for_descriptor(
    descriptor: &MaterialProgramParameterDescriptor,
    value: MaterialProgramParameterValue,
) -> Result<(), MaterialProgramStateError> {
    value
        .validate()
        .map_err(|_| MaterialProgramStateError::NonFiniteValue)?;
    if value.parameter_type() != descriptor.parameter_type {
        return Err(MaterialProgramStateError::ParameterTypeMismatch);
    }
    match (&descriptor.range, value) {
        (
            Some(MaterialProgramParameterRange::Float { min, max }),
            MaterialProgramParameterValue::Float(value),
        ) if value < *min || value > *max => Err(MaterialProgramStateError::ValueOutOfRange),
        (
            Some(MaterialProgramParameterRange::Int { min, max }),
            MaterialProgramParameterValue::Int(value),
        ) if value < *min || value > *max => Err(MaterialProgramStateError::ValueOutOfRange),
        (
            Some(MaterialProgramParameterRange::FloatComponents { min, max, .. }),
            MaterialProgramParameterValue::Vec2(values),
        ) if values
            .iter()
            .enumerate()
            .any(|(index, value)| *value < min[index] || *value > max[index]) =>
        {
            Err(MaterialProgramStateError::ValueOutOfRange)
        }
        (
            Some(MaterialProgramParameterRange::FloatComponents { min, max, .. }),
            MaterialProgramParameterValue::Vec3(values),
        ) if values
            .iter()
            .enumerate()
            .any(|(index, value)| *value < min[index] || *value > max[index]) =>
        {
            Err(MaterialProgramStateError::ValueOutOfRange)
        }
        (
            Some(MaterialProgramParameterRange::FloatComponents { min, max, .. }),
            MaterialProgramParameterValue::Vec4(values),
        ) if values
            .iter()
            .enumerate()
            .any(|(index, value)| *value < min[index] || *value > max[index]) =>
        {
            Err(MaterialProgramStateError::ValueOutOfRange)
        }
        _ => Ok(()),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::typhon::material_program as wire;
    use std::collections::BTreeMap;

    const EFFECTIVE: &str = "system.background_blur";
    const PROGRAM: &str = "studio.soft_glow";

    fn snapshot(
        registry: u64,
        selection_generation: u64,
        schema: u64,
    ) -> wire::MaterialProgramStateSnapshot {
        wire::MaterialProgramStateSnapshot {
            catalog: wire::MaterialProgramCatalogSnapshot {
                registry_generation: registry,
                rendering_available: true,
                programs: vec![
                    wire::MaterialProgramCatalogEntry {
                        name: EFFECTIVE.to_owned(),
                        origin: wire::MaterialProgramOrigin::System,
                        schema_signature: 1,
                        parameter_count: 0,
                    },
                    wire::MaterialProgramCatalogEntry {
                        name: PROGRAM.to_owned(),
                        origin: wire::MaterialProgramOrigin::TrustedLocal,
                        schema_signature: schema,
                        parameter_count: 3,
                    },
                ],
            },
            selection: wire::MaterialProgramSelectionSnapshot {
                generation: selection_generation,
                source: wire::MaterialProgramConfigSource::Persisted,
                configuration: wire::MaterialProgramConfiguration {
                    version: 1,
                    requested_program: "missing.saved_program".to_owned(),
                },
                effective_program: EFFECTIVE.to_owned(),
                registry_generation: registry,
                rendering_available: true,
                fallback_reason: Some(wire::MaterialProgramFallbackReason::Missing),
            },
        }
    }

    fn description(
        registry: u64,
        parameter_generation: u64,
        _schema: u64,
    ) -> wire::MaterialProgramDescriptionSnapshot {
        wire::MaterialProgramDescriptionSnapshot {
            registry_generation: registry,
            parameter_generation,
            name: EFFECTIVE.to_owned(),
            origin: wire::MaterialProgramOrigin::System,
            schema_signature: 1,
            parameters: vec![
                descriptor(
                    "intensity",
                    wire::MaterialProgramParameterType::Float,
                    wire::MaterialProgramParameterValue::Float(0.5),
                    wire::MaterialProgramParameterValue::Float(0.8),
                    Some(wire::MaterialProgramParameterRange::Float { min: 0.0, max: 1.0 }),
                    true,
                ),
                descriptor(
                    "noise",
                    wire::MaterialProgramParameterType::Float,
                    wire::MaterialProgramParameterValue::Float(0.2),
                    wire::MaterialProgramParameterValue::Float(0.2),
                    Some(wire::MaterialProgramParameterRange::Float { min: 0.0, max: 1.0 }),
                    false,
                ),
                descriptor(
                    "offset",
                    wire::MaterialProgramParameterType::Vec3,
                    wire::MaterialProgramParameterValue::Vec3([0.0, 0.0, 0.0]),
                    wire::MaterialProgramParameterValue::Vec3([0.1, 0.2, 0.3]),
                    Some(wire::MaterialProgramParameterRange::FloatComponents {
                        components: 3,
                        min: vec![-1.0; 3],
                        max: vec![1.0; 3],
                    }),
                    true,
                ),
            ],
        }
    }

    fn descriptor(
        name: &str,
        parameter_type: wire::MaterialProgramParameterType,
        default: wire::MaterialProgramParameterValue,
        effective: wire::MaterialProgramParameterValue,
        range: Option<wire::MaterialProgramParameterRange>,
        overridden: bool,
    ) -> wire::MaterialProgramParameterDescriptor {
        wire::MaterialProgramParameterDescriptor {
            name: name.to_owned(),
            parameter_type,
            range,
            default,
            effective,
            overridden,
        }
    }

    fn ready_state() -> MaterialProgramState {
        let mut state = MaterialProgramState::default();
        state.apply_snapshot(snapshot(7, 3, 41)).unwrap();
        state.apply_description(description(7, 10, 1)).unwrap();
        state
    }

    fn parameter_result(
        registry: u64,
        parameter_generation: u64,
        configuration: wire::MaterialProgramParameterConfiguration,
    ) -> wire::MaterialProgramParameterConfigurationSnapshot {
        let mut description = description(
            registry,
            parameter_generation,
            configuration.schema_signature,
        );
        for descriptor in &mut description.parameters {
            if let Some(value) = configuration.overrides.get(&descriptor.name) {
                descriptor.effective = *value;
                descriptor.overridden = true;
            } else {
                descriptor.overridden = false;
            }
        }
        wire::MaterialProgramParameterConfigurationSnapshot {
            registry_generation: registry,
            parameter_generation,
            configuration,
            description: Some(description),
        }
    }

    #[test]
    fn rejects_state_snapshots_with_mismatched_registry_generations() {
        let mut state = MaterialProgramState::default();
        let mut candidate = snapshot(7, 3, 41);
        candidate.selection.registry_generation = 8;
        assert!(state.apply_snapshot(candidate).is_err());
    }

    #[test]
    fn rejects_descriptions_from_another_registry_or_effective_program_or_schema() {
        for mutate in [0, 1, 2] {
            let mut state = MaterialProgramState::default();
            state.apply_snapshot(snapshot(7, 3, 41)).unwrap();
            let mut candidate = description(7, 10, 1);
            match mutate {
                0 => candidate.registry_generation = 8,
                1 => candidate.name = PROGRAM.to_owned(),
                _ => candidate.schema_signature = 2,
            }
            assert!(state.apply_description(candidate).is_err());
        }
    }

    #[test]
    fn lower_parameter_generation_for_the_same_identity_is_stale() {
        let mut state = ready_state();
        assert!(state.apply_description(description(7, 9, 1)).is_err());
    }

    #[test]
    fn requested_fallback_intent_is_preserved_and_editing_targets_effective_program() {
        let mut state = ready_state();
        assert_eq!(state.requested_program(), Some("missing.saved_program"));
        assert_eq!(state.effective_program(), Some(EFFECTIVE));
        assert_eq!(
            state.fallback_reason(),
            Some(wire::MaterialProgramFallbackReason::Missing)
        );
        state.set_float_parameter("noise", 0.4).unwrap();
        let configuration = state.pending_parameter_configuration().unwrap();
        assert_eq!(configuration.program, EFFECTIVE);
    }

    #[test]
    fn overridden_descriptors_reconstruct_all_effective_values() {
        let state = ready_state();
        let configuration = state.current_parameter_configuration().unwrap();
        assert_eq!(configuration.overrides.len(), 2);
        assert_eq!(
            configuration.overrides["intensity"],
            wire::MaterialProgramParameterValue::Float(0.8)
        );
        assert_eq!(
            configuration.overrides["offset"],
            wire::MaterialProgramParameterValue::Vec3([0.1, 0.2, 0.3])
        );
    }

    #[test]
    fn scalar_and_vector_edits_create_complete_override_configurations() {
        let mut state = ready_state();
        state.set_float_parameter("noise", 0.35).unwrap();
        state.set_vector_component("offset", 1, 0.7).unwrap();
        let configuration = state.pending_parameter_configuration().unwrap();
        assert_eq!(configuration.schema_signature, 1);
        assert_eq!(configuration.overrides.len(), 3);
        assert_eq!(
            configuration.overrides["noise"],
            wire::MaterialProgramParameterValue::Float(0.35)
        );
        assert_eq!(
            configuration.overrides["offset"],
            wire::MaterialProgramParameterValue::Vec3([0.1, 0.7, 0.3])
        );
        assert_eq!(
            configuration.overrides["intensity"],
            wire::MaterialProgramParameterValue::Float(0.8)
        );
    }

    #[test]
    fn clearing_one_parameter_and_resetting_all_build_complete_maps() {
        let mut state = ready_state();
        state.clear_parameter("intensity").unwrap();
        let cleared = state.pending_parameter_configuration().unwrap();
        assert_eq!(cleared.overrides.len(), 1);
        assert!(!cleared.overrides.contains_key("intensity"));
        assert!(cleared.overrides.contains_key("offset"));
        state.reset_parameters().unwrap();
        assert!(
            state
                .pending_parameter_configuration()
                .unwrap()
                .overrides
                .is_empty()
        );
    }

    #[test]
    fn typed_mismatch_and_out_of_range_edits_do_not_change_pending_values() {
        let mut state = ready_state();
        state.set_float_parameter("noise", 0.4).unwrap();
        let before = state.pending_parameter_configuration().unwrap();
        assert!(state.set_int_parameter("noise", 2).is_err());
        assert!(state.set_vector_component("offset", 3, 0.2).is_err());
        assert!(state.set_float_parameter("noise", 1.1).is_err());
        assert!(state.set_vector_component("offset", 0, f64::NAN).is_err());
        assert_eq!(state.pending_parameter_configuration().unwrap(), before);
    }

    #[test]
    fn compatible_edits_created_during_a_mutation_survive_its_authoritative_completion() {
        let mut state = ready_state();
        state.set_float_parameter("noise", 0.3).unwrap();
        let submitted = state.pending_parameter_configuration().unwrap();
        state.set_float_parameter("noise", 0.4).unwrap();
        state
            .apply_parameter_result(parameter_result(7, 11, submitted.clone()), &submitted)
            .unwrap();
        assert_eq!(
            state.pending_parameter_configuration().unwrap().overrides["noise"],
            wire::MaterialProgramParameterValue::Float(0.4)
        );
    }

    #[test]
    fn edits_for_a_schema_are_discarded_when_registry_changes_schema() {
        let mut state = ready_state();
        state.set_float_parameter("noise", 0.4).unwrap();
        let mut next = snapshot(8, 4, 42);
        next.selection.configuration.requested_program = PROGRAM.to_owned();
        next.selection.effective_program = PROGRAM.to_owned();
        state.apply_snapshot(next).unwrap();
        assert!(state.pending_parameter_configuration().is_none());
        assert!(state.description().is_none());
    }

    #[test]
    fn an_unavailable_reconnect_accepts_lower_process_local_generations_as_a_new_baseline() {
        let mut state = ready_state();
        state.set_unavailable();
        assert!(state.apply_snapshot(snapshot(2, 1, 41)).is_ok());
        assert!(state.is_available());
        assert_eq!(state.selection_generation(), Some(1));
        assert!(state.description().is_none());
    }

    #[test]
    fn lower_selection_generation_in_the_same_baseline_is_stale() {
        let mut state = ready_state();
        let mut older = snapshot(7, 2, 41);
        older.selection.configuration.requested_program = EFFECTIVE.to_owned();
        older.selection.fallback_reason = None;
        assert!(state.apply_snapshot(older).is_err());
    }

    #[test]
    fn parameter_response_without_description_requests_refresh_without_merging() {
        let mut state = ready_state();
        state.set_float_parameter("noise", 0.4).unwrap();
        let submitted = state.pending_parameter_configuration().unwrap();
        let mut result = parameter_result(7, 11, submitted.clone());
        result.description = None;
        assert_eq!(
            state.apply_parameter_result(result, &submitted).unwrap(),
            ParameterApply::RefreshRequired
        );
        assert!(state.description().is_none());
        assert!(state.pending_parameter_configuration().is_none());
    }

    #[test]
    fn catalog_selection_only_accepts_a_current_catalog_program() {
        let mut state = ready_state();
        assert!(state.select_program("unlisted.program").is_err());
        state.select_program(PROGRAM).unwrap();
        let selection = state.pending_selection_configuration().unwrap();
        assert_eq!(selection.version, 1);
        assert_eq!(selection.requested_program, PROGRAM);
        let _empty_overrides: BTreeMap<String, wire::MaterialProgramParameterValue> =
            BTreeMap::new();
    }
}
