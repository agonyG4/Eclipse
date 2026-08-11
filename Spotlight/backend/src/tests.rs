use crate::desktop_entries::{self, *};
use crate::ranking;
use crate::usage;
use std::collections::HashMap;
use std::ffi::CString;
use std::os::raw::c_char;
use std::path::PathBuf;
use std::sync::{Mutex, MutexGuard, OnceLock};
use std::time::{SystemTime, UNIX_EPOCH};

fn make_entry(name: &str, id: &str) -> DesktopEntry {
    DesktopEntry {
        id: id.to_string(),
        name: name.to_string(),
        desktop_file_name: format!("{}.desktop", id),
        ..Default::default()
    }
}

fn s(
    entries: &[DesktopEntry],
    query: &str,
    usage: &HashMap<String, i32>,
    limit: usize,
) -> Vec<ranking::ScoredEntry> {
    ranking::search_entries(entries, query, usage, limit)
}

fn environment_lock() -> MutexGuard<'static, ()> {
    static LOCK: OnceLock<Mutex<()>> = OnceLock::new();
    LOCK.get_or_init(|| Mutex::new(())).lock().unwrap()
}

fn temporary_test_path(label: &str) -> PathBuf {
    let nanos = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .as_nanos();
    std::env::temp_dir().join(format!("astrea-m7e-{label}-{}-{nanos}", std::process::id()))
}

fn external_entry(name: &str, id: &str) -> DesktopEntry {
    DesktopEntry {
        id: id.to_string(),
        name: name.to_string(),
        desktop_file_name: format!("{id}.desktop"),
        ..Default::default()
    }
}

fn search_has_id(backend: &crate::AstreaSpotlightBackend, query: &str, id: &str) -> bool {
    backend
        .search(query, 6)
        .unwrap()
        .iter()
        .any(|result| result.entry.id == id)
}

#[test]
fn test_exact_match_ranks_first() {
    let entries = vec![
        make_entry("Firefox", "firefox"),
        make_entry("Firefox Developer", "firefox-developer"),
        make_entry("Zathura", "zathura"),
    ];
    let usage = HashMap::new();
    let results = s(&entries, "firefox", &usage, 6);
    assert_eq!(results.len(), 2);
    assert_eq!(results[0].entry.name, "Firefox");
}

#[test]
fn test_prefix_match() {
    let entries = vec![
        make_entry("Firefox", "firefox"),
        make_entry("Firmware", "firmware"),
    ];
    let usage = HashMap::new();
    let results = s(&entries, "fir", &usage, 6);
    assert_eq!(results.len(), 2);
    assert_eq!(results[0].entry.name, "Firefox");
}

#[test]
fn test_token_prefix_match() {
    let entries = vec![
        make_entry("Visual Studio Code", "code"),
        make_entry("Vim", "vim"),
    ];
    let usage = HashMap::new();
    let results = s(&entries, "vis", &usage, 6);
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].entry.name, "Visual Studio Code");
}

#[test]
fn test_acronym_match() {
    let entries = vec![
        make_entry("Visual Studio Code", "vscode"),
        make_entry("Very Secure Tool", "vst"),
    ];
    let usage = HashMap::new();
    let results = s(&entries, "vsc", &usage, 6);
    assert!(!results.is_empty());
}

#[test]
fn test_usage_tiebreak() {
    let mut usage = HashMap::new();
    usage.insert("zathura".to_string(), 10);
    usage.insert("firefox".to_string(), 5);

    let entries = vec![
        make_entry("Firefox", "firefox"),
        make_entry("Zathura", "zathura"),
    ];
    let results = s(&entries, "zathura", &usage, 6);
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].entry.name, "Zathura");
}

#[test]
fn test_usage_tiebreak_same_score() {
    let mut usage = HashMap::new();
    usage.insert("brave".to_string(), 5);
    usage.insert("chrome".to_string(), 0);

    let mut e = make_entry("Chrome", "chrome");
    e.keywords = vec!["browser".to_string()];
    let mut e2 = make_entry("Brave", "brave");
    e2.keywords = vec!["browser".to_string()];
    let entries = vec![e2, e];
    let results = s(&entries, "browser", &usage, 6);
    assert_eq!(results.len(), 2);
    assert_eq!(results[0].entry.name, "Brave");
    assert_eq!(results[1].entry.name, "Chrome");
}

#[test]
fn test_strong_match_cutoff() {
    let mut entries = Vec::new();
    for i in 0..10 {
        entries.push(make_entry(&format!("App {}", i), &format!("app{}", i)));
    }
    entries.push(make_entry("ExactMatch", "exact"));
    let usage = HashMap::new();
    let results = s(&entries, "ExactMatch", &usage, 6);
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].entry.name, "ExactMatch");
}

#[test]
fn test_six_result_limit() {
    let mut entries = Vec::new();
    for i in 0..20 {
        entries.push(make_entry(&format!("App{}", i), &format!("app{}", i)));
    }
    let usage = HashMap::new();
    let results = s(&entries, "app", &usage, 6);
    assert!(results.len() <= 6);
}

#[test]
fn test_hidden_no_display_filtering() {
    let mut e = make_entry("Hidden", "hidden");
    e.hidden = true;
    let mut e2 = make_entry("NoDisplay", "nodisplay");
    e2.no_display = true;
    let entries = vec![e, e2, make_entry("Normal", "normal")];
    let usage = HashMap::new();
    let results = s(&entries, "a", &usage, 6);
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].entry.name, "Normal");
}

#[test]
fn test_duplicate_deduplication() {
    let mut a = make_entry("Firefox", "firefox.desktop");
    a.desktop_file_name = "firefox.desktop".to_string();
    a.id.clear();
    let mut b = make_entry("Firefox", "firefox-esr.desktop");
    b.desktop_file_name = "firefox-esr.desktop".to_string();
    b.id.clear();
    let entries = vec![a, b];
    let usage = HashMap::new();
    let results = s(&entries, "firefox", &usage, 6);
    assert_eq!(results.len(), 2);
}

#[test]
fn test_xdg_precedence_same_id() {
    let _environment_guard = environment_lock();
    let dir1 = std::env::temp_dir().join("spotlight-precedence-1/applications");
    let dir2 = std::env::temp_dir().join("spotlight-precedence-2/applications");
    let _ = std::fs::remove_dir_all(dir1.parent().unwrap());
    let _ = std::fs::remove_dir_all(dir2.parent().unwrap());
    std::fs::create_dir_all(&dir1).unwrap();
    std::fs::create_dir_all(&dir2).unwrap();

    std::fs::write(
        dir1.join("firefox.desktop"),
        "[Desktop Entry]\nType=Application\nName=Firefox ESR\nExec=firefox-esr\n",
    )
    .unwrap();
    std::fs::write(
        dir2.join("firefox.desktop"),
        "[Desktop Entry]\nType=Application\nName=Firefox\nExec=firefox\n",
    )
    .unwrap();

    let orig_data_dirs = std::env::var("XDG_DATA_DIRS").ok();
    unsafe {
        std::env::set_var("XDG_DATA_HOME", dir1.parent().unwrap().parent().unwrap());
    }
    unsafe {
        std::env::set_var(
            "XDG_DATA_DIRS",
            format!(
                "{}:{}",
                dir1.parent().unwrap().to_str().unwrap(),
                dir2.parent().unwrap().to_str().unwrap()
            ),
        );
    }

    let mut index = desktop_entries::DesktopEntryIndex::new();
    index.reload();
    let entries = index.entries();
    let firefox_entries: Vec<_> = entries
        .iter()
        .filter(|e| e.name.contains("Firefox"))
        .collect();
    assert!(!firefox_entries.is_empty());
    assert_eq!(firefox_entries[0].name, "Firefox ESR");
    assert_eq!(firefox_entries.len(), 1);

    if let Some(v) = orig_data_dirs {
        unsafe {
            std::env::set_var("XDG_DATA_DIRS", v);
        }
    } else {
        unsafe {
            std::env::remove_var("XDG_DATA_DIRS");
        }
    }
    let _ = std::fs::remove_dir_all(dir1.parent().unwrap());
    let _ = std::fs::remove_dir_all(dir2.parent().unwrap());
}

#[test]
fn test_fuzzy_subsequence_fallback() {
    let entries = vec![
        make_entry("Blender", "blender"),
        make_entry("Firefox", "firefox"),
    ];
    let usage = HashMap::new();
    let results = s(&entries, "blnr", &usage, 6);
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].entry.name, "Blender");
}

#[test]
fn test_metadata_matching() {
    let mut e = make_entry("Code", "code");
    e.keywords = vec!["editor".to_string(), "programming".to_string()];
    let entries = vec![e];
    let usage = HashMap::new();
    let results = s(&entries, "editor", &usage, 6);
    assert!(!results.is_empty());
}

#[test]
fn test_identifier_matching() {
    let entries = vec![make_entry("Firefox", "org.mozilla.firefox")];
    let usage = HashMap::new();
    let results = s(&entries, "org.mozilla", &usage, 6);
    assert!(!results.is_empty());
}

#[test]
fn test_empty_query_returns_empty() {
    let entries = vec![make_entry("Firefox", "firefox")];
    let usage = HashMap::new();
    let results = s(&entries, "", &usage, 6);
    assert!(results.is_empty());
}

#[test]
fn test_malformed_desktop_files_dont_crash() {
    let _environment_guard = environment_lock();
    let dir = std::env::temp_dir().join("spotlight-test-malformed");
    let _ = std::fs::remove_dir_all(&dir);
    std::fs::create_dir_all(&dir).unwrap();
    std::fs::write(dir.join("broken.desktop"), "not a valid desktop file").unwrap();
    std::fs::write(dir.join("empty.desktop"), "").unwrap();
    std::fs::write(
        dir.join("no_type.desktop"),
        "[Desktop Entry]\nType=Application\nName=Test\nExec=test\n",
    )
    .unwrap();
    let orig = std::env::var("XDG_DATA_HOME").ok();
    unsafe {
        std::env::set_var("XDG_DATA_HOME", dir.parent().unwrap());
    }
    let mut index = desktop_entries::DesktopEntryIndex::new();
    index.reload();
    if let Some(v) = orig {
        unsafe {
            std::env::set_var("XDG_DATA_HOME", v);
        }
    } else {
        unsafe {
            std::env::remove_var("XDG_DATA_HOME");
        }
    }
    let _ = std::fs::remove_dir_all(&dir);
}

#[test]
fn test_atomic_usage_persistence() {
    let dir = std::env::temp_dir().join("spotlight-test-usage");
    let _ = std::fs::remove_dir_all(&dir);
    std::fs::create_dir_all(dir.join("state/Astrea")).unwrap();
    let root = dir.to_str().unwrap();

    let _ = std::fs::write(dir.join("state/Astrea/spotlight-usage.json"), "{}");

    usage::record_launch(root, "firefox").unwrap();
    let loaded = usage::load_usage(root);
    assert_eq!(loaded.get("firefox"), Some(&1));

    usage::record_launch(root, "firefox").unwrap();
    let loaded = usage::load_usage(root);
    assert_eq!(loaded.get("firefox"), Some(&2));

    let _ = std::fs::remove_dir_all(&dir);
}

#[test]
fn test_alphabetical_tiebreaker() {
    let mut e1 = make_entry("Brave", "brave");
    e1.keywords = vec!["browser".to_string()];
    let mut e2 = make_entry("Chrome", "chrome");
    e2.keywords = vec!["browser".to_string()];
    let mut usage = HashMap::new();
    usage.insert("brave".to_string(), 0);
    usage.insert("chrome".to_string(), 0);
    let entries = vec![e1, e2];
    let results = s(&entries, "browser", &usage, 6);
    assert_eq!(results.len(), 2);
    assert_eq!(results[0].entry.name, "Brave");
}

#[test]
fn test_multi_term_matching() {
    let entries = vec![
        make_entry("Visual Studio Code", "vscode"),
        make_entry("Visual Studio", "vs"),
    ];
    let usage = HashMap::new();
    let results = s(&entries, "visual code", &usage, 6);
    assert!(!results.is_empty());
    assert_eq!(results[0].entry.name, "Visual Studio Code");
}

#[test]
fn test_accent_insensitive_matching() {
    let entries = vec![make_entry("Configuração", "config")];
    let usage = HashMap::new();
    let results = s(&entries, "configuracao", &usage, 6);
    assert!(
        !results.is_empty(),
        "configuracao should match Configuração"
    );
}

#[test]
fn test_panic_containment() {
    let result = std::panic::catch_unwind(|| {
        let _ = ranking::search_entries(&[], "\0", &HashMap::new(), 6);
    });
    assert!(result.is_ok());
}

#[test]
fn test_ffi_create_destroy_stress() {
    let dir = std::env::temp_dir().join("spotlight-test-ffi-stress");
    let _ = std::fs::remove_dir_all(&dir);
    std::fs::create_dir_all(&dir).unwrap();

    let root = CString::new(dir.to_str().unwrap()).unwrap();
    let locale = CString::new("en_US").unwrap();

    for i in 0..1000 {
        let mut error: *mut c_char = std::ptr::null_mut();
        let backend = unsafe {
            crate::astrea_spotlight_backend_create(root.as_ptr(), locale.as_ptr(), &mut error)
        };
        assert!(!backend.is_null(), "create failed at iteration {i}");

        let q = CString::new("test").unwrap();
        let mut search_error: *mut c_char = std::ptr::null_mut();
        let result = unsafe {
            crate::astrea_spotlight_backend_search_json(backend, q.as_ptr(), 6, &mut search_error)
        };
        if !result.is_null() {
            unsafe { crate::astrea_spotlight_backend_free_string(result) };
        }
        if !search_error.is_null() {
            unsafe { crate::astrea_spotlight_backend_free_string(search_error) };
        }

        let mut reload_error: *mut c_char = std::ptr::null_mut();
        let _ret = unsafe { crate::astrea_spotlight_backend_reload(backend, &mut reload_error) };
        if !reload_error.is_null() {
            unsafe { crate::astrea_spotlight_backend_free_string(reload_error) };
        }

        unsafe { crate::astrea_spotlight_backend_destroy(backend) };
    }

    let _ = std::fs::remove_dir_all(&dir);
}

#[test]
fn test_ffi_null_backend_is_safe() {
    unsafe {
        crate::astrea_spotlight_backend_destroy(std::ptr::null_mut());
    }
    let mut error: *mut c_char = std::ptr::null_mut();
    let ret = unsafe { crate::astrea_spotlight_backend_reload(std::ptr::null_mut(), &mut error) };
    assert_eq!(ret, -1);
    assert!(!error.is_null());
    unsafe { crate::astrea_spotlight_backend_free_string(error) };
}

#[test]
fn test_desktop_id_is_populated() {
    let _environment_guard = environment_lock();
    let dir = std::env::temp_dir().join("spotlight-test-id");
    let _ = std::fs::remove_dir_all(&dir);
    std::fs::create_dir_all(dir.join("applications")).unwrap();
    std::fs::write(
        dir.join("applications/org.mozilla.firefox.desktop"),
        "[Desktop Entry]\nType=Application\nName=Firefox\nExec=firefox\n",
    )
    .unwrap();
    let orig = std::env::var("XDG_DATA_HOME").ok();
    unsafe {
        std::env::set_var("XDG_DATA_HOME", dir.to_str().unwrap());
    }
    let mut index = desktop_entries::DesktopEntryIndex::new();
    index.reload();
    let entries = index.entries();
    assert!(!entries.is_empty());
    assert_eq!(entries[0].id, "org.mozilla.firefox");
    if let Some(v) = orig {
        unsafe {
            std::env::set_var("XDG_DATA_HOME", v);
        }
    } else {
        unsafe {
            std::env::remove_var("XDG_DATA_HOME");
        }
    }
    let _ = std::fs::remove_dir_all(&dir);
}

#[test]
fn test_locale_priority_exact_then_language_then_unlocalized() {
    let _environment_guard = environment_lock();
    let dir = std::env::temp_dir().join("spotlight-test-locale");
    let _ = std::fs::remove_dir_all(&dir);
    std::fs::create_dir_all(dir.join("applications")).unwrap();
    std::fs::write(
        dir.join("applications/settings.desktop"),
        "[Desktop Entry]\nType=Application\nName=Settings\nName[pt]=Configurações\nName[pt_BR]=Configurações do Sistema\nExec=settings\n",
    )
    .unwrap();

    let orig_home = std::env::var("XDG_DATA_HOME").ok();
    let orig_dirs = std::env::var("XDG_DATA_DIRS").ok();
    unsafe {
        std::env::set_var("XDG_DATA_HOME", dir.to_str().unwrap());
        std::env::set_var("XDG_DATA_DIRS", dir.to_str().unwrap());
    }

    let mut exact = desktop_entries::DesktopEntryIndex::new_with_locale("pt_BR.UTF-8");
    exact.reload();
    assert_eq!(exact.entries()[0].name, "Configurações do Sistema");

    let mut language_only = desktop_entries::DesktopEntryIndex::new_with_locale("pt_PT");
    language_only.reload();
    assert_eq!(language_only.entries()[0].name, "Configurações");

    let mut fallback = desktop_entries::DesktopEntryIndex::new_with_locale("fr_FR");
    fallback.reload();
    assert_eq!(fallback.entries()[0].name, "Settings");

    if let Some(v) = orig_home {
        unsafe {
            std::env::set_var("XDG_DATA_HOME", v);
        }
    } else {
        unsafe {
            std::env::remove_var("XDG_DATA_HOME");
        }
    }
    if let Some(v) = orig_dirs {
        unsafe {
            std::env::set_var("XDG_DATA_DIRS", v);
        }
    } else {
        unsafe {
            std::env::remove_var("XDG_DATA_DIRS");
        }
    }
    let _ = std::fs::remove_dir_all(&dir);
}

#[test]
fn test_external_catalog_selects_locale_and_localized_keywords() {
    let entry = DesktopEntry {
        id: "localized".to_string(),
        name: "Base Name".to_string(),
        generic_name: "Base Generic".to_string(),
        comment: "Base Comment".to_string(),
        keywords: vec!["base-keyword".to_string()],
        localized_names: HashMap::from([
            ("pt".to_string(), "Nome Português".to_string()),
            ("pt_BR".to_string(), "Nome Brasil".to_string()),
            ("sr_RS@latin".to_string(), "Ime Srbija Latin".to_string()),
            ("sr_RS".to_string(), "Ime Srbija".to_string()),
            ("sr@latin".to_string(), "Ime Latin".to_string()),
        ]),
        localized_generic_names: HashMap::from([(
            "pt_BR".to_string(),
            "Genérico Brasil".to_string(),
        )]),
        localized_comments: HashMap::from([("pt_BR".to_string(), "Comentário Brasil".to_string())]),
        localized_keywords: HashMap::from([
            ("pt".to_string(), vec!["palavra-portuguesa".to_string()]),
            ("pt_BR".to_string(), vec!["palavra-brasil".to_string()]),
            ("sr_RS@latin".to_string(), vec!["kljuc-latin".to_string()]),
        ]),
        ..Default::default()
    };

    let root = temporary_test_path("locale");
    let pt_br = crate::AstreaSpotlightBackend::new_with_catalog(
        root.to_str().unwrap(),
        "pt_BR.UTF-8",
        vec![entry.clone()],
    )
    .unwrap();
    assert!(search_has_id(&pt_br, "Nome Brasil", "localized"));
    assert!(search_has_id(&pt_br, "palavra brasil", "localized"));

    let pt_pt = crate::AstreaSpotlightBackend::new_with_catalog(
        root.to_str().unwrap(),
        "pt_PT.UTF-8",
        vec![entry.clone()],
    )
    .unwrap();
    assert!(search_has_id(&pt_pt, "Nome Português", "localized"));
    assert!(search_has_id(&pt_pt, "palavra portuguesa", "localized"));

    let sr = crate::AstreaSpotlightBackend::new_with_catalog(
        root.to_str().unwrap(),
        "sr_RS.UTF-8@latin",
        vec![entry.clone()],
    )
    .unwrap();
    assert!(search_has_id(&sr, "Ime Srbija Latin", "localized"));
    assert!(search_has_id(&sr, "kljuc latin", "localized"));

    let unknown = crate::AstreaSpotlightBackend::new_with_catalog(
        root.to_str().unwrap(),
        "zz_ZZ.UTF-8",
        vec![entry],
    )
    .unwrap();
    assert!(search_has_id(&unknown, "Base Name", "localized"));
    assert!(search_has_id(&unknown, "base keyword", "localized"));

    let _ = std::fs::remove_dir_all(root);
}

#[test]
fn test_external_catalog_applies_visibility_policy_without_tombstoning_records() {
    let _environment_guard = environment_lock();
    let previous_desktop = std::env::var_os("XDG_CURRENT_DESKTOP");
    unsafe {
        std::env::set_var("XDG_CURRENT_DESKTOP", "Astrea:Wayland");
    }

    let mut no_display = external_entry("No Display", "no-display");
    no_display.no_display = true;
    let mut allowed = external_entry("Allowed", "allowed");
    allowed.only_show_in = vec!["Wayland".to_string()];
    let mut rejected = external_entry("Rejected", "rejected");
    rejected.only_show_in = vec!["GNOME".to_string()];
    let mut excluded = external_entry("Excluded", "excluded");
    excluded.not_show_in = vec!["Astrea".to_string()];
    let mut both = external_entry("Both", "both");
    both.only_show_in = vec!["Astrea".to_string()];
    both.not_show_in = vec!["Wayland".to_string()];

    let root = temporary_test_path("visibility");
    let backend = crate::AstreaSpotlightBackend::new_with_catalog(
        root.to_str().unwrap(),
        "en_US",
        vec![no_display, allowed, rejected, excluded, both],
    )
    .unwrap();
    assert!(!search_has_id(&backend, "no display", "no-display"));
    assert!(search_has_id(&backend, "allowed", "allowed"));
    assert!(!search_has_id(&backend, "rejected", "rejected"));
    assert!(!search_has_id(&backend, "excluded", "excluded"));
    assert!(!search_has_id(&backend, "both", "both"));

    match previous_desktop {
        Some(value) => unsafe { std::env::set_var("XDG_CURRENT_DESKTOP", value) },
        None => unsafe { std::env::remove_var("XDG_CURRENT_DESKTOP") },
    }
    let _ = std::fs::remove_dir_all(root);
}

#[test]
fn test_external_catalog_requires_executable_try_exec() {
    let _environment_guard = environment_lock();
    let root = temporary_test_path("try-exec");
    let bin = root.join("bin");
    std::fs::create_dir_all(&bin).unwrap();
    let absolute_executable = root.join("absolute-executable");
    let absolute_non_executable = root.join("absolute-non-executable");
    let path_executable = bin.join("path-executable");
    for path in [
        &absolute_executable,
        &absolute_non_executable,
        &path_executable,
    ] {
        std::fs::write(path, "#!/bin/sh\n").unwrap();
    }
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        std::fs::set_permissions(&absolute_executable, std::fs::Permissions::from_mode(0o755))
            .unwrap();
        std::fs::set_permissions(&path_executable, std::fs::Permissions::from_mode(0o755)).unwrap();
        std::fs::set_permissions(
            &absolute_non_executable,
            std::fs::Permissions::from_mode(0o644),
        )
        .unwrap();
    }

    let previous_path = std::env::var_os("PATH");
    unsafe {
        std::env::set_var("PATH", bin.as_os_str());
    }

    let mut absolute_ok = external_entry("Absolute OK", "absolute-ok");
    absolute_ok.try_exec = absolute_executable.to_string_lossy().into_owned();
    let mut absolute_missing = external_entry("Absolute Missing", "absolute-missing");
    absolute_missing.try_exec = root.join("missing").to_string_lossy().into_owned();
    let mut absolute_bad_mode = external_entry("Absolute Bad Mode", "absolute-bad-mode");
    absolute_bad_mode.try_exec = absolute_non_executable.to_string_lossy().into_owned();
    let mut path_ok = external_entry("PATH OK", "path-ok");
    path_ok.try_exec = "path-executable".to_string();
    let mut path_missing = external_entry("PATH Missing", "path-missing");
    path_missing.try_exec = "path-missing-command".to_string();

    let backend = crate::AstreaSpotlightBackend::new_with_catalog(
        root.to_str().unwrap(),
        "en_US",
        vec![
            absolute_ok,
            absolute_missing,
            absolute_bad_mode,
            path_ok,
            path_missing,
        ],
    )
    .unwrap();
    assert!(search_has_id(&backend, "absolute ok", "absolute-ok"));
    assert!(!search_has_id(
        &backend,
        "absolute missing",
        "absolute-missing"
    ));
    assert!(!search_has_id(
        &backend,
        "absolute bad mode",
        "absolute-bad-mode"
    ));
    assert!(search_has_id(&backend, "path ok", "path-ok"));
    assert!(!search_has_id(&backend, "path missing", "path-missing"));

    match previous_path {
        Some(value) => unsafe { std::env::set_var("PATH", value) },
        None => unsafe { std::env::remove_var("PATH") },
    }
    let _ = std::fs::remove_dir_all(root);
}

#[test]
fn test_external_catalog_replacement_and_reload_keep_external_projection() {
    let _environment_guard = environment_lock();
    let root = temporary_test_path("replacement");
    let xdg_home = root.join("xdg");
    std::fs::create_dir_all(xdg_home.join("applications")).unwrap();
    std::fs::write(
        xdg_home.join("applications/internal.desktop"),
        "[Desktop Entry]\nType=Application\nName=Internal Scanner\n",
    )
    .unwrap();
    let previous_xdg_home = std::env::var_os("XDG_DATA_HOME");
    let previous_xdg_dirs = std::env::var_os("XDG_DATA_DIRS");
    unsafe {
        std::env::set_var("XDG_DATA_HOME", &xdg_home);
        std::env::set_var("XDG_DATA_DIRS", &xdg_home);
    }

    let a = external_entry("Catalog A", "catalog-a");
    let b = external_entry("Catalog B", "catalog-b");
    let mut backend =
        crate::AstreaSpotlightBackend::new_with_catalog(root.to_str().unwrap(), "en_US", vec![a])
            .unwrap();
    assert!(search_has_id(&backend, "catalog a", "catalog-a"));
    assert!(!search_has_id(&backend, "internal scanner", "internal"));

    backend.set_catalog(vec![b]);
    assert!(!search_has_id(&backend, "catalog a", "catalog-a"));
    assert!(search_has_id(&backend, "catalog b", "catalog-b"));
    backend.reload().unwrap();
    assert!(!search_has_id(&backend, "catalog a", "catalog-a"));
    assert!(search_has_id(&backend, "catalog b", "catalog-b"));

    match previous_xdg_home {
        Some(value) => unsafe { std::env::set_var("XDG_DATA_HOME", value) },
        None => unsafe { std::env::remove_var("XDG_DATA_HOME") },
    }
    match previous_xdg_dirs {
        Some(value) => unsafe { std::env::set_var("XDG_DATA_DIRS", value) },
        None => unsafe { std::env::remove_var("XDG_DATA_DIRS") },
    }
    let _ = std::fs::remove_dir_all(root);
}
