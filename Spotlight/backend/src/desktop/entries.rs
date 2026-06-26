use serde::{Deserialize, Serialize};
use std::collections::{HashMap, HashSet};
use std::fs;
use std::path::{Path, PathBuf};

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct DesktopEntry {
    pub id: String,
    pub name: String,
    pub generic_name: String,
    pub comment: String,
    pub icon: String,
    pub exec: String,
    pub try_exec: String,
    pub keywords: Vec<String>,
    pub categories: Vec<String>,
    pub path: String,
    pub startup_wm_class: String,
    pub desktop_file_name: String,
    pub terminal: bool,
    pub hidden: bool,
    pub no_display: bool,
    pub only_show_in: Vec<String>,
    pub not_show_in: Vec<String>,
}

impl DesktopEntry {
    pub fn dedup_key(&self) -> String {
        if !self.id.is_empty() {
            return self.id.clone();
        }
        if !self.desktop_file_name.is_empty() {
            return self.desktop_file_name.clone();
        }
        format!("{}|{}", self.name, self.exec)
    }
}

fn xdg_data_dirs() -> Vec<PathBuf> {
    let mut dirs = Vec::new();
    if let Ok(home) = std::env::var("HOME") {
        let data_home =
            std::env::var("XDG_DATA_HOME").unwrap_or_else(|_| format!("{}/.local/share", home));
        dirs.push(PathBuf::from(data_home).join("applications"));
    }
    let data_dirs = std::env::var("XDG_DATA_DIRS")
        .unwrap_or_else(|_| "/usr/local/share:/usr/share".to_string());
    for d in data_dirs.split(':') {
        let p = PathBuf::from(d).join("applications");
        if p.exists() {
            dirs.push(p);
        }
    }
    dirs
}

pub fn find_application_dirs() -> Vec<PathBuf> {
    xdg_data_dirs()
}

fn truthy(s: &str) -> bool {
    matches!(s.to_ascii_lowercase().as_str(), "true" | "1")
}

fn unescape_desktop_value(value: &str) -> String {
    let mut result = String::with_capacity(value.len());
    let mut chars = value.chars();
    while let Some(c) = chars.next() {
        if c == '\\' {
            match chars.next() {
                Some('s') => result.push(' '),
                Some('n') => result.push('\n'),
                Some('t') => result.push('\t'),
                Some('r') => result.push('\r'),
                Some('\\') => result.push('\\'),
                Some(';') => result.push(';'),
                Some(x) => result.push(x),
                None => result.push('\\'),
            }
        } else {
            result.push(c);
        }
    }
    result
}

fn unescape_list_field(value: &str) -> Vec<String> {
    let mut result = Vec::new();
    let mut current = String::new();
    let mut chars = value.chars();
    while let Some(c) = chars.next() {
        if c == '\\' {
            match chars.next() {
                Some(';') => current.push(';'),
                Some('s') => current.push(' '),
                Some('n') => current.push('\n'),
                Some('t') => current.push('\t'),
                Some('r') => current.push('\r'),
                Some('\\') => current.push('\\'),
                Some(x) => {
                    current.push('\\');
                    current.push(x);
                }
                None => current.push('\\'),
            }
        } else if c == ';' {
            let trimmed = current.trim().to_string();
            if !trimmed.is_empty() {
                result.push(trimmed);
            }
            current.clear();
        } else {
            current.push(c);
        }
    }
    let trimmed = current.trim().to_string();
    if !trimmed.is_empty() {
        result.push(trimmed);
    }
    result
}

fn normalize_locale(locale: &str) -> String {
    let without_encoding = locale.split('.').next().unwrap_or(locale);
    let without_modifier = without_encoding
        .split('@')
        .next()
        .unwrap_or(without_encoding);
    without_modifier.replace('-', "_")
}

pub fn locale_priority_from_astrea(locale: &str) -> Vec<String> {
    let normalized = normalize_locale(locale.trim());
    if normalized.is_empty() {
        return Vec::new();
    }

    let mut priority = Vec::new();
    priority.push(normalized.clone());
    if let Some(lang) = normalized.split(['_', '-']).next()
        && !lang.is_empty()
        && lang != normalized
    {
        priority.push(lang.to_string());
    }
    priority
}

fn select_best_locale(candidates: &HashMap<String, String>, priority: &[String]) -> Option<String> {
    for loc in priority {
        let normalized = normalize_locale(loc);
        if let Some(val) = candidates.get(&normalized) {
            return Some(val.clone());
        }
        if let Some(lang) = normalized.split(['_', '-']).next()
            && let Some(val) = candidates.get(lang)
        {
            return Some(val.clone());
        }
    }
    candidates.get("").cloned()
}

fn select_best_locale_for_keywords(
    candidates: &HashMap<String, Vec<String>>,
    priority: &[String],
) -> Option<Vec<String>> {
    for loc in priority {
        let normalized = normalize_locale(loc);
        if let Some(val) = candidates.get(&normalized) {
            return Some(val.clone());
        }
        if let Some(lang) = normalized.split(['_', '-']).next()
            && let Some(val) = candidates.get(lang)
        {
            return Some(val.clone());
        }
    }
    candidates.get("").cloned()
}

fn parse_desktop_file(
    path: &Path,
    app_dir: &Path,
    locale_priority: &[String],
) -> Option<DesktopEntry> {
    let text = fs::read_to_string(path).ok()?;
    let mut entry = DesktopEntry::default();
    let stem = path.file_stem()?.to_str()?;
    entry.desktop_file_name = format!("{stem}.desktop");

    if let Ok(rel) = path.strip_prefix(app_dir) {
        let rel_str = rel.to_str()?;
        let without_ext = rel_str.strip_suffix(".desktop").unwrap_or(rel_str);
        entry.id = without_ext.replace('/', "-");
    } else {
        entry.id = stem.to_string();
    }

    let mut locale_priority = locale_priority.to_vec();
    if locale_priority.is_empty() {
        let lc_all = std::env::var("LC_ALL").ok();
        let lc_messages = std::env::var("LC_MESSAGES").ok();
        let lang = std::env::var("LANG").ok();
        locale_priority = lc_all.into_iter().chain(lc_messages).chain(lang).collect();
    }

    let mut locale_names: HashMap<String, String> = HashMap::new();
    let mut locale_generic_names: HashMap<String, String> = HashMap::new();
    let mut locale_comments: HashMap<String, String> = HashMap::new();
    let mut locale_keywords: HashMap<String, Vec<String>> = HashMap::new();

    let mut in_desktop_entry = false;
    for line in text.lines() {
        let line = line.trim();
        if line.starts_with('[') {
            in_desktop_entry = line.eq_ignore_ascii_case("[desktop entry]");
            continue;
        }
        if !in_desktop_entry || line.starts_with('#') || line.is_empty() {
            continue;
        }
        let mut parts = line.splitn(2, '=');
        let key = parts.next()?.trim();
        let raw_value = parts.next()?.trim().to_string();
        let value = unescape_desktop_value(&raw_value);

        match key {
            "Type" if value != "Application" => return None,
            "NoDisplay" => entry.no_display = truthy(&value),
            "Hidden" => entry.hidden = truthy(&value),
            "Terminal" => entry.terminal = truthy(&value),
            "Name" => {
                entry.name.clone_from(&value);
                locale_names.insert(String::new(), value);
            }
            "GenericName" => {
                entry.generic_name.clone_from(&value);
                locale_generic_names.insert(String::new(), value);
            }
            "Comment" => {
                entry.comment.clone_from(&value);
                locale_comments.insert(String::new(), value);
            }
            "Icon" => entry.icon = value,
            "Exec" => entry.exec = value,
            "TryExec" => entry.try_exec = value,
            "Path" => entry.path = value,
            "StartupWMClass" => entry.startup_wm_class = value,
            "Keywords" => {
                let kw = unescape_list_field(&value);
                entry.keywords.clone_from(&kw);
                locale_keywords.insert(String::new(), kw);
            }
            "Categories" => {
                entry.categories = unescape_list_field(&value);
            }
            "OnlyShowIn" => {
                entry.only_show_in = unescape_list_field(&value);
            }
            "NotShowIn" => {
                entry.not_show_in = unescape_list_field(&value);
            }
            _ => {
                if let Some(rest) = key.strip_prefix("Name[") {
                    if let Some(lang) = rest.strip_suffix(']') {
                        locale_names.insert(lang.to_string(), value);
                    }
                } else if let Some(rest) = key.strip_prefix("GenericName[") {
                    if let Some(lang) = rest.strip_suffix(']') {
                        locale_generic_names.insert(lang.to_string(), value);
                    }
                } else if let Some(rest) = key.strip_prefix("Comment[") {
                    if let Some(lang) = rest.strip_suffix(']') {
                        locale_comments.insert(lang.to_string(), value);
                    }
                } else if let Some(rest) = key.strip_prefix("Keywords[")
                    && let Some(lang) = rest.strip_suffix(']')
                {
                    locale_keywords.insert(lang.to_string(), unescape_list_field(&value));
                }
            }
        }
    }

    if !locale_priority.is_empty() {
        if let Some(name) = select_best_locale(&locale_names, &locale_priority) {
            entry.name = name;
        }
        if let Some(generic_name) = select_best_locale(&locale_generic_names, &locale_priority) {
            entry.generic_name = generic_name;
        }
        if let Some(comment) = select_best_locale(&locale_comments, &locale_priority) {
            entry.comment = comment;
        }
        if let Some(keywords) = select_best_locale_for_keywords(&locale_keywords, &locale_priority)
        {
            entry.keywords = keywords;
        }
    }

    if entry.name.is_empty() {
        return None;
    }

    if !entry.try_exec.is_empty() {
        let v = &entry.try_exec;
        let v_path = if v.starts_with('/') {
            PathBuf::from(v)
        } else {
            which(v).unwrap_or_default()
        };
        if v_path.as_os_str().is_empty() || !v_path.exists() {
            return None;
        }
    }

    Some(entry)
}

fn which(cmd: &str) -> Option<PathBuf> {
    std::env::var_os("PATH").and_then(|paths| {
        std::env::split_paths(&paths).find_map(|dir| {
            let full = dir.join(cmd);
            if full.is_file() { Some(full) } else { None }
        })
    })
}

#[derive(Debug, Clone, PartialEq)]
pub enum DesktopEnvironment {
    Hyprland,
    GNOME,
    KDE,
    Unity,
    Sway,
    XFCE,
    MATE,
    Cinnamon,
    LXDE,
    Budgie,
    Other,
}

impl DesktopEnvironment {
    fn current_tokens() -> Vec<String> {
        let xdg = std::env::var("XDG_CURRENT_DESKTOP").unwrap_or_default();
        xdg.split(':')
            .map(|s| s.trim().to_string())
            .filter(|s| !s.is_empty())
            .collect()
    }

    fn current() -> Self {
        let tokens = Self::current_tokens();
        if tokens.is_empty() {
            Self::Other
        } else {
            match tokens[0].as_str() {
                "Hyprland" => Self::Hyprland,
                "GNOME" => Self::GNOME,
                "KDE" => Self::KDE,
                "Unity" => Self::Unity,
                "Sway" => Self::Sway,
                "XFCE" => Self::XFCE,
                "MATE" => Self::MATE,
                "Cinnamon" => Self::Cinnamon,
                "LXDE" => Self::LXDE,
                "Budgie" => Self::Budgie,
                _ => Self::Other,
            }
        }
    }

    fn matches(&self, desktop_name: &str) -> bool {
        let xdg = std::env::var("XDG_CURRENT_DESKTOP").unwrap_or_default();
        xdg.split(':')
            .any(|de| de.trim().eq_ignore_ascii_case(desktop_name))
    }
}

fn collect_desktop_files(dir: &Path, max_depth: usize) -> Vec<PathBuf> {
    let mut files = Vec::new();
    let mut dirs_to_scan = vec![(dir.to_path_buf(), 0)];
    let mut visited = HashSet::new();

    while let Some((current, depth)) = dirs_to_scan.pop() {
        if depth >= max_depth || !visited.insert(current.canonicalize().unwrap_or_default()) {
            continue;
        }
        if let Ok(read) = fs::read_dir(&current) {
            for entry in read.filter_map(|e| e.ok()) {
                let path = entry.path();
                if path.is_dir() && !path.is_symlink() {
                    dirs_to_scan.push((path, depth + 1));
                } else if path.extension().and_then(|s| s.to_str()) == Some("desktop")
                    && files.len() < 10000
                {
                    files.push(path);
                }
            }
        }
    }
    files
}

pub struct DesktopEntryIndex {
    entries: Vec<DesktopEntry>,
    watcher_dirs: Vec<PathBuf>,
    locale_priority: Vec<String>,
}

impl Default for DesktopEntryIndex {
    fn default() -> Self {
        Self::new()
    }
}

impl DesktopEntryIndex {
    pub fn new() -> Self {
        Self {
            entries: Vec::new(),
            watcher_dirs: Vec::new(),
            locale_priority: Vec::new(),
        }
    }

    pub fn new_with_locale(locale: &str) -> Self {
        Self {
            entries: Vec::new(),
            watcher_dirs: Vec::new(),
            locale_priority: locale_priority_from_astrea(locale),
        }
    }

    pub fn set_locale(&mut self, locale: &str) {
        self.locale_priority = locale_priority_from_astrea(locale);
    }

    pub fn reload(&mut self) {
        let mut entries = Vec::new();
        let mut seen = HashSet::new();
        let mut tombstones = HashSet::new();
        let dirs = find_application_dirs();
        self.watcher_dirs = dirs.clone();
        let current_de = DesktopEnvironment::current();

        for dir in &dirs {
            if !dir.exists() {
                continue;
            }
            for path in collect_desktop_files(dir, 5) {
                if let Some(de) = parse_desktop_file(&path, dir, &self.locale_priority) {
                    let key = de.dedup_key();
                    if tombstones.contains(&key) {
                        continue;
                    }
                    if seen.contains(&key) {
                        continue;
                    }
                    if de.hidden || de.no_display {
                        tombstones.insert(key);
                        continue;
                    }
                    if !de.only_show_in.is_empty()
                        && !de.only_show_in.iter().any(|s| current_de.matches(s))
                    {
                        tombstones.insert(key);
                        continue;
                    }
                    if !de.not_show_in.is_empty()
                        && de.not_show_in.iter().any(|s| current_de.matches(s))
                    {
                        tombstones.insert(key);
                        continue;
                    }
                    seen.insert(key);
                    entries.push(de);
                }
            }
        }

        self.entries = entries;
    }

    pub fn entries(&self) -> &[DesktopEntry] {
        &self.entries
    }

    pub fn watcher_dirs(&self) -> &[PathBuf] {
        &self.watcher_dirs
    }
}
