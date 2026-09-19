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
        #[qproperty(QString, last_error, cxx_name = "lastError", READ = last_error, NOTIFY = error_changed)]
        type SettingsThemesController = super::SettingsThemesControllerRust;
    }

    unsafe extern "RustQt" {
        fn themes(self: &SettingsThemesController) -> QList_QVariant;
        #[cxx_name = "selectedIconTheme"]
        fn selected_icon_theme(self: &SettingsThemesController) -> QString;
        fn busy(self: &SettingsThemesController) -> bool;
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
    busy: bool,
    last_error: String,
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
            busy: false,
            last_error: String::new(),
        }
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
            .selection
            .selected()
            .map_or_else(QString::default, QString::from)
    }

    fn busy(&self) -> bool {
        self.rust().busy
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
        let generation = worker.request_refresh();
        self.as_mut().rust_mut().generation = generation;
        self.as_mut().rust_mut().busy = true;
        self.as_mut().busy_changed();
    }

    fn set_icon_theme(mut self: Pin<&mut Self>, theme_id: &QString) {
        let theme_id = String::from(theme_id).trim().to_owned();
        if !self.rust().selection.catalog().contains_visible(&theme_id) {
            self.set_error(format!("Icon theme is not installed: {theme_id}"));
            return;
        }
        if let Err(error) = self.rust().store.save_selected(&theme_id) {
            self.set_error(error.to_string());
            return;
        }
        let changed = self.rust().selection.selected() != Some(theme_id.as_str());
        self.as_mut()
            .rust_mut()
            .selection
            .select(&theme_id)
            .expect("theme was validated before selection");
        self.as_mut().rust_mut().configured_selection = Some(theme_id);
        if changed {
            self.as_mut().selected_icon_theme_changed();
        }
        self.clear_error();
    }

    fn use_system_default(mut self: Pin<&mut Self>) {
        if let Err(error) = self.rust().store.clear_selected() {
            self.set_error(error.to_string());
            return;
        }
        let changed = self.rust().selection.selected().is_some();
        self.as_mut().rust_mut().selection.use_system_default();
        self.as_mut().rust_mut().configured_selection = None;
        if changed {
            self.as_mut().selected_icon_theme_changed();
        }
        self.clear_error();
    }

    fn handle_worker_result(mut self: Pin<&mut Self>, result: WorkerResult) {
        if result.generation < self.rust().generation {
            return;
        }
        self.as_mut().rust_mut().busy = false;
        self.as_mut().busy_changed();
        match result.snapshot {
            Ok(ThemeSnapshot { catalog, previews }) => {
                let previous = self.rust().selection.selected().map(str::to_owned);
                self.as_mut().rust_mut().selection.replace_catalog(catalog);
                self.as_mut().rust_mut().previews = previews;
                if let Some(configured) = self.rust().configured_selection.clone() {
                    if self
                        .rust()
                        .selection
                        .catalog()
                        .contains_visible(&configured)
                    {
                        let _ = self.as_mut().rust_mut().selection.select(&configured);
                    } else {
                        self.as_mut().rust_mut().selection.use_system_default();
                    }
                }
                let current = self.rust().selection.selected().map(str::to_owned);
                if previous != current {
                    self.as_mut().selected_icon_theme_changed();
                }
                self.as_mut().clear_error();
                self.as_mut().themes_changed();
            }
            Err(error) => self.set_error(error),
        }
    }

    fn set_error(mut self: Pin<&mut Self>, error: String) {
        if self.rust().last_error == error {
            return;
        }
        self.as_mut().rust_mut().last_error = error;
        self.as_mut().error_changed();
    }

    fn clear_error(self: Pin<&mut Self>) {
        if self.rust().last_error.is_empty() {
            return;
        }
        self.set_error(String::new());
    }
}

impl cxx_qt::Initialize for qobject::SettingsThemesController {
    fn initialize(mut self: Pin<&mut Self>) {
        match self.rust().store.load() {
            Ok(selection) => self.as_mut().rust_mut().configured_selection = selection,
            Err(error) => self.as_mut().set_error(error.to_string()),
        }
        let qt_thread = self.qt_thread();
        let worker = ThemeWorker::new_with_callback(default_search_roots(), move |result| {
            let _ = qt_thread.queue(move |object| object.handle_worker_result(result));
        });
        self.as_mut().rust_mut().worker = Some(worker);
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
