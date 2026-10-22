use super::worker::{PerformanceWorker, PerformanceWorkerEvent};
use cxx_qt::{CxxQtType, Threading};
use cxx_qt_lib::QString;
use serde_json::Value;
use std::pin::Pin;

#[cxx_qt::bridge]
pub mod qobject {
    unsafe extern "C++" {
        include!("cxx-qt-lib/qstring.h");
        type QString = cxx_qt_lib::QString;
    }

    extern "RustQt" {
        #[qobject]
        #[qproperty(QString, state_json, cxx_name = "stateJson", READ = state_json, NOTIFY = state_changed)]
        #[qproperty(bool, busy, READ = busy, NOTIFY = busy_changed)]
        #[qproperty(QString, last_error, cxx_name = "lastError", READ = last_error, NOTIFY = error_changed)]
        type SettingsPerformanceController = super::SettingsPerformanceControllerRust;
    }

    unsafe extern "RustQt" {
        #[cxx_name = "stateJson"]
        fn state_json(self: &SettingsPerformanceController) -> QString;
        fn busy(self: &SettingsPerformanceController) -> bool;
        #[cxx_name = "lastError"]
        fn last_error(self: &SettingsPerformanceController) -> QString;

        #[qsignal]
        #[cxx_name = "stateChanged"]
        fn state_changed(self: Pin<&mut SettingsPerformanceController>);
        #[qsignal]
        #[cxx_name = "busyChanged"]
        fn busy_changed(self: Pin<&mut SettingsPerformanceController>);
        #[qsignal]
        #[cxx_name = "errorChanged"]
        fn error_changed(self: Pin<&mut SettingsPerformanceController>);

        #[qinvokable]
        fn refresh(self: Pin<&mut SettingsPerformanceController>);
        #[qinvokable]
        #[cxx_name = "setPreference"]
        fn set_preference(
            self: Pin<&mut SettingsPerformanceController>,
            key: &QString,
            json_value: &QString,
        );
        #[qinvokable]
        #[cxx_name = "applyProfile"]
        fn apply_profile(self: Pin<&mut SettingsPerformanceController>, profile: &QString);
        #[qinvokable]
        #[cxx_name = "applyScheduler"]
        fn apply_scheduler(
            self: Pin<&mut SettingsPerformanceController>,
            scheduler: &QString,
            mode: &QString,
        );
    }

    impl cxx_qt::Initialize for SettingsPerformanceController {}
    impl cxx_qt::Threading for SettingsPerformanceController {}
}

pub struct SettingsPerformanceControllerRust {
    worker: Option<PerformanceWorker>,
    generation: u64,
    latest_generation: u64,
    state_json: String,
    busy: bool,
    last_error: String,
}

impl Default for SettingsPerformanceControllerRust {
    fn default() -> Self {
        Self {
            worker: None,
            generation: 0,
            latest_generation: 0,
            state_json: String::from("{}"),
            busy: false,
            last_error: String::new(),
        }
    }
}

#[derive(Default)]
struct Effects {
    state: bool,
    busy: bool,
    error: bool,
}

impl SettingsPerformanceControllerRust {
    fn begin(
        &mut self,
        operation: impl FnOnce(&PerformanceWorker, u64) -> Result<(), String>,
    ) -> Effects {
        if self.busy {
            return Effects::default();
        }
        let Some(worker) = self.worker.as_ref() else {
            return Effects {
                error: self.set_error(String::from(
                    "The Settings Performance worker is unavailable.",
                )),
                ..Effects::default()
            };
        };
        let generation = self.generation.wrapping_add(1);
        if let Err(error) = operation(worker, generation) {
            return Effects {
                error: self.set_error(error),
                ..Effects::default()
            };
        }
        self.generation = generation;
        self.latest_generation = generation;
        self.busy = true;
        Effects {
            busy: true,
            ..Effects::default()
        }
    }

    fn handle_worker_event(&mut self, event: PerformanceWorkerEvent) -> Effects {
        if event.generation != self.latest_generation {
            return Effects::default();
        }
        let mut effects = Effects::default();
        if self.busy {
            self.busy = false;
            effects.busy = true;
        }
        if let Some(snapshot) = event.snapshot {
            let state_json = snapshot.to_string();
            if self.state_json != state_json {
                self.state_json = state_json;
                effects.state = true;
            }
        }
        effects.error = match event.error {
            Some(error) => self.set_error(error),
            None => self.clear_error(),
        };
        effects
    }

    fn set_error(&mut self, error: String) -> bool {
        if self.last_error == error {
            return false;
        }
        self.last_error = error;
        true
    }

    fn clear_error(&mut self) -> bool {
        if self.last_error.is_empty() {
            return false;
        }
        self.last_error.clear();
        true
    }
}

impl qobject::SettingsPerformanceController {
    fn state_json(&self) -> QString {
        QString::from(&self.rust().state_json)
    }
    fn busy(&self) -> bool {
        self.rust().busy
    }
    fn last_error(&self) -> QString {
        QString::from(&self.rust().last_error)
    }

    fn refresh(mut self: Pin<&mut Self>) {
        let effects = self
            .as_mut()
            .rust_mut()
            .begin(|worker, generation| worker.refresh(generation));
        self.as_mut().apply_effects(effects);
    }

    fn set_preference(mut self: Pin<&mut Self>, key: &QString, json_value: &QString) {
        let value = match parse_json_value(json_value) {
            Ok(value) => value,
            Err(error) => {
                let changed = self.as_mut().rust_mut().set_error(error);
                if changed {
                    self.as_mut().error_changed();
                }
                return;
            }
        };
        let key = String::from(key);
        let effects = self
            .as_mut()
            .rust_mut()
            .begin(move |worker, generation| worker.set_preference(generation, key, value));
        self.as_mut().apply_effects(effects);
    }

    fn apply_profile(mut self: Pin<&mut Self>, profile: &QString) {
        let profile = String::from(profile);
        let effects = self
            .as_mut()
            .rust_mut()
            .begin(move |worker, generation| worker.apply_profile(generation, profile));
        self.as_mut().apply_effects(effects);
    }

    fn apply_scheduler(mut self: Pin<&mut Self>, scheduler: &QString, mode: &QString) {
        let scheduler = String::from(scheduler);
        let mode = String::from(mode);
        let effects = self
            .as_mut()
            .rust_mut()
            .begin(move |worker, generation| worker.apply_scheduler(generation, scheduler, mode));
        self.as_mut().apply_effects(effects);
    }

    fn apply_effects(mut self: Pin<&mut Self>, effects: Effects) {
        if effects.state {
            self.as_mut().state_changed();
        }
        if effects.busy {
            self.as_mut().busy_changed();
        }
        if effects.error {
            self.as_mut().error_changed();
        }
    }

    fn handle_worker_event(mut self: Pin<&mut Self>, event: PerformanceWorkerEvent) {
        let effects = self.as_mut().rust_mut().handle_worker_event(event);
        self.as_mut().apply_effects(effects);
    }
}

fn parse_json_value(value: &QString) -> Result<Value, String> {
    serde_json::from_str(&String::from(value))
        .map_err(|error| format!("Invalid setting value: {error}"))
}

impl cxx_qt::Initialize for qobject::SettingsPerformanceController {
    fn initialize(mut self: Pin<&mut Self>) {
        let qt_thread = self.qt_thread();
        match PerformanceWorker::new(move |event| {
            let _ = qt_thread.queue(move |object| object.handle_worker_event(event));
        }) {
            Ok(worker) => self.as_mut().rust_mut().worker = Some(worker),
            Err(error) => {
                let changed = self.as_mut().rust_mut().set_error(error);
                if changed {
                    self.as_mut().error_changed();
                }
            }
        }
    }
}
