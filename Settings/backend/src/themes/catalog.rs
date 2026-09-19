use std::collections::{HashMap, HashSet};
use std::fmt::{Display, Formatter};
use std::fs;
use std::path::{Path, PathBuf};

const MAX_THEME_ENTRIES: usize = 4096;
const MAX_INDEX_BYTES: u64 = 1024 * 1024;

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum DirectoryType {
    Fixed,
    Scalable,
    Threshold,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct DirectorySpec {
    pub path: String,
    pub size: u32,
    pub min_size: u32,
    pub max_size: u32,
    pub threshold: u32,
    pub scale: u32,
    pub kind: DirectoryType,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ThemeDescriptor {
    pub id: String,
    pub name: String,
    pub comment: String,
    pub inherits: Vec<String>,
    pub directories: Vec<DirectorySpec>,
    pub roots: Vec<PathBuf>,
    pub example: Option<String>,
    pub user: bool,
}

#[derive(Clone, Debug, Default)]
pub struct ThemeCatalog {
    themes: Vec<ThemeDescriptor>,
    indexes: HashMap<String, usize>,
}

#[derive(Debug)]
pub enum ThemeError {
    Io { path: PathBuf, message: String },
    InvalidMetadata { path: PathBuf, message: String },
}

impl Display for ThemeError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Io { path, message } => write!(formatter, "{}: {}", path.display(), message),
            Self::InvalidMetadata { path, message } => {
                write!(formatter, "{}: {}", path.display(), message)
            }
        }
    }
}

impl std::error::Error for ThemeError {}

impl ThemeCatalog {
    pub fn discover(search_roots: &[PathBuf]) -> Result<Self, ThemeError> {
        let mut catalog = Self::default();
        let mut first_hidden = HashSet::new();
        for (root_index, root) in search_roots.iter().enumerate() {
            let entries = match fs::read_dir(root) {
                Ok(entries) => entries,
                Err(error) if error.kind() == std::io::ErrorKind::NotFound => continue,
                Err(error) => {
                    return Err(ThemeError::Io {
                        path: root.clone(),
                        message: error.to_string(),
                    });
                }
            };
            for entry in entries.flatten().take(MAX_THEME_ENTRIES) {
                let path = entry.path();
                if !entry.file_type().is_ok_and(|kind| kind.is_dir()) {
                    continue;
                }
                let id = entry.file_name().to_string_lossy().into_owned();
                if !valid_theme_id(&id) {
                    continue;
                }
                let index_path = path.join("index.theme");
                if !index_path.is_file() {
                    continue;
                }
                let metadata = match parse_theme(&index_path, &id) {
                    Ok(metadata) => metadata,
                    Err(error) => {
                        if root_index_is_malformed(&error) {
                            continue;
                        }
                        return Err(error);
                    }
                };

                if metadata.hidden && id != "hicolor" {
                    if root_index_is_first(&catalog, &id) {
                        first_hidden.insert(id.clone());
                    }
                    continue;
                }
                if first_hidden.contains(&id) {
                    continue;
                }

                let user = root_index == 0 || is_user_path(root);
                catalog.merge(id, path, metadata, user);
            }
        }
        Ok(catalog)
    }

    pub fn theme(&self, id: &str) -> Option<&ThemeDescriptor> {
        self.indexes
            .get(id)
            .and_then(|index| self.themes.get(*index))
    }

    pub fn user_visible(&self) -> Vec<&ThemeDescriptor> {
        self.themes
            .iter()
            .filter(|theme| theme.id != "hicolor")
            .collect()
    }

    pub fn themes(&self) -> &[ThemeDescriptor] {
        &self.themes
    }

    pub fn contains_visible(&self, id: &str) -> bool {
        id != "hicolor" && self.theme(id).is_some()
    }

    fn merge(&mut self, id: String, root: PathBuf, metadata: ParsedTheme, user: bool) {
        if let Some(index) = self.indexes.get(&id).copied() {
            let descriptor = &mut self.themes[index];
            if !descriptor.roots.contains(&root) {
                descriptor.roots.push(root);
            }
            for directory in metadata.directories {
                if !descriptor.directories.contains(&directory) {
                    descriptor.directories.push(directory);
                }
            }
            return;
        }

        let descriptor = ThemeDescriptor {
            id: id.clone(),
            name: metadata.name,
            comment: metadata.comment,
            inherits: metadata.inherits,
            directories: metadata.directories,
            roots: vec![root],
            example: metadata.example,
            user,
        };
        self.indexes.insert(id, self.themes.len());
        self.themes.push(descriptor);
    }
}

pub fn default_search_roots() -> Vec<PathBuf> {
    let home = std::env::var_os("HOME")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from("/"));
    let data_home = std::env::var_os("XDG_DATA_HOME")
        .map(PathBuf::from)
        .unwrap_or_else(|| home.join(".local/share"));
    let data_dirs = std::env::var_os("XDG_DATA_DIRS")
        .map(|value| std::env::split_paths(&value).collect::<Vec<_>>())
        .filter(|paths| !paths.is_empty())
        .unwrap_or_else(|| {
            vec![
                PathBuf::from("/usr/local/share"),
                PathBuf::from("/usr/share"),
            ]
        });
    let candidates = std::iter::once(home.join(".icons"))
        .chain(std::iter::once(data_home.join("icons")))
        .chain(data_dirs.into_iter().map(|path| path.join("icons")))
        .chain(std::iter::once(
            home.join(".local/share/flatpak/exports/share/icons"),
        ))
        .chain(std::iter::once(PathBuf::from(
            "/var/lib/flatpak/exports/share/icons",
        )));
    let mut seen = HashSet::new();
    candidates
        .filter(|path| seen.insert(path.clone()))
        .collect()
}

#[derive(Default)]
struct ParsedTheme {
    name: String,
    comment: String,
    inherits: Vec<String>,
    directories: Vec<DirectorySpec>,
    example: Option<String>,
    hidden: bool,
}

fn parse_theme(path: &Path, id: &str) -> Result<ParsedTheme, ThemeError> {
    let file_metadata = fs::metadata(path).map_err(|error| ThemeError::Io {
        path: path.to_path_buf(),
        message: error.to_string(),
    })?;
    if file_metadata.len() > MAX_INDEX_BYTES {
        return Err(ThemeError::InvalidMetadata {
            path: path.to_path_buf(),
            message: String::from("index.theme is too large"),
        });
    }
    let contents = fs::read_to_string(path).map_err(|error| ThemeError::Io {
        path: path.to_path_buf(),
        message: error.to_string(),
    })?;
    let sections = parse_ini(&contents);
    let icon_theme = sections
        .get("Icon Theme")
        .ok_or_else(|| ThemeError::InvalidMetadata {
            path: path.to_path_buf(),
            message: String::from("missing [Icon Theme] section"),
        })?;
    let name = icon_theme
        .get("Name")
        .filter(|value| !value.trim().is_empty())
        .cloned()
        .unwrap_or_else(|| id.to_owned());
    let comment = icon_theme.get("Comment").cloned().unwrap_or_default();
    let inherits = split_list(icon_theme.get("Inherits"));
    let example = icon_theme
        .get("Example")
        .filter(|value| !value.is_empty())
        .cloned();
    let hidden = parse_bool(icon_theme.get("Hidden"));
    let directory_names = split_list(icon_theme.get("Directories"));
    let scaled_names = split_list(icon_theme.get("ScaledDirectories"));
    let mut directories = Vec::new();
    let mut seen = HashSet::new();
    for name in directory_names {
        if let Some(directory) = parse_directory(&name, sections.get(&name), false)
            && seen.insert(directory.path.clone())
        {
            directories.push(directory);
        }
    }
    for name in scaled_names {
        if let Some(directory) = parse_directory(&name, sections.get(&name), true)
            && seen.insert(directory.path.clone())
        {
            directories.push(directory);
        }
    }
    Ok(ParsedTheme {
        name,
        comment,
        inherits,
        directories,
        example,
        hidden,
    })
}

fn parse_directory(
    name: &str,
    section: Option<&HashMap<String, String>>,
    scaled: bool,
) -> Option<DirectorySpec> {
    let section = section?;
    let size = parse_u32(section.get("Size"));
    let min_size = parse_u32(section.get("MinSize")).or(size).unwrap_or(0);
    let max_size = parse_u32(section.get("MaxSize"))
        .or(size)
        .unwrap_or(min_size);
    let threshold = parse_u32(section.get("Threshold")).unwrap_or(2);
    let scale = parse_u32(section.get("Scale")).unwrap_or(1).max(1);
    let kind = match section
        .get("Type")
        .map(|value| value.to_ascii_lowercase())
        .as_deref()
    {
        Some("scalable") => DirectoryType::Scalable,
        Some("threshold") => DirectoryType::Threshold,
        _ => DirectoryType::Fixed,
    };
    if !matches!(kind, DirectoryType::Scalable) && size.is_none() {
        return None;
    }
    if scaled || section.contains_key("Scale") {
        return Some(DirectorySpec {
            path: name.to_owned(),
            size: size.unwrap_or(min_size),
            min_size,
            max_size,
            threshold,
            scale,
            kind,
        });
    }
    Some(DirectorySpec {
        path: name.to_owned(),
        size: size.unwrap_or(min_size),
        min_size,
        max_size,
        threshold,
        scale,
        kind,
    })
}

fn parse_ini(contents: &str) -> HashMap<String, HashMap<String, String>> {
    let mut sections = HashMap::new();
    let mut current = String::new();
    for line in contents.lines().take(100_000) {
        let line = line.trim();
        if line.is_empty() || line.starts_with('#') || line.starts_with(';') {
            continue;
        }
        if line.starts_with('[') && line.ends_with(']') {
            current = line[1..line.len() - 1].trim().to_owned();
            sections.entry(current.clone()).or_insert_with(HashMap::new);
            continue;
        }
        let Some((key, value)) = line.split_once('=') else {
            continue;
        };
        if current.is_empty() {
            continue;
        }
        sections
            .entry(current.clone())
            .or_insert_with(HashMap::new)
            .insert(key.trim().to_owned(), value.trim().to_owned());
    }
    sections
}

fn split_list(value: Option<&String>) -> Vec<String> {
    value
        .map(|value| {
            value
                .split(',')
                .map(str::trim)
                .filter(|item| !item.is_empty())
                .map(str::to_owned)
                .collect()
        })
        .unwrap_or_default()
}

fn parse_bool(value: Option<&String>) -> bool {
    value.is_some_and(|value| {
        matches!(
            value.trim().to_ascii_lowercase().as_str(),
            "true" | "yes" | "1"
        )
    })
}

fn parse_u32(value: Option<&String>) -> Option<u32> {
    value.and_then(|value| value.trim().parse().ok())
}

fn valid_theme_id(id: &str) -> bool {
    !id.is_empty() && id != "." && id != ".." && !id.contains('/') && !id.contains('\\')
}

fn is_user_path(path: &Path) -> bool {
    let home = std::env::var_os("HOME").map(PathBuf::from);
    home.is_some_and(|home| path.starts_with(home))
}

fn root_index_is_first(catalog: &ThemeCatalog, id: &str) -> bool {
    catalog.theme(id).is_none()
}

fn root_index_is_malformed(error: &ThemeError) -> bool {
    matches!(error, ThemeError::InvalidMetadata { .. })
}
