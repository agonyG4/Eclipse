use super::{
    EffectiveMaterial, MaterialCapabilities, MaterialConfigSource, MaterialConfiguration,
    MaterialOverrides, MaterialSnapshot, StateError, VisualEffectsState,
};

fn snapshot(generation: u64, configuration: MaterialConfiguration) -> MaterialSnapshot {
    snapshot_with_capabilities(
        generation,
        configuration,
        MaterialCapabilities {
            blur_override: true,
            saturation_override: true,
            noise_override: true,
        },
    )
}

fn snapshot_with_capabilities(
    generation: u64,
    configuration: MaterialConfiguration,
    capabilities: MaterialCapabilities,
) -> MaterialSnapshot {
    MaterialSnapshot {
        generation,
        source: MaterialConfigSource::Runtime,
        effective: EffectiveMaterial {
            blur: configuration.position,
            saturation: 0.8,
            noise: 0.2,
        },
        configuration,
        capabilities,
    }
}

#[test]
fn material_state_starts_at_astrea_default_without_claiming_availability() {
    let state = VisualEffectsState::default();

    assert!(!state.available());
    assert_eq!(state.default_material_position(), 0.5);
    assert_eq!(state.configuration().position, 0.5);
    assert!(state.configuration().overrides.is_empty());
    assert_eq!(state.generation(), 0);
    assert!(!state.blur_override_supported());
    assert!(!state.saturation_override_supported());
    assert!(!state.noise_override_supported());
    assert!(
        !state.configuration_is_compatible_with_latest_snapshot(&MaterialConfiguration::default())
    );
}

#[test]
fn authoritative_snapshot_replaces_configuration_and_effective_projection() {
    let mut state = VisualEffectsState::default();
    let configuration = MaterialConfiguration {
        position: 0.88,
        overrides: MaterialOverrides {
            blur: Some(0.9),
            saturation: None,
            noise: None,
        },
        ..MaterialConfiguration::default()
    };

    state
        .apply_snapshot(snapshot(9, configuration.clone()))
        .unwrap();

    assert!(state.available());
    assert_eq!(state.generation(), 9);
    assert_eq!(state.configuration(), &configuration);
    assert_eq!(state.effective().blur, 0.88);
}

#[test]
fn override_capabilities_project_exactly_from_the_latest_authoritative_snapshot() {
    let mut state = VisualEffectsState::default();
    let first_capabilities = MaterialCapabilities {
        blur_override: false,
        saturation_override: true,
        noise_override: false,
    };
    state
        .apply_snapshot(snapshot_with_capabilities(
            1,
            MaterialConfiguration::default(),
            first_capabilities,
        ))
        .unwrap();

    assert!(!state.blur_override_supported());
    assert!(state.saturation_override_supported());
    assert!(!state.noise_override_supported());

    let latest_capabilities = MaterialCapabilities {
        blur_override: true,
        saturation_override: false,
        noise_override: true,
    };
    state
        .apply_snapshot(snapshot_with_capabilities(
            2,
            MaterialConfiguration::default(),
            latest_capabilities,
        ))
        .unwrap();

    assert!(state.blur_override_supported());
    assert!(!state.saturation_override_supported());
    assert!(state.noise_override_supported());
}

#[test]
fn position_and_advanced_edits_fold_into_one_newest_complete_configuration() {
    let mut state = VisualEffectsState::default();
    state
        .apply_snapshot(snapshot(1, MaterialConfiguration::default()))
        .unwrap();

    state.set_material_position(0.7).unwrap();
    state.set_blur_override(0.91).unwrap();
    state.set_saturation_override(0.65).unwrap();
    state.set_noise_override(0.4).unwrap();

    let pending = state.pending_configuration().unwrap();
    assert_eq!(pending.position, 0.7);
    assert_eq!(pending.overrides.blur, Some(0.91));
    assert_eq!(pending.overrides.noise, Some(0.4));
    assert_eq!(pending.overrides.saturation, Some(0.65));
}

fn assert_unsupported_override_preserves_state(
    set_override: fn(&mut VisualEffectsState, f64) -> Result<(), StateError>,
) {
    let mut state = VisualEffectsState::default();
    let configuration = MaterialConfiguration {
        position: 0.64,
        overrides: MaterialOverrides {
            blur: Some(0.17),
            saturation: Some(0.83),
            noise: Some(0.08),
        },
        ..MaterialConfiguration::default()
    };
    state
        .apply_snapshot(snapshot_with_capabilities(
            1,
            configuration,
            MaterialCapabilities::default(),
        ))
        .unwrap();

    // Keep an existing pending request in place to prove a rejected setter
    // neither changes the requested configuration nor replaces pending work.
    state.set_material_position(0.71).unwrap();
    let requested_before = state.configuration().clone();
    let pending_before = state.pending_configuration();

    assert_eq!(
        set_override(&mut state, 0.42),
        Err(StateError::UnsupportedCapability)
    );
    assert_eq!(state.configuration(), &requested_before);
    assert_eq!(state.pending_configuration(), pending_before);
}

#[test]
fn unsupported_blur_override_is_rejected_locally() {
    assert_unsupported_override_preserves_state(VisualEffectsState::set_blur_override);
}

#[test]
fn unsupported_saturation_override_is_rejected_locally() {
    assert_unsupported_override_preserves_state(VisualEffectsState::set_saturation_override);
}

#[test]
fn unsupported_noise_override_is_rejected_locally() {
    assert_unsupported_override_preserves_state(VisualEffectsState::set_noise_override);
}

#[test]
fn clearing_and_resetting_existing_overrides_is_safe_when_unsupported() {
    let mut state = VisualEffectsState::default();
    state
        .apply_snapshot(snapshot_with_capabilities(
            1,
            MaterialConfiguration {
                position: 0.74,
                overrides: MaterialOverrides {
                    blur: Some(0.21),
                    saturation: Some(0.67),
                    noise: Some(0.12),
                },
                ..MaterialConfiguration::default()
            },
            MaterialCapabilities::default(),
        ))
        .unwrap();

    state.clear_blur_override().unwrap();
    state.clear_saturation_override().unwrap();
    state.clear_noise_override().unwrap();
    state.reset_overrides().unwrap();

    assert_eq!(state.configuration().position, 0.74);
    assert!(state.configuration().overrides.is_empty());
    let pending = state.pending_configuration().unwrap();
    assert_eq!(pending.position, 0.74);
    assert!(pending.overrides.is_empty());
}

#[test]
fn clearing_one_override_and_resetting_overrides_preserve_material_position() {
    let mut state = VisualEffectsState::default();
    state
        .apply_snapshot(snapshot(
            1,
            MaterialConfiguration {
                position: 0.78,
                overrides: MaterialOverrides {
                    blur: Some(0.2),
                    saturation: Some(0.3),
                    noise: Some(0.4),
                },
                ..MaterialConfiguration::default()
            },
        ))
        .unwrap();

    state.clear_blur_override().unwrap();
    assert_eq!(state.configuration().position, 0.78);
    assert_eq!(state.pending_configuration().unwrap().overrides.blur, None);
    state.reset_overrides().unwrap();
    let pending = state.pending_configuration().unwrap();
    assert_eq!(pending.position, 0.78);
    assert!(pending.overrides.is_empty());
}

#[test]
fn restore_defaults_resets_position_and_overrides_as_one_configuration() {
    let mut state = VisualEffectsState::default();
    state
        .apply_snapshot(snapshot(
            1,
            MaterialConfiguration {
                position: 0.9,
                overrides: MaterialOverrides {
                    blur: Some(0.7),
                    ..MaterialOverrides::default()
                },
                ..MaterialConfiguration::default()
            },
        ))
        .unwrap();

    state.restore_defaults().unwrap();

    assert_eq!(
        state.pending_configuration(),
        Some(MaterialConfiguration::default())
    );
}

#[test]
fn stale_authoritative_snapshot_does_not_roll_back_newer_generation() {
    let mut state = VisualEffectsState::default();
    state
        .apply_snapshot(snapshot(
            5,
            MaterialConfiguration {
                position: 0.8,
                ..MaterialConfiguration::default()
            },
        ))
        .unwrap();

    assert!(
        state
            .apply_snapshot(snapshot(4, MaterialConfiguration::default()))
            .is_err()
    );
    assert_eq!(state.generation(), 5);
    assert_eq!(state.configuration().position, 0.8);
}

#[test]
fn unavailable_transition_preserves_last_authoritative_snapshot() {
    let mut state = VisualEffectsState::default();
    let expected = snapshot(2, MaterialConfiguration::default());
    state.apply_snapshot(expected.clone()).unwrap();
    state.set_unavailable();

    assert!(!state.available());
    assert_eq!(state.snapshot(), Some(&expected));
}

#[test]
fn older_in_flight_snapshot_does_not_replace_newer_pending_edits() {
    let mut state = VisualEffectsState::default();
    state
        .apply_snapshot(snapshot(1, MaterialConfiguration::default()))
        .unwrap();

    state.set_material_position(0.7).unwrap();
    let in_flight = state.pending_configuration().unwrap();
    assert!(state.take_pending_configuration_if_matches(&in_flight));
    state.set_noise_override(0.8).unwrap();

    state
        .apply_snapshot(snapshot(2, in_flight.clone()))
        .unwrap();

    assert_eq!(state.configuration().position, 0.7);
    assert_eq!(state.configuration().overrides.noise, Some(0.8));
    assert_eq!(
        state.pending_configuration().unwrap(),
        *state.configuration()
    );
}

#[test]
fn capability_downgrade_reverts_pending_override_to_authoritative_value() {
    let mut state = VisualEffectsState::default();
    state
        .apply_snapshot(snapshot_with_capabilities(
            1,
            MaterialConfiguration::default(),
            MaterialCapabilities {
                blur_override: true,
                ..MaterialCapabilities::default()
            },
        ))
        .unwrap();
    state.set_blur_override(0.7).unwrap();

    state
        .apply_snapshot(snapshot_with_capabilities(
            2,
            MaterialConfiguration::default(),
            MaterialCapabilities::default(),
        ))
        .unwrap();

    assert_eq!(state.configuration().overrides.blur, None);
    assert_eq!(state.pending_configuration(), None);
}

#[test]
fn capability_downgrade_rebases_only_unsupported_pending_fields() {
    let mut state = VisualEffectsState::default();
    state
        .apply_snapshot(snapshot(1, MaterialConfiguration::default()))
        .unwrap();
    state.set_material_position(0.82).unwrap();
    state.set_blur_override(0.7).unwrap();
    state.set_noise_override(0.1).unwrap();

    state
        .apply_snapshot(snapshot_with_capabilities(
            2,
            MaterialConfiguration {
                position: 0.5,
                overrides: MaterialOverrides {
                    blur: Some(0.25),
                    saturation: None,
                    noise: None,
                },
                ..MaterialConfiguration::default()
            },
            MaterialCapabilities {
                blur_override: false,
                saturation_override: false,
                noise_override: true,
            },
        ))
        .unwrap();

    let expected = MaterialConfiguration {
        position: 0.82,
        overrides: MaterialOverrides {
            blur: Some(0.25),
            saturation: None,
            noise: Some(0.1),
        },
        ..MaterialConfiguration::default()
    };
    assert_eq!(state.configuration(), &expected);
    assert_eq!(state.pending_configuration(), Some(expected));
}

#[test]
fn multiple_capability_downgrades_reconcile_each_override_independently() {
    let mut state = VisualEffectsState::default();
    state
        .apply_snapshot(snapshot(1, MaterialConfiguration::default()))
        .unwrap();
    state.set_material_position(0.82).unwrap();
    state.set_blur_override(0.7).unwrap();
    state.set_saturation_override(0.6).unwrap();
    state.set_noise_override(0.1).unwrap();

    state
        .apply_snapshot(snapshot_with_capabilities(
            2,
            MaterialConfiguration {
                position: 0.5,
                overrides: MaterialOverrides {
                    blur: Some(0.25),
                    saturation: Some(0.4),
                    noise: None,
                },
                ..MaterialConfiguration::default()
            },
            MaterialCapabilities::default(),
        ))
        .unwrap();

    let expected = MaterialConfiguration {
        position: 0.82,
        overrides: MaterialOverrides {
            blur: Some(0.25),
            saturation: Some(0.4),
            noise: None,
        },
        ..MaterialConfiguration::default()
    };
    assert_eq!(state.configuration(), &expected);
    assert_eq!(state.pending_configuration(), Some(expected));
}

#[test]
fn position_edit_preserves_unsupported_authoritative_override() {
    let mut state = VisualEffectsState::default();
    let authoritative = MaterialConfiguration {
        overrides: MaterialOverrides {
            blur: Some(0.25),
            ..MaterialOverrides::default()
        },
        ..MaterialConfiguration::default()
    };
    state
        .apply_snapshot(snapshot_with_capabilities(
            1,
            authoritative,
            MaterialCapabilities::default(),
        ))
        .unwrap();

    state.set_material_position(0.82).unwrap();

    let pending = state.pending_configuration().unwrap();
    assert_eq!(pending.position, 0.82);
    assert_eq!(pending.overrides.blur, Some(0.25));
    assert_eq!(state.configuration().overrides.blur, Some(0.25));
    assert!(state.configuration_is_compatible_with_latest_snapshot(&pending));
}

#[test]
fn compatibility_allows_clears_and_authoritative_unsupported_values_only() {
    let mut state = VisualEffectsState::default();
    let authoritative = MaterialConfiguration {
        overrides: MaterialOverrides {
            blur: Some(0.25),
            saturation: Some(0.4),
            noise: Some(0.1),
        },
        ..MaterialConfiguration::default()
    };
    state
        .apply_snapshot(snapshot_with_capabilities(
            1,
            authoritative.clone(),
            MaterialCapabilities::default(),
        ))
        .unwrap();

    assert!(state.configuration_is_compatible_with_latest_snapshot(&authoritative));

    let mut clear = authoritative.clone();
    clear.overrides.blur = None;
    assert!(state.configuration_is_compatible_with_latest_snapshot(&clear));

    let mut changed_blur = authoritative.clone();
    changed_blur.overrides.blur = Some(0.7);
    assert!(!state.configuration_is_compatible_with_latest_snapshot(&changed_blur));

    let mut changed_saturation = authoritative.clone();
    changed_saturation.overrides.saturation = Some(0.6);
    assert!(!state.configuration_is_compatible_with_latest_snapshot(&changed_saturation));

    let mut changed_noise = authoritative;
    changed_noise.overrides.noise = Some(0.8);
    assert!(!state.configuration_is_compatible_with_latest_snapshot(&changed_noise));
}

#[test]
fn compatibility_allows_any_valid_override_for_supported_capabilities() {
    let mut state = VisualEffectsState::default();
    state
        .apply_snapshot(snapshot_with_capabilities(
            1,
            MaterialConfiguration::default(),
            MaterialCapabilities {
                blur_override: true,
                saturation_override: true,
                noise_override: true,
            },
        ))
        .unwrap();
    let requested = MaterialConfiguration {
        overrides: MaterialOverrides {
            blur: Some(0.7),
            saturation: Some(0.6),
            noise: Some(0.8),
        },
        ..MaterialConfiguration::default()
    };

    assert!(state.configuration_is_compatible_with_latest_snapshot(&requested));
}

#[test]
fn clear_reset_and_restore_defaults_remain_valid_when_capabilities_are_unsupported() {
    let mut state = VisualEffectsState::default();
    state
        .apply_snapshot(snapshot_with_capabilities(
            1,
            MaterialConfiguration {
                position: 0.74,
                overrides: MaterialOverrides {
                    blur: Some(0.25),
                    saturation: Some(0.4),
                    noise: Some(0.1),
                },
                ..MaterialConfiguration::default()
            },
            MaterialCapabilities::default(),
        ))
        .unwrap();

    state.clear_blur_override().unwrap();
    let cleared = state.pending_configuration().unwrap();
    assert_eq!(cleared.overrides.blur, None);
    assert!(state.configuration_is_compatible_with_latest_snapshot(&cleared));
    state.reset_overrides().unwrap();
    let reset = state.pending_configuration().unwrap();
    assert!(reset.overrides.is_empty());
    assert!(state.configuration_is_compatible_with_latest_snapshot(&reset));
    state.restore_defaults().unwrap();
    let restored = state.pending_configuration().unwrap();
    assert_eq!(restored, MaterialConfiguration::default());
    assert!(state.configuration_is_compatible_with_latest_snapshot(&restored));
}

#[test]
fn rejecting_latest_configuration_restores_authoritative_values() {
    let mut state = VisualEffectsState::default();
    let authoritative = MaterialConfiguration::default();
    state
        .apply_snapshot(snapshot(3, authoritative.clone()))
        .unwrap();

    state.set_material_position(0.9).unwrap();
    let rejected = state.pending_configuration().unwrap();
    assert!(state.take_pending_configuration_if_matches(&rejected));
    state.reject_if_current(&rejected);

    assert_eq!(state.configuration(), &authoritative);
    assert_eq!(state.pending_configuration(), None);
}

#[test]
fn clearing_each_override_keeps_the_other_dimensions_and_position() {
    let mut state = VisualEffectsState::default();
    state
        .apply_snapshot(snapshot(
            1,
            MaterialConfiguration {
                position: 0.63,
                overrides: MaterialOverrides {
                    blur: Some(0.2),
                    saturation: Some(0.3),
                    noise: Some(0.4),
                },
                ..MaterialConfiguration::default()
            },
        ))
        .unwrap();

    state.clear_saturation_override().unwrap();
    let after_saturation = state.pending_configuration().unwrap();
    assert_eq!(after_saturation.position, 0.63);
    assert_eq!(after_saturation.overrides.blur, Some(0.2));
    assert_eq!(after_saturation.overrides.saturation, None);
    assert_eq!(after_saturation.overrides.noise, Some(0.4));

    state.clear_noise_override().unwrap();
    let after_noise = state.pending_configuration().unwrap();
    assert_eq!(after_noise.overrides.blur, Some(0.2));
    assert_eq!(after_noise.overrides.saturation, None);
    assert_eq!(after_noise.overrides.noise, None);
}
