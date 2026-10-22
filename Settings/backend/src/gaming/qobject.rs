use super::worker::{GamingWorker, GamingWorkerEvent};
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
        type SettingsGamingController = super::SettingsGamingControllerRust;
    }

    unsafe extern "RustQt" {
        #[cxx_name = "stateJson"]
        fn state_json(self: &SettingsGamingController) -> QString;
        fn busy(self: &SettingsGamingController) -> bool;
        #[cxx_name = "lastError"]
        fn last_error(self: &SettingsGamingController) -> QString;

        #[qsignal]
        #[cxx_name = "stateChanged"]
        fn state_changed(self: Pin<&mut SettingsGamingController>);
        #[qsignal]
        #[cxx_name = "busyChanged"]
        fn busy_changed(self: Pin<&mut SettingsGamingController>);
        #[qsignal]
        #[cxx_name = "errorChanged"]
        fn error_changed(self: Pin<&mut SettingsGamingController>);

        #[qinvokable]
        fn refresh(self: Pin<&mut SettingsGamingController>);
        #[qinvokable]
        #[cxx_name = "setProtonOption"]
        fn set_proton_option(
            self: Pin<&mut SettingsGamingController>,
            key: &QString,
            json_value: &QString,
        );
        #[qinvokable]
        #[cxx_name = "setCompatibilityOption"]
        fn set_compatibility_option(
            self: Pin<&mut SettingsGamingController>,
            key: &QString,
            json_value: &QString,
        );
    }

    impl cxx_qt::Initialize for SettingsGamingController {}
    impl cxx_qt::Threading for SettingsGamingController {}
}

pub struct SettingsGamingControllerRust {
    worker: Option<GamingWorker>,
    generation: u64,
    latest_generation: u64,
    state_json: String,
    busy: bool,
    last_error: String,
}

impl Default for SettingsGamingControllerRust {
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

impl SettingsGamingControllerRust {
    fn begin(
        &mut self,
        operation: impl FnOnce(&GamingWorker, u64) -> Result<(), String>,
    ) -> Effects {
        if self.busy {
            return Effects::default();
        }
        let Some(worker) = self.worker.as_ref() else {
            return Effects {
                error: self.set_error(String::from("The Settings Gaming worker is unavailable.")),
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

    fn handle_worker_event(&mut self, event: GamingWorkerEvent) -> Effects {
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

impl qobject::SettingsGamingController {
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

    fn set_proton_option(mut self: Pin<&mut Self>, key: &QString, json_value: &QString) {
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
            .begin(move |worker, generation| worker.set_proton(generation, key, value));
        self.as_mut().apply_effects(effects);
    }

    fn set_compatibility_option(mut self: Pin<&mut Self>, key: &QString, json_value: &QString) {
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
            .begin(move |worker, generation| worker.set_compatibility(generation, key, value));
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

    fn handle_worker_event(mut self: Pin<&mut Self>, event: GamingWorkerEvent) {
        let effects = self.as_mut().rust_mut().handle_worker_event(event);
        self.as_mut().apply_effects(effects);
    }
}

fn parse_json_value(value: &QString) -> Result<Value, String> {
    serde_json::from_str(&String::from(value))
        .map_err(|error| format!("Invalid setting value: {error}"))
}

impl cxx_qt::Initialize for qobject::SettingsGamingController {
    fn initialize(mut self: Pin<&mut Self>) {
        let qt_thread = self.qt_thread();
        match GamingWorker::new(move |event| {
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
