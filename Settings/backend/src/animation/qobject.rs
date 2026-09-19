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
        include!("cxx-qt-lib/core/qlist/qlist_QVariant.h");
        type QList_QVariant = cxx_qt_lib::QList<QVariant>;
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
        #[qproperty(QList_QVariant, slot_capabilities, cxx_name = "slots", READ = slot_capabilities, NOTIFY = snapshot_changed)]
        #[qproperty(QList_QVariant, presets, READ = presets, NOTIFY = snapshot_changed)]
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
        fn slot_capabilities(self: &SettingsAnimationController) -> QList_QVariant;
        fn presets(self: &SettingsAnimationController) -> QList_QVariant;
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
    requests: RequestState,
    busy: bool,
    last_error: String,
    debounce_token: u64,
}

#[derive(Default)]
struct RequestState {
    next_id: u64,
    active_id: Option<u64>,
    active_is_refresh: bool,
}

impl RequestState {
    fn start(&mut self, is_refresh: bool) -> u64 {
        let id = self.next_id;
        self.next_id = self.next_id.wrapping_add(1).max(1);
        self.active_id = Some(id);
        self.active_is_refresh = is_refresh;
        id
    }

    fn finish(&mut self, id: u64) -> Option<bool> {
        if self.active_id != Some(id) {
            return None;
        }
        self.active_id = None;
        Some(self.active_is_refresh)
    }

    fn cancel(&mut self) {
        self.active_id = None;
    }
}

impl Default for SettingsAnimationControllerRust {
    fn default() -> Self {
        Self {
            state: AnimationState::default(),
            worker: None,
            requests: RequestState {
                next_id: 1,
                ..RequestState::default()
            },
            busy: false,
            last_error: String::new(),
            debounce_token: 0,
        }
    }
}

#[derive(Default)]
struct WorkerEventEffects {
    busy_changed: bool,
    availability_changed: bool,
    snapshot_changed: bool,
    error_changed: bool,
    submit_pending: bool,
}

impl SettingsAnimationControllerRust {
    fn handle_worker_event(&mut self, event: WorkerEvent) -> Option<WorkerEventEffects> {
        match event {
            WorkerEvent::DebounceElapsed { token } => {
                (self.debounce_token == token).then(|| WorkerEventEffects {
                    submit_pending: true,
                    ..WorkerEventEffects::default()
                })
            }
            WorkerEvent::RequestFinished { id, result } => {
                self.handle_request_finished(id, *result)
            }
        }
    }

    fn handle_request_finished(
        &mut self,
        id: u64,
        result: Result<ProtocolOutcome, crate::typhon::client::ClientError>,
    ) -> Option<WorkerEventEffects> {
        let refresh = self.requests.finish(id)?;
        self.busy = false;
        let mut effects = WorkerEventEffects {
            busy_changed: true,
            submit_pending: true,
            ..WorkerEventEffects::default()
        };
        match result {
            Ok(ProtocolOutcome::Success(snapshot)) => {
                let was_available = self.state.available();
                self.state.set_available();
                effects.availability_changed = !was_available;
                match self.state.apply_snapshot(snapshot) {
                    Ok(()) => {
                        effects.snapshot_changed = true;
                        effects.error_changed = self.set_error_value(String::new());
                    }
                    Err(StateError::IncompleteSnapshot) => {
                        effects.error_changed = self.set_error_value(String::from(
                            "Typhon returned an incomplete animation snapshot.",
                        ));
                    }
                }
            }
            Ok(ProtocolOutcome::ServerRejected(error)) => {
                if refresh && self.state.available() {
                    self.state.set_unavailable();
                    effects.availability_changed = true;
                }
                effects.error_changed = self.set_error_value(error);
            }
            Err(error) => {
                let should_mark_unavailable = refresh
                    || !matches!(error, crate::typhon::client::ClientError::WorkerUnavailable);
                if should_mark_unavailable && self.state.available() {
                    self.state.set_unavailable();
                    effects.availability_changed = true;
                }
                effects.error_changed = self.set_error_value(error.to_string());
            }
        }
        Some(effects)
    }

    fn set_error_value(&mut self, error: String) -> bool {
        if self.last_error == error {
            return false;
        }
        self.last_error = error;
        true
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

    fn slot_capabilities(&self) -> QList<QVariant> {
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
        slots
    }

    fn presets(&self) -> QList<QVariant> {
        let mut presets = QList::default();
        if let Some(snapshot) = self.rust().state.snapshot() {
            for preset in &snapshot.catalog.presets {
                presets.append(QVariant::from(&QString::from(&preset.id)));
            }
        }
        presets
    }

    fn last_error(&self) -> QString {
        QString::from(&self.rust().last_error)
    }

    fn refresh(self: Pin<&mut Self>) {
        if self.rust().busy {
            return;
        }
        if self.rust().state.available() && self.rust().state.pending_configuration().is_some() {
            self.submit_pending();
            return;
        }
        self.start_request(AnimationRequest::Get, true);
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

    fn start_request(mut self: Pin<&mut Self>, request: AnimationRequest, is_refresh: bool) {
        let (id, worker_available) = {
            let mut rust = self.as_mut().rust_mut();
            let id = rust.requests.start(is_refresh);
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
        self.as_mut().rust_mut().requests.cancel();
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
        self.as_mut()
            .start_request(AnimationRequest::Set(configuration), false);
        self.as_mut().rust_mut().state.mark_submitted();
    }

    fn handle_worker_event(mut self: Pin<&mut Self>, event: WorkerEvent) {
        let Some(effects) = self.as_mut().rust_mut().handle_worker_event(event) else {
            return;
        };
        if effects.busy_changed {
            self.as_mut().busy_changed();
        }
        if effects.availability_changed {
            self.as_mut().availability_changed();
        }
        if effects.snapshot_changed {
            self.as_mut().snapshot_changed();
        }
        if effects.error_changed {
            self.as_mut().error_changed();
        }
        if effects.submit_pending {
            self.as_mut().submit_pending();
        }
    }

    fn set_error(mut self: Pin<&mut Self>, error: String) {
        if self.as_mut().rust_mut().set_error_value(error) {
            self.error_changed();
        }
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

#[cfg(test)]
mod tests {
    use super::{RequestState, SettingsAnimationControllerRust};
    use crate::animation::state::{AnimationCatalog, AnimationConfiguration, AnimationSnapshot};
    use crate::typhon::client::WorkerEvent;
    use crate::typhon::protocol::ProtocolOutcome;

    #[test]
    fn stale_operation_token_cannot_mutate_newer_state() {
        let mut requests = RequestState {
            next_id: 1,
            ..RequestState::default()
        };
        let mut state = super::AnimationState::default();
        state.apply_snapshot(snapshot("initial")).unwrap();
        let first = requests.start(true);
        assert_eq!(requests.finish(first), Some(true));
        let second = requests.start(false);

        let stale_snapshot = snapshot("stale");
        if requests.finish(first).is_some() {
            state.apply_snapshot(stale_snapshot).unwrap();
        }
        assert_eq!(state.configuration().preset, "initial");
        assert_eq!(requests.finish(first), None);
        assert_eq!(requests.finish(second), Some(false));
        state.apply_snapshot(snapshot("newer")).unwrap();
        assert!(state.available());
        assert_eq!(state.configuration().preset, "newer");
    }

    #[test]
    fn stale_request_finished_event_is_ignored_by_controller_state_machine() {
        let mut controller = SettingsAnimationControllerRust::default();
        controller
            .state
            .apply_snapshot(snapshot("initial"))
            .unwrap();
        controller.busy = true;
        let first = controller.requests.start(true);
        let second = controller.requests.start(false);
        controller.busy = true;
        controller.state.apply_snapshot(snapshot("newer")).unwrap();
        controller.state.set_speed(1.5);
        controller.last_error = String::from("newer error");

        let effects = controller.handle_worker_event(WorkerEvent::RequestFinished {
            id: first,
            result: Box::new(Ok(ProtocolOutcome::Success(snapshot("stale")))),
        });

        assert!(effects.is_none());
        assert!(controller.busy);
        assert_eq!(controller.state.configuration().preset, "newer");
        assert!(controller.state.available());
        assert_eq!(controller.last_error, "newer error");
        assert!(controller.state.pending_configuration().is_some());
        assert_eq!(controller.requests.active_id, Some(second));
    }

    fn snapshot(preset: &str) -> AnimationSnapshot {
        AnimationSnapshot {
            generation: 0,
            source: String::from("test"),
            config: Some(AnimationConfiguration {
                preset: preset.to_owned(),
                ..AnimationConfiguration::default()
            }),
            requested: Default::default(),
            effective: Default::default(),
            catalog: AnimationCatalog::default(),
        }
    }
}
