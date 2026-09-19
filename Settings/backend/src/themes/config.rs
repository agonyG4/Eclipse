use serde_json::{Map, Value};
use std::fmt::{Display, Formatter};
use std::fs::{self, File, OpenOptions};
use std::io::Write;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicU64, Ordering};

static TEMP_FILE_SEQUENCE: AtomicU64 = AtomicU64::new(1);

#[derive(Debug)]
pub enum ConfigError {
    Io(String),
    Malformed(String),
    NotObject,
}

impl Display for ConfigError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Io(message) | Self::Malformed(message) => formatter.write_str(message),
            Self::NotObject => formatter.write_str("theme.json must contain a JSON object"),
        }
    }
}

impl std::error::Error for ConfigError {}

#[derive(Clone, Debug)]
pub struct ThemePreferenceStore {
    path: PathBuf,
}

impl ThemePreferenceStore {
    pub fn new(path: PathBuf) -> Self {
        Self { path }
    }

    pub fn default_path() -> PathBuf {
        let home = std::env::var_os("HOME")
            .map(PathBuf::from)
            .unwrap_or_else(|| PathBuf::from("/"));
        home.join(".config/AstreaOS/ui/theme.json")
    }

    pub fn path(&self) -> &Path {
        &self.path
    }

    pub fn load(&self) -> Result<Option<String>, ConfigError> {
        let object = self.read_object()?;
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
        let mut object = self.read_object()?.unwrap_or_default();
        object.insert(
            String::from("system_icon_theme"),
            Value::String(theme_id.trim().to_owned()),
        );
        self.write_object(&object)
    }

    pub fn clear_selected(&self) -> Result<(), ConfigError> {
        let mut object = self.read_object()?.unwrap_or_default();
        object.remove("system_icon_theme");
        self.write_object(&object)
    }

    fn read_object(&self) -> Result<Option<Map<String, Value>>, ConfigError> {
        let bytes = match fs::read(&self.path) {
            Ok(bytes) => bytes,
            Err(error) if error.kind() == std::io::ErrorKind::NotFound => return Ok(None),
            Err(error) => return Err(ConfigError::Io(error.to_string())),
        };
        let value: Value = serde_json::from_slice(&bytes)
            .map_err(|error| ConfigError::Malformed(error.to_string()))?;
        value
            .as_object()
            .cloned()
            .map(Some)
            .ok_or(ConfigError::NotObject)
    }

    fn write_object(&self, object: &Map<String, Value>) -> Result<(), ConfigError> {
        let parent = self.path.parent().unwrap_or_else(|| Path::new("."));
        fs::create_dir_all(parent).map_err(|error| ConfigError::Io(error.to_string()))?;
        let sequence = TEMP_FILE_SEQUENCE.fetch_add(1, Ordering::Relaxed);
        let file_name = self
            .path
            .file_name()
            .and_then(|name| name.to_str())
            .unwrap_or("theme.json");
        let temporary_path = parent.join(format!(
            ".{file_name}.tmp-{}-{sequence}",
            std::process::id()
        ));
        let mut file = OpenOptions::new()
            .create_new(true)
            .write(true)
            .open(&temporary_path)
            .map_err(|error| ConfigError::Io(error.to_string()))?;
        let result = write_and_sync(&mut file, object)
            .and_then(|()| {
                fs::rename(&temporary_path, &self.path)
                    .map_err(|error| ConfigError::Io(error.to_string()))
            })
            .and_then(|()| {
                File::open(parent)
                    .and_then(|directory| directory.sync_all())
                    .map_err(|error| ConfigError::Io(error.to_string()))
            });
        if result.is_err() {
            let _ = fs::remove_file(&temporary_path);
        }
        result
    }
}

impl Default for ThemePreferenceStore {
    fn default() -> Self {
        Self::new(Self::default_path())
    }
}

fn write_and_sync(file: &mut File, object: &Map<String, Value>) -> Result<(), ConfigError> {
    let bytes =
        serde_json::to_vec_pretty(object).map_err(|error| ConfigError::Io(error.to_string()))?;
    file.write_all(&bytes)
        .map_err(|error| ConfigError::Io(error.to_string()))?;
    file.write_all(b"\n")
        .map_err(|error| ConfigError::Io(error.to_string()))?;
    file.sync_all()
        .map_err(|error| ConfigError::Io(error.to_string()))
}
