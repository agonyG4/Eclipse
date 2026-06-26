use crate::desktop_entries::{self, *};
use crate::ranking;
use crate::usage;
use std::collections::HashMap;
use std::ffi::CString;
use std::os::raw::c_char;

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
    }
    let _ = std::fs::remove_dir_all(&dir);
}

#[test]
fn test_locale_priority_exact_then_language_then_unlocalized() {
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
    }
    if let Some(v) = orig_dirs {
        unsafe {
            std::env::set_var("XDG_DATA_DIRS", v);
        }
    }
    let _ = std::fs::remove_dir_all(&dir);
}
