use crate::theme_config::{ThemeConfigError, ThemeConfigStore};
use serde_json::Value;
use std::path::{Path, PathBuf};

pub type ConfigError = ThemeConfigError;

#[derive(Clone, Debug)]
pub struct IconThemePreferenceStore {
    config: ThemeConfigStore,
}

impl IconThemePreferenceStore {
    pub fn new(path: PathBuf) -> Self {
        Self {
            config: ThemeConfigStore::new(path),
        }
    }

    pub fn default_path() -> PathBuf {
        ThemeConfigStore::default_path()
    }

    pub fn path(&self) -> &Path {
        self.config.path()
    }

    pub fn load(&self) -> Result<Option<String>, ConfigError> {
        let object = self.config.read_object()?;
        Ok(object
            .and_then(|object| {
                object
                    .get("system_icon_theme")
                    .and_then(Value::as_str)
                    .map(str::trim)
                    .map(str::to_owned)
            })
            .filter(|value| !value.is_empty()))
    }

    pub fn save_selected(&self, theme_id: &str) -> Result<(), ConfigError> {
        if theme_id.trim().is_empty() {
            return self.clear_selected();
        }
        let selected = theme_id.trim().to_owned();
        self.config.patch_object(|object| {
            object.insert(String::from("system_icon_theme"), Value::String(selected));
        })
    }

    pub fn clear_selected(&self) -> Result<(), ConfigError> {
        self.config.patch_object(|object| {
            object.remove("system_icon_theme");
        })
    }
}

impl Default for IconThemePreferenceStore {
    fn default() -> Self {
        Self::new(Self::default_path())
    }
}

#[cfg(test)]
mod tests {
    use super::IconThemePreferenceStore;
    use serde_json::Value;
    use std::fs;
    use tempfile::TempDir;

    #[test]
    fn reads_and_trims_the_persisted_system_icon_theme() {
        let directory = TempDir::new().unwrap();
        let path = directory.path().join("theme.json");
        fs::write(&path, br#"{"system_icon_theme":"  Breeze  "}"#).unwrap();
        assert_eq!(
            IconThemePreferenceStore::new(path)
                .load()
                .unwrap()
                .as_deref(),
            Some("Breeze")
        );
    }

    #[test]
    fn saves_and_clears_only_the_system_icon_theme_key() {
        let directory = TempDir::new().unwrap();
        let path = directory.path().join("theme.json");
        fs::write(
            &path,
            br##"{"accent":"#123456","future":true,"system_icon_theme":"Breeze"}"##,
        )
        .unwrap();
        let store = IconThemePreferenceStore::new(path.clone());
        store.save_selected(" Papirus ").unwrap();
        let saved: Value = serde_json::from_slice(&fs::read(&path).unwrap()).unwrap();
        assert_eq!(saved["system_icon_theme"], "Papirus");
        assert_eq!(saved["accent"], "#123456");
        assert_eq!(saved["future"], true);
        store.clear_selected().unwrap();
        let cleared: Value = serde_json::from_slice(&fs::read(path).unwrap()).unwrap();
        assert!(
            !cleared
                .as_object()
                .unwrap()
                .contains_key("system_icon_theme")
        );
        assert_eq!(cleared["accent"], "#123456");
    }
}
