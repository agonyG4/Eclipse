use super::{
    ActiveRequest, MaterialConfiguration, RequestState, SettingsVisualEffectsControllerRust,
    control_request_for,
};
use crate::typhon::client::{ClientError, WorkerEvent};
use crate::typhon::material_program as wire;
use crate::typhon::protocol::{
    ControlRejection, ControlRequest, ControlSuccess, MaterialProgramRequest, MaterialRequest,
    ProtocolOutcome,
};
use crate::visual_effects::state::{
    EffectiveMaterial, MaterialCapabilities, MaterialConfigSource, MaterialOverrides,
    MaterialSnapshot,
};

fn snapshot(
    generation: u64,
    configuration: MaterialConfiguration,
    capabilities: MaterialCapabilities,
) -> MaterialSnapshot {
    MaterialSnapshot {
        generation,
        source: MaterialConfigSource::Runtime,
        effective: EffectiveMaterial::default(),
        configuration,
        capabilities,
    }
}

const EFFECTIVE: &str = "system.background_blur";
const TRUSTED: &str = "studio.soft_glow";

fn program_snapshot(registry: u64, generation: u64) -> wire::MaterialProgramStateSnapshot {
    program_snapshot_select(
        registry,
        generation,
        "saved.missing",
        EFFECTIVE,
        Some(wire::MaterialProgramFallbackReason::Missing),
    )
}

fn program_snapshot_select(
    registry: u64,
    generation: u64,
    requested: &str,
    effective: &str,
    fallback_reason: Option<wire::MaterialProgramFallbackReason>,
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
                    parameter_count: 2,
                },
                wire::MaterialProgramCatalogEntry {
                    name: TRUSTED.to_owned(),
                    origin: wire::MaterialProgramOrigin::TrustedLocal,
                    schema_signature: 77,
                    parameter_count: 1,
                },
            ],
        },
        selection: wire::MaterialProgramSelectionSnapshot {
            generation,
            source: wire::MaterialProgramConfigSource::Persisted,
            configuration: wire::MaterialProgramConfiguration {
                version: 1,
                requested_program: requested.to_owned(),
            },
            effective_program: effective.to_owned(),
            registry_generation: registry,
            rendering_available: true,
            fallback_reason,
        },
    }
}

fn program_description(
    registry: u64,
    parameter_generation: u64,
    name: &str,
    schema_signature: u64,
    intensity: f64,
    intensity_overridden: bool,
) -> wire::MaterialProgramDescriptionSnapshot {
    wire::MaterialProgramDescriptionSnapshot {
        registry_generation: registry,
        parameter_generation,
        name: name.to_owned(),
        origin: if name == TRUSTED {
            wire::MaterialProgramOrigin::TrustedLocal
        } else {
            wire::MaterialProgramOrigin::System
        },
        schema_signature,
        parameters: vec![
            wire::MaterialProgramParameterDescriptor {
                name: "intensity".to_owned(),
                parameter_type: wire::MaterialProgramParameterType::Float,
                range: Some(wire::MaterialProgramParameterRange::Float { min: 0.0, max: 1.0 }),
                default: wire::MaterialProgramParameterValue::Float(0.5),
                effective: wire::MaterialProgramParameterValue::Float(intensity),
                overridden: intensity_overridden,
            },
            wire::MaterialProgramParameterDescriptor {
                name: "offset".to_owned(),
                parameter_type: wire::MaterialProgramParameterType::Vec3,
                range: Some(wire::MaterialProgramParameterRange::FloatComponents {
                    components: 3,
                    min: vec![-1.0; 3],
                    max: vec![1.0; 3],
                }),
                default: wire::MaterialProgramParameterValue::Vec3([0.0; 3]),
                effective: wire::MaterialProgramParameterValue::Vec3([0.0; 3]),
                overridden: false,
            },
        ],
    }
}

fn complete(
    controller: &mut SettingsVisualEffectsControllerRust,
    active: ActiveRequest,
    result: Result<ProtocolOutcome, ClientError>,
) {
    let id = controller.requests.start(active);
    controller.busy = true;
    let _ = controller.handle_request_finished(id, result);
}

#[test]
fn stale_completion_id_cannot_finish_the_current_request() {
    let mut requests = RequestState::default();
    let active = ActiveRequest::Phase2ASet(MaterialConfiguration::default());
    let current_id = requests.start(active.clone());

    assert!(requests.finish(current_id.wrapping_add(1)).is_none());
    assert!(requests.finish(current_id).is_some());
    assert!(requests.finish(current_id).is_none());
}

#[test]
fn completion_capability_downgrade_filters_pending_set_but_preserves_position_edit() {
    let mut controller = SettingsVisualEffectsControllerRust::default();
    controller
        .state
        .apply_snapshot(snapshot(
            1,
            MaterialConfiguration::default(),
            MaterialCapabilities {
                blur_override: true,
                ..MaterialCapabilities::default()
            },
        ))
        .unwrap();
    controller.debounce_elapsed = true;

    controller.state.set_material_position(0.6).unwrap();
    let request_a = controller.state.pending_configuration().unwrap();
    assert!(
        controller
            .state
            .take_pending_configuration_if_matches(&request_a)
    );
    let request_a_id = controller
        .requests
        .start(ActiveRequest::Phase2ASet(request_a.clone()));
    controller.busy = true;

    controller.state.set_material_position(0.82).unwrap();
    controller.state.set_blur_override(0.7).unwrap();
    assert_eq!(
        controller
            .state
            .pending_configuration()
            .unwrap()
            .overrides
            .blur,
        Some(0.7)
    );

    controller.handle_event(WorkerEvent::RequestFinished {
        id: request_a_id,
        result: Box::new(Ok(ProtocolOutcome::Success(ControlSuccess::Material(
            snapshot(
                2,
                MaterialConfiguration {
                    position: 0.6,
                    overrides: MaterialOverrides::default(),
                    ..MaterialConfiguration::default()
                },
                MaterialCapabilities::default(),
            ),
        )))),
    });

    let Some(configuration) = controller.state.pending_configuration_for_submission() else {
        panic!("position edit should still produce a Material Set request");
    };
    let outgoing_request = control_request_for(&ActiveRequest::Phase2ASet(configuration));
    let ControlRequest::Material(MaterialRequest::Set(outgoing_configuration)) = outgoing_request
    else {
        panic!("pending Visual Effects state should be sent as Material Set");
    };
    assert_eq!(outgoing_configuration.position, 0.82);
    assert_eq!(outgoing_configuration.overrides.blur, None);
    assert!(
        controller
            .state
            .configuration_is_compatible_with_latest_snapshot(&outgoing_configuration)
    );
}

#[test]
fn material_program_refresh_uses_one_typed_atomic_state_request() {
    let request = control_request_for(&ActiveRequest::MaterialProgramStateRefresh);
    assert_eq!(
        request,
        ControlRequest::MaterialProgram(MaterialProgramRequest::StateGet)
    );
}

#[test]
fn material_program_selection_request_contains_complete_version_one_configuration() {
    let configuration = wire::MaterialProgramConfiguration {
        version: 1,
        requested_program: "studio.soft_glow".to_owned(),
    };
    assert_eq!(
        control_request_for(&ActiveRequest::MaterialProgramSelectionSet(
            configuration.clone()
        )),
        ControlRequest::MaterialProgram(MaterialProgramRequest::Set(configuration))
    );
}

#[test]
fn material_program_parameter_request_contains_the_complete_typed_override_map() {
    let configuration = wire::MaterialProgramParameterConfiguration {
        version: 1,
        program: "studio.soft_glow".to_owned(),
        schema_signature: 9_007_199_254_741_113,
        overrides: [
            (
                "intensity".to_owned(),
                wire::MaterialProgramParameterValue::Float(0.8),
            ),
            (
                "offset".to_owned(),
                wire::MaterialProgramParameterValue::Vec3([0.1, 0.2, 0.3]),
            ),
        ]
        .into_iter()
        .collect(),
    };
    assert_eq!(
        control_request_for(&ActiveRequest::MaterialProgramParametersSet(
            configuration.clone()
        )),
        ControlRequest::MaterialProgram(MaterialProgramRequest::ParametersSet(configuration))
    );
}

#[test]
fn refresh_runs_phase2a_then_atomic_program_state_then_effective_description() {
    let mut controller = SettingsVisualEffectsControllerRust::default();
    controller.phase2a_refresh_pending = true;
    controller.material_program_refresh_pending = true;

    assert!(matches!(
        controller.next_request(),
        Some(ActiveRequest::Phase2ARefresh)
    ));
    complete(
        &mut controller,
        ActiveRequest::Phase2ARefresh,
        Ok(ProtocolOutcome::Success(ControlSuccess::Material(
            snapshot(
                1,
                MaterialConfiguration::default(),
                MaterialCapabilities::default(),
            ),
        ))),
    );
    assert!(matches!(
        controller.next_request(),
        Some(ActiveRequest::MaterialProgramStateRefresh)
    ));
    complete(
        &mut controller,
        ActiveRequest::MaterialProgramStateRefresh,
        Ok(ProtocolOutcome::Success(
            ControlSuccess::MaterialProgramState(program_snapshot(1, 1)),
        )),
    );
    let Some(ActiveRequest::MaterialProgramDescribe {
        registry_generation,
        name,
        schema_signature,
    }) = controller.next_request()
    else {
        panic!("refresh must describe the effective program")
    };
    assert_eq!(
        (registry_generation, name.as_str(), schema_signature),
        (1, EFFECTIVE, 1)
    );
    let args = control_request_for(&ActiveRequest::MaterialProgramDescribe {
        registry_generation,
        name,
        schema_signature,
    });
    assert_eq!(
        args,
        ControlRequest::MaterialProgram(MaterialProgramRequest::Describe(
            wire::MaterialProgramNameArguments {
                name: EFFECTIVE.to_owned()
            }
        ))
    );
}

#[test]
fn same_registry_selection_is_followed_by_description_of_new_effective_program() {
    let mut controller = SettingsVisualEffectsControllerRust::default();
    controller
        .material_program
        .apply_snapshot(program_snapshot(1, 3))
        .unwrap();
    controller
        .material_program
        .apply_description(program_description(1, 2, EFFECTIVE, 1, 0.5, false))
        .unwrap();
    controller.material_program.select_program(TRUSTED).unwrap();
    let Some(ActiveRequest::MaterialProgramSelectionSet(configuration)) = controller.next_request()
    else {
        panic!("selection request expected")
    };
    complete(
        &mut controller,
        ActiveRequest::MaterialProgramSelectionSet(configuration),
        Ok(ProtocolOutcome::Success(
            ControlSuccess::MaterialProgramSelection(wire::MaterialProgramSelectionSnapshot {
                generation: 4,
                source: wire::MaterialProgramConfigSource::Runtime,
                configuration: wire::MaterialProgramConfiguration {
                    version: 1,
                    requested_program: TRUSTED.to_owned(),
                },
                effective_program: TRUSTED.to_owned(),
                registry_generation: 1,
                rendering_available: true,
                fallback_reason: None,
            }),
        )),
    );
    assert!(
        matches!(controller.next_request(), Some(ActiveRequest::MaterialProgramDescribe { registry_generation: 1, name, schema_signature: 77 }) if name == TRUSTED)
    );
}

#[test]
fn cross_registry_selection_result_refreshes_atomic_state_instead_of_merging() {
    let mut controller = SettingsVisualEffectsControllerRust::default();
    controller
        .material_program
        .apply_snapshot(program_snapshot(1, 3))
        .unwrap();
    controller.material_program.select_program(TRUSTED).unwrap();
    let configuration = controller
        .material_program
        .pending_selection_configuration()
        .unwrap();
    complete(
        &mut controller,
        ActiveRequest::MaterialProgramSelectionSet(configuration),
        Ok(ProtocolOutcome::Success(
            ControlSuccess::MaterialProgramSelection(wire::MaterialProgramSelectionSnapshot {
                generation: 4,
                source: wire::MaterialProgramConfigSource::Runtime,
                configuration: wire::MaterialProgramConfiguration {
                    version: 1,
                    requested_program: TRUSTED.to_owned(),
                },
                effective_program: TRUSTED.to_owned(),
                registry_generation: 2,
                rendering_available: true,
                fallback_reason: None,
            }),
        )),
    );
    assert!(matches!(
        controller.next_request(),
        Some(ActiveRequest::MaterialProgramStateRefresh)
    ));
    assert_eq!(
        controller.material_program.effective_program(),
        Some(EFFECTIVE)
    );
}

#[test]
fn mismatched_description_is_discarded_and_refreshed() {
    let mut controller = SettingsVisualEffectsControllerRust::default();
    controller
        .material_program
        .apply_snapshot(program_snapshot(1, 1))
        .unwrap();
    complete(
        &mut controller,
        ActiveRequest::MaterialProgramDescribe {
            registry_generation: 1,
            name: EFFECTIVE.to_owned(),
            schema_signature: 1,
        },
        Ok(ProtocolOutcome::Success(
            ControlSuccess::MaterialProgramDescription(program_description(
                2, 1, EFFECTIVE, 1, 0.5, false,
            )),
        )),
    );
    assert!(controller.material_program.description().is_none());
    assert!(matches!(
        controller.next_request(),
        Some(ActiveRequest::MaterialProgramStateRefresh)
    ));
}

#[test]
fn same_effective_selection_still_refreshes_its_description() {
    let mut controller = SettingsVisualEffectsControllerRust::default();
    controller
        .material_program
        .apply_snapshot(program_snapshot(1, 1))
        .unwrap();
    controller
        .material_program
        .apply_description(program_description(1, 1, EFFECTIVE, 1, 0.5, false))
        .unwrap();
    controller
        .material_program
        .select_program(EFFECTIVE)
        .unwrap();
    let Some(ActiveRequest::MaterialProgramSelectionSet(configuration)) = controller.next_request()
    else {
        panic!("selection request expected")
    };
    complete(
        &mut controller,
        ActiveRequest::MaterialProgramSelectionSet(configuration),
        Ok(ProtocolOutcome::Success(
            ControlSuccess::MaterialProgramSelection(wire::MaterialProgramSelectionSnapshot {
                generation: 2,
                source: wire::MaterialProgramConfigSource::Runtime,
                configuration: wire::MaterialProgramConfiguration {
                    version: 1,
                    requested_program: EFFECTIVE.to_owned(),
                },
                effective_program: EFFECTIVE.to_owned(),
                registry_generation: 1,
                rendering_available: true,
                fallback_reason: None,
            }),
        )),
    );
    assert!(matches!(
        controller.next_request(),
        Some(ActiveRequest::MaterialProgramDescribe { .. })
    ));
}

#[test]
fn repeated_material_program_races_stop_after_bounded_recovery() {
    let mut controller = SettingsVisualEffectsControllerRust::default();
    controller
        .material_program
        .apply_snapshot(program_snapshot(1, 1))
        .unwrap();
    for retry in 0..3 {
        let Some(ActiveRequest::MaterialProgramDescribe {
            registry_generation,
            name,
            schema_signature,
        }) = controller.next_request()
        else {
            panic!("description request expected before race {retry}")
        };
        complete(
            &mut controller,
            ActiveRequest::MaterialProgramDescribe {
                registry_generation,
                name: name.clone(),
                schema_signature,
            },
            Ok(ProtocolOutcome::Success(
                ControlSuccess::MaterialProgramDescription(program_description(
                    registry_generation + 1,
                    2,
                    &name,
                    schema_signature,
                    0.5,
                    false,
                )),
            )),
        );
        if retry < 2 {
            assert!(matches!(
                controller.next_request(),
                Some(ActiveRequest::MaterialProgramStateRefresh)
            ));
            complete(
                &mut controller,
                ActiveRequest::MaterialProgramStateRefresh,
                Ok(ProtocolOutcome::Success(
                    ControlSuccess::MaterialProgramState(program_snapshot(1, 1)),
                )),
            );
        }
    }
    assert!(controller.material_program_recovery_blocked);
    assert!(controller.next_request().is_none());
}

#[test]
fn stale_schema_rejection_discards_edits_and_refreshes_without_retry() {
    let mut controller = SettingsVisualEffectsControllerRust::default();
    controller
        .material_program
        .apply_snapshot(program_snapshot(1, 1))
        .unwrap();
    controller
        .material_program
        .apply_description(program_description(1, 2, EFFECTIVE, 1, 0.5, false))
        .unwrap();
    controller
        .material_program
        .set_float_parameter("intensity", 0.8)
        .unwrap();
    let configuration = controller
        .material_program
        .pending_parameter_configuration()
        .unwrap();
    complete(
        &mut controller,
        ActiveRequest::MaterialProgramParametersSet(configuration),
        Ok(ProtocolOutcome::ServerRejected(ControlRejection {
            code: Some("stale".to_owned()),
            message: "schema changed".to_owned(),
            detail: Some("stale_material_program_schema".to_owned()),
        })),
    );
    assert!(
        controller
            .material_program
            .pending_parameter_configuration()
            .is_none()
    );
    assert!(matches!(
        controller.next_request(),
        Some(ActiveRequest::MaterialProgramStateRefresh)
    ));
}

#[test]
fn parameter_set_response_without_description_refreshes_state() {
    let mut controller = SettingsVisualEffectsControllerRust::default();
    controller
        .material_program
        .apply_snapshot(program_snapshot(1, 1))
        .unwrap();
    controller
        .material_program
        .apply_description(program_description(1, 1, EFFECTIVE, 1, 0.5, false))
        .unwrap();
    controller
        .material_program
        .set_float_parameter("intensity", 0.8)
        .unwrap();
    let configuration = controller
        .material_program
        .pending_parameter_configuration()
        .unwrap();
    let response = wire::MaterialProgramParameterConfigurationSnapshot {
        registry_generation: 1,
        parameter_generation: 2,
        configuration: configuration.clone(),
        description: None,
    };
    complete(
        &mut controller,
        ActiveRequest::MaterialProgramParametersSet(configuration),
        Ok(ProtocolOutcome::Success(
            ControlSuccess::MaterialProgramParameters(response),
        )),
    );
    assert!(matches!(
        controller.next_request(),
        Some(ActiveRequest::MaterialProgramStateRefresh)
    ));
}

#[test]
fn newer_parameter_edits_survive_in_flight_authoritative_completion() {
    let mut controller = SettingsVisualEffectsControllerRust::default();
    controller
        .material_program
        .apply_snapshot(program_snapshot(1, 1))
        .unwrap();
    controller
        .material_program
        .apply_description(program_description(1, 1, EFFECTIVE, 1, 0.5, false))
        .unwrap();
    controller
        .material_program
        .set_float_parameter("intensity", 0.7)
        .unwrap();
    let submitted = controller
        .material_program
        .pending_parameter_configuration()
        .unwrap();
    controller
        .material_program
        .mark_parameter_configuration_submitted(&submitted);
    controller
        .material_program
        .set_float_parameter("intensity", 0.9)
        .unwrap();
    controller.debounce_elapsed = true;
    let response = wire::MaterialProgramParameterConfigurationSnapshot {
        registry_generation: 1,
        parameter_generation: 2,
        configuration: submitted.clone(),
        description: Some(program_description(1, 2, EFFECTIVE, 1, 0.7, true)),
    };
    complete(
        &mut controller,
        ActiveRequest::MaterialProgramParametersSet(submitted),
        Ok(ProtocolOutcome::Success(
            ControlSuccess::MaterialProgramParameters(response),
        )),
    );
    let Some(ActiveRequest::MaterialProgramParametersSet(next)) = controller.next_request() else {
        panic!("newer edits should be sent after A finishes")
    };
    assert_eq!(
        next.overrides.get("intensity"),
        Some(&wire::MaterialProgramParameterValue::Float(0.9))
    );
}

#[test]
fn queued_selection_waits_for_current_program_parameters_to_flush() {
    let mut controller = SettingsVisualEffectsControllerRust::default();
    controller
        .material_program
        .apply_snapshot(program_snapshot(1, 1))
        .unwrap();
    controller
        .material_program
        .apply_description(program_description(1, 1, EFFECTIVE, 1, 0.5, false))
        .unwrap();
    controller
        .material_program
        .set_float_parameter("intensity", 0.8)
        .unwrap();
    controller.material_program.select_program(TRUSTED).unwrap();
    let Some(ActiveRequest::MaterialProgramParametersSet(configuration)) =
        controller.next_request()
    else {
        panic!("parameter flush must precede selection")
    };
    let response = wire::MaterialProgramParameterConfigurationSnapshot {
        registry_generation: 1,
        parameter_generation: 2,
        configuration: configuration.clone(),
        description: Some(program_description(1, 2, EFFECTIVE, 1, 0.8, true)),
    };
    complete(
        &mut controller,
        ActiveRequest::MaterialProgramParametersSet(configuration),
        Ok(ProtocolOutcome::Success(
            ControlSuccess::MaterialProgramParameters(response),
        )),
    );
    assert!(matches!(
        controller.next_request(),
        Some(ActiveRequest::MaterialProgramSelectionSet(_))
    ));
}

#[test]
fn material_program_failure_does_not_destroy_phase2a_snapshot() {
    let mut controller = SettingsVisualEffectsControllerRust::default();
    controller
        .state
        .apply_snapshot(snapshot(
            2,
            MaterialConfiguration::default(),
            MaterialCapabilities::default(),
        ))
        .unwrap();
    complete(
        &mut controller,
        ActiveRequest::MaterialProgramStateRefresh,
        Ok(ProtocolOutcome::ServerRejected(ControlRejection {
            code: None,
            message: "unsupported command".to_owned(),
            detail: None,
        })),
    );
    assert!(controller.state.available());
    assert!(controller.material_program.snapshot().is_none());
}

#[test]
fn transport_loss_marks_both_domains_unavailable_and_explicit_refresh_recovers() {
    let mut controller = SettingsVisualEffectsControllerRust::default();
    controller
        .state
        .apply_snapshot(snapshot(
            2,
            MaterialConfiguration::default(),
            MaterialCapabilities::default(),
        ))
        .unwrap();
    controller
        .material_program
        .apply_snapshot(program_snapshot(4, 8))
        .unwrap();
    complete(
        &mut controller,
        ActiveRequest::MaterialProgramStateRefresh,
        Err(ClientError::Timeout),
    );
    assert!(!controller.state.available());
    assert!(!controller.material_program.is_available());
    controller.phase2a_refresh_pending = true;
    controller.material_program_refresh_pending = true;
    assert!(matches!(
        controller.next_request(),
        Some(ActiveRequest::Phase2ARefresh)
    ));
}
