use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;

pub const MIN_SPEED: f64 = 0.5;
pub const MAX_SPEED: f64 = 2.0;

#[derive(Clone, Debug, Deserialize, PartialEq, Serialize)]
pub struct AnimationConfiguration {
    pub enabled: bool,
    pub preset: String,
    pub speed: f64,
    #[serde(default)]
    pub overrides: BTreeMap<String, String>,
}

impl Default for AnimationConfiguration {
    fn default() -> Self {
        Self {
            enabled: true,
            preset: String::from("astrea"),
            speed: 1.0,
            overrides: BTreeMap::new(),
        }
    }
}

#[derive(Clone, Debug, Deserialize, PartialEq, Serialize)]
pub struct AnimationSnapshot {
    #[serde(default)]
    pub generation: u64,
    #[serde(default)]
    pub source: String,
    pub config: Option<AnimationConfiguration>,
    #[serde(default)]
    pub requested: BTreeMap<String, String>,
    #[serde(default)]
    pub effective: BTreeMap<String, String>,
    #[serde(default)]
    pub catalog: AnimationCatalog,
}

#[derive(Clone, Debug, Default, Deserialize, PartialEq, Serialize)]
pub struct AnimationCatalog {
    #[serde(default)]
    pub presets: Vec<String>,
    #[serde(default)]
    pub slots: Vec<AnimationSlot>,
    #[serde(default)]
    pub effects: Vec<AnimationEffect>,
}

#[derive(Clone, Debug, Deserialize, PartialEq, Serialize)]
pub struct AnimationSlot {
    pub id: String,
    #[serde(rename = "compatibleEffects", default)]
    pub compatible_effects: Vec<String>,
}

#[derive(Clone, Debug, Deserialize, PartialEq, Serialize)]
pub struct AnimationEffect {
    pub id: String,
    #[serde(default)]
    pub availability: String,
}

#[derive(Clone, Debug, PartialEq)]
pub struct AnimationSlotCapability {
    pub id: String,
    pub compatible_effects: Vec<String>,
    pub available_effects: Vec<String>,
    pub planned_effects: Vec<String>,
    pub unavailable_effects: Vec<String>,
    pub requested: Option<String>,
    pub effective: Option<String>,
    pub override_effect: Option<String>,
}

#[derive(Clone, Debug, PartialEq)]
pub enum AnimationMutation {
    SetEnabled(bool),
    SetPreset(String),
    SetSlotEffect { slot_id: String, effect_id: String },
    ClearSlotOverride(String),
    ResetOverrides,
    RestoreDefaults,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum MutationError {
    UnavailableEffect,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum StateError {
    IncompleteSnapshot,
}

#[derive(Default)]
pub struct AnimationState {
    available: bool,
    snapshot: Option<AnimationSnapshot>,
    configuration: AnimationConfiguration,
    pending_mutations: Vec<AnimationMutation>,
    pending_speed: Option<f64>,
}

impl AnimationState {
    pub fn available(&self) -> bool {
        self.available
    }

    pub fn configuration(&self) -> &AnimationConfiguration {
        &self.configuration
    }

    pub fn snapshot(&self) -> Option<&AnimationSnapshot> {
        self.snapshot.as_ref()
    }

    pub fn has_overrides(&self) -> bool {
        !self.configuration.overrides.is_empty()
    }

    pub fn set_speed(&mut self, value: f64) -> Option<f64> {
        if !self.available || !value.is_finite() {
            return None;
        }
        let speed = value.clamp(MIN_SPEED, MAX_SPEED);
        self.pending_speed = Some(speed);
        Some(speed)
    }

    pub fn enqueue_mutation(&mut self, mutation: AnimationMutation) {
        if self.available {
            self.pending_mutations.push(mutation);
        }
    }

    pub fn pending_configuration(&self) -> Option<AnimationConfiguration> {
        if !self.available || (self.pending_speed.is_none() && self.pending_mutations.is_empty()) {
            return None;
        }
        let mut configuration = self.configuration.clone();
        for mutation in &self.pending_mutations {
            apply_mutation(&mut configuration, mutation);
        }
        if let Some(speed) = self.pending_speed {
            configuration.speed = speed;
        }
        Some(configuration)
    }

    pub fn mark_submitted(&mut self) {
        self.pending_mutations.clear();
        self.pending_speed = None;
    }

    pub fn apply_snapshot(&mut self, snapshot: AnimationSnapshot) -> Result<(), StateError> {
        let Some(configuration) = snapshot.config.clone() else {
            return Err(StateError::IncompleteSnapshot);
        };
        self.snapshot = Some(snapshot);
        self.configuration = configuration;
        self.available = true;
        Ok(())
    }

    pub fn set_unavailable(&mut self) {
        self.available = false;
    }

    pub fn validate_slot_effect(
        &self,
        slot_id: &str,
        effect_id: &str,
    ) -> Result<(), MutationError> {
        let Some(snapshot) = self.snapshot.as_ref() else {
            return Err(MutationError::UnavailableEffect);
        };
        let compatible = snapshot
            .catalog
            .slots
            .iter()
            .find(|slot| slot.id == slot_id)
            .is_some_and(|slot| {
                slot.compatible_effects
                    .iter()
                    .any(|effect| effect == effect_id)
            });
        let executable = snapshot
            .catalog
            .effects
            .iter()
            .find(|effect| effect.id == effect_id)
            .is_some_and(|effect| effect.availability == "available");
        if compatible && executable {
            Ok(())
        } else {
            Err(MutationError::UnavailableEffect)
        }
    }

    pub fn slot_capabilities(&self) -> Vec<AnimationSlotCapability> {
        let Some(snapshot) = self.snapshot.as_ref() else {
            return Vec::new();
        };
        snapshot
            .catalog
            .slots
            .iter()
            .map(|slot| {
                let mut available_effects = Vec::new();
                let mut planned_effects = Vec::new();
                let mut unavailable_effects = Vec::new();
                for effect_id in &slot.compatible_effects {
                    match snapshot
                        .catalog
                        .effects
                        .iter()
                        .find(|effect| effect.id == *effect_id)
                    {
                        Some(effect) if effect.availability == "available" => {
                            available_effects.push(effect_id.clone())
                        }
                        Some(effect) if effect.availability == "unavailable" => {
                            unavailable_effects.push(effect_id.clone())
                        }
                        _ => planned_effects.push(effect_id.clone()),
                    }
                }
                AnimationSlotCapability {
                    id: slot.id.clone(),
                    compatible_effects: slot.compatible_effects.clone(),
                    available_effects,
                    planned_effects,
                    unavailable_effects,
                    requested: snapshot.requested.get(&slot.id).cloned(),
                    effective: snapshot.effective.get(&slot.id).cloned(),
                    override_effect: self.configuration.overrides.get(&slot.id).cloned(),
                }
            })
            .collect()
    }
}

fn apply_mutation(configuration: &mut AnimationConfiguration, mutation: &AnimationMutation) {
    match mutation {
        AnimationMutation::SetEnabled(value) => configuration.enabled = *value,
        AnimationMutation::SetPreset(value) => configuration.preset.clone_from(value),
        AnimationMutation::SetSlotEffect { slot_id, effect_id } => {
            configuration
                .overrides
                .insert(slot_id.clone(), effect_id.clone());
        }
        AnimationMutation::ClearSlotOverride(slot_id) => {
            configuration.overrides.remove(slot_id);
        }
        AnimationMutation::ResetOverrides => configuration.overrides.clear(),
        AnimationMutation::RestoreDefaults => *configuration = AnimationConfiguration::default(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn defaults_are_enabled_astrea_speed_one_and_have_no_overrides() {
        let state = AnimationState::default();
        assert!(state.configuration().enabled);
        assert_eq!(state.configuration().preset, "astrea");
        assert_eq!(state.configuration().speed, 1.0);
        assert!(state.configuration().overrides.is_empty());
    }

    #[test]
    fn speed_is_ignored_when_non_finite_and_clamped_to_supported_range() {
        let mut state = AnimationState::default();
        state
            .apply_snapshot(test_snapshot("available", "none"))
            .unwrap();
        assert_eq!(state.set_speed(f64::NAN), None);
        assert_eq!(state.set_speed(f64::INFINITY), None);
        assert_eq!(state.set_speed(0.1), Some(0.5));
        assert_eq!(state.set_speed(3.0), Some(2.0));
    }

    #[test]
    fn planned_and_unavailable_effects_are_projected_separately() {
        let mut state = AnimationState::default();
        state
            .apply_snapshot(test_snapshot("planned", "none"))
            .unwrap();
        let slot = state
            .slot_capabilities()
            .into_iter()
            .find(|slot| slot.id == "window.minimize")
            .unwrap();
        assert_eq!(slot.available_effects, vec!["none"]);
        assert_eq!(slot.planned_effects, vec!["minimize.lamp"]);
        assert!(slot.unavailable_effects.is_empty());

        state
            .apply_snapshot(test_snapshot("unavailable", "none"))
            .unwrap();
        let slot = state
            .slot_capabilities()
            .into_iter()
            .find(|slot| slot.id == "window.minimize")
            .unwrap();
        assert_eq!(slot.available_effects, vec!["none"]);
        assert!(slot.planned_effects.is_empty());
        assert_eq!(slot.unavailable_effects, vec!["minimize.lamp"]);
    }

    #[test]
    fn unavailable_effect_selection_returns_the_existing_user_error() {
        let mut state = AnimationState::default();
        state
            .apply_snapshot(test_snapshot("planned", "none"))
            .unwrap();
        assert_eq!(
            state.validate_slot_effect("window.minimize", "minimize.lamp"),
            Err(MutationError::UnavailableEffect)
        );
    }

    #[test]
    fn pending_speed_is_folded_into_the_next_mutation() {
        let mut state = AnimationState::default();
        state
            .apply_snapshot(test_snapshot("available", "minimize.lamp"))
            .unwrap();
        assert_eq!(state.set_speed(1.5), Some(1.5));
        state.enqueue_mutation(AnimationMutation::SetPreset("macos".to_owned()));
        let configuration = state.pending_configuration().unwrap();
        assert_eq!(configuration.speed, 1.5);
        assert_eq!(configuration.preset, "macos");
        state.mark_submitted();
        assert!(state.pending_configuration().is_none());
    }

    #[test]
    fn snapshot_without_config_is_rejected_without_replacing_authoritative_state() {
        let mut state = AnimationState::default();
        let initial = test_snapshot("available", "minimize.lamp");
        state.apply_snapshot(initial.clone()).unwrap();
        let mut incomplete = initial;
        incomplete.config = None;
        assert_eq!(
            state.apply_snapshot(incomplete),
            Err(StateError::IncompleteSnapshot)
        );
        assert_eq!(
            state.snapshot().unwrap().config,
            Some(test_configuration("available"))
        );
    }

    fn test_configuration(_availability: &str) -> AnimationConfiguration {
        AnimationConfiguration {
            enabled: true,
            preset: "astrea".to_owned(),
            speed: 1.0,
            overrides: Default::default(),
        }
    }

    fn test_snapshot(lamp_availability: &str, lamp_effective: &str) -> AnimationSnapshot {
        AnimationSnapshot {
            generation: 4,
            source: "persisted".to_owned(),
            config: Some(test_configuration(lamp_availability)),
            requested: [("window.minimize".to_owned(), "minimize.lamp".to_owned())]
                .into_iter()
                .collect(),
            effective: [("window.minimize".to_owned(), lamp_effective.to_owned())]
                .into_iter()
                .collect(),
            catalog: AnimationCatalog {
                presets: vec!["astrea".to_owned(), "kde".to_owned(), "macos".to_owned()],
                slots: vec![AnimationSlot {
                    id: "window.minimize".to_owned(),
                    compatible_effects: vec!["none".to_owned(), "minimize.lamp".to_owned()],
                }],
                effects: vec![
                    AnimationEffect {
                        id: "none".to_owned(),
                        availability: "available".to_owned(),
                    },
                    AnimationEffect {
                        id: "minimize.lamp".to_owned(),
                        availability: lamp_availability.to_owned(),
                    },
                ],
            },
        }
    }
}
