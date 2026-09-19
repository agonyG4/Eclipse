use super::discovery::{DiscoveryError, discover_socket_from_environment};
use super::protocol::{
    AnimationRequest, ProtocolError, ProtocolOutcome, decode_response, encode_request,
};
use std::fmt::{Display, Formatter};
use std::io::{ErrorKind, Read, Write};
use std::os::fd::{FromRawFd, IntoRawFd, OwnedFd};
use std::os::unix::net::UnixStream;
use std::sync::mpsc::{Receiver, SyncSender, TrySendError, sync_channel};
use std::sync::{Arc, Mutex};
use std::thread::{self, JoinHandle};
use std::time::{Duration, Instant};

pub const DEFAULT_DEADLINE: Duration = Duration::from_millis(2000);
const DEBOUNCE: Duration = Duration::from_millis(80);

#[derive(Debug)]
pub enum ClientError {
    Discovery(DiscoveryError),
    Request(ProtocolError),
    Transport(String),
    Timeout,
    WorkerUnavailable,
}

impl Display for ClientError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Discovery(error) => write!(formatter, "Typhon control: {}", error),
            Self::Request(error) => write!(formatter, "Typhon control: {}", error),
            Self::Transport(error) => write!(formatter, "Typhon control: {error}"),
            Self::Timeout => formatter.write_str("Typhon control: response timeout"),
            Self::WorkerUnavailable => formatter.write_str("Typhon control: worker is unavailable"),
        }
    }
}

impl Display for DiscoveryError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> std::fmt::Result {
        let message = match self {
            Self::RuntimeNotSecure => "XDG_RUNTIME_DIR is not secure",
            Self::TyphonRuntimeNotSecure => "Typhon runtime directory is not secure",
            Self::NoSecureInstance => "no secure Typhon instance found",
            Self::MultipleInstances => "multiple Typhon instances found",
        };
        formatter.write_str(message)
    }
}

impl Display for ProtocolError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> std::fmt::Result {
        let message = match self {
            Self::RequestTooLarge => "request exceeds 64 KiB",
            Self::ResponseTooLarge => "response exceeds 1 MiB",
            Self::InvalidFraming => "response framing is invalid",
            Self::InvalidJson => "response JSON is invalid",
            Self::Incompatible => "response protocol is incompatible",
            Self::MismatchedId => "response id does not match request",
            Self::InvalidSuccessFlag => "response success flag is invalid",
            Self::MissingResult => "successful response has no result object",
            Self::MissingError => "error response has no error object",
        };
        formatter.write_str(message)
    }
}

#[derive(Debug)]
pub enum WorkerEvent {
    RequestFinished {
        id: u64,
        result: Box<Result<ProtocolOutcome, ClientError>>,
    },
    DebounceElapsed {
        token: u64,
    },
}

enum WorkerMessage {
    Request { id: u64, request: AnimationRequest },
    DebounceElapsed { token: u64 },
}

struct DebounceState {
    deadline: Mutex<Option<(Instant, u64)>>,
}

pub struct ClientWorker {
    sender: SyncSender<WorkerMessage>,
    debounce: Arc<DebounceState>,
    thread: Option<JoinHandle<()>>,
}

impl ClientWorker {
    pub fn new<F>(completion: F) -> Result<Self, ClientError>
    where
        F: Fn(WorkerEvent) + Send + Sync + 'static,
    {
        Self::new_with_deadline(DEFAULT_DEADLINE, completion)
    }

    fn new_with_deadline<F>(deadline: Duration, completion: F) -> Result<Self, ClientError>
    where
        F: Fn(WorkerEvent) + Send + Sync + 'static,
    {
        let (sender, receiver) = sync_channel(1);
        let debounce = Arc::new(DebounceState {
            deadline: Mutex::new(None),
        });
        let worker_debounce = Arc::clone(&debounce);
        let completion = Arc::new(completion);
        let worker_completion = Arc::clone(&completion);
        let thread = thread::Builder::new()
            .name(String::from("astrea-settings-typhon"))
            .spawn(move || worker_loop(receiver, worker_debounce, worker_completion, deadline))
            .map_err(|_| ClientError::WorkerUnavailable)?;
        Ok(Self {
            sender,
            debounce,
            thread: Some(thread),
        })
    }

    pub fn submit(&self, id: u64, request: AnimationRequest) -> Result<(), ClientError> {
        self.sender
            .try_send(WorkerMessage::Request { id, request })
            .map_err(|error| match error {
                TrySendError::Full(_) => ClientError::WorkerUnavailable,
                TrySendError::Disconnected(_) => ClientError::WorkerUnavailable,
            })
    }

    pub fn schedule_debounce(&self, token: u64) {
        if let Ok(mut deadline) = self.debounce.deadline.lock() {
            *deadline = Some((Instant::now() + DEBOUNCE, token));
        }
    }
}

impl Drop for ClientWorker {
    fn drop(&mut self) {
        if let Some(thread) = self.thread.take() {
            drop(thread);
        }
    }
}

fn worker_loop<F>(
    receiver: Receiver<WorkerMessage>,
    debounce: Arc<DebounceState>,
    completion: Arc<F>,
    deadline: Duration,
) where
    F: Fn(WorkerEvent) + Send + Sync + 'static,
{
    loop {
        let message = next_message(&receiver, &debounce);
        let Some(message) = message else {
            return;
        };
        match message {
            WorkerMessage::Request { id, request } => {
                let result = execute_request(id, request, deadline);
                completion(WorkerEvent::RequestFinished {
                    id,
                    result: Box::new(result),
                });
            }
            WorkerMessage::DebounceElapsed { token } => {
                completion(WorkerEvent::DebounceElapsed { token });
            }
        }
    }
}

fn next_message(
    receiver: &Receiver<WorkerMessage>,
    debounce: &DebounceState,
) -> Option<WorkerMessage> {
    const POLL_INTERVAL: Duration = Duration::from_millis(10);
    loop {
        let deadline = debounce.deadline.lock().ok()?.to_owned();
        let Some((deadline, token)) = deadline else {
            match receiver.recv_timeout(POLL_INTERVAL) {
                Ok(message) => return Some(message),
                Err(std::sync::mpsc::RecvTimeoutError::Timeout) => continue,
                Err(std::sync::mpsc::RecvTimeoutError::Disconnected) => return None,
            }
        };
        let remaining = deadline
            .saturating_duration_since(Instant::now())
            .min(POLL_INTERVAL);
        match receiver.recv_timeout(remaining) {
            Ok(message) => return Some(message),
            Err(std::sync::mpsc::RecvTimeoutError::Timeout) => {
                let mut current = debounce.deadline.lock().ok()?;
                if current.is_some_and(|(current_deadline, current_token)| {
                    current_token == token && current_deadline <= Instant::now()
                }) {
                    *current = None;
                    return Some(WorkerMessage::DebounceElapsed { token });
                }
            }
            Err(std::sync::mpsc::RecvTimeoutError::Disconnected) => return None,
        }
    }
}

pub fn execute_request(
    id: u64,
    request: AnimationRequest,
    deadline: Duration,
) -> Result<ProtocolOutcome, ClientError> {
    let deadline = Instant::now() + deadline;
    let path = discover_socket_from_environment().map_err(ClientError::Discovery)?;
    let encoded = encode_request(id, request).map_err(ClientError::Request)?;
    let mut stream = connect(&path, deadline)?;
    let remaining = remaining(deadline)?;
    stream
        .set_write_timeout(Some(remaining))
        .map_err(|_| transport_failure())?;
    stream
        .write_all(&encoded)
        .map_err(|error| transport_or_timeout(error, deadline))?;
    read_response(&mut stream, id, deadline)
}

fn connect(path: &std::path::Path, deadline: Instant) -> Result<UnixStream, ClientError> {
    let address = socket2::SockAddr::unix(path).map_err(|_| transport_failure())?;
    let socket = socket2::Socket::new(socket2::Domain::UNIX, socket2::Type::STREAM, None)
        .map_err(|_| transport_failure())?;
    socket
        .connect_timeout(&address, remaining(deadline)?)
        .map_err(|error| transport_or_timeout(error, deadline))?;
    let descriptor = socket.into_raw_fd();
    // SAFETY: the descriptor is owned by `socket` and transferred exactly once.
    let descriptor = unsafe { OwnedFd::from_raw_fd(descriptor) };
    Ok(UnixStream::from(descriptor))
}

fn read_response(
    stream: &mut UnixStream,
    id: u64,
    deadline: Instant,
) -> Result<ProtocolOutcome, ClientError> {
    let mut response = Vec::new();
    let mut chunk = [0_u8; 8192];
    loop {
        let remaining = remaining(deadline)?;
        stream
            .set_read_timeout(Some(remaining))
            .map_err(|_| transport_failure())?;
        let count = stream
            .read(&mut chunk)
            .map_err(|error| transport_or_timeout(error, deadline))?;
        if count == 0 {
            return Err(ClientError::Transport(String::from("socket disconnected")));
        }
        response.extend_from_slice(&chunk[..count]);
        if response.len() > super::protocol::MAX_RESPONSE_BYTES {
            return Err(ClientError::Request(ProtocolError::ResponseTooLarge));
        }
        if response.contains(&b'\n') {
            break;
        }
    }
    decode_response(&response, id).map_err(ClientError::Request)
}

fn remaining(deadline: Instant) -> Result<Duration, ClientError> {
    let remaining = deadline.saturating_duration_since(Instant::now());
    if remaining.is_zero() {
        Err(ClientError::Timeout)
    } else {
        Ok(remaining)
    }
}

fn transport_or_timeout(error: std::io::Error, deadline: Instant) -> ClientError {
    if matches!(error.kind(), ErrorKind::TimedOut | ErrorKind::WouldBlock) {
        let _ = deadline;
        return ClientError::Timeout;
    }
    let _ = error;
    transport_failure()
}

fn transport_failure() -> ClientError {
    ClientError::Transport(String::from("socket transport failed"))
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::{BufRead, BufReader, Write};
    use std::os::unix::fs::PermissionsExt;
    use std::os::unix::net::{UnixListener, UnixStream};
    use std::path::Path;
    use std::sync::mpsc::{Receiver, channel, sync_channel};
    use std::sync::{
        MutexGuard, OnceLock,
        atomic::{AtomicBool, Ordering},
    };
    use std::thread::{self, JoinHandle};
    use tempfile::{TempDir, tempdir};

    static ENVIRONMENT_LOCK: OnceLock<std::sync::Mutex<()>> = OnceLock::new();

    #[test]
    fn successful_request_completes_on_the_worker_thread() {
        let (_environment_lock, runtime) = test_runtime();
        let listener = runtime.listener().try_clone().unwrap();
        let server = serve_requests(listener, 1, |_| {});
        let caller = thread::current().id();
        let (events, receive) = channel();
        let worker = ClientWorker::new(move |event| {
            events.send((thread::current().id(), event)).unwrap();
        })
        .unwrap();

        worker.submit(1, AnimationRequest::Get).unwrap();
        let (callback_thread, event) = receive.recv_timeout(DEFAULT_DEADLINE * 2).unwrap();
        assert_ne!(callback_thread, caller);
        assert!(
            matches!(event, WorkerEvent::RequestFinished { id: 1, result } if matches!(*result, Ok(ProtocolOutcome::Success(_))))
        );
        server.join().unwrap();
    }

    #[test]
    fn response_timeout_is_reported_by_the_transport_path() {
        let (_environment_lock, runtime) = test_runtime();
        let listener = runtime.listener().try_clone().unwrap();
        let server = thread::spawn(move || {
            let (_stream, _) = listener.accept().unwrap();
            thread::sleep(Duration::from_millis(100));
        });

        let result = execute_request(1, AnimationRequest::Get, Duration::from_millis(20));
        assert!(matches!(result, Err(ClientError::Timeout)));
        server.join().unwrap();
    }

    #[test]
    fn connection_failure_is_not_reported_as_a_timeout() {
        let (_environment_lock, mut runtime) = test_runtime();
        drop(runtime.take_listener());

        let result = execute_request(1, AnimationRequest::Get, Duration::from_millis(100));
        assert!(matches!(result, Err(ClientError::Transport(_))));
    }

    #[test]
    fn request_lane_rejects_submission_when_the_worker_and_queue_are_busy() {
        let (_environment_lock, runtime) = test_runtime();
        let listener = runtime.listener().try_clone().unwrap();
        let (accepted, accepted_receive) = sync_channel(0);
        let (release, release_receive) = sync_channel(0);
        let server = thread::spawn(move || {
            let (mut stream, _) = listener.accept().unwrap();
            read_request(&mut stream);
            accepted.send(()).unwrap();
            release_receive.recv().unwrap();
            stream.write_all(&success_response(1)).unwrap();
        });
        let (events, receive) = channel();
        let worker = ClientWorker::new(move |event| events.send(event).unwrap()).unwrap();

        worker.submit(1, AnimationRequest::Get).unwrap();
        accepted_receive
            .recv_timeout(Duration::from_secs(1))
            .unwrap();
        worker.submit(2, AnimationRequest::Get).unwrap();
        assert!(matches!(
            worker.submit(3, AnimationRequest::Get),
            Err(ClientError::WorkerUnavailable)
        ));

        release.send(()).unwrap();
        let event = receive.recv_timeout(DEFAULT_DEADLINE * 2).unwrap();
        assert!(
            matches!(event, WorkerEvent::RequestFinished { id: 1, result } if matches!(*result, Ok(ProtocolOutcome::Success(_))))
        );
        server.join().unwrap();
    }

    #[test]
    fn worker_can_be_reused_after_a_completed_request() {
        let (_environment_lock, runtime) = test_runtime();
        let listener = runtime.listener().try_clone().unwrap();
        let server = serve_requests(listener, 2, |_| {});
        let (events, receive) = channel();
        let worker = ClientWorker::new(move |event| events.send(event).unwrap()).unwrap();

        worker.submit(1, AnimationRequest::Get).unwrap();
        assert_request_succeeded(&receive, 1);
        worker.submit(2, AnimationRequest::Get).unwrap();
        assert_request_succeeded(&receive, 2);
        server.join().unwrap();
    }

    #[test]
    fn dropping_worker_with_outstanding_io_is_safe() {
        let (_environment_lock, runtime) = test_runtime();
        let listener = runtime.listener().try_clone().unwrap();
        let (accepted, accepted_receive) = sync_channel(0);
        let server = thread::spawn(move || {
            let (_stream, _) = listener.accept().unwrap();
            accepted.send(()).unwrap();
            thread::sleep(Duration::from_millis(100));
        });
        let completed = std::sync::Arc::new(AtomicBool::new(false));
        let completion_flag = std::sync::Arc::clone(&completed);
        let (events, receive) = channel();
        let worker = ClientWorker::new_with_deadline(Duration::from_millis(20), move |event| {
            completion_flag.store(true, Ordering::Release);
            events.send(event).unwrap();
        })
        .unwrap();

        worker.submit(1, AnimationRequest::Get).unwrap();
        accepted_receive
            .recv_timeout(Duration::from_secs(1))
            .unwrap();
        drop(worker);
        let _ = receive.recv_timeout(Duration::from_secs(1));
        assert!(completed.load(Ordering::Acquire));
        server.join().unwrap();
    }

    #[test]
    fn response_size_limit_is_enforced_after_transport_read() {
        let (_environment_lock, runtime) = test_runtime();
        let listener = runtime.listener().try_clone().unwrap();
        let server = thread::spawn(move || {
            let (mut stream, _) = listener.accept().unwrap();
            read_request(&mut stream);
            stream
                .write_all(&vec![b'x'; super::super::protocol::MAX_RESPONSE_BYTES + 1])
                .unwrap();
        });

        let result = execute_request(1, AnimationRequest::Get, Duration::from_secs(1));
        assert!(matches!(
            result,
            Err(ClientError::Request(ProtocolError::ResponseTooLarge))
        ));
        server.join().unwrap();
    }

    #[test]
    fn timeout_form_socket_errors_are_classified_as_timeouts_at_the_deadline_boundary() {
        let future = Instant::now() + Duration::from_secs(1);
        assert!(matches!(
            transport_or_timeout(std::io::Error::from(ErrorKind::TimedOut), future),
            ClientError::Timeout
        ));
        assert!(matches!(
            transport_or_timeout(std::io::Error::from(ErrorKind::WouldBlock), future),
            ClientError::Timeout
        ));
        assert!(matches!(
            transport_or_timeout(std::io::Error::from(ErrorKind::BrokenPipe), future),
            ClientError::Transport(_)
        ));
    }

    fn assert_request_succeeded(receive: &Receiver<WorkerEvent>, expected_id: u64) {
        let event = receive.recv_timeout(DEFAULT_DEADLINE * 2).unwrap();
        assert!(
            matches!(event, WorkerEvent::RequestFinished { id, result } if id == expected_id && matches!(*result, Ok(ProtocolOutcome::Success(_))))
        );
    }

    fn serve_requests<F>(listener: UnixListener, count: usize, after_read: F) -> JoinHandle<()>
    where
        F: Fn(usize) + Send + Sync + 'static,
    {
        thread::spawn(move || {
            for index in 0..count {
                let (mut stream, _) = listener.accept().unwrap();
                read_request(&mut stream);
                after_read(index);
                stream
                    .write_all(&success_response(index as u64 + 1))
                    .unwrap();
            }
        })
    }

    fn read_request(stream: &mut UnixStream) {
        let mut request = Vec::new();
        BufReader::new(stream.try_clone().unwrap())
            .read_until(b'\n', &mut request)
            .unwrap();
        assert!(request.ends_with(b"\n"));
    }

    fn success_response(id: u64) -> Vec<u8> {
        let response = serde_json::json!({
            "protocol": "astrea.control",
            "version": 1,
            "id": id,
            "ok": true,
            "result": {
                "config": {"enabled": true, "preset": "astrea", "speed": 1.0}
            }
        });
        serde_json::to_vec(&response)
            .unwrap()
            .into_iter()
            .chain(std::iter::once(b'\n'))
            .collect()
    }

    fn test_runtime() -> (MutexGuard<'static, ()>, TestRuntime) {
        let lock = ENVIRONMENT_LOCK
            .get_or_init(|| std::sync::Mutex::new(()))
            .lock()
            .unwrap();
        let runtime = TestRuntime::new();
        // SAFETY: tests serialize changes to these process-wide variables with the mutex above.
        unsafe {
            std::env::set_var("XDG_RUNTIME_DIR", runtime.path());
            std::env::set_var("WAYLAND_DISPLAY", "test");
        }
        (lock, runtime)
    }

    struct TestRuntime {
        directory: TempDir,
        listener: Option<UnixListener>,
    }

    impl TestRuntime {
        fn new() -> Self {
            let directory = tempdir().unwrap();
            let runtime = directory.path();
            std::fs::create_dir(runtime.join("astrea")).unwrap();
            std::fs::create_dir(runtime.join("astrea/typhon")).unwrap();
            let instance = runtime.join("astrea/typhon/test");
            std::fs::create_dir(&instance).unwrap();
            set_mode(runtime, 0o700);
            set_mode(&runtime.join("astrea"), 0o700);
            set_mode(&runtime.join("astrea/typhon"), 0o700);
            set_mode(&instance, 0o700);
            let socket_path = instance.join("control.sock");
            let listener = UnixListener::bind(&socket_path).unwrap();
            set_mode(&socket_path, 0o600);
            Self {
                directory,
                listener: Some(listener),
            }
        }

        fn listener(&self) -> &UnixListener {
            self.listener.as_ref().unwrap()
        }

        fn take_listener(&mut self) -> UnixListener {
            self.listener.take().unwrap()
        }

        fn path(&self) -> &Path {
            self.directory.path()
        }
    }

    fn set_mode(path: &Path, mode: u32) {
        std::fs::set_permissions(path, std::fs::Permissions::from_mode(mode)).unwrap();
    }
}
