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
        let (sender, receiver) = sync_channel(1);
        let debounce = Arc::new(DebounceState {
            deadline: Mutex::new(None),
        });
        let worker_debounce = Arc::clone(&debounce);
        let completion = Arc::new(completion);
        let worker_completion = Arc::clone(&completion);
        let thread = thread::Builder::new()
            .name(String::from("astrea-settings-typhon"))
            .spawn(move || worker_loop(receiver, worker_debounce, worker_completion))
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
                let result = execute_request(id, request, DEFAULT_DEADLINE);
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
    if (error.kind() == ErrorKind::TimedOut || error.kind() == ErrorKind::WouldBlock)
        && Instant::now() >= deadline
    {
        return ClientError::Timeout;
    }
    let _ = error;
    transport_failure()
}

fn transport_failure() -> ClientError {
    ClientError::Transport(String::from("socket transport failed"))
}
