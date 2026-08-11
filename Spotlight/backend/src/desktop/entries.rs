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
    #[serde(default)]
    pub localized_names: HashMap<String, String>,
    #[serde(default)]
    pub localized_generic_names: HashMap<String, String>,
    #[serde(default)]
    pub localized_comments: HashMap<String, String>,
    #[serde(default)]
    pub localized_keywords: HashMap<String, Vec<String>>,
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
    let (base, modifier) = locale.split_once('@').unwrap_or((locale, ""));
    let base_without_encoding = base.split('.').next().unwrap_or(base);
    let normalized_base = base_without_encoding.replace('-', "_");
    if modifier.is_empty() {
        normalized_base
    } else {
        format!("{normalized_base}@{modifier}")
    }
}

pub fn locale_priority_from_astrea(locale: &str) -> Vec<String> {
    let normalized = normalize_locale(locale.trim());
    if normalized.is_empty() {
        return Vec::new();
    }

    let (base, modifier) = normalized.split_once('@').unwrap_or((&normalized, ""));
    let language = base.split('_').next().unwrap_or(base);
    let mut priority = Vec::new();
    let mut append = |candidate: String| {
        if !candidate.is_empty() && !priority.contains(&candidate) {
            priority.push(candidate);
        }
    };
    if !modifier.is_empty() {
        append(format!("{base}@{modifier}"));
    } else {
        append(base.to_string());
    }
    if !modifier.is_empty() {
        append(base.to_string());
        if language != base {
            append(format!("{language}@{modifier}"));
        }
    }
    if language != base {
        append(language.to_string());
    }
    priority
}

fn locale_value_matches(key: &str, priority: &str) -> bool {
    normalize_locale(key) == priority
}

fn select_best_locale(candidates: &HashMap<String, String>, priority: &[String]) -> Option<String> {
    for loc in priority {
        if let Some(val) = candidates.get(loc) {
            return Some(val.clone());
        }
        let mut keys: Vec<&String> = candidates.keys().collect();
        keys.sort();
        for key in keys {
            if locale_value_matches(key, loc) {
                return candidates.get(key).cloned();
            }
        }
    }
    candidates.get("").cloned()
}

fn select_best_locale_for_keywords(
    candidates: &HashMap<String, Vec<String>>,
    priority: &[String],
) -> Option<Vec<String>> {
    for loc in priority {
        if let Some(val) = candidates.get(loc) {
            return Some(val.clone());
        }
        let mut keys: Vec<&String> = candidates.keys().collect();
        keys.sort();
        for key in keys {
            if locale_value_matches(key, loc) {
                return candidates.get(key).cloned();
            }
        }
    }
    candidates.get("").cloned()
}

fn is_executable_regular_file(path: &Path) -> bool {
    let Ok(metadata) = fs::metadata(path) else {
        return false;
    };
    if !metadata.is_file() {
        return false;
    }
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        metadata.permissions().mode() & 0o111 != 0
    }
    #[cfg(not(unix))]
    {
        true
    }
}

fn resolve_try_exec(command: &str) -> Option<PathBuf> {
    let candidate = Path::new(command);
    if candidate.is_absolute() {
        return is_executable_regular_file(candidate).then(|| candidate.to_path_buf());
    }

    let paths = std::env::var_os("PATH")?;
    std::env::split_paths(&paths)
        .map(|directory| directory.join(command))
        .find(|path| is_executable_regular_file(path))
}

fn current_desktop_tokens() -> Vec<String> {
    std::env::var("XDG_CURRENT_DESKTOP")
        .unwrap_or_default()
        .split(':')
        .map(str::trim)
        .filter(|token| !token.is_empty())
        .map(ToOwned::to_owned)
        .collect()
}

fn desktop_token_matches(tokens: &[String], candidate: &str) -> bool {
    tokens.iter().any(|token| token == candidate)
}

pub fn project_external_catalog(entries: &[DesktopEntry], locale: &str) -> Vec<DesktopEntry> {
    let priority = locale_priority_from_astrea(locale);
    let desktop_tokens = current_desktop_tokens();

    entries
        .iter()
        .filter_map(|entry| {
            if entry.hidden || entry.no_display {
                return None;
            }
            if !entry.only_show_in.is_empty()
                && !entry
                    .only_show_in
                    .iter()
                    .any(|candidate| desktop_token_matches(&desktop_tokens, candidate))
            {
                return None;
            }
            if entry
                .not_show_in
                .iter()
                .any(|candidate| desktop_token_matches(&desktop_tokens, candidate))
            {
                return None;
            }
            if !entry.try_exec.is_empty() && resolve_try_exec(&entry.try_exec).is_none() {
                return None;
            }

            let mut projected = entry.clone();
            if let Some(name) = select_best_locale(&entry.localized_names, &priority) {
                projected.name = name;
            }
            if let Some(generic_name) =
                select_best_locale(&entry.localized_generic_names, &priority)
            {
                projected.generic_name = generic_name;
            }
            if let Some(comment) = select_best_locale(&entry.localized_comments, &priority) {
                projected.comment = comment;
            }
            if let Some(keywords) =
                select_best_locale_for_keywords(&entry.localized_keywords, &priority)
            {
                projected.keywords = keywords;
            }
            Some(projected)
        })
        .collect()
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
                let kw = unescape_list_field(&raw_value);
                entry.keywords.clone_from(&kw);
                locale_keywords.insert(String::new(), kw);
            }
            "Categories" => {
                entry.categories = unescape_list_field(&raw_value);
            }
            "OnlyShowIn" => {
                entry.only_show_in = unescape_list_field(&raw_value);
            }
            "NotShowIn" => {
                entry.not_show_in = unescape_list_field(&raw_value);
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
                    locale_keywords.insert(lang.to_string(), unescape_list_field(&raw_value));
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
        resolve_try_exec(v)?;
    }

    Some(entry)
}

fn collect_desktop_files(dir: &Path, max_depth: usize) -> Vec<PathBuf> {
    let mut files = Vec::new();
    let mut dirs_to_scan = vec![(dir.to_path_buf(), 0)];
    let mut visited = HashSet::new();

    while let Some((current, depth)) = dirs_to_scan.pop() {
        if depth > max_depth || !visited.insert(current.canonicalize().unwrap_or_default()) {
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
        let current_desktop = current_desktop_tokens();

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
                        && !de
                            .only_show_in
                            .iter()
                            .any(|s| desktop_token_matches(&current_desktop, s))
                    {
                        tombstones.insert(key);
                        continue;
                    }
                    if !de.not_show_in.is_empty()
                        && de
                            .not_show_in
                            .iter()
                            .any(|s| desktop_token_matches(&current_desktop, s))
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
