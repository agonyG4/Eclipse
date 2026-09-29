use super::material_program::{
    MaterialProgramState, MaterialProgramStateError, ParameterApply, SelectionApply,
};
use super::qt_projection;
use super::state::{MaterialConfiguration, StateError, VisualEffectsState};
use crate::typhon::client::{ClientError, ClientWorker, WorkerEvent};
use crate::typhon::material_program as wire;
use crate::typhon::protocol::{
    ControlRejection, ControlRequest, ControlSuccess, MaterialProgramRequest, MaterialRequest,
    ProtocolOutcome,
};
use cxx_qt::{CxxQtType, Threading};
use cxx_qt_lib::{QList, QString, QVariant};
use std::pin::Pin;

#[cxx_qt::bridge]
pub mod qobject {
    unsafe extern "C++" {
        include!("cxx-qt-lib/qstring.h");
        include!("cxx-qt-lib/core/qlist/qlist_QVariant.h");
        type QString = cxx_qt_lib::QString;
        type QVariant = cxx_qt_lib::QVariant;
        type QList_QVariant = cxx_qt_lib::QList<QVariant>;
    }

    extern "RustQt" {
        #[qobject]
        #[qproperty(bool, available, READ = available, NOTIFY = availability_changed)]
        #[qproperty(bool, busy, READ = busy, NOTIFY = busy_changed)]
        #[qproperty(f64, material_position, cxx_name = "materialPosition", READ = material_position, NOTIFY = snapshot_changed)]
        #[qproperty(f64, default_material_position, cxx_name = "defaultMaterialPosition", READ = default_material_position, NOTIFY = snapshot_changed)]
        #[qproperty(f64, effective_blur, cxx_name = "effectiveBlur", READ = effective_blur, NOTIFY = snapshot_changed)]
        #[qproperty(f64, effective_saturation, cxx_name = "effectiveSaturation", READ = effective_saturation, NOTIFY = snapshot_changed)]
        #[qproperty(f64, effective_noise, cxx_name = "effectiveNoise", READ = effective_noise, NOTIFY = snapshot_changed)]
        #[qproperty(f64, blur_value, cxx_name = "blurValue", READ = blur_value, NOTIFY = snapshot_changed)]
        #[qproperty(f64, saturation_value, cxx_name = "saturationValue", READ = saturation_value, NOTIFY = snapshot_changed)]
        #[qproperty(f64, noise_value, cxx_name = "noiseValue", READ = noise_value, NOTIFY = snapshot_changed)]
        #[qproperty(bool, blur_override_supported, cxx_name = "blurOverrideSupported", READ = blur_override_supported, NOTIFY = snapshot_changed)]
        #[qproperty(bool, saturation_override_supported, cxx_name = "saturationOverrideSupported", READ = saturation_override_supported, NOTIFY = snapshot_changed)]
        #[qproperty(bool, noise_override_supported, cxx_name = "noiseOverrideSupported", READ = noise_override_supported, NOTIFY = snapshot_changed)]
        #[qproperty(bool, blur_overridden, cxx_name = "blurOverridden", READ = blur_overridden, NOTIFY = snapshot_changed)]
        #[qproperty(bool, saturation_overridden, cxx_name = "saturationOverridden", READ = saturation_overridden, NOTIFY = snapshot_changed)]
        #[qproperty(bool, noise_overridden, cxx_name = "noiseOverridden", READ = noise_overridden, NOTIFY = snapshot_changed)]
        #[qproperty(bool, has_overrides, cxx_name = "hasOverrides", READ = has_overrides, NOTIFY = snapshot_changed)]
        #[qproperty(u64, generation, READ = generation, NOTIFY = snapshot_changed)]
        #[qproperty(QString, source, READ = source, NOTIFY = snapshot_changed)]
        #[qproperty(QString, last_error, cxx_name = "lastError", READ = last_error, NOTIFY = error_changed)]
        #[qproperty(bool, material_program_available, cxx_name = "materialProgramAvailable", READ = material_program_available, NOTIFY = material_program_availability_changed)]
        #[qproperty(bool, material_program_rendering_available, cxx_name = "materialProgramRenderingAvailable", READ = material_program_rendering_available, NOTIFY = material_program_changed)]
        #[qproperty(bool, material_program_busy, cxx_name = "materialProgramBusy", READ = material_program_busy, NOTIFY = busy_changed)]
        #[qproperty(bool, material_program_editing_available, cxx_name = "materialProgramEditingAvailable", READ = material_program_editing_available, NOTIFY = material_program_changed)]
        #[qproperty(QList_QVariant, material_program_catalog, cxx_name = "materialProgramCatalog", READ = material_program_catalog, NOTIFY = material_program_changed)]
        #[qproperty(QString, material_program_requested, cxx_name = "materialProgramRequested", READ = material_program_requested, NOTIFY = material_program_changed)]
        #[qproperty(QString, material_program_pending_selection, cxx_name = "materialProgramPendingSelection", READ = material_program_pending_selection, NOTIFY = material_program_changed)]
        #[qproperty(QString, material_program_effective, cxx_name = "materialProgramEffective", READ = material_program_effective, NOTIFY = material_program_changed)]
        #[qproperty(QString, material_program_fallback_reason, cxx_name = "materialProgramFallbackReason", READ = material_program_fallback_reason, NOTIFY = material_program_changed)]
        #[qproperty(QString, material_program_effective_origin, cxx_name = "materialProgramEffectiveOrigin", READ = material_program_effective_origin, NOTIFY = material_program_changed)]
        #[qproperty(QList_QVariant, material_program_parameters, cxx_name = "materialProgramParameters", READ = material_program_parameters, NOTIFY = material_program_changed)]
        #[qproperty(bool, material_program_has_overrides, cxx_name = "materialProgramHasOverrides", READ = material_program_has_overrides, NOTIFY = material_program_changed)]
        #[qproperty(QString, material_program_error, cxx_name = "materialProgramError", READ = material_program_error, NOTIFY = error_changed)]
        type SettingsVisualEffectsController = super::SettingsVisualEffectsControllerRust;
    }

    unsafe extern "RustQt" {
        fn available(self: &SettingsVisualEffectsController) -> bool;
        fn busy(self: &SettingsVisualEffectsController) -> bool;
        #[cxx_name = "materialPosition"]
        fn material_position(self: &SettingsVisualEffectsController) -> f64;
        #[cxx_name = "defaultMaterialPosition"]
        fn default_material_position(self: &SettingsVisualEffectsController) -> f64;
        #[cxx_name = "effectiveBlur"]
        fn effective_blur(self: &SettingsVisualEffectsController) -> f64;
        #[cxx_name = "effectiveSaturation"]
        fn effective_saturation(self: &SettingsVisualEffectsController) -> f64;
        #[cxx_name = "effectiveNoise"]
        fn effective_noise(self: &SettingsVisualEffectsController) -> f64;
        #[cxx_name = "blurValue"]
        fn blur_value(self: &SettingsVisualEffectsController) -> f64;
        #[cxx_name = "saturationValue"]
        fn saturation_value(self: &SettingsVisualEffectsController) -> f64;
        #[cxx_name = "noiseValue"]
        fn noise_value(self: &SettingsVisualEffectsController) -> f64;
        #[cxx_name = "blurOverrideSupported"]
        fn blur_override_supported(self: &SettingsVisualEffectsController) -> bool;
        #[cxx_name = "saturationOverrideSupported"]
        fn saturation_override_supported(self: &SettingsVisualEffectsController) -> bool;
        #[cxx_name = "noiseOverrideSupported"]
        fn noise_override_supported(self: &SettingsVisualEffectsController) -> bool;
        #[cxx_name = "blurOverridden"]
        fn blur_overridden(self: &SettingsVisualEffectsController) -> bool;
        #[cxx_name = "saturationOverridden"]
        fn saturation_overridden(self: &SettingsVisualEffectsController) -> bool;
        #[cxx_name = "noiseOverridden"]
        fn noise_overridden(self: &SettingsVisualEffectsController) -> bool;
        #[cxx_name = "hasOverrides"]
        fn has_overrides(self: &SettingsVisualEffectsController) -> bool;
        fn generation(self: &SettingsVisualEffectsController) -> u64;
        fn source(self: &SettingsVisualEffectsController) -> QString;
        #[cxx_name = "lastError"]
        fn last_error(self: &SettingsVisualEffectsController) -> QString;
        #[cxx_name = "materialProgramAvailable"]
        fn material_program_available(self: &SettingsVisualEffectsController) -> bool;
        #[cxx_name = "materialProgramRenderingAvailable"]
        fn material_program_rendering_available(self: &SettingsVisualEffectsController) -> bool;
        #[cxx_name = "materialProgramBusy"]
        fn material_program_busy(self: &SettingsVisualEffectsController) -> bool;
        #[cxx_name = "materialProgramEditingAvailable"]
        fn material_program_editing_available(self: &SettingsVisualEffectsController) -> bool;
        #[cxx_name = "materialProgramCatalog"]
        fn material_program_catalog(self: &SettingsVisualEffectsController) -> QList_QVariant;
        #[cxx_name = "materialProgramRequested"]
        fn material_program_requested(self: &SettingsVisualEffectsController) -> QString;
        #[cxx_name = "materialProgramPendingSelection"]
        fn material_program_pending_selection(self: &SettingsVisualEffectsController) -> QString;
        #[cxx_name = "materialProgramEffective"]
        fn material_program_effective(self: &SettingsVisualEffectsController) -> QString;
        #[cxx_name = "materialProgramFallbackReason"]
        fn material_program_fallback_reason(self: &SettingsVisualEffectsController) -> QString;
        #[cxx_name = "materialProgramEffectiveOrigin"]
        fn material_program_effective_origin(self: &SettingsVisualEffectsController) -> QString;
        #[cxx_name = "materialProgramParameters"]
        fn material_program_parameters(self: &SettingsVisualEffectsController) -> QList_QVariant;
        #[cxx_name = "materialProgramHasOverrides"]
        fn material_program_has_overrides(self: &SettingsVisualEffectsController) -> bool;
        #[cxx_name = "materialProgramError"]
        fn material_program_error(self: &SettingsVisualEffectsController) -> QString;

        #[qsignal]
        #[cxx_name = "availabilityChanged"]
        fn availability_changed(self: Pin<&mut SettingsVisualEffectsController>);
        #[qsignal]
        #[cxx_name = "busyChanged"]
        fn busy_changed(self: Pin<&mut SettingsVisualEffectsController>);
        #[qsignal]
        #[cxx_name = "snapshotChanged"]
        fn snapshot_changed(self: Pin<&mut SettingsVisualEffectsController>);
        #[qsignal]
        #[cxx_name = "errorChanged"]
        fn error_changed(self: Pin<&mut SettingsVisualEffectsController>);
        #[qsignal]
        #[cxx_name = "materialProgramAvailabilityChanged"]
        fn material_program_availability_changed(self: Pin<&mut SettingsVisualEffectsController>);
        #[qsignal]
        #[cxx_name = "materialProgramChanged"]
        fn material_program_changed(self: Pin<&mut SettingsVisualEffectsController>);

        #[qinvokable]
        fn refresh(self: Pin<&mut SettingsVisualEffectsController>);
        #[qinvokable]
        #[cxx_name = "setMaterialPosition"]
        fn set_material_position(self: Pin<&mut SettingsVisualEffectsController>, value: f64);
        #[qinvokable]
        #[cxx_name = "setBlurOverride"]
        fn set_blur_override(self: Pin<&mut SettingsVisualEffectsController>, value: f64);
        #[qinvokable]
        #[cxx_name = "clearBlurOverride"]
        fn clear_blur_override(self: Pin<&mut SettingsVisualEffectsController>);
        #[qinvokable]
        #[cxx_name = "setSaturationOverride"]
        fn set_saturation_override(self: Pin<&mut SettingsVisualEffectsController>, value: f64);
        #[qinvokable]
        #[cxx_name = "clearSaturationOverride"]
        fn clear_saturation_override(self: Pin<&mut SettingsVisualEffectsController>);
        #[qinvokable]
        #[cxx_name = "setNoiseOverride"]
        fn set_noise_override(self: Pin<&mut SettingsVisualEffectsController>, value: f64);
        #[qinvokable]
        #[cxx_name = "clearNoiseOverride"]
        fn clear_noise_override(self: Pin<&mut SettingsVisualEffectsController>);
        #[qinvokable]
        #[cxx_name = "resetOverrides"]
        fn reset_overrides(self: Pin<&mut SettingsVisualEffectsController>);
        #[qinvokable]
        #[cxx_name = "restoreDefaults"]
        fn restore_defaults(self: Pin<&mut SettingsVisualEffectsController>);
        #[qinvokable]
        fn flush(self: Pin<&mut SettingsVisualEffectsController>);
        #[qinvokable]
        #[cxx_name = "selectMaterialProgram"]
        fn select_material_program(self: Pin<&mut SettingsVisualEffectsController>, name: QString);
        #[qinvokable]
        #[cxx_name = "setMaterialProgramFloatParameter"]
        fn set_material_program_float_parameter(
            self: Pin<&mut SettingsVisualEffectsController>,
            name: QString,
            value: f64,
        );
        #[qinvokable]
        #[cxx_name = "setMaterialProgramIntParameter"]
        fn set_material_program_int_parameter(
            self: Pin<&mut SettingsVisualEffectsController>,
            name: QString,
            value: i32,
        );
        #[qinvokable]
        #[cxx_name = "setMaterialProgramVectorComponent"]
        fn set_material_program_vector_component(
            self: Pin<&mut SettingsVisualEffectsController>,
            name: QString,
            component: i32,
            value: f64,
        );
        #[qinvokable]
        #[cxx_name = "clearMaterialProgramParameter"]
        fn clear_material_program_parameter(
            self: Pin<&mut SettingsVisualEffectsController>,
            name: QString,
        );
        #[qinvokable]
        #[cxx_name = "resetMaterialProgramParameters"]
        fn reset_material_program_parameters(self: Pin<&mut SettingsVisualEffectsController>);
        #[qinvokable]
        #[cxx_name = "flushMaterialProgramParameters"]
        fn flush_material_program_parameters(self: Pin<&mut SettingsVisualEffectsController>);
    }

    impl cxx_qt::Initialize for SettingsVisualEffectsController {}
    impl cxx_qt::Threading for SettingsVisualEffectsController {}
}

pub struct SettingsVisualEffectsControllerRust {
    state: VisualEffectsState,
    material_program: MaterialProgramState,
    worker: Option<ClientWorker>,
    requests: RequestState,
    busy: bool,
    last_error: String,
    debounce_token: u64,
    debounce_elapsed: bool,
    phase2a_refresh_pending: bool,
    material_program_refresh_pending: bool,
    material_program_description_pending: bool,
    material_program_recovery_count: u8,
    material_program_selection_recovery_count: u8,
    material_program_recovery_blocked: bool,
    material_program_error: String,
}

#[derive(Clone)]
enum ActiveRequest {
    Phase2ARefresh,
    Phase2ASet(MaterialConfiguration),
    MaterialProgramStateRefresh,
    MaterialProgramDescribe {
        registry_generation: u64,
        name: String,
        schema_signature: u64,
    },
    MaterialProgramSelectionSet(wire::MaterialProgramConfiguration),
    MaterialProgramParametersSet(wire::MaterialProgramParameterConfiguration),
}

fn control_request_for(active: &ActiveRequest) -> ControlRequest {
    match active {
        ActiveRequest::Phase2ARefresh => ControlRequest::Material(MaterialRequest::Get),
        ActiveRequest::Phase2ASet(configuration) => {
            ControlRequest::Material(MaterialRequest::Set(configuration.clone()))
        }
        ActiveRequest::MaterialProgramStateRefresh => {
            ControlRequest::MaterialProgram(MaterialProgramRequest::StateGet)
        }
        ActiveRequest::MaterialProgramDescribe { name, .. } => {
            ControlRequest::MaterialProgram(MaterialProgramRequest::Describe(
                wire::MaterialProgramNameArguments { name: name.clone() },
            ))
        }
        ActiveRequest::MaterialProgramSelectionSet(configuration) => {
            ControlRequest::MaterialProgram(MaterialProgramRequest::Set(configuration.clone()))
        }
        ActiveRequest::MaterialProgramParametersSet(configuration) => {
            ControlRequest::MaterialProgram(MaterialProgramRequest::ParametersSet(
                configuration.clone(),
            ))
        }
    }
}

#[derive(Default)]
struct RequestState {
    next_id: u64,
    active_id: Option<u64>,
    active: Option<ActiveRequest>,
}

impl RequestState {
    fn start(&mut self, active: ActiveRequest) -> u64 {
        let id = if self.next_id == 0 { 1 } else { self.next_id };
        self.next_id = id.wrapping_add(1).max(1);
        self.active_id = Some(id);
        self.active = Some(active);
        id
    }

    fn finish(&mut self, id: u64) -> Option<ActiveRequest> {
        if self.active_id != Some(id) {
            return None;
        }
        self.active_id = None;
        self.active.take()
    }
}

impl Default for SettingsVisualEffectsControllerRust {
    fn default() -> Self {
        Self {
            state: VisualEffectsState::default(),
            material_program: MaterialProgramState::default(),
            worker: None,
            requests: RequestState {
                next_id: 1,
                ..RequestState::default()
            },
            busy: false,
            last_error: String::new(),
            debounce_token: 0,
            debounce_elapsed: false,
            phase2a_refresh_pending: false,
            material_program_refresh_pending: false,
            material_program_description_pending: false,
            material_program_recovery_count: 0,
            material_program_selection_recovery_count: 0,
            material_program_recovery_blocked: false,
            material_program_error: String::new(),
        }
    }
}

#[derive(Default)]
struct EventChanges {
    availability: bool,
    material_program_availability: bool,
    busy: bool,
    snapshot: bool,
    material_program: bool,
    error: bool,
}

impl SettingsVisualEffectsControllerRust {
    fn handle_event(&mut self, event: WorkerEvent) -> EventChanges {
        match event {
            WorkerEvent::DebounceElapsed { token } => {
                if self.debounce_token == token {
                    self.debounce_elapsed = true;
                }
                EventChanges::default()
            }
            WorkerEvent::RequestFinished { id, result } => {
                self.handle_request_finished(id, *result)
            }
        }
    }

    fn handle_request_finished(
        &mut self,
        id: u64,
        result: Result<ProtocolOutcome, ClientError>,
    ) -> EventChanges {
        let Some(active) = self.requests.finish(id) else {
            return EventChanges::default();
        };
        let mut changes = EventChanges::default();
        if self.busy {
            self.busy = false;
            changes.busy = true;
        }

        match result {
            Ok(ProtocolOutcome::Success(success)) => {
                self.apply_success(active, success, &mut changes);
            }
            Ok(ProtocolOutcome::ServerRejected(rejection)) => {
                self.apply_rejection(active, rejection, &mut changes);
            }
            Err(error) => self.apply_client_error(active, error, &mut changes),
        }

        changes
    }

    fn apply_success(
        &mut self,
        active: ActiveRequest,
        success: ControlSuccess,
        changes: &mut EventChanges,
    ) {
        match (active, success) {
            (
                ActiveRequest::Phase2ARefresh | ActiveRequest::Phase2ASet(_),
                ControlSuccess::Material(snapshot),
            ) => {
                let was_available = self.state.available();
                match self.state.apply_snapshot(snapshot) {
                    Ok(()) => {
                        changes.snapshot = true;
                        changes.availability |= !was_available;
                        changes.error |= self.set_error_value(String::new());
                    }
                    Err(error) => changes.error |= self.set_error_value(error.to_string()),
                }
            }
            (
                ActiveRequest::MaterialProgramStateRefresh,
                ControlSuccess::MaterialProgramState(snapshot),
            ) => {
                let was_available = self.material_program.is_available();
                match self.material_program.apply_snapshot(snapshot) {
                    Ok(()) => {
                        self.material_program_description_pending = true;
                        changes.material_program = true;
                        changes.material_program_availability |= !was_available;
                        changes.error |= self.set_material_program_error(String::new());
                    }
                    Err(error) => {
                        self.material_program_error = bounded_error(&error.to_string());
                        changes.error = true;
                    }
                }
            }
            (
                ActiveRequest::MaterialProgramDescribe {
                    registry_generation,
                    name,
                    schema_signature,
                },
                ControlSuccess::MaterialProgramDescription(description),
            ) => {
                let identity_matches = description.registry_generation == registry_generation
                    && description.name == name
                    && description.schema_signature == schema_signature;
                if identity_matches && self.material_program.apply_description(description).is_ok()
                {
                    self.material_program_recovery_count = 0;
                    self.material_program_description_pending = false;
                    changes.material_program = true;
                    changes.error |= self.set_material_program_error(String::new());
                } else {
                    self.request_bounded_material_program_recovery(changes);
                }
            }
            (
                ActiveRequest::MaterialProgramSelectionSet(configuration),
                ControlSuccess::MaterialProgramSelection(selection),
            ) => {
                match self
                    .material_program
                    .apply_selection_result(selection, &configuration)
                {
                    Ok(SelectionApply::Accepted) => {
                        self.material_program_selection_recovery_count = 0;
                        self.material_program_description_pending = true;
                        changes.material_program = true;
                        changes.error |= self.set_material_program_error(String::new());
                    }
                    Ok(SelectionApply::RefreshRequired) | Err(_) => {
                        self.request_material_program_selection_refresh(
                            &configuration.requested_program,
                            changes,
                        );
                    }
                }
            }
            (
                ActiveRequest::MaterialProgramParametersSet(configuration),
                ControlSuccess::MaterialProgramParameters(result),
            ) => {
                match self
                    .material_program
                    .apply_parameter_result(result, &configuration)
                {
                    Ok(ParameterApply::Accepted) => {
                        changes.material_program = true;
                        changes.error |= self.set_material_program_error(String::new());
                    }
                    Ok(ParameterApply::RefreshRequired) => {
                        self.material_program_refresh_pending = true;
                        changes.material_program = true;
                    }
                    Err(error) => {
                        if matches!(
                            error,
                            MaterialProgramStateError::WrongEffectiveProgram
                                | MaterialProgramStateError::WrongSchemaSignature
                        ) {
                            self.material_program.discard_parameter_edits();
                        }
                        self.request_bounded_material_program_recovery(changes);
                    }
                }
            }
            _ => {
                changes.error |= self.set_error_value(String::from(
                    "Typhon returned a response for a different Visual Effects request.",
                ))
            }
        }
    }

    fn apply_rejection(
        &mut self,
        active: ActiveRequest,
        rejection: ControlRejection,
        changes: &mut EventChanges,
    ) {
        match active {
            ActiveRequest::Phase2ASet(configuration) => {
                self.state.reject_if_current(&configuration);
                changes.snapshot = true;
                changes.error |= self.set_error_value(rejection.message);
            }
            ActiveRequest::MaterialProgramSelectionSet(configuration) => {
                self.material_program
                    .reject_selection_if_current(&configuration.requested_program);
                changes.material_program = true;
                changes.error |= self.set_material_program_error(rejection.message);
            }
            ActiveRequest::MaterialProgramParametersSet(configuration) => {
                if rejection.detail.as_deref() == Some("stale_material_program_schema") {
                    self.material_program.discard_parameter_edits();
                    self.material_program_refresh_pending = true;
                    changes.material_program = true;
                } else {
                    self.material_program
                        .reject_parameter_configuration_if_current(&configuration);
                    changes.material_program = true;
                }
                changes.error |= self.set_material_program_error(rejection.message);
            }
            ActiveRequest::MaterialProgramStateRefresh
            | ActiveRequest::MaterialProgramDescribe { .. } => {
                self.material_program.set_unavailable();
                self.material_program_description_pending = false;
                changes.material_program = true;
                changes.material_program_availability = true;
                changes.error |= self.set_material_program_error(rejection.message);
            }
            ActiveRequest::Phase2ARefresh => {
                changes.error |= self.set_error_value(rejection.message)
            }
        }
    }

    fn apply_client_error(
        &mut self,
        active: ActiveRequest,
        error: ClientError,
        changes: &mut EventChanges,
    ) {
        let material_program_refresh_error = matches!(
            &active,
            ActiveRequest::MaterialProgramStateRefresh
                | ActiveRequest::MaterialProgramDescribe { .. }
        );
        match active {
            ActiveRequest::Phase2ASet(configuration) => {
                self.state.reject_if_current(&configuration);
                changes.snapshot = true;
                changes.error |= self.set_error_value(error.to_string());
            }
            ActiveRequest::MaterialProgramSelectionSet(configuration) => {
                self.material_program
                    .reject_selection_if_current(&configuration.requested_program);
                changes.material_program = true;
                changes.error |= self.set_material_program_error(error.to_string());
            }
            ActiveRequest::MaterialProgramParametersSet(configuration) => {
                self.material_program
                    .reject_parameter_configuration_if_current(&configuration);
                changes.material_program = true;
                changes.error |= self.set_material_program_error(error.to_string());
            }
            ActiveRequest::MaterialProgramStateRefresh
            | ActiveRequest::MaterialProgramDescribe { .. } => {
                changes.error |= self.set_material_program_error(error.to_string());
            }
            ActiveRequest::Phase2ARefresh => {
                changes.error |= self.set_error_value(error.to_string())
            }
        }
        if is_transport_error(&error) {
            self.material_program_recovery_count = 0;
            self.material_program_selection_recovery_count = 0;
            self.material_program_recovery_blocked = false;
            let phase_available = self.state.available();
            let program_available = self.material_program.is_available();
            self.state.set_unavailable();
            self.material_program.set_unavailable();
            self.material_program_description_pending = false;
            changes.availability |= phase_available;
            changes.material_program_availability |= program_available;
            changes.snapshot |= phase_available;
            changes.material_program |= program_available;
        } else if matches!(&error, ClientError::Request(_)) {
            if material_program_refresh_error {
                let was_available = self.material_program.is_available();
                self.material_program.set_unavailable();
                self.material_program_description_pending = false;
                changes.material_program_availability |= was_available;
                changes.material_program = true;
            }
        }
    }

    fn request_bounded_material_program_recovery(&mut self, changes: &mut EventChanges) {
        if self.material_program_recovery_count < 2 {
            self.material_program_recovery_count += 1;
            self.material_program_refresh_pending = true;
        } else {
            self.block_material_program_recovery(changes);
        }
    }

    fn request_material_program_selection_refresh(
        &mut self,
        requested_program: &str,
        changes: &mut EventChanges,
    ) {
        if self.material_program_selection_recovery_count < 2 {
            self.material_program_selection_recovery_count += 1;
            self.material_program_refresh_pending = true;
        } else {
            self.material_program
                .reject_selection_if_current(requested_program);
            self.block_material_program_recovery(changes);
        }
    }

    fn block_material_program_recovery(&mut self, changes: &mut EventChanges) {
        let was_available =
            self.material_program.is_available() && !self.material_program_recovery_blocked;
        self.material_program_recovery_blocked = true;
        self.material_program_description_pending = false;
        self.material_program.discard_parameter_edits();
        changes.material_program_availability |= was_available;
        changes.material_program = true;
        changes.error |= self.set_material_program_error(String::from(
            "Typhon Material Program state changed repeatedly; refresh to try again.",
        ));
    }

    fn set_error_value(&mut self, value: String) -> bool {
        let bounded = bounded_error(&value);
        if self.last_error == bounded {
            return false;
        }
        self.last_error = bounded;
        true
    }

    fn set_material_program_error(&mut self, value: String) -> bool {
        let bounded = bounded_error(&value);
        if self.material_program_error == bounded {
            return false;
        }
        self.material_program_error = bounded;
        true
    }

    fn schedule_debounce(&mut self) {
        self.debounce_token = self.debounce_token.wrapping_add(1);
        self.debounce_elapsed = false;
        if let Some(worker) = &self.worker {
            worker.schedule_debounce(self.debounce_token);
        }
    }

    fn next_request(&mut self) -> Option<ActiveRequest> {
        if self.busy || self.requests.active_id.is_some() {
            return None;
        }
        if self.phase2a_refresh_pending {
            self.phase2a_refresh_pending = false;
            return Some(ActiveRequest::Phase2ARefresh);
        }
        if !self.material_program_recovery_blocked {
            if self.material_program_refresh_pending {
                self.material_program_refresh_pending = false;
                return Some(ActiveRequest::MaterialProgramStateRefresh);
            }
            if self.material_program_description_pending
                || (self.material_program.is_available()
                    && !self.material_program.has_current_description())
            {
                if let Some((registry_generation, name, schema_signature)) =
                    self.material_program.description_target()
                {
                    self.material_program_description_pending = false;
                    return Some(ActiveRequest::MaterialProgramDescribe {
                        registry_generation,
                        name,
                        schema_signature,
                    });
                }
            }
        }
        if self.debounce_elapsed
            && let Some(configuration) = self.state.pending_configuration_for_submission()
        {
            return Some(ActiveRequest::Phase2ASet(configuration));
        }
        if self.material_program_recovery_blocked {
            return None;
        }
        let queued_selection = self
            .material_program
            .pending_selection_configuration()
            .is_some();
        if (self.debounce_elapsed || queued_selection)
            && let Some(configuration) = self.material_program.pending_parameter_configuration()
        {
            return Some(ActiveRequest::MaterialProgramParametersSet(configuration));
        }
        self.material_program
            .pending_selection_configuration()
            .map(ActiveRequest::MaterialProgramSelectionSet)
    }
}

impl qobject::SettingsVisualEffectsController {
    fn available(&self) -> bool {
        self.rust().state.available()
    }

    fn busy(&self) -> bool {
        self.rust().busy
    }

    fn material_position(&self) -> f64 {
        self.rust().state.configuration().position
    }

    fn default_material_position(&self) -> f64 {
        self.rust().state.default_material_position()
    }

    fn effective_blur(&self) -> f64 {
        self.rust().state.effective().blur
    }

    fn effective_saturation(&self) -> f64 {
        self.rust().state.effective().saturation
    }

    fn effective_noise(&self) -> f64 {
        self.rust().state.effective().noise
    }

    fn blur_value(&self) -> f64 {
        self.rust()
            .state
            .configuration()
            .overrides
            .blur
            .unwrap_or_else(|| self.rust().state.effective().blur)
    }

    fn saturation_value(&self) -> f64 {
        self.rust()
            .state
            .configuration()
            .overrides
            .saturation
            .unwrap_or_else(|| self.rust().state.effective().saturation)
    }

    fn noise_value(&self) -> f64 {
        self.rust()
            .state
            .configuration()
            .overrides
            .noise
            .unwrap_or_else(|| self.rust().state.effective().noise)
    }

    fn blur_override_supported(&self) -> bool {
        self.rust().state.blur_override_supported()
    }

    fn saturation_override_supported(&self) -> bool {
        self.rust().state.saturation_override_supported()
    }

    fn noise_override_supported(&self) -> bool {
        self.rust().state.noise_override_supported()
    }

    fn blur_overridden(&self) -> bool {
        self.rust().state.blur_overridden()
    }

    fn saturation_overridden(&self) -> bool {
        self.rust().state.saturation_overridden()
    }

    fn noise_overridden(&self) -> bool {
        self.rust().state.noise_overridden()
    }

    fn has_overrides(&self) -> bool {
        self.rust().state.has_overrides()
    }

    fn generation(&self) -> u64 {
        self.rust().state.generation()
    }

    fn source(&self) -> QString {
        QString::from(self.rust().state.source().as_str())
    }

    fn last_error(&self) -> QString {
        QString::from(self.rust().last_error.as_str())
    }

    fn material_program_available(&self) -> bool {
        let rust = self.rust();
        rust.material_program.is_available()
            && rust.worker.is_some()
            && !rust.material_program_recovery_blocked
    }

    fn material_program_rendering_available(&self) -> bool {
        self.rust().material_program.rendering_available()
    }

    fn material_program_busy(&self) -> bool {
        self.rust().requests.active.as_ref().is_some_and(|request| {
            matches!(
                request,
                ActiveRequest::MaterialProgramStateRefresh
                    | ActiveRequest::MaterialProgramDescribe { .. }
                    | ActiveRequest::MaterialProgramSelectionSet(_)
                    | ActiveRequest::MaterialProgramParametersSet(_)
            )
        })
    }

    fn material_program_editing_available(&self) -> bool {
        let rust = self.rust();
        rust.material_program.is_available()
            && rust.worker.is_some()
            && !rust.material_program_recovery_blocked
            && !rust.requests.active.as_ref().is_some_and(|request| {
                matches!(
                    request,
                    ActiveRequest::MaterialProgramStateRefresh
                        | ActiveRequest::MaterialProgramDescribe { .. }
                        | ActiveRequest::MaterialProgramSelectionSet(_)
                )
            })
    }

    fn material_program_catalog(&self) -> QList<QVariant> {
        qt_projection::catalog(&self.rust().material_program)
    }

    fn material_program_requested(&self) -> QString {
        QString::from(
            self.rust()
                .material_program
                .requested_program()
                .unwrap_or_default(),
        )
    }

    fn material_program_pending_selection(&self) -> QString {
        QString::from(
            self.rust()
                .material_program
                .pending_selection()
                .unwrap_or_default(),
        )
    }

    fn material_program_effective(&self) -> QString {
        QString::from(
            self.rust()
                .material_program
                .effective_program()
                .unwrap_or_default(),
        )
    }

    fn material_program_fallback_reason(&self) -> QString {
        QString::from(
            self.rust()
                .material_program
                .fallback_reason()
                .map(fallback_reason_name)
                .unwrap_or_default(),
        )
    }

    fn material_program_effective_origin(&self) -> QString {
        QString::from(
            self.rust()
                .material_program
                .effective_origin()
                .map(origin_name)
                .unwrap_or_default(),
        )
    }

    fn material_program_parameters(&self) -> QList<QVariant> {
        qt_projection::parameters(&self.rust().material_program)
    }

    fn material_program_has_overrides(&self) -> bool {
        self.rust().material_program.has_parameter_overrides()
    }

    fn material_program_error(&self) -> QString {
        QString::from(self.rust().material_program_error.as_str())
    }

    fn refresh(mut self: Pin<&mut Self>) {
        let mut rust = self.as_mut().rust_mut();
        let was_program_available = rust.material_program.is_available()
            && rust.worker.is_some()
            && !rust.material_program_recovery_blocked;
        let active_phase2a_refresh = matches!(
            rust.requests.active.as_ref(),
            Some(ActiveRequest::Phase2ARefresh)
        );
        let active_program_refresh = matches!(
            rust.requests.active.as_ref(),
            Some(ActiveRequest::MaterialProgramStateRefresh)
        );
        if !active_phase2a_refresh {
            rust.phase2a_refresh_pending = true;
        }
        if !active_program_refresh {
            rust.material_program_refresh_pending = true;
            rust.material_program_description_pending = false;
        }
        rust.material_program_recovery_count = 0;
        rust.material_program_selection_recovery_count = 0;
        rust.material_program_recovery_blocked = false;
        let program_availability_changed = was_program_available
            != (rust.material_program.is_available() && rust.worker.is_some());
        if program_availability_changed {
            self.as_mut().material_program_availability_changed();
        }
        self.as_mut().submit_pending();
    }

    fn set_material_position(mut self: Pin<&mut Self>, value: f64) {
        self.as_mut()
            .mutate(|state| state.set_material_position(value));
    }

    fn set_blur_override(mut self: Pin<&mut Self>, value: f64) {
        self.as_mut().mutate(|state| state.set_blur_override(value));
    }

    fn clear_blur_override(mut self: Pin<&mut Self>) {
        self.as_mut().mutate(|state| state.clear_blur_override());
    }

    fn set_saturation_override(mut self: Pin<&mut Self>, value: f64) {
        self.as_mut()
            .mutate(|state| state.set_saturation_override(value));
    }

    fn clear_saturation_override(mut self: Pin<&mut Self>) {
        self.as_mut()
            .mutate(|state| state.clear_saturation_override());
    }

    fn set_noise_override(mut self: Pin<&mut Self>, value: f64) {
        self.as_mut()
            .mutate(|state| state.set_noise_override(value));
    }

    fn clear_noise_override(mut self: Pin<&mut Self>) {
        self.as_mut().mutate(|state| state.clear_noise_override());
    }

    fn reset_overrides(mut self: Pin<&mut Self>) {
        self.as_mut().mutate(VisualEffectsState::reset_overrides);
    }

    fn restore_defaults(mut self: Pin<&mut Self>) {
        self.as_mut().mutate(VisualEffectsState::restore_defaults);
    }

    fn flush(mut self: Pin<&mut Self>) {
        let mut rust = self.as_mut().rust_mut();
        rust.debounce_token = rust.debounce_token.wrapping_add(1);
        rust.debounce_elapsed = true;
        self.as_mut().submit_pending();
    }

    fn select_material_program(mut self: Pin<&mut Self>, name: QString) {
        let value = name.to_string();
        let result = self
            .as_mut()
            .mutate_material_program(|state| state.select_program(&value));
        if result.is_ok() {
            self.as_mut()
                .rust_mut()
                .material_program_selection_recovery_count = 0;
        }
        self.as_mut().finish_material_program_mutation(result);
    }

    fn set_material_program_float_parameter(mut self: Pin<&mut Self>, name: QString, value: f64) {
        let name = name.to_string();
        let result = self
            .as_mut()
            .mutate_material_program(|state| state.set_float_parameter(&name, value));
        self.as_mut().finish_material_program_mutation(result);
    }

    fn set_material_program_int_parameter(mut self: Pin<&mut Self>, name: QString, value: i32) {
        let name = name.to_string();
        let result = self
            .as_mut()
            .mutate_material_program(|state| state.set_int_parameter(&name, value));
        self.as_mut().finish_material_program_mutation(result);
    }

    fn set_material_program_vector_component(
        mut self: Pin<&mut Self>,
        name: QString,
        component: i32,
        value: f64,
    ) {
        let name = name.to_string();
        let result = usize::try_from(component)
            .map_err(|_| MaterialProgramStateError::ComponentOutOfRange)
            .and_then(|component| {
                self.as_mut().mutate_material_program(|state| {
                    state.set_vector_component(&name, component, value)
                })
            });
        self.as_mut().finish_material_program_mutation(result);
    }

    fn clear_material_program_parameter(mut self: Pin<&mut Self>, name: QString) {
        let name = name.to_string();
        let result = self
            .as_mut()
            .mutate_material_program(|state| state.clear_parameter(&name));
        self.as_mut().finish_material_program_mutation(result);
    }

    fn reset_material_program_parameters(mut self: Pin<&mut Self>) {
        let result = self
            .as_mut()
            .mutate_material_program(MaterialProgramState::reset_parameters);
        self.as_mut().finish_material_program_mutation(result);
    }

    fn flush_material_program_parameters(mut self: Pin<&mut Self>) {
        let mut rust = self.as_mut().rust_mut();
        rust.debounce_token = rust.debounce_token.wrapping_add(1);
        rust.debounce_elapsed = true;
        self.as_mut().submit_pending();
    }

    fn finish_material_program_mutation(
        mut self: Pin<&mut Self>,
        result: Result<(), MaterialProgramStateError>,
    ) {
        match result {
            Ok(()) => {
                let mut rust = self.as_mut().rust_mut();
                rust.schedule_debounce();
                let error_changed = rust.set_material_program_error(String::new());
                self.as_mut().material_program_changed();
                if error_changed {
                    self.as_mut().error_changed();
                }
                self.as_mut().submit_pending();
            }
            Err(error) => {
                let changed = self
                    .as_mut()
                    .rust_mut()
                    .set_material_program_error(error.to_string());
                if changed {
                    self.as_mut().error_changed();
                }
            }
        }
    }

    fn mutate_material_program<F>(
        mut self: Pin<&mut Self>,
        mutation: F,
    ) -> Result<(), MaterialProgramStateError>
    where
        F: FnOnce(&mut MaterialProgramState) -> Result<(), MaterialProgramStateError>,
    {
        let mut rust = self.as_mut().rust_mut();
        if rust.material_program_recovery_blocked {
            return Err(MaterialProgramStateError::Unavailable);
        }
        mutation(&mut rust.material_program)
    }

    fn mutate<F>(mut self: Pin<&mut Self>, mutation: F)
    where
        F: FnOnce(&mut VisualEffectsState) -> Result<(), StateError>,
    {
        let result = {
            let mut rust = self.as_mut().rust_mut();
            let result = mutation(&mut rust.state);
            if result.is_ok() {
                rust.schedule_debounce();
            }
            result
        };
        match result {
            Ok(()) => {
                self.as_mut().snapshot_changed();
            }
            Err(error) => {
                let changed = self.as_mut().rust_mut().set_error_value(error.to_string());
                if changed {
                    self.as_mut().error_changed();
                }
            }
        }
    }

    fn handle_worker_event(mut self: Pin<&mut Self>, event: WorkerEvent) {
        let changes = self.as_mut().rust_mut().handle_event(event);
        if changes.availability {
            self.as_mut().availability_changed();
        }
        if changes.busy {
            self.as_mut().busy_changed();
        }
        if changes.snapshot {
            self.as_mut().snapshot_changed();
        }
        if changes.error {
            self.as_mut().error_changed();
        }
        if changes.material_program_availability {
            self.as_mut().material_program_availability_changed();
        }
        if changes.material_program {
            self.as_mut().material_program_changed();
        }
        self.as_mut().submit_pending();
    }

    fn start_request(mut self: Pin<&mut Self>, active: ActiveRequest) {
        let material_program_request = is_material_program_request(&active);
        let (id, request, worker_available) = {
            let mut rust = self.as_mut().rust_mut();
            let request = control_request_for(&active);
            let id = rust.requests.start(active.clone());
            rust.busy = true;
            (id, request, rust.worker.is_some())
        };
        self.as_mut().busy_changed();
        if material_program_request {
            self.as_mut().material_program_changed();
        }
        if !worker_available {
            self.as_mut()
                .complete_start_failure(id, active, ClientError::WorkerUnavailable);
            return;
        }
        let submitted = self
            .rust()
            .worker
            .as_ref()
            .is_some_and(|worker| worker.submit(id, request).is_ok());
        if submitted {
            match active {
                ActiveRequest::Phase2ASet(configuration) => {
                    self.as_mut()
                        .rust_mut()
                        .state
                        .take_pending_configuration_if_matches(&configuration);
                }
                ActiveRequest::MaterialProgramParametersSet(configuration) => {
                    self.as_mut()
                        .rust_mut()
                        .material_program
                        .mark_parameter_configuration_submitted(&configuration);
                }
                _ => {}
            }
        } else {
            self.as_mut()
                .complete_start_failure(id, active, ClientError::WorkerUnavailable);
        }
    }

    fn complete_start_failure(
        mut self: Pin<&mut Self>,
        id: u64,
        active: ActiveRequest,
        error: ClientError,
    ) {
        let material_program_request = is_material_program_request(&active);
        let mut rust = self.as_mut().rust_mut();
        let _ = rust.requests.finish(id);
        rust.busy = false;
        match active {
            ActiveRequest::Phase2ASet(configuration) => {
                rust.state.reject_if_current(&configuration)
            }
            ActiveRequest::MaterialProgramSelectionSet(configuration) => rust
                .material_program
                .reject_selection_if_current(&configuration.requested_program),
            ActiveRequest::MaterialProgramParametersSet(configuration) => rust
                .material_program
                .reject_parameter_configuration_if_current(&configuration),
            _ => {}
        }
        let availability_changed = rust.state.available();
        let material_program_availability_changed = rust.material_program.is_available();
        if availability_changed {
            rust.state.set_unavailable();
        }
        if material_program_availability_changed {
            rust.material_program.set_unavailable();
        }
        let error_changed = rust.set_error_value(error.to_string());
        self.as_mut().busy_changed();
        if availability_changed {
            self.as_mut().availability_changed();
        }
        if material_program_availability_changed {
            self.as_mut().material_program_availability_changed();
        }
        if material_program_request {
            self.as_mut().material_program_changed();
        }
        if error_changed {
            self.as_mut().error_changed();
        }
        self.as_mut().snapshot_changed();
    }

    fn submit_pending(mut self: Pin<&mut Self>) {
        let next = self.as_mut().rust_mut().next_request();
        let Some(next) = next else {
            return;
        };
        self.as_mut().start_request(next);
    }
}

impl cxx_qt::Initialize for qobject::SettingsVisualEffectsController {
    fn initialize(mut self: Pin<&mut Self>) {
        let qt_thread = self.qt_thread();
        let worker = ClientWorker::new(move |event| {
            let _ = qt_thread.queue(move |object| object.handle_worker_event(event));
        });
        self.as_mut().rust_mut().worker = worker.ok();
        if self.rust().worker.is_some() {
            self.as_mut().refresh();
        } else {
            let mut rust = self.as_mut().rust_mut();
            let _ = rust.set_error_value(String::from("Typhon control: worker is unavailable"));
            self.as_mut().error_changed();
        }
    }
}

fn bounded_error(error: &str) -> String {
    const MAX_ERROR_CHARS: usize = 320;
    error.chars().take(MAX_ERROR_CHARS).collect()
}

fn is_transport_error(error: &ClientError) -> bool {
    matches!(
        error,
        ClientError::Discovery(_)
            | ClientError::Transport(_)
            | ClientError::Timeout
            | ClientError::WorkerUnavailable
    )
}

fn is_material_program_request(active: &ActiveRequest) -> bool {
    matches!(
        active,
        ActiveRequest::MaterialProgramStateRefresh
            | ActiveRequest::MaterialProgramDescribe { .. }
            | ActiveRequest::MaterialProgramSelectionSet(_)
            | ActiveRequest::MaterialProgramParametersSet(_)
    )
}

fn fallback_reason_name(reason: wire::MaterialProgramFallbackReason) -> &'static str {
    match reason {
        wire::MaterialProgramFallbackReason::Missing => "missing",
        wire::MaterialProgramFallbackReason::Unqualified => "unqualified",
    }
}

fn origin_name(origin: wire::MaterialProgramOrigin) -> &'static str {
    match origin {
        wire::MaterialProgramOrigin::System => "system",
        wire::MaterialProgramOrigin::TrustedLocal => "trusted_local",
    }
}

#[cfg(test)]
#[path = "qobject_tests.rs"]
mod tests;
