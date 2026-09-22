use super::config::{AppearanceConfigStore, AppearancePatch};
use super::worker::{AppearanceWorker, AppearanceWorkerResult};
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
        #[qproperty(bool, busy, READ = busy, NOTIFY = busy_changed)]
        #[qproperty(QString, last_error, cxx_name = "lastError", READ = last_error, NOTIFY = error_changed)]
        type SettingsAppearanceController = super::SettingsAppearanceControllerRust;
    }

    unsafe extern "RustQt" {
        fn busy(self: &SettingsAppearanceController) -> bool;
        #[cxx_name = "lastError"]
        fn last_error(self: &SettingsAppearanceController) -> QString;

        #[qsignal]
        #[cxx_name = "busyChanged"]
        fn busy_changed(self: Pin<&mut SettingsAppearanceController>);
        #[qsignal]
        #[cxx_name = "errorChanged"]
        fn error_changed(self: Pin<&mut SettingsAppearanceController>);
        #[qsignal]
        #[cxx_name = "configurationChanged"]
        fn configuration_changed(self: Pin<&mut SettingsAppearanceController>);

        #[qinvokable]
        #[cxx_name = "setThemePreference"]
        fn set_theme_preference(self: Pin<&mut SettingsAppearanceController>, value: &QString);
        #[qinvokable]
        #[cxx_name = "setAccentHex"]
        fn set_accent_hex(self: Pin<&mut SettingsAppearanceController>, value: &QString);
        #[qinvokable]
        #[cxx_name = "setIconAppearance"]
        fn set_icon_appearance(self: Pin<&mut SettingsAppearanceController>, value: &QString);
    }

    impl cxx_qt::Initialize for SettingsAppearanceController {}
    impl cxx_qt::Threading for SettingsAppearanceController {}
}

#[derive(Default)]
pub struct SettingsAppearanceControllerRust {
    store: AppearanceConfigStore,
    worker: Option<AppearanceWorker>,
    generation: u64,
    latest_generation: u64,
    latest_error_revision: u64,
    error_revision: u64,
    busy: bool,
    last_error: String,
}

#[derive(Default)]
struct ControllerEffects {
    busy_changed: bool,
    error_changed: bool,
    configuration_changed: bool,
}

impl SettingsAppearanceControllerRust {
    fn handle_worker_result(&mut self, result: AppearanceWorkerResult) -> ControllerEffects {
        let mut effects = ControllerEffects {
            configuration_changed: result.result.is_ok(),
            ..ControllerEffects::default()
        };
        if result.generation != self.latest_generation {
            return effects;
        }

        effects.busy_changed = self.set_busy(false);
        if self.error_revision == self.latest_error_revision {
            match result.result {
                Ok(()) => effects.error_changed = self.clear_error(),
                Err(error) => effects.error_changed = self.set_error(error),
            }
        }
        effects
    }

    fn queue_patch(&mut self, patch: AppearancePatch) -> ControllerEffects {
        let Some(worker) = self.worker.as_ref() else {
            return ControllerEffects {
                error_changed: self.set_error(String::from(
                    "The Settings Appearance worker is unavailable.",
                )),
                ..ControllerEffects::default()
            };
        };
        let generation = self.generation.wrapping_add(1);
        if let Err(error) = worker.request(generation, patch) {
            return ControllerEffects {
                error_changed: self.set_error(error.to_string()),
                ..ControllerEffects::default()
            };
        }

        self.generation = generation;
        self.latest_generation = generation;
        self.latest_error_revision = self.error_revision;
        ControllerEffects {
            busy_changed: self.set_busy(true),
            ..ControllerEffects::default()
        }
    }

    fn set_busy(&mut self, busy: bool) -> bool {
        if self.busy == busy {
            return false;
        }
        self.busy = busy;
        true
    }

    fn set_error(&mut self, error: String) -> bool {
        if self.last_error == error {
            return false;
        }
        self.last_error = error;
        self.error_revision = self.error_revision.wrapping_add(1);
        true
    }

    fn clear_error(&mut self) -> bool {
        if self.last_error.is_empty() {
            return false;
        }
        self.last_error.clear();
        self.error_revision = self.error_revision.wrapping_add(1);
        true
    }
}

impl qobject::SettingsAppearanceController {
    fn busy(&self) -> bool {
        self.rust().busy
    }

    fn last_error(&self) -> QString {
        QString::from(&self.rust().last_error)
    }

    fn set_theme_preference(self: Pin<&mut Self>, value: &QString) {
        self.queue_patch(AppearancePatch::theme_preference(&String::from(value)));
    }

    fn set_accent_hex(self: Pin<&mut Self>, value: &QString) {
        self.queue_patch(AppearancePatch::accent(&String::from(value)));
    }

    fn set_icon_appearance(self: Pin<&mut Self>, value: &QString) {
        self.queue_patch(AppearancePatch::icon_appearance(&String::from(value)));
    }

    fn queue_patch(mut self: Pin<&mut Self>, patch: AppearancePatch) {
        let effects = self.as_mut().rust_mut().queue_patch(patch);
        self.as_mut().apply_effects(effects);
    }

    fn handle_worker_result(mut self: Pin<&mut Self>, result: AppearanceWorkerResult) {
        let effects = self.as_mut().rust_mut().handle_worker_result(result);
        self.as_mut().apply_effects(effects);
    }

    fn apply_effects(mut self: Pin<&mut Self>, effects: ControllerEffects) {
        if effects.busy_changed {
            self.as_mut().busy_changed();
        }
        if effects.error_changed {
            self.as_mut().error_changed();
        }
        if effects.configuration_changed {
            self.as_mut().configuration_changed();
        }
    }
}

impl cxx_qt::Initialize for qobject::SettingsAppearanceController {
    fn initialize(mut self: Pin<&mut Self>) {
        let qt_thread = self.qt_thread();
        let store = self.rust().store.clone();
        match AppearanceWorker::new_with_callback(store, move |result| {
            let _ = qt_thread.queue(move |object| object.handle_worker_result(result));
        }) {
            Ok(worker) => self.as_mut().rust_mut().worker = Some(worker),
            Err(error) => {
                let changed = self.as_mut().rust_mut().set_error(error.to_string());
                if changed {
                    self.as_mut().error_changed();
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::SettingsAppearanceControllerRust;
    use crate::appearance::worker::AppearanceWorkerResult;

    #[test]
    fn successful_commit_requests_live_projection_reload_effect() {
        let mut controller = SettingsAppearanceControllerRust {
            latest_generation: 2,
            latest_error_revision: 0,
            busy: true,
            ..Default::default()
        };
        let effects = controller.handle_worker_result(AppearanceWorkerResult {
            generation: 2,
            result: Ok(()),
        });
        assert!(effects.configuration_changed);
        assert!(effects.busy_changed);
        assert!(!controller.busy);
    }

    #[test]
    fn failed_commit_reports_error_without_claiming_configuration_changed() {
        let mut controller = SettingsAppearanceControllerRust {
            latest_generation: 1,
            latest_error_revision: 0,
            busy: true,
            ..Default::default()
        };
        let effects = controller.handle_worker_result(AppearanceWorkerResult {
            generation: 1,
            result: Err(String::from("write failed")),
        });
        assert!(!effects.configuration_changed);
        assert!(effects.error_changed);
        assert_eq!(controller.last_error, "write failed");
        assert!(!controller.busy);
    }

    #[test]
    fn stale_completion_does_not_change_newer_busy_or_error_state() {
        let mut controller = SettingsAppearanceControllerRust {
            latest_generation: 3,
            busy: true,
            ..Default::default()
        };
        let effects = controller.handle_worker_result(AppearanceWorkerResult {
            generation: 1,
            result: Err(String::from("stale failure")),
        });
        assert!(!effects.configuration_changed);
        assert!(!effects.busy_changed);
        assert!(!effects.error_changed);
        assert_eq!(controller.last_error, "");
        assert!(controller.busy);
    }

    #[test]
    fn unavailable_worker_surfaces_submission_error() {
        let mut controller = SettingsAppearanceControllerRust::default();
        let effects = controller.queue_patch(Default::default());
        assert!(!effects.configuration_changed);
        assert!(effects.error_changed);
        assert!(!controller.busy);
        assert!(controller.last_error.contains("worker is unavailable"));
    }
}
