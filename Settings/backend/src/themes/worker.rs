use super::catalog::{ThemeCatalog, default_search_roots};
use super::icon_lookup::PreviewResolver;
use std::collections::HashMap;
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
pub struct WorkerResult {
    pub generation: u64,
    pub snapshot: Result<ThemeSnapshot, String>,
}

#[derive(Debug)]
pub struct ThemeSnapshot {
    pub catalog: ThemeCatalog,
    pub previews: HashMap<String, Vec<Option<PathBuf>>>,
}

struct WorkerState {
    pending: Option<u64>,
    shutdown: bool,
}

pub struct ThemeWorker {
    state: Arc<(Mutex<WorkerState>, Condvar)>,
    next_generation: AtomicU64,
    results: Option<mpsc::Receiver<WorkerResult>>,
    thread: Option<JoinHandle<()>>,
}

impl ThemeWorker {
    pub fn new(search_roots: Vec<PathBuf>) -> Self {
        let (sender, results) = mpsc::sync_channel(1);
        Self::spawn(search_roots, Some(results), move |result| {
            let _ = sender.try_send(result);
        })
    }

    pub fn new_with_callback<F>(search_roots: Vec<PathBuf>, callback: F) -> Self
    where
        F: Fn(WorkerResult) + Send + 'static,
    {
        Self::spawn(search_roots, None, callback)
    }

    fn spawn<F>(
        search_roots: Vec<PathBuf>,
        results: Option<mpsc::Receiver<WorkerResult>>,
        callback: F,
    ) -> Self
    where
        F: Fn(WorkerResult) + Send + 'static,
    {
        let state = Arc::new((
            Mutex::new(WorkerState {
                pending: None,
                shutdown: false,
            }),
            Condvar::new(),
        ));
        let worker_state = Arc::clone(&state);
        let thread = thread::Builder::new()
            .name(String::from("astrea-settings-themes"))
            .spawn(move || worker_loop(worker_state, callback, search_roots))
            .expect("failed to create Settings Themes worker");
        Self {
            state,
            next_generation: AtomicU64::new(1),
            results,
            thread: Some(thread),
        }
    }

    pub fn with_default_roots() -> Self {
        Self::new(default_search_roots())
    }

    pub fn request_refresh(&self) -> u64 {
        let generation = self.next_generation.fetch_add(1, Ordering::Relaxed);
        let (lock, wake) = &*self.state;
        if let Ok(mut state) = lock.lock() {
            state.pending = Some(generation);
            wake.notify_one();
        }
        generation
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
            .map(|state| usize::from(state.pending.is_some()))
            .unwrap_or(1)
    }
}

impl Drop for ThemeWorker {
    fn drop(&mut self) {
        let (lock, wake) = &*self.state;
        if let Ok(mut state) = lock.lock() {
            state.shutdown = true;
            wake.notify_one();
        }
        if let Some(thread) = self.thread.take() {
            let _ = thread.join();
        }
    }
}

fn worker_loop<F>(
    state: Arc<(Mutex<WorkerState>, Condvar)>,
    callback: F,
    search_roots: Vec<PathBuf>,
) where
    F: Fn(WorkerResult),
{
    loop {
        let generation = {
            let (lock, wake) = &*state;
            let mut state = match lock.lock() {
                Ok(state) => state,
                Err(_) => return,
            };
            while state.pending.is_none() && !state.shutdown {
                state = match wake.wait(state) {
                    Ok(state) => state,
                    Err(_) => return,
                };
            }
            if state.shutdown {
                return;
            }
            state.pending.take().unwrap_or(0)
        };
        let snapshot = ThemeCatalog::discover(&search_roots)
            .map(|catalog| {
                let resolver = PreviewResolver::new(catalog.clone());
                let previews = catalog
                    .user_visible()
                    .into_iter()
                    .map(|theme| {
                        let values = PREVIEW_ICON_NAMES
                            .into_iter()
                            .map(|icon| resolver.resolve(&theme.id, icon, 48))
                            .collect();
                        (theme.id.clone(), values)
                    })
                    .collect();
                ThemeSnapshot { catalog, previews }
            })
            .map_err(|error| error.to_string());
        callback(WorkerResult {
            generation,
            snapshot,
        });
    }
}

#[cfg(test)]
mod tests {
    use super::ThemeWorker;
    use std::time::Duration;
    use tempfile::TempDir;

    #[test]
    fn repeated_refresh_requests_keep_one_pending_work_item() {
        let root = TempDir::new().unwrap();
        let worker = ThemeWorker::new(vec![root.path().into()]);
        for _ in 0..128 {
            worker.request_refresh();
        }
        assert!(worker.pending_capacity() <= 1);
        let result = worker.recv_timeout(Duration::from_secs(1)).unwrap();
        assert!(result.generation > 0);
        assert!(result.snapshot.is_ok());
    }
}
