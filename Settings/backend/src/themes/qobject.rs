use super::catalog::{ThemeCatalog, default_search_roots};
use super::config::ThemePreferenceStore;
use super::state::ThemeSelection;
use super::worker::{PREVIEW_ICON_NAMES, ThemeSnapshot, ThemeWorker, WorkerResult};
use cxx_qt::{CxxQtType, Threading};
use cxx_qt_lib::{QList, QMap, QMapPair_QString_QVariant, QString, QVariant};
use std::path::Path;
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
        #[qproperty(QList_QVariant, themes, READ = themes, NOTIFY = themes_changed)]
        #[qproperty(QString, selected_icon_theme, cxx_name = "selectedIconTheme", READ = selected_icon_theme, NOTIFY = selected_icon_theme_changed)]
        #[qproperty(bool, busy, READ = busy, NOTIFY = busy_changed)]
        #[qproperty(bool, refreshing, READ = refreshing, NOTIFY = refreshing_changed)]
        #[qproperty(QString, last_error, cxx_name = "lastError", READ = last_error, NOTIFY = error_changed)]
        type SettingsThemesController = super::SettingsThemesControllerRust;
    }

    unsafe extern "RustQt" {
        fn themes(self: &SettingsThemesController) -> QList_QVariant;
        #[cxx_name = "selectedIconTheme"]
        fn selected_icon_theme(self: &SettingsThemesController) -> QString;
        fn busy(self: &SettingsThemesController) -> bool;
        fn refreshing(self: &SettingsThemesController) -> bool;
        #[cxx_name = "lastError"]
        fn last_error(self: &SettingsThemesController) -> QString;

        #[qsignal]
        #[cxx_name = "themesChanged"]
        fn themes_changed(self: Pin<&mut SettingsThemesController>);
        #[qsignal]
        #[cxx_name = "selectedIconThemeChanged"]
        fn selected_icon_theme_changed(self: Pin<&mut SettingsThemesController>);
        #[qsignal]
        #[cxx_name = "busyChanged"]
        fn busy_changed(self: Pin<&mut SettingsThemesController>);
        #[qsignal]
        #[cxx_name = "refreshingChanged"]
        fn refreshing_changed(self: Pin<&mut SettingsThemesController>);
        #[qsignal]
        #[cxx_name = "errorChanged"]
        fn error_changed(self: Pin<&mut SettingsThemesController>);

        #[qinvokable]
        fn refresh(self: Pin<&mut SettingsThemesController>);
        #[qinvokable]
        #[cxx_name = "setIconTheme"]
        fn set_icon_theme(self: Pin<&mut SettingsThemesController>, theme_id: &QString);
        #[qinvokable]
        #[cxx_name = "useSystemDefault"]
        fn use_system_default(self: Pin<&mut SettingsThemesController>);
    }

    impl cxx_qt::Initialize for SettingsThemesController {}
    impl cxx_qt::Threading for SettingsThemesController {}
}

pub struct SettingsThemesControllerRust {
    selection: ThemeSelection,
    configured_selection: Option<String>,
    previews: std::collections::HashMap<String, Vec<Option<std::path::PathBuf>>>,
    worker: Option<ThemeWorker>,
    store: ThemePreferenceStore,
    generation: u64,
    selection_generation: u64,
    pending_selection: Option<PendingSelection>,
    refresh_busy: bool,
    busy: bool,
    last_error: String,
    error_revision: u64,
}

struct PendingSelection {
    generation: u64,
    selected: Option<String>,
    error_revision: u64,
}

#[derive(Default)]
struct ControllerEffects {
    busy_changed: bool,
    refreshing_changed: bool,
    themes_changed: bool,
    selected_icon_theme_changed: bool,
    error_changed: bool,
}

impl Default for SettingsThemesControllerRust {
    fn default() -> Self {
        Self {
            selection: ThemeSelection::new(ThemeCatalog::default()),
            configured_selection: None,
            previews: std::collections::HashMap::new(),
            worker: None,
            store: ThemePreferenceStore::default(),
            generation: 0,
            selection_generation: 0,
            pending_selection: None,
            refresh_busy: false,
            busy: false,
            last_error: String::new(),
            error_revision: 0,
        }
    }
}

impl SettingsThemesControllerRust {
    fn effective_selected(&self) -> Option<&str> {
        match self.pending_selection.as_ref() {
            Some(pending) => pending.selected.as_deref(),
            None => self.selection.selected(),
        }
    }

    fn reconcile_configured_selection(&mut self, configured: Option<String>) {
        self.configured_selection = configured.clone();
        if let Some(configured) = configured
            && self.selection.catalog().contains_visible(&configured)
        {
            if self.selection.select(&configured).is_err() {
                self.selection.use_system_default();
            }
        } else {
            self.selection.use_system_default();
        }
    }

    fn handle_worker_result(&mut self, result: WorkerResult) -> ControllerEffects {
        let mut effects = ControllerEffects::default();
        match result {
            WorkerResult::Refresh {
                generation,
                snapshot,
            } => {
                if generation < self.generation {
                    return effects;
                }
                let was_refreshing = self.refresh_busy;
                self.refresh_busy = false;
                effects.refreshing_changed = was_refreshing;
                effects.busy_changed = self.update_busy();
                match snapshot {
                    Ok(ThemeSnapshot {
                        catalog,
                        previews,
                        configured_selection,
                    }) => {
                        let previous = self.effective_selected().map(str::to_owned);
                        self.selection.replace_catalog(catalog);
                        self.previews = previews;
                        let preference_error = if self.pending_selection.is_none() {
                            match configured_selection {
                                Ok(configured) => {
                                    self.reconcile_configured_selection(configured);
                                    None
                                }
                                Err(error) => {
                                    self.reconcile_configured_selection(
                                        self.configured_selection.clone(),
                                    );
                                    Some(error)
                                }
                            }
                        } else {
                            None
                        };
                        let current = self.effective_selected().map(str::to_owned);
                        effects.selected_icon_theme_changed = previous != current;
                        if self.pending_selection.is_none() {
                            effects.error_changed = if let Some(error) = preference_error {
                                self.set_error_value(error)
                            } else {
                                self.clear_error_value()
                            };
                        }
                        effects.themes_changed = true;
                    }
                    Err(error) => {
                        effects.error_changed = self.set_error_value(error);
                    }
                }
            }
            WorkerResult::Persistence {
                generation,
                selected,
                result,
            } => {
                let matches_pending = self.pending_selection.as_ref().is_some_and(|pending| {
                    pending.generation == generation && pending.selected == selected
                });
                if !matches_pending {
                    return effects;
                }
                let previous = self.effective_selected().map(str::to_owned);
                let Some(pending) = self.pending_selection.take() else {
                    return effects;
                };
                match result {
                    Ok(()) => {
                        let selected_is_valid = match selected.as_deref() {
                            Some(theme_id) => match self.selection.select(theme_id) {
                                Ok(()) => true,
                                Err(_) => {
                                    if self.error_revision == pending.error_revision {
                                        effects.error_changed = self.set_error_value(format!(
                                            "Icon theme is not installed: {theme_id}"
                                        ));
                                    }
                                    false
                                }
                            },
                            None => {
                                self.selection.use_system_default();
                                true
                            }
                        };
                        if selected_is_valid {
                            self.configured_selection = selected;
                        }
                        if self.error_revision == pending.error_revision {
                            effects.error_changed |= self.clear_error_value();
                        }
                    }
                    Err(error) if self.error_revision == pending.error_revision => {
                        effects.error_changed = self.set_error_value(error);
                    }
                    Err(_) => {}
                }
                let current = self.effective_selected().map(str::to_owned);
                effects.selected_icon_theme_changed = previous != current;
                effects.busy_changed |= self.update_busy();
            }
        }
        effects
    }

    fn set_error_value(&mut self, error: String) -> bool {
        self.error_revision = self.error_revision.wrapping_add(1);
        if self.last_error == error {
            return false;
        }
        self.last_error = error;
        true
    }

    fn clear_error_value(&mut self) -> bool {
        if self.last_error.is_empty() {
            return false;
        }
        self.set_error_value(String::new())
    }

    fn record_worker_start_failure(&mut self, error: String) -> bool {
        self.set_error_value(error)
    }

    fn update_busy(&mut self) -> bool {
        let busy = self.refresh_busy || self.pending_selection.is_some();
        if self.busy == busy {
            return false;
        }
        self.busy = busy;
        true
    }
}

impl qobject::SettingsThemesController {
    fn themes(&self) -> QList<QVariant> {
        let mut themes = QList::default();
        for theme in self.rust().selection.catalog().user_visible() {
            let mut descriptor = QMap::default();
            insert_string(&mut descriptor, "id", &theme.id);
            insert_string(&mut descriptor, "name", &theme.name);
            insert_string(&mut descriptor, "comment", &theme.comment);
            insert_string(
                &mut descriptor,
                "source",
                if theme.user { "user" } else { "system" },
            );
            let mut previews = QList::default();
            if let Some(paths) = self.rust().previews.get(&theme.id) {
                for path in paths {
                    previews.append(QVariant::from(&QString::from(
                        path.as_deref().map_or_else(String::new, path_string),
                    )));
                }
            } else {
                for _ in PREVIEW_ICON_NAMES {
                    previews.append(QVariant::from(&QString::default()));
                }
            }
            insert_variant(&mut descriptor, "previewUrls", QVariant::from(&previews));
            themes.append(QVariant::from(&descriptor));
        }
        themes
    }

    fn selected_icon_theme(&self) -> QString {
        self.rust()
            .effective_selected()
            .map_or_else(QString::default, QString::from)
    }

    fn busy(&self) -> bool {
        self.rust().busy
    }

    fn refreshing(&self) -> bool {
        self.rust().refresh_busy
    }

    fn last_error(&self) -> QString {
        QString::from(&self.rust().last_error)
    }

    fn refresh(mut self: Pin<&mut Self>) {
        if self.rust().busy {
            return;
        }
        let Some(worker) = self.rust().worker.as_ref() else {
            self.set_error(String::from("The icon theme scanner is unavailable."));
            return;
        };
        let generation = match worker.request_refresh() {
            Ok(generation) => generation,
            Err(error) => {
                self.set_error(error.to_string());
                return;
            }
        };
        self.as_mut().rust_mut().generation = generation;
        self.as_mut().rust_mut().refresh_busy = true;
        self.as_mut().refreshing_changed();
        self.as_mut().update_busy();
    }

    fn set_icon_theme(self: Pin<&mut Self>, theme_id: &QString) {
        let theme_id = String::from(theme_id).trim().to_owned();
        if !self.rust().selection.catalog().contains_visible(&theme_id) {
            self.set_error(format!("Icon theme is not installed: {theme_id}"));
            return;
        }
        self.queue_selection_persistence(Some(theme_id));
    }

    fn use_system_default(self: Pin<&mut Self>) {
        self.queue_selection_persistence(None);
    }

    fn handle_worker_result(mut self: Pin<&mut Self>, result: WorkerResult) {
        let effects = self.as_mut().rust_mut().handle_worker_result(result);
        if effects.busy_changed {
            self.as_mut().busy_changed();
        }
        if effects.refreshing_changed {
            self.as_mut().refreshing_changed();
        }
        if effects.themes_changed {
            self.as_mut().themes_changed();
        }
        if effects.selected_icon_theme_changed {
            self.as_mut().selected_icon_theme_changed();
        }
        if effects.error_changed {
            self.as_mut().error_changed();
        }
    }

    fn queue_selection_persistence(mut self: Pin<&mut Self>, selected: Option<String>) {
        let previous = self.rust().effective_selected().map(str::to_owned);
        if previous == selected {
            return;
        }
        let generation = self.rust().selection_generation.wrapping_add(1);
        let error_revision = self.rust().error_revision;
        self.as_mut().rust_mut().selection_generation = generation;
        self.as_mut().rust_mut().pending_selection = Some(PendingSelection {
            generation,
            selected: selected.clone(),
            error_revision,
        });
        self.as_mut().update_busy();
        self.as_mut().selected_icon_theme_changed();

        let submission = self
            .rust()
            .worker
            .as_ref()
            .map(|worker| worker.request_persistence(generation, selected));
        match submission {
            Some(Ok(())) => {}
            Some(Err(error)) => {
                self.as_mut().rust_mut().pending_selection = None;
                self.as_mut().update_busy();
                self.as_mut().selected_icon_theme_changed();
                self.set_error(error.to_string());
            }
            None => {
                self.as_mut().rust_mut().pending_selection = None;
                self.as_mut().update_busy();
                self.as_mut().selected_icon_theme_changed();
                self.set_error(String::from("The icon theme worker is unavailable."));
            }
        }
    }

    fn update_busy(mut self: Pin<&mut Self>) {
        if self.as_mut().rust_mut().update_busy() {
            self.busy_changed();
        }
    }

    fn set_error(mut self: Pin<&mut Self>, error: String) {
        if self.as_mut().rust_mut().set_error_value(error) {
            self.error_changed();
        }
    }
}

impl cxx_qt::Initialize for qobject::SettingsThemesController {
    fn initialize(mut self: Pin<&mut Self>) {
        let qt_thread = self.qt_thread();
        let worker = ThemeWorker::new_with_callback(
            default_search_roots(),
            self.rust().store.clone(),
            move |result| {
                let _ = qt_thread.queue(move |object| object.handle_worker_result(result));
            },
        );
        match worker {
            Ok(worker) => self.as_mut().rust_mut().worker = Some(worker),
            Err(error) => {
                let changed = self
                    .as_mut()
                    .rust_mut()
                    .record_worker_start_failure(error.to_string());
                if changed {
                    self.as_mut().error_changed();
                }
                return;
            }
        }
        self.refresh();
    }
}

fn path_string(path: &Path) -> String {
    path.to_string_lossy().into_owned()
}

fn insert_string(map: &mut QMap<QMapPair_QString_QVariant>, key: &str, value: &str) {
    map.insert(QString::from(key), QVariant::from(&QString::from(value)));
}

fn insert_variant(map: &mut QMap<QMapPair_QString_QVariant>, key: &str, value: QVariant) {
    map.insert(QString::from(key), value);
}

#[cfg(test)]
mod tests {
    use super::{PendingSelection, SettingsThemesControllerRust};
    use crate::themes::catalog::ThemeCatalog;
    use crate::themes::worker::{ThemeSnapshot, WorkerResult};
    use std::fs;
    use std::path::Path;
    use tempfile::TempDir;

    fn write_theme(root: &Path, id: &str) {
        let theme = root.join(id);
        fs::create_dir_all(&theme).unwrap();
        fs::write(
            theme.join("index.theme"),
            "[Icon Theme]\nName=Theme\nDirectories=48x48/apps\n\n[48x48/apps]\nSize=48\nType=Fixed\n",
        )
        .unwrap();
    }

    fn controller_with_catalog(root: &Path) -> SettingsThemesControllerRust {
        for id in ["theme-a", "theme-b", "theme-c"] {
            write_theme(root, id);
        }
        let catalog = ThemeCatalog::discover(&[root.into()]).unwrap();
        let mut controller = SettingsThemesControllerRust::default();
        controller.selection.replace_catalog(catalog);
        controller.selection.select("theme-a").unwrap();
        controller.configured_selection = Some(String::from("theme-a"));
        controller
    }

    fn refresh_result(catalog: ThemeCatalog, selected: Option<&str>) -> WorkerResult {
        WorkerResult::Refresh {
            generation: 0,
            snapshot: Ok(ThemeSnapshot {
                catalog,
                previews: Default::default(),
                configured_selection: Ok(selected.map(str::to_owned)),
            }),
        }
    }

    #[test]
    fn refresh_reconciles_external_selection_disappearance_reappearance_and_default() {
        let directory = TempDir::new().unwrap();
        let mut controller = controller_with_catalog(directory.path());
        let catalog = ThemeCatalog::discover(&[directory.path().into()]).unwrap();

        controller.handle_worker_result(refresh_result(catalog.clone(), Some("theme-b")));
        assert_eq!(controller.selection.selected(), Some("theme-b"));
        assert_eq!(controller.configured_selection.as_deref(), Some("theme-b"));

        fs::remove_dir_all(directory.path().join("theme-b")).unwrap();
        let catalog_without_b = ThemeCatalog::discover(&[directory.path().into()]).unwrap();
        controller.handle_worker_result(refresh_result(catalog_without_b, Some("theme-b")));
        assert_eq!(controller.selection.selected(), None);
        assert_eq!(controller.configured_selection.as_deref(), Some("theme-b"));

        write_theme(directory.path(), "theme-b");
        let catalog_with_b = ThemeCatalog::discover(&[directory.path().into()]).unwrap();
        controller.handle_worker_result(refresh_result(catalog_with_b.clone(), Some("theme-b")));
        assert_eq!(controller.selection.selected(), Some("theme-b"));

        controller.handle_worker_result(refresh_result(catalog_with_b, None));
        assert_eq!(controller.selection.selected(), None);
        assert_eq!(controller.configured_selection, None);
    }

    #[test]
    fn worker_startup_failure_reports_error_and_keeps_safe_default_state() {
        let mut controller = SettingsThemesControllerRust::default();

        assert!(controller.record_worker_start_failure(String::from(
            "failed to create Settings Themes worker: injected failure"
        )));

        assert_eq!(
            controller.last_error,
            "failed to create Settings Themes worker: injected failure"
        );
        assert!(controller.worker.is_none());
        assert!(controller.selection.catalog().themes().is_empty());
        assert_eq!(controller.selection.selected(), None);
        assert!(!controller.busy);
    }

    #[test]
    fn stale_persistence_completion_preserves_newer_selection_error_and_busy_state() {
        let directory = TempDir::new().unwrap();
        let mut controller = controller_with_catalog(directory.path());
        controller.set_error_value(String::from("older error"));
        controller.pending_selection = Some(PendingSelection {
            generation: 3,
            selected: Some(String::from("theme-c")),
            error_revision: controller.error_revision,
        });
        controller.selection_generation = 3;
        controller.busy = true;
        controller.set_error_value(String::from("newer error"));

        let effects = controller.handle_worker_result(WorkerResult::Persistence {
            generation: 1,
            selected: Some(String::from("theme-a")),
            result: Ok(()),
        });

        assert!(!effects.busy_changed);
        assert!(!effects.selected_icon_theme_changed);
        assert!(!effects.error_changed);
        assert_eq!(controller.selection.selected(), Some("theme-a"));
        assert_eq!(controller.configured_selection.as_deref(), Some("theme-a"));
        assert_eq!(controller.last_error, "newer error");
        assert!(controller.busy);
        assert_eq!(
            controller
                .pending_selection
                .as_ref()
                .map(|pending| pending.generation),
            Some(3)
        );
    }

    #[test]
    fn pending_selection_projects_newest_value_and_stale_completion_cannot_revert_it() {
        let directory = TempDir::new().unwrap();
        let mut controller = controller_with_catalog(directory.path());
        assert_eq!(controller.effective_selected(), Some("theme-a"));

        controller.pending_selection = Some(PendingSelection {
            generation: 2,
            selected: Some(String::from("theme-b")),
            error_revision: controller.error_revision,
        });
        assert_eq!(controller.effective_selected(), Some("theme-b"));

        controller.pending_selection = Some(PendingSelection {
            generation: 3,
            selected: Some(String::from("theme-c")),
            error_revision: controller.error_revision,
        });
        controller.selection_generation = 3;
        let effects = controller.handle_worker_result(WorkerResult::Persistence {
            generation: 1,
            selected: Some(String::from("theme-b")),
            result: Ok(()),
        });

        assert!(!effects.selected_icon_theme_changed);
        assert_eq!(controller.selection.selected(), Some("theme-a"));
        assert_eq!(controller.effective_selected(), Some("theme-c"));
        assert_eq!(
            controller
                .pending_selection
                .as_ref()
                .map(|pending| pending.generation),
            Some(3)
        );
    }

    #[test]
    fn matching_persistence_failure_rolls_back_projected_selection() {
        let directory = TempDir::new().unwrap();
        let mut controller = controller_with_catalog(directory.path());
        controller.pending_selection = Some(PendingSelection {
            generation: 4,
            selected: Some(String::from("theme-c")),
            error_revision: controller.error_revision,
        });
        controller.selection_generation = 4;
        controller.busy = true;

        let effects = controller.handle_worker_result(WorkerResult::Persistence {
            generation: 4,
            selected: Some(String::from("theme-c")),
            result: Err(String::from("persistence blocked")),
        });

        assert!(effects.selected_icon_theme_changed);
        assert!(effects.error_changed);
        assert_eq!(controller.selection.selected(), Some("theme-a"));
        assert_eq!(controller.effective_selected(), Some("theme-a"));
        assert!(controller.pending_selection.is_none());
        assert_eq!(controller.last_error, "persistence blocked");
        assert!(!controller.busy);
    }

    #[test]
    fn matching_persistence_success_does_not_duplicate_projected_selection_signal() {
        let directory = TempDir::new().unwrap();
        let mut controller = controller_with_catalog(directory.path());
        controller.pending_selection = Some(PendingSelection {
            generation: 5,
            selected: Some(String::from("theme-c")),
            error_revision: controller.error_revision,
        });
        controller.selection_generation = 5;
        controller.busy = true;

        let effects = controller.handle_worker_result(WorkerResult::Persistence {
            generation: 5,
            selected: Some(String::from("theme-c")),
            result: Ok(()),
        });

        assert!(!effects.selected_icon_theme_changed);
        assert_eq!(controller.selection.selected(), Some("theme-c"));
        assert_eq!(controller.effective_selected(), Some("theme-c"));
        assert!(controller.pending_selection.is_none());
        assert!(!controller.busy);
    }

    #[test]
    fn newest_persistence_completion_updates_selected_theme_and_busy_state() {
        let directory = TempDir::new().unwrap();
        let mut controller = controller_with_catalog(directory.path());
        controller.pending_selection = Some(PendingSelection {
            generation: 3,
            selected: Some(String::from("theme-c")),
            error_revision: controller.error_revision,
        });
        controller.selection_generation = 3;
        controller.busy = true;

        let effects = controller.handle_worker_result(WorkerResult::Persistence {
            generation: 3,
            selected: Some(String::from("theme-c")),
            result: Ok(()),
        });

        assert!(effects.busy_changed);
        assert!(!effects.selected_icon_theme_changed);
        assert_eq!(controller.selection.selected(), Some("theme-c"));
        assert_eq!(controller.configured_selection.as_deref(), Some("theme-c"));
        assert!(!controller.busy);
        assert!(controller.pending_selection.is_none());
    }
}
