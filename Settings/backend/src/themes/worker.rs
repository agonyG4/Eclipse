use super::catalog::{ThemeCatalog, default_search_roots};
use std::path::PathBuf;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Condvar, Mutex, mpsc};
use std::thread::{self, JoinHandle};
use std::time::Duration;

#[derive(Debug)]
pub struct WorkerResult {
    pub generation: u64,
    pub catalog: Result<ThemeCatalog, String>,
}

struct WorkerState {
    pending: Option<u64>,
    shutdown: bool,
}

pub struct ThemeWorker {
    state: Arc<(Mutex<WorkerState>, Condvar)>,
    next_generation: AtomicU64,
    results: mpsc::Receiver<WorkerResult>,
    thread: Option<JoinHandle<()>>,
}

impl ThemeWorker {
    pub fn new(search_roots: Vec<PathBuf>) -> Self {
        let state = Arc::new((
            Mutex::new(WorkerState {
                pending: None,
                shutdown: false,
            }),
            Condvar::new(),
        ));
        let worker_state = Arc::clone(&state);
        let (sender, results) = mpsc::sync_channel(1);
        let thread = thread::Builder::new()
            .name(String::from("astrea-settings-themes"))
            .spawn(move || worker_loop(worker_state, sender, search_roots))
            .expect("failed to create Settings Themes worker");
        Self {
            state,
            next_generation: AtomicU64::new(1),
            results,
            thread: Some(thread),
        }
    }

    pub fn default() -> Self {
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
        self.results.try_recv().ok()
    }

    pub fn recv_timeout(&self, timeout: Duration) -> Result<WorkerResult, mpsc::RecvTimeoutError> {
        self.results.recv_timeout(timeout)
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

fn worker_loop(
    state: Arc<(Mutex<WorkerState>, Condvar)>,
    sender: mpsc::SyncSender<WorkerResult>,
    search_roots: Vec<PathBuf>,
) {
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
        let result = ThemeCatalog::discover(&search_roots).map_err(|error| error.to_string());
        let output = WorkerResult {
            generation,
            catalog: result,
        };
        if let Err(mpsc::TrySendError::Full(output)) = sender.try_send(output) {
            let _ = sender.try_send(output);
        }
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
    }
}
