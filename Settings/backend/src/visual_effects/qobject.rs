use super::state::{MaterialConfiguration, StateError, VisualEffectsState};
use crate::typhon::client::{ClientError, ClientWorker, WorkerEvent};
use crate::typhon::protocol::{ControlRequest, ControlSuccess, MaterialRequest, ProtocolOutcome};
use cxx_qt::{CxxQtType, Threading};
use cxx_qt_lib::QString;
use std::pin::Pin;

#[cxx_qt::bridge]
pub mod qobject {
    unsafe extern "C++" {
        include!("cxx-qt-lib/qstring.h");
        type QString = cxx_qt_lib::QString;
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
    }

    impl cxx_qt::Initialize for SettingsVisualEffectsController {}
    impl cxx_qt::Threading for SettingsVisualEffectsController {}
}

pub struct SettingsVisualEffectsControllerRust {
    state: VisualEffectsState,
    worker: Option<ClientWorker>,
    requests: RequestState,
    busy: bool,
    last_error: String,
    debounce_token: u64,
    debounce_elapsed: bool,
}

#[derive(Clone)]
enum ActiveRequest {
    Refresh,
    Set(MaterialConfiguration),
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
            worker: None,
            requests: RequestState {
                next_id: 1,
                ..RequestState::default()
            },
            busy: false,
            last_error: String::new(),
            debounce_token: 0,
            debounce_elapsed: false,
        }
    }
}

#[derive(Default)]
struct EventChanges {
    availability: bool,
    busy: bool,
    snapshot: bool,
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
            Ok(ProtocolOutcome::Success(ControlSuccess::Material(snapshot))) => {
                let was_available = self.state.available();
                match self.state.apply_snapshot(snapshot) {
                    Ok(()) => {
                        changes.snapshot = true;
                        changes.availability = !was_available;
                        changes.error = self.set_error_value(String::new());
                    }
                    Err(error) => {
                        changes.error = self.set_error_value(error.to_string());
                    }
                }
            }
            Ok(ProtocolOutcome::Success(ControlSuccess::Animation(_))) => {
                changes.error = self.set_error_value(String::from(
                    "Typhon returned an animation snapshot to the Visual Effects controller.",
                ));
            }
            Ok(ProtocolOutcome::ServerRejected(error)) => {
                if let ActiveRequest::Set(configuration) = active {
                    self.state.reject_if_current(&configuration);
                    changes.snapshot = true;
                }
                changes.error = self.set_error_value(error);
            }
            Err(error) => {
                if let ActiveRequest::Set(configuration) = active {
                    self.state.reject_if_current(&configuration);
                    changes.snapshot = true;
                }
                if !matches!(error, ClientError::WorkerUnavailable) && self.state.available() {
                    self.state.set_unavailable();
                    changes.availability = true;
                }
                changes.error = self.set_error_value(error.to_string());
            }
        }

        changes
    }

    fn set_error_value(&mut self, value: String) -> bool {
        let bounded = bounded_error(&value);
        if self.last_error == bounded {
            return false;
        }
        self.last_error = bounded;
        true
    }

    fn schedule_debounce(&mut self) {
        self.debounce_token = self.debounce_token.wrapping_add(1);
        self.debounce_elapsed = false;
        if let Some(worker) = &self.worker {
            worker.schedule_debounce(self.debounce_token);
        }
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

    fn refresh(mut self: Pin<&mut Self>) {
        if self.rust().busy {
            return;
        }
        self.as_mut().start_request(ActiveRequest::Refresh);
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
        self.as_mut().submit_pending();
    }

    fn start_request(mut self: Pin<&mut Self>, active: ActiveRequest) {
        let (id, request, worker_available) = {
            let mut rust = self.as_mut().rust_mut();
            let request = match &active {
                ActiveRequest::Refresh => ControlRequest::Material(MaterialRequest::Get),
                ActiveRequest::Set(configuration) => {
                    ControlRequest::Material(MaterialRequest::Set(configuration.clone()))
                }
            };
            let id = rust.requests.start(active.clone());
            rust.busy = true;
            (id, request, rust.worker.is_some())
        };
        self.as_mut().busy_changed();
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
            if let ActiveRequest::Set(configuration) = active {
                self.as_mut()
                    .rust_mut()
                    .state
                    .take_pending_configuration_if_matches(&configuration);
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
        let mut rust = self.as_mut().rust_mut();
        let _ = rust.requests.finish(id);
        rust.busy = false;
        if let ActiveRequest::Set(configuration) = active {
            rust.state.reject_if_current(&configuration);
        }
        let availability_changed = rust.state.available();
        if availability_changed {
            rust.state.set_unavailable();
        }
        let error_changed = rust.set_error_value(error.to_string());
        self.as_mut().busy_changed();
        if availability_changed {
            self.as_mut().availability_changed();
        }
        if error_changed {
            self.as_mut().error_changed();
        }
        self.as_mut().snapshot_changed();
    }

    fn submit_pending(mut self: Pin<&mut Self>) {
        let can_submit = {
            let rust = self.rust();
            rust.state.available()
                && !rust.busy
                && rust.debounce_elapsed
                && rust.state.pending_configuration().is_some()
        };
        if !can_submit {
            return;
        }
        let Some(configuration) = self.rust().state.pending_configuration() else {
            return;
        };
        self.as_mut()
            .start_request(ActiveRequest::Set(configuration));
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

#[cfg(test)]
mod tests {
    use super::{ActiveRequest, MaterialConfiguration, RequestState};

    #[test]
    fn stale_completion_id_cannot_finish_the_current_request() {
        let mut requests = RequestState::default();
        let active = ActiveRequest::Set(MaterialConfiguration::default());
        let current_id = requests.start(active.clone());

        assert!(requests.finish(current_id.wrapping_add(1)).is_none());
        assert!(requests.finish(current_id).is_some());
        assert!(requests.finish(current_id).is_none());
    }
}
