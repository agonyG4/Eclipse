pub mod catalog;
pub mod config;
pub mod icon_lookup;
pub mod qobject;
pub mod state;
pub mod worker;

#[cfg(test)]
mod tests {
    use super::catalog::{ThemeCatalog, ThemeDescriptor};
    use super::config::ThemePreferenceStore;
    use super::icon_lookup::PreviewResolver;
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
    fn catalog_uses_first_index_metadata_and_keeps_all_split_roots() {
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
        assert_eq!(theme.directories.len(), 1);
    }

    #[test]
    fn catalog_keeps_unindexed_earlier_root_for_icons_and_uses_later_index_metadata() {
        let user = TempDir::new().unwrap();
        let system = TempDir::new().unwrap();
        let user_theme = user.path().join("split-theme");
        fs::create_dir_all(user_theme.join("48x48/apps")).unwrap();
        fs::write(user_theme.join("48x48/apps/folder.png"), b"user icon").unwrap();
        write_theme(
            system.path(),
            "split-theme",
            "[Icon Theme]\nName=System Metadata\nDirectories=48x48/apps\n\n[48x48/apps]\nSize=48\nType=Fixed\n",
        );
        let system_icon = system.path().join("split-theme/48x48/apps/folder.png");
        fs::create_dir_all(system_icon.parent().unwrap()).unwrap();
        fs::write(&system_icon, b"system icon").unwrap();

        let catalog = ThemeCatalog::discover(&[user.path().into(), system.path().into()]).unwrap();
        let theme = catalog.theme("split-theme").unwrap();
        assert_eq!(theme.name, "System Metadata");
        assert_eq!(
            theme.roots,
            vec![user_theme, system.path().join("split-theme")]
        );

        let preview = PreviewResolver::new(catalog)
            .resolve("split-theme", "folder", 48, 1)
            .unwrap();
        assert!(preview.starts_with(user.path()));
    }

    #[test]
    fn catalog_uses_only_the_first_valid_index_for_all_theme_metadata() {
        let user = TempDir::new().unwrap();
        let system = TempDir::new().unwrap();
        write_theme(
            user.path(),
            "split-theme",
            "[Icon Theme]\nName=User Metadata\nComment=authoritative\nInherits=user-parent\nDirectories=user/apps\nExample=user-example\n\n[user/apps]\nSize=48\nType=Fixed\n",
        );
        write_theme(
            system.path(),
            "split-theme",
            "[Icon Theme]\nName=System Metadata\nComment=ignored\nInherits=system-parent\nDirectories=user/apps,system/apps\nExample=system-example\n\n[user/apps]\nSize=48\nType=Fixed\n\n[system/apps]\nSize=64\nType=Fixed\n",
        );

        let catalog = ThemeCatalog::discover(&[user.path().into(), system.path().into()]).unwrap();
        let theme = catalog.theme("split-theme").unwrap();
        assert_eq!(theme.name, "User Metadata");
        assert_eq!(theme.comment, "authoritative");
        assert_eq!(theme.inherits, ["user-parent"]);
        assert_eq!(theme.example.as_deref(), Some("user-example"));
        assert_eq!(theme.directories.len(), 1);
        assert_eq!(theme.directories[0].path, "user/apps");
    }

    #[test]
    fn catalog_omits_theme_when_no_root_has_a_usable_index() {
        let first = TempDir::new().unwrap();
        let second = TempDir::new().unwrap();
        for root in [first.path(), second.path()] {
            fs::create_dir_all(root.join("unindexed-theme/apps")).unwrap();
        }

        let catalog = ThemeCatalog::discover(&[first.path().into(), second.path().into()]).unwrap();
        assert!(catalog.theme("unindexed-theme").is_none());
    }

    #[test]
    fn catalog_skips_malformed_index_when_a_later_root_has_a_valid_one() {
        let first = TempDir::new().unwrap();
        let second = TempDir::new().unwrap();
        write_theme(
            first.path(),
            "recoverable-theme",
            "[Not Icon Theme]\nName=Invalid\n",
        );
        write_theme(
            second.path(),
            "recoverable-theme",
            "[Icon Theme]\nName=Valid\nDirectories=48/apps\n\n[48/apps]\nSize=48\nType=Fixed\n",
        );

        let catalog = ThemeCatalog::discover(&[first.path().into(), second.path().into()]).unwrap();
        let theme = catalog.theme("recoverable-theme").unwrap();
        assert_eq!(theme.name, "Valid");
        assert_eq!(theme.roots.len(), 2);
    }

    #[test]
    fn catalog_preserves_first_root_icon_precedence_with_authoritative_metadata() {
        let user = TempDir::new().unwrap();
        let system = TempDir::new().unwrap();
        write_theme(
            user.path(),
            "split-theme",
            "[Icon Theme]\nName=User Metadata\nDirectories=48x48/apps\n\n[48x48/apps]\nSize=48\nType=Fixed\n",
        );
        write_theme(
            system.path(),
            "split-theme",
            "[Icon Theme]\nName=System Metadata\nDirectories=48x48/apps\n\n[48x48/apps]\nSize=48\nType=Fixed\n",
        );
        for root in [user.path(), system.path()] {
            let icon = root.join("split-theme/48x48/apps/folder.png");
            fs::create_dir_all(icon.parent().unwrap()).unwrap();
            fs::write(&icon, root.to_string_lossy().as_bytes()).unwrap();
        }

        let catalog = ThemeCatalog::discover(&[user.path().into(), system.path().into()]).unwrap();
        let theme = catalog.theme("split-theme").unwrap();
        assert_eq!(theme.name, "User Metadata");
        assert_eq!(theme.roots.len(), 2);
        let preview = PreviewResolver::new(catalog)
            .resolve("split-theme", "folder", 48, 1)
            .unwrap();
        assert!(preview.starts_with(user.path()));
    }

    #[test]
    fn user_visible_themes_sort_by_case_insensitive_name_then_id() {
        let root = TempDir::new().unwrap();
        write_theme(
            root.path(),
            "zeta",
            "[Icon Theme]\nName=beta\nDirectories=48/apps\n\n[48/apps]\nSize=48\n",
        );
        write_theme(
            root.path(),
            "z-same",
            "[Icon Theme]\nName=Same\nDirectories=48/apps\n\n[48/apps]\nSize=48\n",
        );
        write_theme(
            root.path(),
            "a-same",
            "[Icon Theme]\nName=same\nDirectories=48/apps\n\n[48/apps]\nSize=48\n",
        );
        write_theme(
            root.path(),
            "alpha",
            "[Icon Theme]\nName=Alpha\nDirectories=48/apps\n\n[48/apps]\nSize=48\n",
        );

        let catalog = ThemeCatalog::discover(&[root.path().into()]).unwrap();
        let ids = catalog
            .user_visible()
            .into_iter()
            .map(|theme| theme.id.as_str())
            .collect::<Vec<_>>();
        assert_eq!(ids, ["alpha", "zeta", "a-same", "z-same"]);
    }

    #[test]
    fn missing_directory_type_defaults_to_threshold() {
        let root = TempDir::new().unwrap();
        write_theme(
            root.path(),
            "default-type",
            "[Icon Theme]\nName=Default Type\nDirectories=32/apps\n\n[32/apps]\nSize=32\n",
        );

        let catalog = ThemeCatalog::discover(&[root.path().into()]).unwrap();
        assert_eq!(
            catalog.theme("default-type").unwrap().directories[0].kind,
            super::catalog::DirectoryType::Threshold
        );
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
    fn catalog_omits_scalable_directory_without_required_size() {
        let root = TempDir::new().unwrap();
        write_theme(
            root.path(),
            "scalable-metadata",
            "[Icon Theme]\nName=Scalable Metadata\nDirectories=scalable/apps,valid-scalable/apps\n\n[scalable/apps]\nType=Scalable\nMinSize=16\nMaxSize=256\n\n[valid-scalable/apps]\nSize=48\nType=Scalable\nMinSize=16\nMaxSize=256\n",
        );

        let catalog = ThemeCatalog::discover(&[root.path().into()]).unwrap();
        let directories = &catalog.theme("scalable-metadata").unwrap().directories;
        assert_eq!(directories.len(), 1);
        assert_eq!(directories[0].path, "valid-scalable/apps");
        assert_eq!(directories[0].size, 48);
        assert_eq!(directories[0].min_size, 16);
        assert_eq!(directories[0].max_size, 256);
    }

    #[test]
    fn catalog_keeps_theme_ids_case_sensitive() {
        let root = TempDir::new().unwrap();
        let metadata = "[Icon Theme]\nDirectories=48/apps\n\n[48/apps]\nSize=48\nType=Fixed\n";
        write_theme(root.path(), "Oasis", metadata);
        write_theme(root.path(), "oasis", metadata);

        let catalog = ThemeCatalog::discover(&[root.path().into()]).unwrap();
        assert_eq!(catalog.theme("Oasis").unwrap().id, "Oasis");
        assert_eq!(catalog.theme("oasis").unwrap().id, "oasis");
        assert_eq!(catalog.themes().len(), 2);
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

    #[test]
    fn atomic_replacement_leaves_no_temporary_file() {
        let directory = TempDir::new().unwrap();
        let path = directory.path().join("theme.json");
        ThemePreferenceStore::new(path.clone())
            .save_selected("theme")
            .unwrap();

        let temporary_files = fs::read_dir(directory.path())
            .unwrap()
            .filter_map(Result::ok)
            .filter(|entry| {
                entry
                    .file_name()
                    .to_string_lossy()
                    .contains(".theme.json.tmp-")
            })
            .count();
        assert_eq!(temporary_files, 0);
        let saved: serde_json::Value = serde_json::from_slice(&fs::read(path).unwrap()).unwrap();
        assert_eq!(saved["system_icon_theme"], "theme");
    }
}
