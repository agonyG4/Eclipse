use serde_json::{Map, Value};
use std::fmt::{Display, Formatter};
use std::fs::{self, File, OpenOptions};
use std::io::Write;
use std::os::fd::AsRawFd;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicU64, Ordering};

static TEMP_FILE_SEQUENCE: AtomicU64 = AtomicU64::new(1);

#[derive(Debug)]
pub enum ThemeConfigError {
    Io(String),
    Malformed(String),
    NotObject,
}

impl Display for ThemeConfigError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Io(message) | Self::Malformed(message) => formatter.write_str(message),
            Self::NotObject => formatter.write_str("theme.json must contain a JSON object"),
        }
    }
}

impl std::error::Error for ThemeConfigError {}

#[derive(Clone, Debug)]
pub struct ThemeConfigStore {
    path: PathBuf,
}

impl ThemeConfigStore {
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

    pub fn read_object(&self) -> Result<Option<Map<String, Value>>, ThemeConfigError> {
        let bytes = match fs::read(&self.path) {
            Ok(bytes) => bytes,
            Err(error) if error.kind() == std::io::ErrorKind::NotFound => return Ok(None),
            Err(error) => return Err(ThemeConfigError::Io(error.to_string())),
        };
        let value: Value = serde_json::from_slice(&bytes)
            .map_err(|error| ThemeConfigError::Malformed(error.to_string()))?;
        value
            .as_object()
            .cloned()
            .map(Some)
            .ok_or(ThemeConfigError::NotObject)
    }

    pub fn patch_object(
        &self,
        patch: impl FnOnce(&mut Map<String, Value>),
    ) -> Result<(), ThemeConfigError> {
        let parent = parent_or_current(&self.path);
        fs::create_dir_all(parent).map_err(|error| ThemeConfigError::Io(error.to_string()))?;

        let _lock = acquire_config_lock(&self.path)?;
        let mut object = self.read_object()?.unwrap_or_default();
        patch(&mut object);
        self.write_object(&object)
    }

    fn write_object(&self, object: &Map<String, Value>) -> Result<(), ThemeConfigError> {
        self.write_object_with(object, write_and_sync)
    }

    fn write_object_with(
        &self,
        object: &Map<String, Value>,
        write: impl FnOnce(&mut File, &Map<String, Value>) -> Result<(), ThemeConfigError>,
    ) -> Result<(), ThemeConfigError> {
        let parent = parent_or_current(&self.path);
        fs::create_dir_all(parent).map_err(|error| ThemeConfigError::Io(error.to_string()))?;
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
            .map_err(|error| ThemeConfigError::Io(error.to_string()))?;
        let result = write(&mut file, object)
            .and_then(|()| {
                fs::rename(&temporary_path, &self.path)
                    .map_err(|error| ThemeConfigError::Io(error.to_string()))
            })
            .and_then(|()| {
                File::open(parent)
                    .and_then(|directory| directory.sync_all())
                    .map_err(|error| ThemeConfigError::Io(error.to_string()))
            });
        if result.is_err() {
            let _ = fs::remove_file(&temporary_path);
        }
        result
    }
}

fn parent_or_current(path: &Path) -> &Path {
    path.parent()
        .filter(|parent| !parent.as_os_str().is_empty())
        .unwrap_or_else(|| Path::new("."))
}

fn acquire_config_lock(path: &Path) -> Result<File, ThemeConfigError> {
    let parent = parent_or_current(path);
    fs::create_dir_all(parent).map_err(|error| ThemeConfigError::Io(error.to_string()))?;

    let mut lock_path = path.as_os_str().to_os_string();
    lock_path.push(".lock");
    let lock = OpenOptions::new()
        .create(true)
        .truncate(false)
        .read(true)
        .write(true)
        .open(lock_path)
        .map_err(|error| ThemeConfigError::Io(error.to_string()))?;
    loop {
        // SAFETY: `lock` owns a live file descriptor for the duration of the lock.
        let result = unsafe { libc::flock(lock.as_raw_fd(), libc::LOCK_EX) };
        if result == 0 {
            return Ok(lock);
        }
        let error = std::io::Error::last_os_error();
        if error.kind() != std::io::ErrorKind::Interrupted {
            return Err(ThemeConfigError::Io(error.to_string()));
        }
    }
}

fn write_and_sync(file: &mut File, object: &Map<String, Value>) -> Result<(), ThemeConfigError> {
    let bytes = serde_json::to_vec_pretty(object)
        .map_err(|error| ThemeConfigError::Io(error.to_string()))?;
    file.write_all(&bytes)
        .map_err(|error| ThemeConfigError::Io(error.to_string()))?;
    file.write_all(b"\n")
        .map_err(|error| ThemeConfigError::Io(error.to_string()))?;
    file.sync_all()
        .map_err(|error| ThemeConfigError::Io(error.to_string()))
}

#[cfg(test)]
mod tests {
    use super::{ThemeConfigError, ThemeConfigStore};
    use serde_json::{Map, Value, json};
    use std::fs::{self, File, OpenOptions};
    use std::os::fd::AsRawFd;
    use std::os::unix::fs::MetadataExt;
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

    fn acquire_cpp_style_lock(config_path: &Path) -> File {
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

    #[test]
    fn missing_config_reads_as_absent_and_patch_creates_an_object() {
        let directory = TempDir::new().unwrap();
        let path = directory.path().join("theme.json");
        let store = ThemeConfigStore::new(path.clone());
        assert_eq!(store.read_object().unwrap(), None);
        store
            .patch_object(|object| {
                object.insert(
                    String::from("accent"),
                    Value::String(String::from("#0a84ff")),
                );
            })
            .unwrap();
        assert_eq!(store.read_object().unwrap().unwrap()["accent"], "#0a84ff");
    }

    #[test]
    fn reads_valid_json_object() {
        let directory = TempDir::new().unwrap();
        let path = directory.path().join("theme.json");
        fs::write(&path, br##"{"accent":"#123456","future":true}"##).unwrap();
        let store = ThemeConfigStore::new(path);
        let object = store.read_object().unwrap().unwrap();
        assert_eq!(object["accent"], "#123456");
        assert_eq!(object["future"], true);
    }

    #[test]
    fn malformed_and_non_object_configs_are_rejected_without_replacement() {
        let directory = TempDir::new().unwrap();
        for original in [b"{ malformed".as_slice(), b"[1, 2]".as_slice()] {
            let path = directory.path().join("theme.json");
            fs::write(&path, original).unwrap();
            let store = ThemeConfigStore::new(path.clone());
            assert!(store.read_object().is_err());
            assert!(
                store
                    .patch_object(|object| {
                        object.insert(String::from("accent"), Value::String(String::from("red")));
                    })
                    .is_err()
            );
            assert_eq!(fs::read(path).unwrap(), original);
        }
    }

    #[test]
    fn patch_preserves_unknown_and_unrelated_keys() {
        let directory = TempDir::new().unwrap();
        let path = directory.path().join("theme.json");
        fs::write(
            &path,
            serde_json::to_vec(&json!({
                "theme_preference": "dark",
                "accent": "#123456",
                "system_icon_theme": "Breeze",
                "future": { "enabled": true }
            }))
            .unwrap(),
        )
        .unwrap();
        ThemeConfigStore::new(path.clone())
            .patch_object(|object| {
                object.insert(String::from("shell_style"), json!(2));
            })
            .unwrap();
        let saved: Value = serde_json::from_slice(&fs::read(path).unwrap()).unwrap();
        assert_eq!(saved["theme_preference"], "dark");
        assert_eq!(saved["accent"], "#123456");
        assert_eq!(saved["system_icon_theme"], "Breeze");
        assert_eq!(saved["future"]["enabled"], true);
        assert_eq!(saved["shell_style"], 2);
    }

    #[test]
    fn patch_uses_the_cpp_theme_json_lock_protocol() {
        let directory = TempDir::new().unwrap();
        let path = directory.path().join("theme.json");
        fs::write(&path, br#"{"value":"before"}"#).unwrap();
        let lock = acquire_cpp_style_lock(&path);
        let (started_tx, started_rx) = mpsc::channel();
        let (completed_tx, completed_rx) = mpsc::channel();
        let writer_path = path.clone();
        let writer = thread::spawn(move || {
            started_tx.send(()).unwrap();
            completed_tx
                .send(ThemeConfigStore::new(writer_path).patch_object(|object| {
                    object.insert(String::from("value"), json!("after"));
                }))
                .unwrap();
        });
        started_rx.recv_timeout(Duration::from_secs(2)).unwrap();
        assert!(matches!(
            completed_rx.recv_timeout(Duration::from_millis(100)),
            Err(mpsc::RecvTimeoutError::Timeout)
        ));
        assert!(lock_path(&path).exists());
        assert_eq!(lock_path(&path).file_name().unwrap(), "theme.json.lock");
        drop(lock);
        completed_rx
            .recv_timeout(Duration::from_secs(2))
            .unwrap()
            .unwrap();
        writer.join().unwrap();
        assert_eq!(
            ThemeConfigStore::new(path).read_object().unwrap().unwrap()["value"],
            "after"
        );
    }

    #[test]
    fn patch_atomically_replaces_in_the_same_directory_and_syncs_file() {
        let directory = TempDir::new().unwrap();
        let path = directory.path().join("theme.json");
        fs::write(&path, br#"{"value":"before"}"#).unwrap();
        let old_inode = fs::metadata(&path).unwrap().ino();
        ThemeConfigStore::new(path.clone())
            .patch_object(|object| {
                object.insert(String::from("value"), json!("after"));
            })
            .unwrap();
        assert_ne!(fs::metadata(&path).unwrap().ino(), old_inode);
        let saved: Value = serde_json::from_slice(&fs::read(&path).unwrap()).unwrap();
        assert_eq!(saved["value"], "after");
        assert!(fs::read_dir(directory.path()).unwrap().all(|entry| {
            !entry
                .unwrap()
                .file_name()
                .to_string_lossy()
                .starts_with(".theme.json.tmp-")
        }));
    }

    #[test]
    fn failed_temporary_write_keeps_previous_valid_config_and_cleans_up() {
        let directory = TempDir::new().unwrap();
        let path = directory.path().join("theme.json");
        let original = br##"{"accent":"#123456","system_icon_theme":"Breeze"}"##;
        fs::write(&path, original).unwrap();
        let store = ThemeConfigStore::new(path.clone());
        let mut replacement = Map::new();
        replacement.insert(String::from("accent"), json!("#30d158"));

        let error = store
            .write_object_with(&replacement, |_, _| {
                Err(ThemeConfigError::Io(String::from(
                    "injected file write failure",
                )))
            })
            .unwrap_err();

        assert!(matches!(error, ThemeConfigError::Io(_)));
        assert_eq!(fs::read(&path).unwrap(), original);
        assert!(fs::read_dir(directory.path()).unwrap().all(|entry| {
            !entry
                .unwrap()
                .file_name()
                .to_string_lossy()
                .starts_with(".theme.json.tmp-")
        }));
    }

    #[test]
    fn lock_open_failure_preserves_the_previous_valid_file() {
        let directory = TempDir::new().unwrap();
        let path = directory.path().join("theme.json");
        let original = br#"{"system_icon_theme":"Before"}"#;
        fs::write(&path, original).unwrap();
        fs::create_dir(lock_path(&path)).unwrap();
        let error = ThemeConfigStore::new(path.clone())
            .patch_object(|object: &mut Map<String, Value>| {
                object.insert(String::from("accent"), json!("after"));
            })
            .unwrap_err();
        assert!(matches!(error, ThemeConfigError::Io(_)));
        assert_eq!(fs::read(path).unwrap(), original);
    }

    #[test]
    fn default_path_matches_the_settings_theme_config_location() {
        let home = std::env::var_os("HOME")
            .map(PathBuf::from)
            .unwrap_or_else(|| PathBuf::from("/"));
        assert_eq!(
            ThemeConfigStore::default_path(),
            home.join(".config/AstreaOS/ui/theme.json")
        );
    }
}
