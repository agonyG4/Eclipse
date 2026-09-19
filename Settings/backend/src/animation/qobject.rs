use super::state::{AnimationMutation, AnimationState, StateError};
use crate::typhon::client::{ClientWorker, WorkerEvent};
use crate::typhon::protocol::{AnimationRequest, ProtocolOutcome};
use cxx_qt::{CxxQtType, Threading};
use cxx_qt_lib::{QList, QMap, QMapPair_QString_QVariant, QString, QVariant};
use std::pin::Pin;

#[cxx_qt::bridge]
pub mod qobject {
    unsafe extern "C++" {
        include!("cxx-qt-lib/qstring.h");
        type QString = cxx_qt_lib::QString;
        include!("cxx-qt-lib/qvariant.h");
        type QVariant = cxx_qt_lib::QVariant;
    }

    extern "RustQt" {
        #[qobject]
        #[qproperty(bool, available, READ = available, NOTIFY = availability_changed)]
        #[qproperty(bool, busy, READ = busy, NOTIFY = busy_changed)]
        #[qproperty(bool, enabled, READ = enabled, NOTIFY = snapshot_changed)]
        #[qproperty(QString, preset, READ = preset, NOTIFY = snapshot_changed)]
        #[qproperty(f64, speed, READ = speed, NOTIFY = snapshot_changed)]
        #[qproperty(u64, generation, READ = generation, NOTIFY = snapshot_changed)]
        #[qproperty(QString, source, READ = source, NOTIFY = snapshot_changed)]
        #[qproperty(bool, has_overrides, cxx_name = "hasOverrides", READ = has_overrides, NOTIFY = snapshot_changed)]
        #[qproperty(QVariant, slot_capabilities, cxx_name = "slots", READ = slot_capabilities, NOTIFY = snapshot_changed)]
        #[qproperty(QVariant, presets, READ = presets, NOTIFY = snapshot_changed)]
        #[qproperty(QString, last_error, cxx_name = "lastError", READ = last_error, NOTIFY = error_changed)]
        type SettingsAnimationController = super::SettingsAnimationControllerRust;
    }

    unsafe extern "RustQt" {
        fn available(self: &SettingsAnimationController) -> bool;
        fn busy(self: &SettingsAnimationController) -> bool;
        fn enabled(self: &SettingsAnimationController) -> bool;
        fn preset(self: &SettingsAnimationController) -> QString;
        fn speed(self: &SettingsAnimationController) -> f64;
        fn generation(self: &SettingsAnimationController) -> u64;
        fn source(self: &SettingsAnimationController) -> QString;
        #[cxx_name = "hasOverrides"]
        fn has_overrides(self: &SettingsAnimationController) -> bool;
        #[cxx_name = "slotCapabilities"]
        fn slot_capabilities(self: &SettingsAnimationController) -> QVariant;
        fn presets(self: &SettingsAnimationController) -> QVariant;
        #[cxx_name = "lastError"]
        fn last_error(self: &SettingsAnimationController) -> QString;

        #[qsignal]
        #[cxx_name = "availabilityChanged"]
        fn availability_changed(self: Pin<&mut SettingsAnimationController>);
        #[qsignal]
        #[cxx_name = "busyChanged"]
        fn busy_changed(self: Pin<&mut SettingsAnimationController>);
        #[qsignal]
        #[cxx_name = "snapshotChanged"]
        fn snapshot_changed(self: Pin<&mut SettingsAnimationController>);
        #[qsignal]
        #[cxx_name = "errorChanged"]
        fn error_changed(self: Pin<&mut SettingsAnimationController>);

        #[qinvokable]
        fn refresh(self: Pin<&mut SettingsAnimationController>);
        #[qinvokable]
        #[cxx_name = "setEnabled"]
        fn set_enabled(self: Pin<&mut SettingsAnimationController>, value: bool);
        #[qinvokable]
        #[cxx_name = "setPreset"]
        fn set_preset(self: Pin<&mut SettingsAnimationController>, value: &QString);
        #[qinvokable]
        #[cxx_name = "setSpeed"]
        fn set_speed(self: Pin<&mut SettingsAnimationController>, value: f64);
        #[qinvokable]
        #[cxx_name = "setSlotEffect"]
        fn set_slot_effect(
            self: Pin<&mut SettingsAnimationController>,
            slot_id: &QString,
            effect_id: &QString,
        );
        #[qinvokable]
        #[cxx_name = "clearSlotOverride"]
        fn clear_slot_override(self: Pin<&mut SettingsAnimationController>, slot_id: &QString);
        #[qinvokable]
        #[cxx_name = "resetOverrides"]
        fn reset_overrides(self: Pin<&mut SettingsAnimationController>);
        #[qinvokable]
        #[cxx_name = "restoreDefaults"]
        fn restore_defaults(self: Pin<&mut SettingsAnimationController>);
        #[qinvokable]
        fn flush(self: Pin<&mut SettingsAnimationController>);
    }

    impl cxx_qt::Initialize for SettingsAnimationController {}
    impl cxx_qt::Threading for SettingsAnimationController {}
}

pub struct SettingsAnimationControllerRust {
    state: AnimationState,
    worker: Option<ClientWorker>,
    next_request_id: u64,
    active_request_id: Option<u64>,
    active_request_is_refresh: bool,
    busy: bool,
    last_error: String,
    debounce_token: u64,
}

impl Default for SettingsAnimationControllerRust {
    fn default() -> Self {
        Self {
            state: AnimationState::default(),
            worker: None,
            next_request_id: 1,
            active_request_id: None,
            active_request_is_refresh: false,
            busy: false,
            last_error: String::new(),
            debounce_token: 0,
        }
    }
}

impl qobject::SettingsAnimationController {
    fn available(&self) -> bool {
        self.rust().state.available()
    }

    fn busy(&self) -> bool {
        self.rust().busy
    }

    fn enabled(&self) -> bool {
        self.rust().state.configuration().enabled
    }

    fn preset(&self) -> QString {
        QString::from(&self.rust().state.configuration().preset)
    }

    fn speed(&self) -> f64 {
        self.rust().state.configuration().speed
    }

    fn generation(&self) -> u64 {
        self.rust()
            .state
            .snapshot()
            .map_or(0, |snapshot| snapshot.generation)
    }

    fn source(&self) -> QString {
        self.rust()
            .state
            .snapshot()
            .map_or_else(QString::default, |snapshot| QString::from(&snapshot.source))
    }

    fn has_overrides(&self) -> bool {
        self.rust().state.has_overrides()
    }

    fn slot_capabilities(&self) -> QVariant {
        let mut slots = QList::default();
        for capability in self.rust().state.slot_capabilities() {
            let mut slot = QMap::default();
            insert_string(&mut slot, "id", &capability.id);
            insert_variant(
                &mut slot,
                "compatibleEffects",
                string_list(&capability.compatible_effects),
            );
            insert_variant(
                &mut slot,
                "availableEffects",
                string_list(&capability.available_effects),
            );
            insert_variant(
                &mut slot,
                "plannedEffects",
                string_list(&capability.planned_effects),
            );
            insert_variant(
                &mut slot,
                "unavailableEffects",
                string_list(&capability.unavailable_effects),
            );
            insert_optional_string(&mut slot, "requested", capability.requested.as_deref());
            insert_optional_string(&mut slot, "effective", capability.effective.as_deref());
            insert_optional_string(&mut slot, "override", capability.override_effect.as_deref());
            slots.append(QVariant::from(&slot));
        }
        QVariant::from(&slots)
    }

    fn presets(&self) -> QVariant {
        let mut presets = QList::default();
        if let Some(snapshot) = self.rust().state.snapshot() {
            for preset in &snapshot.catalog.presets {
                presets.append(QVariant::from(&QString::from(&preset.id)));
            }
        }
        QVariant::from(&presets)
    }

    fn last_error(&self) -> QString {
        QString::from(&self.rust().last_error)
    }

    fn refresh(mut self: Pin<&mut Self>) {
        if self.rust().busy {
            return;
        }
        if self.rust().state.available() && self.rust().state.pending_configuration().is_some() {
            self.submit_pending();
            return;
        }
        self.as_mut().rust_mut().active_request_is_refresh = true;
        self.start_request(AnimationRequest::Get);
    }

    fn set_enabled(mut self: Pin<&mut Self>, value: bool) {
        self.as_mut()
            .rust_mut()
            .state
            .enqueue_mutation(AnimationMutation::SetEnabled(value));
        self.submit_pending();
    }

    fn set_preset(mut self: Pin<&mut Self>, value: &QString) {
        self.as_mut()
            .rust_mut()
            .state
            .enqueue_mutation(AnimationMutation::SetPreset(String::from(value)));
        self.submit_pending();
    }

    fn set_speed(mut self: Pin<&mut Self>, value: f64) {
        if self.as_mut().rust_mut().state.set_speed(value).is_some() {
            let token = {
                let mut rust = self.as_mut().rust_mut();
                rust.debounce_token = rust.debounce_token.wrapping_add(1);
                rust.debounce_token
            };
            if let Some(worker) = self.rust().worker.as_ref() {
                worker.schedule_debounce(token);
            }
        }
    }

    fn set_slot_effect(mut self: Pin<&mut Self>, slot_id: &QString, effect_id: &QString) {
        let slot_id = String::from(slot_id);
        let effect_id = String::from(effect_id);
        if self
            .rust()
            .state
            .validate_slot_effect(&slot_id, &effect_id)
            .is_err()
        {
            self.set_error(String::from(
                "The selected animation is unavailable for this slot.",
            ));
            return;
        }
        self.as_mut()
            .rust_mut()
            .state
            .enqueue_mutation(AnimationMutation::SetSlotEffect { slot_id, effect_id });
        self.submit_pending();
    }

    fn clear_slot_override(mut self: Pin<&mut Self>, slot_id: &QString) {
        self.as_mut()
            .rust_mut()
            .state
            .enqueue_mutation(AnimationMutation::ClearSlotOverride(String::from(slot_id)));
        self.submit_pending();
    }

    fn reset_overrides(mut self: Pin<&mut Self>) {
        self.as_mut()
            .rust_mut()
            .state
            .enqueue_mutation(AnimationMutation::ResetOverrides);
        self.submit_pending();
    }

    fn restore_defaults(mut self: Pin<&mut Self>) {
        self.as_mut()
            .rust_mut()
            .state
            .enqueue_mutation(AnimationMutation::RestoreDefaults);
        self.submit_pending();
    }

    fn flush(self: Pin<&mut Self>) {
        self.submit_pending();
    }

    fn start_request(mut self: Pin<&mut Self>, request: AnimationRequest) {
        let (id, worker_available) = {
            let mut rust = self.as_mut().rust_mut();
            let id = rust.next_request_id;
            rust.next_request_id = rust.next_request_id.wrapping_add(1).max(1);
            rust.active_request_id = Some(id);
            rust.busy = true;
            (id, rust.worker.is_some())
        };
        self.as_mut().busy_changed();
        if !worker_available {
            self.complete_start_failure(String::from("Typhon control: worker is unavailable"));
            return;
        }
        let submitted = self
            .rust()
            .worker
            .as_ref()
            .is_some_and(|worker| worker.submit(id, request).is_ok());
        if !submitted {
            self.complete_start_failure(String::from("Typhon control: worker is unavailable"));
        }
    }

    fn complete_start_failure(mut self: Pin<&mut Self>, error: String) {
        let was_available = self.rust().state.available();
        self.as_mut().rust_mut().active_request_id = None;
        self.as_mut().rust_mut().busy = false;
        self.as_mut().busy_changed();
        if was_available {
            self.as_mut().rust_mut().state.set_unavailable();
            self.as_mut().availability_changed();
        }
        self.set_error(error);
    }

    fn submit_pending(mut self: Pin<&mut Self>) {
        if self.rust().busy || !self.rust().state.available() {
            return;
        }
        let Some(configuration) = self.rust().state.pending_configuration() else {
            return;
        };
        self.as_mut().rust_mut().active_request_is_refresh = false;
        self.as_mut()
            .start_request(AnimationRequest::Set(configuration));
        self.as_mut().rust_mut().state.mark_submitted();
    }

    fn handle_worker_event(mut self: Pin<&mut Self>, event: WorkerEvent) {
        match event {
            WorkerEvent::DebounceElapsed { token } => {
                if self.rust().debounce_token == token {
                    self.submit_pending();
                }
            }
            WorkerEvent::RequestFinished { id, result } => {
                if self.rust().active_request_id != Some(id) {
                    return;
                }
                let refresh = self.rust().active_request_is_refresh;
                self.as_mut().rust_mut().active_request_id = None;
                self.as_mut().rust_mut().busy = false;
                self.as_mut().busy_changed();
                match *result {
                    Ok(ProtocolOutcome::Success(snapshot)) => {
                        let was_available = self.rust().state.available();
                        self.as_mut().rust_mut().state.set_available();
                        if !was_available {
                            self.as_mut().availability_changed();
                        }
                        match self.as_mut().rust_mut().state.apply_snapshot(snapshot) {
                            Ok(()) => {
                                self.as_mut().snapshot_changed();
                                self.as_mut().set_error(String::new());
                            }
                            Err(StateError::IncompleteSnapshot) => self.as_mut().set_error(
                                String::from("Typhon returned an incomplete animation snapshot."),
                            ),
                        }
                    }
                    Ok(ProtocolOutcome::ServerRejected(error)) => {
                        self.as_mut().set_error(error);
                    }
                    Err(error) => {
                        let should_mark_unavailable = refresh
                            || !matches!(
                                error,
                                crate::typhon::client::ClientError::WorkerUnavailable
                            );
                        if should_mark_unavailable && self.rust().state.available() {
                            self.as_mut().rust_mut().state.set_unavailable();
                            self.as_mut().availability_changed();
                        }
                        self.as_mut().set_error(error.to_string());
                    }
                }
                self.as_mut().submit_pending();
            }
        }
    }

    fn set_error(mut self: Pin<&mut Self>, error: String) {
        if self.rust().last_error == error {
            return;
        }
        self.as_mut().rust_mut().last_error = error;
        self.error_changed();
    }
}

impl cxx_qt::Initialize for qobject::SettingsAnimationController {
    fn initialize(mut self: Pin<&mut Self>) {
        let qt_thread = self.qt_thread();
        let worker = ClientWorker::new(move |event| {
            let _ = qt_thread.queue(move |object| object.handle_worker_event(event));
        });
        self.as_mut().rust_mut().worker = worker.ok();
        if self.rust().worker.is_none() {
            self.set_error(String::from("Typhon control: worker is unavailable"));
        }
    }
}

fn string_list(values: &[String]) -> QVariant {
    let mut list = QList::default();
    for value in values {
        list.append(QVariant::from(&QString::from(value)));
    }
    QVariant::from(&list)
}

fn insert_string(map: &mut QMap<QMapPair_QString_QVariant>, key: &str, value: &str) {
    map.insert(QString::from(key), QVariant::from(&QString::from(value)));
}

fn insert_variant(map: &mut QMap<QMapPair_QString_QVariant>, key: &str, value: QVariant) {
    map.insert(QString::from(key), value);
}

fn insert_optional_string(
    map: &mut QMap<QMapPair_QString_QVariant>,
    key: &str,
    value: Option<&str>,
) {
    let variant = value.map_or_else(QVariant::default, |value| {
        QVariant::from(&QString::from(value))
    });
    insert_variant(map, key, variant);
}
