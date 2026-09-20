use serde_json::{Map, Value};
use std::fmt::{Display, Formatter};
use std::fs::{self, File, OpenOptions};
use std::io::Write;
use std::os::fd::AsRawFd;
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
        let selected = theme_id.trim().to_owned();
        self.update_object(|object| {
            object.insert(String::from("system_icon_theme"), Value::String(selected));
        })
    }

    pub fn clear_selected(&self) -> Result<(), ConfigError> {
        self.update_object(|object| {
            object.remove("system_icon_theme");
        })
    }

    fn update_object(
        &self,
        update: impl FnOnce(&mut Map<String, Value>),
    ) -> Result<(), ConfigError> {
        let parent = self
            .path
            .parent()
            .filter(|parent| !parent.as_os_str().is_empty())
            .unwrap_or_else(|| Path::new("."));
        fs::create_dir_all(parent).map_err(|error| ConfigError::Io(error.to_string()))?;

        let _lock = acquire_config_lock(&self.path)?;
        let mut object = self.read_object()?.unwrap_or_default();
        update(&mut object);
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
        let parent = self
            .path
            .parent()
            .filter(|parent| !parent.as_os_str().is_empty())
            .unwrap_or_else(|| Path::new("."));
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

fn acquire_config_lock(path: &Path) -> Result<File, ConfigError> {
    let parent = path
        .parent()
        .filter(|parent| !parent.as_os_str().is_empty())
        .unwrap_or_else(|| Path::new("."));
    fs::create_dir_all(parent).map_err(|error| ConfigError::Io(error.to_string()))?;

    let mut lock_path = path.as_os_str().to_os_string();
    lock_path.push(".lock");
    let lock = OpenOptions::new()
        .create(true)
        .truncate(false)
        .read(true)
        .write(true)
        .open(lock_path)
        .map_err(|error| ConfigError::Io(error.to_string()))?;
    loop {
        // SAFETY: `lock` owns a live file descriptor for the duration of the lock.
        let result = unsafe { libc::flock(lock.as_raw_fd(), libc::LOCK_EX) };
        if result == 0 {
            return Ok(lock);
        }
        let error = std::io::Error::last_os_error();
        if error.kind() != std::io::ErrorKind::Interrupted {
            return Err(ConfigError::Io(error.to_string()));
        }
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

#[cfg(test)]
mod tests {
    use super::{ConfigError, ThemePreferenceStore};
    use serde_json::Value;
    use std::fs::{self, File, OpenOptions};
    use std::io::Write;
    use std::os::fd::AsRawFd;
    use std::path::{Path, PathBuf};
    use std::sync::mpsc;
    use std::thread;
    use std::time::Duration;
    use tempfile::TempDir;

    fn lock_path(config_path: &Path) -> PathBuf {
        let mut path = config_path.as_os_str().to_os_string();
        path.push(".lock");
        PathBuf::from(path)
    }

    fn acquire_lock(config_path: &Path) -> File {
        let lock = OpenOptions::new()
            .create(true)
            .truncate(false)
            .read(true)
            .write(true)
            .open(lock_path(config_path))
            .unwrap();
        assert_eq!(unsafe { libc::flock(lock.as_raw_fd(), libc::LOCK_EX) }, 0);
        lock
    }

    fn write_cpp_transaction(config_path: &Path, mut object: Value) {
        object["accent"] = Value::String(String::from("#30d158"));
        object["theme_preference"] = Value::String(String::from("light"));
        object["icon_appearance"] = Value::String(String::from("monochrome"));
        let parent = config_path.parent().unwrap();
        let temporary_path = parent.join(".theme.json.cpp-test-tmp");
        let mut temporary = OpenOptions::new()
            .create(true)
            .truncate(true)
            .write(true)
            .open(&temporary_path)
            .unwrap();
        temporary
            .write_all(&serde_json::to_vec_pretty(&object).unwrap())
            .unwrap();
        temporary.sync_all().unwrap();
        fs::rename(temporary_path, config_path).unwrap();
        File::open(parent).unwrap().sync_all().unwrap();
    }

    #[test]
    fn rust_persistence_waits_for_cpp_transaction_and_preserves_both_updates() {
        let directory = TempDir::new().unwrap();
        let config_path = directory.path().join("theme.json");
        let initial = serde_json::json!({
            "accent": "#123456",
            "theme_preference": "auto",
            "icon_appearance": "default",
            "system_icon_theme": "Before",
            "future_setting": { "enabled": true }
        });
        fs::write(&config_path, serde_json::to_vec(&initial).unwrap()).unwrap();

        // Hold the common lock as the C++ transaction, read the current object,
        // and let the real Rust store contend before committing the C++ patch.
        let lock = acquire_lock(&config_path);
        let mut cpp_object: Value =
            serde_json::from_slice(&fs::read(&config_path).unwrap()).unwrap();
        let store = ThemePreferenceStore::new(config_path.clone());
        let (started_tx, started_rx) = mpsc::channel();
        let (completed_tx, completed_rx) = mpsc::channel();
        let rust_writer = thread::spawn(move || {
            started_tx.send(()).unwrap();
            completed_tx.send(store.save_selected("Nordic")).unwrap();
        });

        started_rx.recv_timeout(Duration::from_secs(2)).unwrap();
        assert!(matches!(
            completed_rx.recv_timeout(Duration::from_millis(250)),
            Err(mpsc::RecvTimeoutError::Timeout)
        ));
        write_cpp_transaction(&config_path, std::mem::take(&mut cpp_object));
        drop(lock);
        completed_rx
            .recv_timeout(Duration::from_secs(2))
            .unwrap()
            .unwrap();
        rust_writer.join().unwrap();

        let saved: Value = serde_json::from_slice(&fs::read(&config_path).unwrap()).unwrap();
        assert_eq!(saved["accent"], "#30d158");
        assert_eq!(saved["theme_preference"], "light");
        assert_eq!(saved["icon_appearance"], "monochrome");
        assert_eq!(saved["system_icon_theme"], "Nordic");
        assert!(saved["future_setting"]["enabled"].as_bool().unwrap());
    }

    #[test]
    fn lock_open_failure_is_reported_without_replacing_config() {
        let directory = TempDir::new().unwrap();
        let config_path = directory.path().join("theme.json");
        let original = br#"{"system_icon_theme":"Before"}"#;
        fs::write(&config_path, original).unwrap();
        fs::create_dir(lock_path(&config_path)).unwrap();

        let error = ThemePreferenceStore::new(config_path.clone())
            .save_selected("After")
            .unwrap_err();

        assert!(matches!(error, ConfigError::Io(_)));
        assert_eq!(fs::read(config_path).unwrap(), original);
    }
}
