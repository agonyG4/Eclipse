use super::config::{AppearanceConfigStore, AppearancePatch};
use std::fmt::{Display, Formatter};
use std::io;
use std::sync::{Arc, Condvar, Mutex, mpsc};
use std::thread::{self, JoinHandle};
use std::time::Duration;

#[derive(Debug)]
pub struct AppearanceWorkerInitError(io::Error);

impl Display for AppearanceWorkerInitError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> std::fmt::Result {
        write!(
            formatter,
            "failed to create Settings Appearance worker: {}",
            self.0
        )
    }
}

impl std::error::Error for AppearanceWorkerInitError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        Some(&self.0)
    }
}

#[derive(Debug, Eq, PartialEq)]
pub enum AppearanceWorkerRequestError {
    Shutdown,
    StateUnavailable,
}

impl Display for AppearanceWorkerRequestError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Shutdown => {
                formatter.write_str("the Settings Appearance worker is shutting down")
            }
            Self::StateUnavailable => {
                formatter.write_str("the Settings Appearance worker state is unavailable")
            }
        }
    }
}

impl std::error::Error for AppearanceWorkerRequestError {}

#[derive(Debug)]
pub struct AppearanceWorkerResult {
    pub generation: u64,
    pub result: Result<(), String>,
}

struct AppearanceRequest {
    generation: u64,
    patch: AppearancePatch,
}

struct WorkerState {
    pending: Option<AppearanceRequest>,
    shutdown: bool,
}

type WorkerTask = Box<dyn FnOnce() + Send + 'static>;
type Writer = Box<dyn Fn(AppearancePatch) -> Result<(), String> + Send + 'static>;

pub struct AppearanceWorker {
    state: Arc<(Mutex<WorkerState>, Condvar)>,
    results: Option<mpsc::Receiver<AppearanceWorkerResult>>,
}

impl AppearanceWorker {
    pub fn new(store: AppearanceConfigStore) -> Result<Self, AppearanceWorkerInitError> {
        let (sender, results) = mpsc::sync_channel(1);
        Self::spawn(store, Some(results), move |result| {
            let _ = sender.send(result);
        })
    }

    pub fn new_with_callback<F>(
        store: AppearanceConfigStore,
        callback: F,
    ) -> Result<Self, AppearanceWorkerInitError>
    where
        F: Fn(AppearanceWorkerResult) + Send + 'static,
    {
        Self::spawn(store, None, callback)
    }

    fn spawn<F>(
        store: AppearanceConfigStore,
        results: Option<mpsc::Receiver<AppearanceWorkerResult>>,
        callback: F,
    ) -> Result<Self, AppearanceWorkerInitError>
    where
        F: Fn(AppearanceWorkerResult) + Send + 'static,
    {
        let writer_store = store.clone();
        Self::spawn_with(
            results,
            callback,
            Box::new(move |patch| writer_store.patch(patch).map_err(|error| error.to_string())),
            |run| {
                thread::Builder::new()
                    .name(String::from("astrea-settings-appearance"))
                    .spawn(run)
            },
        )
    }

    fn spawn_with<F, W, T>(
        results: Option<mpsc::Receiver<AppearanceWorkerResult>>,
        callback: F,
        writer: W,
        spawn_thread: T,
    ) -> Result<Self, AppearanceWorkerInitError>
    where
        F: Fn(AppearanceWorkerResult) + Send + 'static,
        W: Fn(AppearancePatch) -> Result<(), String> + Send + 'static,
        T: FnOnce(WorkerTask) -> io::Result<JoinHandle<()>>,
    {
        let state = Arc::new((
            Mutex::new(WorkerState {
                pending: None,
                shutdown: false,
            }),
            Condvar::new(),
        ));
        let worker_state = Arc::clone(&state);
        let writer: Writer = Box::new(writer);
        let run = Box::new(move || worker_loop(worker_state, callback, writer));
        let thread = spawn_thread(run).map_err(AppearanceWorkerInitError)?;
        drop(thread);
        Ok(Self { state, results })
    }

    pub fn request(
        &self,
        generation: u64,
        patch: AppearancePatch,
    ) -> Result<(), AppearanceWorkerRequestError> {
        let (lock, wake) = &*self.state;
        let mut state = lock
            .lock()
            .map_err(|_| AppearanceWorkerRequestError::StateUnavailable)?;
        if state.shutdown {
            return Err(AppearanceWorkerRequestError::Shutdown);
        }
        if let Some(pending) = state.pending.as_mut() {
            pending.patch.merge(patch);
            pending.generation = generation;
        } else {
            state.pending = Some(AppearanceRequest { generation, patch });
        }
        wake.notify_one();
        Ok(())
    }

    pub fn try_receive(&self) -> Option<AppearanceWorkerResult> {
        self.results.as_ref()?.try_recv().ok()
    }

    pub fn recv_timeout(
        &self,
        timeout: Duration,
    ) -> Result<AppearanceWorkerResult, mpsc::RecvTimeoutError> {
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

    #[cfg(test)]
    fn request_shutdown(&self) {
        let (lock, wake) = &*self.state;
        let mut state = lock.lock().unwrap();
        state.shutdown = true;
        state.pending = None;
        wake.notify_all();
    }
}

impl Drop for AppearanceWorker {
    fn drop(&mut self) {
        let (lock, wake) = &*self.state;
        let mut state = match lock.lock() {
            Ok(state) => state,
            Err(poisoned) => poisoned.into_inner(),
        };
        state.shutdown = true;
        state.pending = None;
        wake.notify_all();
    }
}

fn worker_loop<F>(state: Arc<(Mutex<WorkerState>, Condvar)>, callback: F, writer: Writer)
where
    F: Fn(AppearanceWorkerResult),
{
    loop {
        let request = {
            let (lock, wake) = &*state;
            let mut state = match lock.lock() {
                Ok(state) => state,
                Err(_) => break,
            };
            let mut wait_failed = false;
            while state.pending.is_none() && !state.shutdown {
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
            } else {
                state.pending.take()
            }
        };
        let Some(request) = request else {
            break;
        };
        let result = writer(request.patch);
        if worker_is_shutting_down(&state) {
            break;
        }
        callback(AppearanceWorkerResult {
            generation: request.generation,
            result,
        });
    }
}

fn worker_is_shutting_down(state: &Arc<(Mutex<WorkerState>, Condvar)>) -> bool {
    let (lock, _) = &**state;
    lock.lock().map_or(true, |state| state.shutdown)
}

#[cfg(test)]
mod tests {
    use super::{AppearanceWorker, AppearanceWorkerInitError};
    use crate::appearance::config::{AppearanceConfigStore, AppearancePatch};
    use serde_json::Value;
    use std::io;
    use std::sync::{Arc, Mutex, mpsc};
    use std::thread;
    use std::time::Duration;
    use tempfile::TempDir;

    fn wait_until_started(receiver: &mpsc::Receiver<()>) {
        receiver.recv_timeout(Duration::from_secs(2)).unwrap();
    }

    #[test]
    fn rapid_requests_keep_one_pending_patch_latest_value_wins_and_other_fields_merge() {
        let directory = TempDir::new().unwrap();
        let config_path = directory.path().join("theme.json");
        std::fs::write(&config_path, br#"{"theme_preference":"auto"}"#).unwrap();
        let (started_tx, started_rx) = mpsc::channel();
        let (release_tx, release_rx) = mpsc::channel();
        let (patch_tx, patch_rx) = mpsc::channel();
        let release_rx = Arc::new(Mutex::new(release_rx));
        let write_path = config_path.clone();
        let worker = AppearanceWorker::spawn_with(
            None,
            move |result| {
                patch_tx.send(result).unwrap();
            },
            {
                let started_tx = started_tx.clone();
                let release_rx = Arc::clone(&release_rx);
                let store = AppearanceConfigStore::new(write_path);
                move |patch| {
                    if patch.accent.as_deref() == Some("red") {
                        started_tx.send(()).unwrap();
                        release_rx
                            .lock()
                            .unwrap()
                            .recv_timeout(Duration::from_secs(2))
                            .unwrap();
                    }
                    store.patch(patch).map_err(|error| error.to_string())
                }
            },
            |run| thread::Builder::new().spawn(run),
        )
        .unwrap();
        worker.request(1, AppearancePatch::accent("red")).unwrap();
        wait_until_started(&started_rx);
        worker.request(2, AppearancePatch::accent("blue")).unwrap();
        worker
            .request(3, AppearancePatch::icon_appearance("tinted"))
            .unwrap();
        worker.request(4, AppearancePatch::accent("green")).unwrap();
        assert_eq!(worker.pending_capacity(), 1);
        release_tx.send(()).unwrap();

        let first = patch_rx.recv_timeout(Duration::from_secs(2)).unwrap();
        let second = patch_rx.recv_timeout(Duration::from_secs(2)).unwrap();
        assert_eq!(first.generation, 1);
        assert_eq!(second.generation, 4);
        assert!(first.result.is_ok());
        assert!(second.result.is_ok());
        let saved: Value = serde_json::from_slice(&std::fs::read(config_path).unwrap()).unwrap();
        assert_eq!(saved["accent"], "green");
        assert_eq!(saved["icon_appearance"], "tinted");
        assert_eq!(saved["theme_preference"], "auto");
    }

    #[test]
    fn failed_commit_is_returned_as_an_error_result() {
        let (result_tx, result_rx) = mpsc::channel();
        let worker = AppearanceWorker::spawn_with(
            None,
            move |result| result_tx.send(result).unwrap(),
            |_| Err(String::from("injected write failure")),
            |run| thread::Builder::new().spawn(run),
        )
        .unwrap();
        worker.request(1, AppearancePatch::accent("blue")).unwrap();
        let result = result_rx.recv_timeout(Duration::from_secs(2)).unwrap();
        assert_eq!(result.generation, 1);
        assert_eq!(result.result, Err(String::from("injected write failure")));
    }

    #[test]
    fn thread_creation_failure_is_reported_as_a_typed_initialization_error() {
        let result = AppearanceWorker::spawn_with(
            None,
            |_| {},
            |_| Ok(()),
            |_| Err(io::Error::other("injected thread failure")),
        );
        assert!(matches!(result, Err(AppearanceWorkerInitError(_))));
    }

    #[test]
    fn shutdown_rejects_a_new_request() {
        let (result_tx, _result_rx) = mpsc::channel();
        let worker = AppearanceWorker::spawn_with(
            None,
            move |result| result_tx.send(result).unwrap(),
            |_| Ok(()),
            |run| thread::Builder::new().spawn(run),
        )
        .unwrap();
        worker.request_shutdown();
        assert_eq!(
            worker.request(1, AppearancePatch::accent("blue")),
            Err(super::AppearanceWorkerRequestError::Shutdown)
        );
    }
}
