use crate::Result;
use serde_json::{Map, Value};
use std::{
    fs::{self, File, OpenOptions},
    io::{Read, Write},
    os::unix::{
        fs::{OpenOptionsExt, PermissionsExt},
        io::AsRawFd,
    },
    path::{Path, PathBuf},
    thread,
    time::{Duration, Instant, SystemTime, UNIX_EPOCH},
};

pub struct JsonStore {
    path: PathBuf,
}

impl JsonStore {
    pub fn new(path: PathBuf) -> Self {
        Self { path }
    }
    pub fn path(&self) -> &Path {
        &self.path
    }

    pub fn read(&self) -> Result<Map<String, Value>> {
        read_document(&self.path)
    }

    pub fn update(
        &self,
        patch: impl FnOnce(&mut Map<String, Value>) -> Result<()>,
    ) -> Result<Map<String, Value>> {
        let parent = self
            .path
            .parent()
            .ok_or_else(|| format!("{} has no parent directory", self.path.display()))?;
        fs::create_dir_all(parent).map_err(|e| format!("create {}: {e}", parent.display()))?;
        let lock_path = append_suffix(&self.path, ".lock");
        let lock = OpenOptions::new()
            .create(true)
            .truncate(false)
            .read(true)
            .write(true)
            .mode(0o600)
            .open(&lock_path)
            .map_err(|e| format!("open lock {}: {e}", lock_path.display()))?;
        acquire_lock(&lock, &lock_path)?;
        let mut doc = read_document(&self.path)?;
        patch(&mut doc)?;
        atomic_write(&self.path, &doc)?;
        Ok(doc)
    }
}

fn read_document(path: &Path) -> Result<Map<String, Value>> {
    let mut file = match File::open(path) {
        Ok(file) => file,
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => return Ok(Map::new()),
        Err(e) => return Err(format!("read {}: {e}", path.display())),
    };
    let mut bytes = Vec::new();
    file.read_to_end(&mut bytes)
        .map_err(|e| format!("read {}: {e}", path.display()))?;
    let value: Value =
        serde_json::from_slice(&bytes).map_err(|e| format!("parse {}: {e}", path.display()))?;
    value
        .as_object()
        .cloned()
        .ok_or_else(|| format!("{} must contain a JSON object", path.display()))
}

fn acquire_lock(file: &File, path: &Path) -> Result<()> {
    let deadline = Instant::now() + Duration::from_secs(30);
    loop {
        // SAFETY: flock operates on this live file descriptor and does not retain pointers.
        if unsafe { libc::flock(file.as_raw_fd(), libc::LOCK_EX | libc::LOCK_NB) } == 0 {
            return Ok(());
        }
        let err = std::io::Error::last_os_error();
        if err.kind() != std::io::ErrorKind::WouldBlock {
            return Err(format!("lock {}: {err}", path.display()));
        }
        if Instant::now() >= deadline {
            return Err(format!("timed out waiting for lock {}", path.display()));
        }
        thread::sleep(Duration::from_millis(20));
    }
}

fn atomic_write(path: &Path, value: &Map<String, Value>) -> Result<()> {
    let parent = path
        .parent()
        .ok_or_else(|| format!("{} has no parent directory", path.display()))?;
    let mode = fs::metadata(path)
        .map(|m| m.permissions().mode() & 0o777)
        .unwrap_or(0o600);
    let nonce = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_nanos();
    let tmp = parent.join(format!(
        ".{}.{}.{}.tmp",
        path.file_name().unwrap_or_default().to_string_lossy(),
        std::process::id(),
        nonce
    ));
    let result = (|| {
        let mut file = OpenOptions::new()
            .write(true)
            .create_new(true)
            .mode(mode)
            .open(&tmp)
            .map_err(|e| format!("create {}: {e}", tmp.display()))?;
        let data = serde_json::to_vec_pretty(value)
            .map_err(|e| format!("serialize {}: {e}", path.display()))?;
        file.write_all(&data)
            .and_then(|_| file.write_all(b"\n"))
            .map_err(|e| format!("write {}: {e}", tmp.display()))?;
        file.sync_all()
            .map_err(|e| format!("sync {}: {e}", tmp.display()))?;
        fs::set_permissions(&tmp, fs::Permissions::from_mode(mode))
            .map_err(|e| format!("permissions {}: {e}", tmp.display()))?;
        fs::rename(&tmp, path).map_err(|e| format!("replace {}: {e}", path.display()))?;
        if let Ok(dir) = File::open(parent) {
            let _ = dir.sync_all();
        }
        Ok(())
    })();
    if result.is_err() {
        let _ = fs::remove_file(&tmp);
    }
    result
}

fn append_suffix(path: &Path, suffix: &str) -> PathBuf {
    let mut name = path.as_os_str().to_os_string();
    name.push(suffix);
    PathBuf::from(name)
}

pub fn config_home() -> Result<PathBuf> {
    if let Some(value) = std::env::var_os("XDG_CONFIG_HOME") {
        let path = PathBuf::from(value);
        if path.is_absolute() {
            return Ok(path);
        }
    }
    let home = std::env::var_os("HOME")
        .map(PathBuf::from)
        .ok_or_else(|| "HOME is not set".to_string())?;
    if !home.is_absolute() {
        return Err("HOME must be absolute".into());
    }
    Ok(home.join(".config"))
}
