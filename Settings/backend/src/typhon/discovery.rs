use std::path::{Path, PathBuf};

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum DiscoveryError {
    RuntimeNotSecure,
    TyphonRuntimeNotSecure,
    NoSecureInstance,
    MultipleInstances,
}

pub fn discover_socket_from_environment() -> Result<PathBuf, DiscoveryError> {
    let runtime = std::env::var_os("XDG_RUNTIME_DIR").ok_or(DiscoveryError::RuntimeNotSecure)?;
    let display = std::env::var_os("WAYLAND_DISPLAY");
    discover_socket(Path::new(&runtime), display.as_deref())
}

pub fn discover_socket(
    runtime: &Path,
    preferred_instance: Option<&std::ffi::OsStr>,
) -> Result<PathBuf, DiscoveryError> {
    if !runtime.is_absolute() || !secure_directory(runtime) {
        return Err(DiscoveryError::RuntimeNotSecure);
    }
    let astrea = runtime.join("astrea");
    let typhon = astrea.join("typhon");
    if !secure_directory(&astrea) || !secure_directory(&typhon) {
        return Err(DiscoveryError::TyphonRuntimeNotSecure);
    }

    if let Some(preferred) = preferred_instance.and_then(valid_instance_name)
        && let Some(path) = secure_candidate(&typhon, &preferred)
    {
        return Ok(path);
    }

    let mut found = None;
    let mut count = 0;
    let entries = std::fs::read_dir(&typhon).map_err(|_| DiscoveryError::NoSecureInstance)?;
    for entry in entries.flatten() {
        let Some(name) = valid_instance_name(&entry.file_name()) else {
            continue;
        };
        if let Some(path) = secure_candidate(&typhon, &name) {
            found = Some(path);
            count += 1;
        }
    }
    match (count, found) {
        (1, Some(path)) => Ok(path),
        (0, _) => Err(DiscoveryError::NoSecureInstance),
        _ => Err(DiscoveryError::MultipleInstances),
    }
}

fn secure_candidate(root: &Path, name: &str) -> Option<PathBuf> {
    let directory = root.join(name);
    let socket = directory.join("control.sock");
    (secure_directory(&directory) && secure_socket(&socket)).then_some(socket)
}

fn valid_instance_name(name: &std::ffi::OsStr) -> Option<String> {
    let value = name.to_str()?;
    let length = value.encode_utf16().count();
    if value.is_empty()
        || length > 128
        || value == "."
        || value == ".."
        || value.contains("..")
        || !value
            .chars()
            .all(|character| character.is_alphanumeric() || ".-_".contains(character))
    {
        return None;
    }
    Some(value.to_owned())
}

#[cfg(unix)]
fn secure_directory(path: &Path) -> bool {
    use std::os::unix::fs::{MetadataExt, PermissionsExt};

    let Ok(metadata) = std::fs::symlink_metadata(path) else {
        return false;
    };
    metadata.file_type().is_dir()
        && metadata.uid() == effective_user_id()
        && metadata.permissions().mode() & 0o777 == 0o700
}

#[cfg(not(unix))]
fn secure_directory(_path: &Path) -> bool {
    false
}

#[cfg(unix)]
fn secure_socket(path: &Path) -> bool {
    use std::os::unix::fs::{FileTypeExt, MetadataExt, PermissionsExt};

    let Ok(metadata) = std::fs::symlink_metadata(path) else {
        return false;
    };
    metadata.file_type().is_socket()
        && metadata.uid() == effective_user_id()
        && metadata.permissions().mode() & 0o777 == 0o600
}

#[cfg(not(unix))]
fn secure_socket(_path: &Path) -> bool {
    false
}

#[cfg(unix)]
fn effective_user_id() -> u32 {
    // SAFETY: geteuid has no preconditions and only reads the process credentials.
    unsafe { libc::geteuid() }
}

#[cfg(test)]
mod tests {
    use std::ffi::OsStr;
    use std::os::unix::fs::PermissionsExt;
    use std::os::unix::net::UnixListener;
    use std::path::{Path, PathBuf};
    use tempfile::{TempDir, tempdir};

    use super::*;

    #[test]
    fn preferred_secure_instance_wins_over_other_instances() {
        let runtime = secure_runtime(&["preferred", "other"]);
        let path = discover_socket(runtime.path(), Some(OsStr::new("preferred"))).unwrap();
        assert_eq!(
            path,
            runtime.path().join("astrea/typhon/preferred/control.sock")
        );
    }

    #[test]
    fn ambiguous_secure_instances_are_rejected_without_preference() {
        let runtime = secure_runtime(&["one", "two"]);
        assert_eq!(
            discover_socket(runtime.path(), Some(OsStr::new("missing"))),
            Err(DiscoveryError::MultipleInstances)
        );
    }

    #[test]
    fn insecure_runtime_and_symlinked_runtime_are_rejected() {
        let directory = tempdir().unwrap();
        let runtime = directory.path().join("runtime");
        std::fs::create_dir(&runtime).unwrap();
        set_mode(&runtime, 0o755);
        assert_eq!(
            discover_socket(&runtime, None),
            Err(DiscoveryError::RuntimeNotSecure)
        );

        let symlink = directory.path().join("runtime-link");
        std::os::unix::fs::symlink(&runtime, &symlink).unwrap();
        assert_eq!(
            discover_socket(&symlink, None),
            Err(DiscoveryError::RuntimeNotSecure)
        );
    }

    #[test]
    fn insecure_nested_directory_is_rejected() {
        let directory = tempdir().unwrap();
        let runtime = directory.path().join("runtime");
        std::fs::create_dir_all(runtime.join("astrea/typhon")).unwrap();
        set_mode(&runtime, 0o700);
        set_mode(&runtime.join("astrea"), 0o755);
        set_mode(&runtime.join("astrea/typhon"), 0o700);
        assert_eq!(
            discover_socket(&runtime, None),
            Err(DiscoveryError::TyphonRuntimeNotSecure)
        );
    }

    #[test]
    fn insecure_instance_and_socket_are_not_candidates() {
        let runtime = secure_runtime(&["valid"]);
        set_mode(&runtime.path().join("astrea/typhon/valid"), 0o755);
        assert_eq!(
            discover_socket(runtime.path(), None),
            Err(DiscoveryError::NoSecureInstance)
        );
        set_mode(&runtime.path().join("astrea/typhon/valid"), 0o700);
        set_mode(&runtime.socket_paths[0], 0o666);
        assert_eq!(
            discover_socket(runtime.path(), None),
            Err(DiscoveryError::NoSecureInstance)
        );
    }

    #[test]
    fn symlinked_instance_and_socket_are_not_candidates() {
        let directory = tempdir().unwrap();
        let runtime = directory.path().join("runtime");
        std::fs::create_dir_all(runtime.join("astrea/typhon")).unwrap();
        set_mode(&runtime, 0o700);
        set_mode(&runtime.join("astrea"), 0o700);
        set_mode(&runtime.join("astrea/typhon"), 0o700);
        let target = directory.path().join("target");
        std::fs::create_dir(&target).unwrap();
        set_mode(&target, 0o700);
        std::os::unix::fs::symlink(&target, runtime.join("astrea/typhon/link")).unwrap();
        assert_eq!(
            discover_socket(&runtime, None),
            Err(DiscoveryError::NoSecureInstance)
        );

        let runtime = secure_runtime(&["valid"]);
        let target = runtime.path().join("target.sock");
        let target_listener = UnixListener::bind(&target).unwrap();
        set_mode(&target, 0o600);
        let link = runtime.path().join("astrea/typhon/valid/control.sock");
        std::fs::remove_file(&runtime.socket_paths[0]).unwrap();
        std::os::unix::fs::symlink(&target, &link).unwrap();
        assert_eq!(
            discover_socket(runtime.path(), None),
            Err(DiscoveryError::NoSecureInstance)
        );
        drop(target_listener);
    }

    #[test]
    fn instance_name_validation_preserves_bounds_and_character_rules() {
        assert!(valid_instance_name(OsStr::new("wayland-0")).is_some());
        assert!(valid_instance_name(OsStr::new("écran_1")).is_some());
        assert!(valid_instance_name(OsStr::new("a..b")).is_none());
        assert!(valid_instance_name(OsStr::new("bad/name")).is_none());
        assert!(valid_instance_name(OsStr::new(&"a".repeat(129))).is_none());
    }

    struct TestRuntime {
        directory: TempDir,
        _listeners: Vec<UnixListener>,
        socket_paths: Vec<PathBuf>,
    }

    impl TestRuntime {
        fn path(&self) -> &Path {
            self.directory.path()
        }
    }

    fn secure_runtime(instances: &[&str]) -> TestRuntime {
        let directory = tempdir().unwrap();
        let runtime = directory.path();
        std::fs::create_dir(runtime.join("astrea")).unwrap();
        std::fs::create_dir(runtime.join("astrea/typhon")).unwrap();
        set_mode(runtime, 0o700);
        set_mode(&runtime.join("astrea"), 0o700);
        set_mode(&runtime.join("astrea/typhon"), 0o700);
        let mut listeners = Vec::new();
        let mut socket_paths = Vec::new();
        for instance in instances {
            let directory = runtime.join("astrea/typhon").join(instance);
            std::fs::create_dir(&directory).unwrap();
            set_mode(&directory, 0o700);
            let path = directory.join("control.sock");
            let listener = UnixListener::bind(&path).unwrap();
            set_mode(&path, 0o600);
            listeners.push(listener);
            socket_paths.push(path);
        }
        TestRuntime {
            directory,
            _listeners: listeners,
            socket_paths,
        }
    }

    fn set_mode(path: &Path, mode: u32) {
        std::fs::set_permissions(path, std::fs::Permissions::from_mode(mode)).unwrap();
    }
}
