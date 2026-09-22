use crate::theme_config::{ThemeConfigError, ThemeConfigStore};
use serde_json::{Map, Value};
use std::path::PathBuf;

#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct AppearancePatch {
    pub theme_preference: Option<String>,
    pub accent: Option<String>,
    pub icon_appearance: Option<String>,
}

impl AppearancePatch {
    pub fn theme_preference(value: &str) -> Self {
        Self {
            theme_preference: Some(normalize_theme_preference(value)),
            ..Self::default()
        }
    }

    pub fn accent(value: &str) -> Self {
        Self {
            accent: Some(normalize_accent(value)),
            ..Self::default()
        }
    }

    pub fn icon_appearance(value: &str) -> Self {
        Self {
            icon_appearance: Some(normalize_icon_appearance(value)),
            ..Self::default()
        }
    }

    pub fn merge(&mut self, newer: Self) {
        if newer.theme_preference.is_some() {
            self.theme_preference = newer.theme_preference;
        }
        if newer.accent.is_some() {
            self.accent = newer.accent;
        }
        if newer.icon_appearance.is_some() {
            self.icon_appearance = newer.icon_appearance;
        }
    }

    fn apply_to(self, object: &mut Map<String, Value>) {
        if let Some(value) = self.theme_preference {
            object.insert(String::from("theme_preference"), Value::String(value));
        }
        if let Some(value) = self.accent {
            object.insert(String::from("accent"), Value::String(value));
        }
        if let Some(value) = self.icon_appearance {
            object.insert(String::from("icon_appearance"), Value::String(value));
        }
    }
}

#[derive(Clone, Debug)]
pub struct AppearanceConfigStore {
    store: ThemeConfigStore,
}

impl AppearanceConfigStore {
    pub fn new(path: PathBuf) -> Self {
        Self {
            store: ThemeConfigStore::new(path),
        }
    }

    pub fn default_path() -> PathBuf {
        ThemeConfigStore::default_path()
    }

    pub fn patch(&self, patch: AppearancePatch) -> Result<(), ThemeConfigError> {
        self.store
            .patch_object(move |object| patch.apply_to(object))
    }
}

impl Default for AppearanceConfigStore {
    fn default() -> Self {
        Self::new(Self::default_path())
    }
}

pub fn normalize_theme_preference(value: &str) -> String {
    let normalized = value.trim().to_ascii_lowercase();
    match normalized.as_str() {
        "auto" | "light" | "dark" => normalized,
        _ => String::from("auto"),
    }
}

pub fn normalize_icon_appearance(value: &str) -> String {
    let normalized = value.trim().to_ascii_lowercase();
    match normalized.as_str() {
        "default" | "monochrome" | "tinted" => normalized,
        _ => String::from("default"),
    }
}

pub fn normalize_accent(value: &str) -> String {
    let normalized = value.trim();
    if normalized.is_empty() {
        String::from("#0a84ff")
    } else {
        normalized.to_owned()
    }
}

#[cfg(test)]
mod tests {
    use super::{
        AppearanceConfigStore, AppearancePatch, normalize_accent, normalize_icon_appearance,
        normalize_theme_preference,
    };
    use serde_json::Value;
    use std::fs;
    use tempfile::TempDir;

    #[test]
    fn theme_preference_normalizes_case_and_whitespace_and_defaults_invalid_values() {
        assert_eq!(normalize_theme_preference(" DARK "), "dark");
        assert_eq!(normalize_theme_preference("LiGhT"), "light");
        assert_eq!(normalize_theme_preference(" AUTO "), "auto");
        assert_eq!(normalize_theme_preference("unsupported"), "auto");
    }

    #[test]
    fn icon_appearance_normalizes_case_and_whitespace_and_defaults_invalid_values() {
        assert_eq!(normalize_icon_appearance(" MONOCHROME "), "monochrome");
        assert_eq!(normalize_icon_appearance("TiNtEd"), "tinted");
        assert_eq!(normalize_icon_appearance(" DEFAULT "), "default");
        assert_eq!(normalize_icon_appearance("unsupported"), "default");
    }

    #[test]
    fn accent_trims_values_defaults_empty_and_preserves_non_empty_content() {
        assert_eq!(normalize_accent("  "), "#0a84ff");
        assert_eq!(
            normalize_accent("  not a strict color  "),
            "not a strict color"
        );
    }

    #[test]
    fn patches_only_appearance_keys_and_preserves_icon_and_unknown_fields() {
        let directory = TempDir::new().unwrap();
        let path = directory.path().join("theme.json");
        fs::write(
            &path,
            br##"{"theme_preference":"dark","accent":"#123456","icon_appearance":"monochrome","shell_style":2,"system_icon_theme":"Breeze","icon_theme":"legacy","future":{"value":3}}"##,
        )
        .unwrap();
        let store = AppearanceConfigStore::new(path.clone());
        store.patch(AppearancePatch::accent(" red ")).unwrap();
        let saved: Value = serde_json::from_slice(&fs::read(&path).unwrap()).unwrap();
        assert_eq!(saved["accent"], "red");
        assert_eq!(saved["theme_preference"], "dark");
        assert_eq!(saved["icon_appearance"], "monochrome");
        assert_eq!(saved["shell_style"], 2);
        assert_eq!(saved["system_icon_theme"], "Breeze");
        assert_eq!(saved["icon_theme"], "legacy");
        assert_eq!(saved["future"]["value"], 3);
    }

    #[test]
    fn malformed_appearance_config_is_not_overwritten() {
        let directory = TempDir::new().unwrap();
        let path = directory.path().join("theme.json");
        let original = b"{broken";
        fs::write(&path, original).unwrap();
        assert!(
            AppearanceConfigStore::new(path.clone())
                .patch(AppearancePatch::accent("blue"))
                .is_err()
        );
        assert_eq!(fs::read(path).unwrap(), original);
    }

    #[test]
    fn concurrent_patches_keep_other_appearance_and_icon_fields() {
        let directory = TempDir::new().unwrap();
        let path = directory.path().join("theme.json");
        fs::write(
            &path,
            br##"{"theme_preference":"auto","accent":"#123456","icon_appearance":"default","system_icon_theme":"Papirus"}"##,
        )
        .unwrap();
        let store = AppearanceConfigStore::new(path.clone());
        let accent_store = store.clone();
        let preference_store = store.clone();
        let accent_writer = std::thread::spawn(move || {
            accent_store
                .patch(AppearancePatch::accent("#bf5af2"))
                .unwrap();
        });
        let preference_writer = std::thread::spawn(move || {
            preference_store
                .patch(AppearancePatch::theme_preference("light"))
                .unwrap();
        });
        accent_writer.join().unwrap();
        preference_writer.join().unwrap();
        let saved: Value = serde_json::from_slice(&fs::read(path).unwrap()).unwrap();
        assert_eq!(saved["theme_preference"], "light");
        assert_eq!(saved["accent"], "#bf5af2");
        assert_eq!(saved["icon_appearance"], "default");
        assert_eq!(saved["system_icon_theme"], "Papirus");
    }
}
