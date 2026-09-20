use super::catalog::{ThemeCatalog, ThemeDescriptor, default_search_roots};
use super::config::ThemePreferenceStore;
use super::icon_lookup::PreviewResolver;
use std::collections::HashMap;
use std::fmt::{Display, Formatter};
use std::io;
use std::path::PathBuf;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Condvar, Mutex, mpsc};
use std::thread::{self, JoinHandle};
use std::time::Duration;

pub const PREVIEW_ICON_NAMES: [&str; 5] = [
    "folder",
    "utilities-terminal",
    "preferences-system",
    "web-browser",
    "audio-card",
];

#[derive(Debug)]
pub enum WorkerResult {
    Refresh {
        generation: u64,
        snapshot: Result<ThemeSnapshot, String>,
    },
    Persistence {
        generation: u64,
        selected: Option<String>,
        result: Result<(), String>,
    },
}

#[derive(Debug)]
pub struct ThemeSnapshot {
    pub catalog: ThemeCatalog,
    pub previews: HashMap<String, Vec<Option<PathBuf>>>,
    pub configured_selection: Result<Option<String>, String>,
}

struct WorkerState {
    pending_refresh: Option<u64>,
    pending_persistence: Option<PersistenceRequest>,
    shutdown: bool,
    exited: bool,
}

struct PersistenceRequest {
    generation: u64,
    selected: Option<String>,
}

enum ThemeWork {
    Refresh { generation: u64 },
    Persistence(PersistenceRequest),
}

#[derive(Debug)]
pub struct ThemeWorkerInitError(io::Error);

impl Display for ThemeWorkerInitError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> std::fmt::Result {
        write!(
            formatter,
            "failed to create Settings Themes worker: {}",
            self.0
        )
    }
}

impl std::error::Error for ThemeWorkerInitError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        Some(&self.0)
    }
}

#[derive(Debug, Eq, PartialEq)]
pub enum ThemeWorkerRequestError {
    Shutdown,
    StateUnavailable,
}

impl Display for ThemeWorkerRequestError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Shutdown => formatter.write_str("the Settings Themes worker is shutting down"),
            Self::StateUnavailable => {
                formatter.write_str("the Settings Themes worker state is unavailable")
            }
        }
    }
}

impl std::error::Error for ThemeWorkerRequestError {}

type WorkerTask = Box<dyn FnOnce() + Send + 'static>;
type Scanner = Box<dyn Fn(&[PathBuf]) -> Result<ThemeSnapshot, String> + Send + 'static>;

pub struct ThemeWorker {
    state: Arc<(Mutex<WorkerState>, Condvar)>,
    next_generation: AtomicU64,
    results: Option<mpsc::Receiver<WorkerResult>>,
}

impl ThemeWorker {
    pub fn new(
        search_roots: Vec<PathBuf>,
        store: ThemePreferenceStore,
    ) -> Result<Self, ThemeWorkerInitError> {
        let (sender, results) = mpsc::sync_channel(1);
        Self::spawn(search_roots, store, Some(results), move |result| {
            let _ = sender.try_send(result);
        })
    }

    pub fn new_with_callback<F>(
        search_roots: Vec<PathBuf>,
        store: ThemePreferenceStore,
        callback: F,
    ) -> Result<Self, ThemeWorkerInitError>
    where
        F: Fn(WorkerResult) + Send + 'static,
    {
        Self::spawn(search_roots, store, None, callback)
    }

    fn spawn<F>(
        search_roots: Vec<PathBuf>,
        store: ThemePreferenceStore,
        results: Option<mpsc::Receiver<WorkerResult>>,
        callback: F,
    ) -> Result<Self, ThemeWorkerInitError>
    where
        F: Fn(WorkerResult) + Send + 'static,
    {
        Self::spawn_with(
            search_roots,
            store,
            results,
            callback,
            scan_theme_snapshot,
            |run| {
                thread::Builder::new()
                    .name(String::from("astrea-settings-themes"))
                    .spawn(run)
            },
        )
    }

    fn spawn_with<F, S, T>(
        search_roots: Vec<PathBuf>,
        store: ThemePreferenceStore,
        results: Option<mpsc::Receiver<WorkerResult>>,
        callback: F,
        scanner: S,
        spawn_thread: T,
    ) -> Result<Self, ThemeWorkerInitError>
    where
        F: Fn(WorkerResult) + Send + 'static,
        S: Fn(&[PathBuf]) -> Result<ThemeSnapshot, String> + Send + 'static,
        T: FnOnce(WorkerTask) -> io::Result<JoinHandle<()>>,
    {
        let state = Arc::new((
            Mutex::new(WorkerState {
                pending_refresh: None,
                pending_persistence: None,
                shutdown: false,
                exited: false,
            }),
            Condvar::new(),
        ));
        let worker_state = Arc::clone(&state);
        let run = Box::new(move || {
            worker_loop(
                worker_state,
                callback,
                search_roots,
                store,
                Box::new(scanner),
            );
        });
        let thread = spawn_thread(run).map_err(ThemeWorkerInitError)?;
        drop(thread);
        Ok(Self {
            state,
            next_generation: AtomicU64::new(1),
            results,
        })
    }

    pub fn with_default_roots(store: ThemePreferenceStore) -> Result<Self, ThemeWorkerInitError> {
        Self::new(default_search_roots(), store)
    }

    pub fn request_refresh(&self) -> Result<u64, ThemeWorkerRequestError> {
        let generation = self.next_generation.fetch_add(1, Ordering::Relaxed);
        let (lock, wake) = &*self.state;
        let mut state = lock
            .lock()
            .map_err(|_| ThemeWorkerRequestError::StateUnavailable)?;
        if state.shutdown {
            return Err(ThemeWorkerRequestError::Shutdown);
        }
        state.pending_refresh = Some(generation);
        wake.notify_one();
        Ok(generation)
    }

    pub fn request_persistence(
        &self,
        generation: u64,
        selected: Option<String>,
    ) -> Result<(), ThemeWorkerRequestError> {
        let (lock, wake) = &*self.state;
        let mut state = lock
            .lock()
            .map_err(|_| ThemeWorkerRequestError::StateUnavailable)?;
        if state.shutdown {
            return Err(ThemeWorkerRequestError::Shutdown);
        }
        state.pending_persistence = Some(PersistenceRequest {
            generation,
            selected,
        });
        wake.notify_one();
        Ok(())
    }

    pub fn try_receive(&self) -> Option<WorkerResult> {
        self.results.as_ref()?.try_recv().ok()
    }

    pub fn recv_timeout(&self, timeout: Duration) -> Result<WorkerResult, mpsc::RecvTimeoutError> {
        self.results
            .as_ref()
            .map_or(Err(mpsc::RecvTimeoutError::Disconnected), |results| {
                results.recv_timeout(timeout)
            })
    }

    pub fn pending_capacity(&self) -> usize {
        let (lock, _) = &*self.state;
        lock.lock()
            .map(|state| {
                usize::from(state.pending_refresh.is_some())
                    + usize::from(state.pending_persistence.is_some())
            })
            .unwrap_or(2)
    }
}

impl Drop for ThemeWorker {
    fn drop(&mut self) {
        let (lock, wake) = &*self.state;
        let mut state = match lock.lock() {
            Ok(state) => state,
            Err(poisoned) => poisoned.into_inner(),
        };
        state.shutdown = true;
        state.pending_refresh = None;
        state.pending_persistence = None;
        wake.notify_all();
    }
}

fn worker_loop<F>(
    state: Arc<(Mutex<WorkerState>, Condvar)>,
    callback: F,
    search_roots: Vec<PathBuf>,
    store: ThemePreferenceStore,
    scanner: Scanner,
) where
    F: Fn(WorkerResult),
{
    loop {
        let work = {
            let (lock, wake) = &*state;
            let mut state = match lock.lock() {
                Ok(state) => state,
                Err(_) => break,
            };
            let mut wait_failed = false;
            while state.pending_refresh.is_none()
                && state.pending_persistence.is_none()
                && !state.shutdown
            {
                match wake.wait(state) {
                    Ok(next_state) => state = next_state,
                    Err(poisoned) => {
                        state = poisoned.into_inner();
                        wait_failed = true;
                        break;
                    }
                }
            }
            if wait_failed || state.shutdown {
                None
            } else if let Some(request) = state.pending_persistence.take() {
                Some(ThemeWork::Persistence(request))
            } else {
                state
                    .pending_refresh
                    .take()
                    .map(|generation| ThemeWork::Refresh { generation })
            }
        };
        let Some(work) = work else {
            break;
        };
        let result = match work {
            ThemeWork::Refresh { generation } => WorkerResult::Refresh {
                generation,
                snapshot: scanner(&search_roots).map(|mut snapshot| {
                    snapshot.configured_selection = store.load().map_err(|error| error.to_string());
                    snapshot
                }),
            },
            ThemeWork::Persistence(request) => {
                let result = match request.selected.as_deref() {
                    Some(theme_id) => store.save_selected(theme_id),
                    None => store.clear_selected(),
                }
                .map_err(|error| error.to_string());
                WorkerResult::Persistence {
                    generation: request.generation,
                    selected: request.selected,
                    result,
                }
            }
        };
        if worker_is_shutting_down(&state) {
            break;
        }
        callback(result);
    }

    let (lock, wake) = &*state;
    let mut state = match lock.lock() {
        Ok(state) => state,
        Err(poisoned) => poisoned.into_inner(),
    };
    state.exited = true;
    wake.notify_all();
}

fn worker_is_shutting_down(state: &Arc<(Mutex<WorkerState>, Condvar)>) -> bool {
    let (lock, _) = &**state;
    lock.lock().map_or(true, |state| state.shutdown)
}

fn scan_theme_snapshot(search_roots: &[PathBuf]) -> Result<ThemeSnapshot, String> {
    ThemeCatalog::discover(search_roots)
        .map(|catalog| {
            let resolver = PreviewResolver::new(catalog.clone());
            let previews = catalog
                .user_visible()
                .into_iter()
                .map(|theme| (theme.id.clone(), previews_for_theme(&resolver, theme)))
                .collect();
            ThemeSnapshot {
                catalog,
                previews,
                configured_selection: Ok(None),
            }
        })
        .map_err(|error| error.to_string())
}

fn previews_for_theme(resolver: &PreviewResolver, theme: &ThemeDescriptor) -> Vec<Option<PathBuf>> {
    let mut previews =
        Vec::with_capacity(PREVIEW_ICON_NAMES.len() + usize::from(theme.example.is_some()));
    if let Some(example) = theme.example.as_deref()
        && let Some(path) = resolver.resolve(&theme.id, example, 48, 1)
    {
        previews.push(Some(path));
    }
    for icon in PREVIEW_ICON_NAMES {
        let path = resolver.resolve(&theme.id, icon, 48, 1);
        if path.as_ref().is_some_and(|path| {
            previews
                .iter()
                .any(|preview| preview.as_ref() == Some(path))
        }) {
            continue;
        }
        previews.push(path);
    }
    previews
}

#[cfg(test)]
mod tests {
    use super::{PreviewResolver, previews_for_theme};
    use super::{ThemeSnapshot, ThemeWorker, WorkerResult};
    use crate::themes::catalog::ThemeCatalog;
    use crate::themes::config::ThemePreferenceStore;
    use serde_json::json;
    use std::fs;
    use std::path::PathBuf;
    use std::sync::{Arc, Condvar, Mutex, mpsc};
    use std::thread;
    use std::time::Duration;
    use tempfile::TempDir;

    fn empty_snapshot() -> ThemeSnapshot {
        ThemeSnapshot {
            catalog: ThemeCatalog::discover(&[]).unwrap(),
            previews: Default::default(),
            configured_selection: Ok(None),
        }
    }

    fn write_theme(root: &std::path::Path, id: &str, metadata: &str) {
        let theme = root.join(id);
        fs::create_dir_all(&theme).unwrap();
        fs::write(theme.join("index.theme"), metadata).unwrap();
    }

    fn write_icon(root: &std::path::Path, theme: &str, name: &str) {
        let path = root.join(theme).join("48x48/apps");
        fs::create_dir_all(&path).unwrap();
        fs::write(path.join(format!("{name}.png")), b"preview").unwrap();
    }

    fn spawn_with_scanner<F, S>(
        store: ThemePreferenceStore,
        callback: F,
        scanner: S,
    ) -> Result<ThemeWorker, super::ThemeWorkerInitError>
    where
        F: Fn(WorkerResult) + Send + 'static,
        S: Fn(&[PathBuf]) -> Result<ThemeSnapshot, String> + Send + 'static,
    {
        ThemeWorker::spawn_with(Vec::new(), store, None, callback, scanner, |run| {
            thread::Builder::new().spawn(run)
        })
    }

    fn wait_until_exited(state: &Arc<(Mutex<super::WorkerState>, Condvar)>) -> bool {
        let (lock, wake) = &**state;
        let Ok(state) = lock.lock() else {
            return false;
        };
        let Ok((state, _)) =
            wake.wait_timeout_while(state, Duration::from_secs(1), |state| !state.exited)
        else {
            return false;
        };
        state.exited
    }

    #[test]
    fn repeated_refresh_requests_keep_one_pending_work_item() {
        let root = TempDir::new().unwrap();
        let worker = ThemeWorker::new(
            vec![root.path().into()],
            ThemePreferenceStore::new(root.path().join("theme.json")),
        )
        .unwrap();
        for _ in 0..128 {
            worker.request_refresh().unwrap();
        }
        assert!(worker.pending_capacity() <= 1);
        let result = worker.recv_timeout(Duration::from_secs(1)).unwrap();
        let WorkerResult::Refresh {
            generation,
            snapshot,
        } = result
        else {
            panic!("expected a refresh result");
        };
        assert!(generation > 0);
        assert!(snapshot.is_ok());
    }

    #[test]
    fn dropping_worker_during_outstanding_scan_is_non_blocking_and_discards_result() {
        let directory = TempDir::new().unwrap();
        let (scan_started_tx, scan_started_rx) = mpsc::sync_channel(1);
        let (release_scan_tx, release_scan_rx) = mpsc::sync_channel(1);
        let (scan_finished_tx, scan_finished_rx) = mpsc::sync_channel(1);
        let (result_tx, result_rx) = mpsc::sync_channel(1);
        let worker = spawn_with_scanner(
            ThemePreferenceStore::new(directory.path().join("theme.json")),
            move |result| {
                let _ = result_tx.try_send(result);
            },
            move |_| {
                let _ = scan_started_tx.send(());
                let _ = release_scan_rx.recv();
                let _ = scan_finished_tx.send(());
                Ok(empty_snapshot())
            },
        )
        .unwrap();
        worker.request_refresh().unwrap();
        scan_started_rx
            .recv_timeout(Duration::from_secs(1))
            .unwrap();

        let state = Arc::clone(&worker.state);
        let (dropped_tx, dropped_rx) = mpsc::sync_channel(1);
        let drop_thread = thread::spawn(move || {
            drop(worker);
            let _ = dropped_tx.send(());
        });
        let dropped_before_scan_release =
            dropped_rx.recv_timeout(Duration::from_millis(100)).is_ok();

        let _ = release_scan_tx.send(());
        let _ = drop_thread.join();
        scan_finished_rx
            .recv_timeout(Duration::from_secs(1))
            .unwrap();
        assert!(wait_until_exited(&state));
        assert!(result_rx.try_recv().is_err());
        assert!(
            dropped_before_scan_release,
            "dropping the worker waited for the injected scan to finish"
        );
    }

    #[test]
    fn dropping_idle_worker_wakes_it_and_waits_for_no_join() {
        let directory = TempDir::new().unwrap();
        let worker = ThemeWorker::new_with_callback(
            Vec::new(),
            ThemePreferenceStore::new(directory.path().join("theme.json")),
            |_| {},
        )
        .unwrap();
        let state = Arc::clone(&worker.state);

        drop(worker);

        assert!(wait_until_exited(&state));
    }

    #[test]
    fn selection_persistence_completes_on_worker_thread_and_preserves_other_keys() {
        let directory = TempDir::new().unwrap();
        let path = directory.path().join("theme.json");
        fs::write(&path, json!({"other": true}).to_string()).unwrap();
        let (result_tx, result_rx) = mpsc::sync_channel(1);
        let worker = ThemeWorker::new_with_callback(
            Vec::new(),
            ThemePreferenceStore::new(path.clone()),
            move |result| {
                let _ = result_tx.send((thread::current().id(), result));
            },
        )
        .unwrap();
        let caller_thread = thread::current().id();

        worker
            .request_persistence(17, Some(String::from("theme-a")))
            .unwrap();

        let (worker_thread, result) = result_rx.recv_timeout(Duration::from_secs(1)).unwrap();
        assert_ne!(worker_thread, caller_thread);
        assert!(matches!(
            result,
            WorkerResult::Persistence {
                generation: 17,
                selected: Some(ref selected),
                result: Ok(()),
            } if selected == "theme-a"
        ));
        let saved: serde_json::Value = serde_json::from_slice(&fs::read(path).unwrap()).unwrap();
        assert_eq!(saved["system_icon_theme"], "theme-a");
        assert_eq!(saved["other"], true);
    }

    #[test]
    fn persistence_failure_is_returned_as_a_worker_result() {
        let directory = TempDir::new().unwrap();
        let parent = directory.path().join("not-a-directory");
        fs::write(&parent, b"file").unwrap();
        let (result_tx, result_rx) = mpsc::sync_channel(1);
        let worker = ThemeWorker::new_with_callback(
            Vec::new(),
            ThemePreferenceStore::new(parent.join("theme.json")),
            move |result| {
                let _ = result_tx.send(result);
            },
        )
        .unwrap();

        worker
            .request_persistence(1, Some(String::from("theme-a")))
            .unwrap();

        let result = result_rx.recv_timeout(Duration::from_secs(1)).unwrap();
        assert!(matches!(
            result,
            WorkerResult::Persistence {
                generation: 1,
                result: Err(_),
                ..
            }
        ));
    }

    #[test]
    fn rapid_selection_requests_coalesce_to_the_newest_pending_write() {
        let directory = TempDir::new().unwrap();
        let path = directory.path().join("theme.json");
        let (scan_started_tx, scan_started_rx) = mpsc::sync_channel(1);
        let (release_scan_tx, release_scan_rx) = mpsc::sync_channel(1);
        let (result_tx, result_rx) = mpsc::sync_channel(4);
        let worker = spawn_with_scanner(
            ThemePreferenceStore::new(path.clone()),
            move |result| {
                let _ = result_tx.send(result);
            },
            move |_| {
                let _ = scan_started_tx.send(());
                let _ = release_scan_rx.recv();
                Ok(empty_snapshot())
            },
        )
        .unwrap();
        worker.request_refresh().unwrap();
        scan_started_rx
            .recv_timeout(Duration::from_secs(1))
            .unwrap();

        worker
            .request_persistence(1, Some(String::from("theme-a")))
            .unwrap();
        worker
            .request_persistence(2, Some(String::from("theme-b")))
            .unwrap();
        worker
            .request_persistence(3, Some(String::from("theme-c")))
            .unwrap();
        assert_eq!(worker.pending_capacity(), 1);
        release_scan_tx.send(()).unwrap();

        let _refresh_result = result_rx.recv_timeout(Duration::from_secs(1)).unwrap();
        let result = result_rx.recv_timeout(Duration::from_secs(1)).unwrap();
        assert!(matches!(
            result,
            WorkerResult::Persistence {
                generation: 3,
                selected: Some(ref selected),
                result: Ok(()),
            } if selected == "theme-c"
        ));
        assert!(result_rx.try_recv().is_err());
        let saved: serde_json::Value = serde_json::from_slice(&fs::read(path).unwrap()).unwrap();
        assert_eq!(saved["system_icon_theme"], "theme-c");
    }

    #[test]
    fn system_default_selection_is_persisted_as_a_removal() {
        let directory = TempDir::new().unwrap();
        let path = directory.path().join("theme.json");
        fs::write(
            &path,
            json!({"system_icon_theme": "theme-a", "other": 9}).to_string(),
        )
        .unwrap();
        let (result_tx, result_rx) = mpsc::sync_channel(1);
        let worker = ThemeWorker::new_with_callback(
            Vec::new(),
            ThemePreferenceStore::new(path.clone()),
            move |result| {
                let _ = result_tx.send(result);
            },
        )
        .unwrap();

        worker.request_persistence(2, None).unwrap();

        assert!(matches!(
            result_rx.recv_timeout(Duration::from_secs(1)).unwrap(),
            WorkerResult::Persistence {
                generation: 2,
                selected: None,
                result: Ok(()),
            }
        ));
        let saved: serde_json::Value = serde_json::from_slice(&fs::read(path).unwrap()).unwrap();
        assert!(saved.get("system_icon_theme").is_none());
        assert_eq!(saved["other"], 9);
    }

    #[test]
    fn thread_creation_failure_is_reported_as_a_typed_initialization_error() {
        let directory = TempDir::new().unwrap();
        let result = ThemeWorker::spawn_with(
            Vec::new(),
            ThemePreferenceStore::new(directory.path().join("theme.json")),
            None,
            |_| {},
            |_| Ok(empty_snapshot()),
            |_| Err(std::io::Error::other("injected thread creation failure")),
        );
        let error = result.err().expect("thread creation should fail");

        assert!(
            error
                .to_string()
                .contains("injected thread creation failure")
        );
    }

    #[test]
    fn example_preview_precedes_semantic_previews_and_failures_fall_back() {
        let root = TempDir::new().unwrap();
        write_theme(
            root.path(),
            "example-theme",
            "[Icon Theme]\nName=Example\nExample=theme-example\nDirectories=48x48/apps\n\n[48x48/apps]\nSize=48\nType=Fixed\n",
        );
        write_theme(
            root.path(),
            "missing-example-theme",
            "[Icon Theme]\nName=Missing Example\nExample=not-installed\nDirectories=48x48/apps\n\n[48x48/apps]\nSize=48\nType=Fixed\n",
        );
        write_theme(
            root.path(),
            "unsafe-example-theme",
            "[Icon Theme]\nName=Unsafe Example\nExample=../outside\nDirectories=48x48/apps\n\n[48x48/apps]\nSize=48\nType=Fixed\n",
        );
        write_icon(root.path(), "example-theme", "theme-example");
        write_icon(root.path(), "example-theme", "folder");
        write_icon(root.path(), "missing-example-theme", "folder");
        write_icon(root.path(), "unsafe-example-theme", "folder");

        let catalog = ThemeCatalog::discover(&[root.path().into()]).unwrap();
        let resolver = PreviewResolver::new(catalog.clone());
        let example = previews_for_theme(&resolver, catalog.theme("example-theme").unwrap());
        assert!(example[0].as_ref().unwrap().ends_with("theme-example.png"));
        assert!(example[1].as_ref().unwrap().ends_with("folder.png"));

        for theme_id in ["missing-example-theme", "unsafe-example-theme"] {
            let previews = previews_for_theme(&resolver, catalog.theme(theme_id).unwrap());
            assert!(previews[0].as_ref().unwrap().ends_with("folder.png"));
        }
    }
}
