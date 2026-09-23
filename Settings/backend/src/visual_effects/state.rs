use serde::{Deserialize, Serialize};

pub const DEFAULT_MATERIAL_POSITION: f64 = 0.5;

#[derive(Clone, Debug, Deserialize, PartialEq, Serialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct MaterialConfiguration {
    pub version: u8,
    pub position: f64,
    pub overrides: MaterialOverrides,
}

impl Default for MaterialConfiguration {
    fn default() -> Self {
        Self {
            version: 1,
            position: DEFAULT_MATERIAL_POSITION,
            overrides: MaterialOverrides::default(),
        }
    }
}

impl MaterialConfiguration {
    pub fn validate(&self) -> Result<(), StateError> {
        if self.version != 1 {
            return Err(StateError::InvalidConfiguration);
        }
        validate_value(self.position)?;
        for value in [
            self.overrides.blur,
            self.overrides.saturation,
            self.overrides.noise,
        ]
        .into_iter()
        .flatten()
        {
            validate_value(value)?;
        }
        Ok(())
    }
}

#[derive(Clone, Debug, Default, Deserialize, PartialEq, Serialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct MaterialOverrides {
    pub blur: Option<f64>,
    pub saturation: Option<f64>,
    pub noise: Option<f64>,
}

impl MaterialOverrides {
    pub fn clear(&mut self) {
        self.blur = None;
        self.saturation = None;
        self.noise = None;
    }

    pub fn is_empty(&self) -> bool {
        self.blur.is_none() && self.saturation.is_none() && self.noise.is_none()
    }
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct EffectiveMaterial {
    pub blur: f64,
    pub saturation: f64,
    pub noise: f64,
}

impl Default for EffectiveMaterial {
    fn default() -> Self {
        Self {
            blur: 0.0,
            saturation: 1.0,
            noise: 0.0,
        }
    }
}

#[derive(Clone, Copy, Debug, Default, Deserialize, Eq, PartialEq, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum MaterialConfigSource {
    #[default]
    Default,
    Persisted,
    Runtime,
}

impl MaterialConfigSource {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::Default => "default",
            Self::Persisted => "persisted",
            Self::Runtime => "runtime",
        }
    }
}

#[derive(Clone, Copy, Debug, Default, Deserialize, Eq, PartialEq, Serialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct MaterialCapabilities {
    pub blur_override: bool,
    pub saturation_override: bool,
    pub noise_override: bool,
}

#[derive(Clone, Debug, Deserialize, PartialEq, Serialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct MaterialSnapshot {
    pub generation: u64,
    pub source: MaterialConfigSource,
    pub configuration: MaterialConfiguration,
    pub effective: EffectiveMaterial,
    pub capabilities: MaterialCapabilities,
}

impl MaterialSnapshot {
    pub fn validate(&self) -> Result<(), StateError> {
        self.configuration.validate()?;
        for value in [
            self.effective.blur,
            self.effective.saturation,
            self.effective.noise,
        ] {
            validate_value(value)?;
        }
        Ok(())
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum StateError {
    IncompleteSnapshot,
    StaleSnapshot,
    InvalidConfiguration,
    Unavailable,
}

impl std::fmt::Display for StateError {
    fn fmt(&self, formatter: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        formatter.write_str(match self {
            Self::IncompleteSnapshot => "Typhon returned an incomplete material snapshot",
            Self::StaleSnapshot => "Typhon returned a stale material snapshot",
            Self::InvalidConfiguration => "material configuration is outside supported bounds",
            Self::Unavailable => "Typhon is unavailable",
        })
    }
}

impl std::error::Error for StateError {}

#[derive(Default)]
pub struct VisualEffectsState {
    available: bool,
    snapshot: Option<MaterialSnapshot>,
    configuration: MaterialConfiguration,
    pending: Option<MaterialConfiguration>,
}

impl VisualEffectsState {
    pub fn available(&self) -> bool {
        self.available
    }

    pub fn configuration(&self) -> &MaterialConfiguration {
        &self.configuration
    }

    pub fn snapshot(&self) -> Option<&MaterialSnapshot> {
        self.snapshot.as_ref()
    }

    pub fn effective(&self) -> EffectiveMaterial {
        self.snapshot
            .as_ref()
            .map_or(EffectiveMaterial::default(), |snapshot| snapshot.effective)
    }

    pub fn generation(&self) -> u64 {
        self.snapshot
            .as_ref()
            .map_or(0, |snapshot| snapshot.generation)
    }

    pub fn source(&self) -> MaterialConfigSource {
        self.snapshot
            .as_ref()
            .map_or(MaterialConfigSource::Default, |snapshot| snapshot.source)
    }

    pub fn default_material_position(&self) -> f64 {
        DEFAULT_MATERIAL_POSITION
    }

    pub fn has_overrides(&self) -> bool {
        !self.configuration.overrides.is_empty()
    }

    pub fn blur_overridden(&self) -> bool {
        self.configuration.overrides.blur.is_some()
    }

    pub fn saturation_overridden(&self) -> bool {
        self.configuration.overrides.saturation.is_some()
    }

    pub fn noise_overridden(&self) -> bool {
        self.configuration.overrides.noise.is_some()
    }

    pub fn pending_configuration(&self) -> Option<MaterialConfiguration> {
        self.pending.clone()
    }

    pub fn take_pending_configuration(&mut self) -> Option<MaterialConfiguration> {
        self.pending.take()
    }

    pub fn take_pending_configuration_if_matches(
        &mut self,
        configuration: &MaterialConfiguration,
    ) -> bool {
        if self.pending.as_ref() == Some(configuration) {
            self.pending = None;
            true
        } else {
            false
        }
    }

    pub fn apply_snapshot(&mut self, snapshot: MaterialSnapshot) -> Result<(), StateError> {
        snapshot.validate()?;
        if self
            .snapshot
            .as_ref()
            .is_some_and(|current| snapshot.generation < current.generation)
        {
            return Err(StateError::StaleSnapshot);
        }
        self.snapshot = Some(snapshot);
        self.available = true;
        if self.pending.is_none()
            && let Some(snapshot) = self.snapshot.as_ref()
        {
            self.configuration = snapshot.configuration.clone();
        }
        Ok(())
    }

    pub fn set_unavailable(&mut self) {
        self.available = false;
    }

    pub fn set_material_position(&mut self, value: f64) -> Result<(), StateError> {
        self.ensure_available()?;
        validate_value(value)?;
        self.configuration.position = value;
        self.queue_configuration();
        Ok(())
    }

    pub fn set_blur_override(&mut self, value: f64) -> Result<(), StateError> {
        self.ensure_available()?;
        validate_value(value)?;
        self.configuration.overrides.blur = Some(value);
        self.queue_configuration();
        Ok(())
    }

    pub fn clear_blur_override(&mut self) -> Result<(), StateError> {
        self.ensure_available()?;
        self.configuration.overrides.blur = None;
        self.queue_configuration();
        Ok(())
    }

    pub fn set_saturation_override(&mut self, value: f64) -> Result<(), StateError> {
        self.ensure_available()?;
        validate_value(value)?;
        self.configuration.overrides.saturation = Some(value);
        self.queue_configuration();
        Ok(())
    }

    pub fn clear_saturation_override(&mut self) -> Result<(), StateError> {
        self.ensure_available()?;
        self.configuration.overrides.saturation = None;
        self.queue_configuration();
        Ok(())
    }

    pub fn set_noise_override(&mut self, value: f64) -> Result<(), StateError> {
        self.ensure_available()?;
        validate_value(value)?;
        self.configuration.overrides.noise = Some(value);
        self.queue_configuration();
        Ok(())
    }

    pub fn clear_noise_override(&mut self) -> Result<(), StateError> {
        self.ensure_available()?;
        self.configuration.overrides.noise = None;
        self.queue_configuration();
        Ok(())
    }

    pub fn reset_overrides(&mut self) -> Result<(), StateError> {
        self.ensure_available()?;
        self.configuration.overrides.clear();
        self.queue_configuration();
        Ok(())
    }

    pub fn restore_defaults(&mut self) -> Result<(), StateError> {
        self.ensure_available()?;
        self.configuration = MaterialConfiguration::default();
        self.queue_configuration();
        Ok(())
    }

    pub fn reject_if_current(&mut self, rejected: &MaterialConfiguration) {
        if self.pending.is_none()
            && &self.configuration == rejected
            && let Some(snapshot) = &self.snapshot
        {
            self.configuration = snapshot.configuration.clone();
        }
    }

    fn ensure_available(&self) -> Result<(), StateError> {
        self.available.then_some(()).ok_or(StateError::Unavailable)
    }

    fn queue_configuration(&mut self) {
        self.pending = Some(self.configuration.clone());
    }
}

fn validate_value(value: f64) -> Result<(), StateError> {
    if value.is_finite() && (0.0..=1.0).contains(&value) {
        Ok(())
    } else {
        Err(StateError::InvalidConfiguration)
    }
}
