pub mod catalog;
pub mod config;
pub mod icon_lookup;
pub mod state;
pub mod worker;

#[cfg(test)]
mod tests {
    use super::catalog::{ThemeCatalog, ThemeDescriptor};
    use super::config::ThemePreferenceStore;
    use super::state::ThemeSelection;
    use std::fs;
    use std::path::Path;
    use tempfile::TempDir;

    fn write_theme(root: &Path, id: &str, metadata: &str) {
        let theme = root.join(id);
        fs::create_dir_all(&theme).unwrap();
        fs::write(theme.join("index.theme"), metadata).unwrap();
    }

    #[test]
    fn catalog_prefers_earlier_root_and_merges_split_content() {
        let first = TempDir::new().unwrap();
        let second = TempDir::new().unwrap();
        write_theme(
            first.path(),
            "split-theme",
            "[Icon Theme]\nName=User Theme\nDirectories=48x48/apps\n\n[48x48/apps]\nSize=48\nType=Fixed\n",
        );
        write_theme(
            second.path(),
            "split-theme",
            "[Icon Theme]\nName=System Theme\nDirectories=scalable/apps\n\n[scalable/apps]\nSize=48\nType=Scalable\n",
        );

        let catalog = ThemeCatalog::discover(&[first.path().into(), second.path().into()]).unwrap();
        let theme = catalog.theme("split-theme").unwrap();
        assert_eq!(theme.name, "User Theme");
        assert_eq!(theme.roots.len(), 2);
        assert_eq!(theme.directories.len(), 2);
    }

    #[test]
    fn catalog_deduplicates_ids_and_hides_hidden_themes() {
        let root = TempDir::new().unwrap();
        write_theme(
            root.path(),
            "visible-theme",
            "[Icon Theme]\nName=Visible\nDirectories=48x48/apps\n\n[48x48/apps]\nSize=48\nType=Fixed\n",
        );
        write_theme(
            root.path(),
            "hidden-theme",
            "[Icon Theme]\nName=Hidden\nHidden=true\nDirectories=48x48/apps\n\n[48x48/apps]\nSize=48\nType=Fixed\n",
        );
        write_theme(
            root.path(),
            "hicolor",
            "[Icon Theme]\nName=hicolor\nDirectories=48x48/apps\n\n[48x48/apps]\nSize=48\nType=Fixed\n",
        );

        let catalog = ThemeCatalog::discover(&[root.path().into()]).unwrap();
        assert!(catalog.theme("visible-theme").is_some());
        assert!(catalog.theme("hidden-theme").is_none());
        assert!(
            catalog
                .user_visible()
                .iter()
                .all(|theme| theme.id != "hicolor")
        );
    }

    #[test]
    fn catalog_parses_metadata_and_selection_validates_discovered_ids() {
        let root = TempDir::new().unwrap();
        write_theme(
            root.path(),
            "metadata-theme",
            "[Icon Theme]\nName=Metadata\nComment=Example theme\nInherits=parent,hicolor\nDirectories=48x48/apps\nScaledDirectories=48x48/apps\nExample=example.png\n\n[48x48/apps]\nSize=48\nType=Fixed\nContext=Applications\n",
        );
        let catalog = ThemeCatalog::discover(&[root.path().into()]).unwrap();
        let descriptor: &ThemeDescriptor = catalog.theme("metadata-theme").unwrap();
        assert_eq!(descriptor.comment, "Example theme");
        assert_eq!(descriptor.inherits, vec!["parent", "hicolor"]);
        assert_eq!(descriptor.example.as_deref(), Some("example.png"));
        assert_eq!(descriptor.directories[0].size, 48);

        let mut selection = ThemeSelection::new(catalog);
        selection.select("metadata-theme").unwrap();
        assert_eq!(selection.selected(), Some("metadata-theme"));
        assert!(selection.select("missing").is_err());
        selection.use_system_default();
        assert_eq!(selection.selected(), None);
    }

    #[test]
    fn config_updates_preserve_unrelated_keys_and_clear_only_selection() {
        let directory = TempDir::new().unwrap();
        let path = directory.path().join("theme.json");
        fs::write(&path, r#"{"icon_theme":"dark","unknown":42}"#).unwrap();
        let store = ThemePreferenceStore::new(path.clone());

        store.save_selected("metadata-theme").unwrap();
        let saved: serde_json::Value = serde_json::from_slice(&fs::read(&path).unwrap()).unwrap();
        assert_eq!(saved["system_icon_theme"], "metadata-theme");
        assert_eq!(saved["icon_theme"], "dark");
        assert_eq!(saved["unknown"], 42);

        store.clear_selected().unwrap();
        let cleared: serde_json::Value = serde_json::from_slice(&fs::read(&path).unwrap()).unwrap();
        assert!(cleared.get("system_icon_theme").is_none());
        assert_eq!(cleared["unknown"], 42);
    }

    #[test]
    fn malformed_config_is_not_overwritten() {
        let directory = TempDir::new().unwrap();
        let path = directory.path().join("theme.json");
        let original = b"{ malformed";
        fs::write(&path, original).unwrap();

        let result = ThemePreferenceStore::new(path.clone()).save_selected("theme");
        assert!(result.is_err());
        assert_eq!(fs::read(&path).unwrap(), original);
    }
}
