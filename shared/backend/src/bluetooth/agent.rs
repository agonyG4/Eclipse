use std::{
    fmt,
    sync::{Arc, Mutex, atomic::AtomicU64},
    time::Duration,
};

use async_channel::{Receiver, Sender};
use futures::{
    future::{Either, select},
    pin_mut,
};
use zbus::zvariant::OwnedObjectPath;
use zbus::{DBusError, message::Header};

pub const AGENT_OBJECT_PATH: &str = "/org/astrea/bluetooth/agent";
pub const AGENT_CAPABILITY: &str = "KeyboardDisplay";

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum AgentPromptKind {
    #[default]
    None,
    PinCodeInput,
    PasskeyInput,
    PasskeyConfirmation,
    Authorization,
    ServiceAuthorization,
    DisplayPinCode,
    DisplayPasskey,
}

#[derive(Clone, Default, PartialEq, Eq)]
pub struct AgentPromptView {
    pub active: bool,
    pub request_id: u64,
    pub pairing_epoch: u64,
    pub session_generation: u64,
    pub bluez_generation: u64,
    pub kind: AgentPromptKind,
    pub device_path: String,
    pub passkey: Option<u32>,
    pub entered: Option<u16>,
    pub service_uuid: Option<String>,
    pub display_pin: Option<String>,
}

impl fmt::Debug for AgentPromptView {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        formatter
            .debug_struct("AgentPromptView")
            .field("active", &self.active)
            .field("request_id", &self.request_id)
            .field("pairing_epoch", &self.pairing_epoch)
            .field("session_generation", &self.session_generation)
            .field("bluez_generation", &self.bluez_generation)
            .field("kind", &self.kind)
            .field("device_path", &self.device_path)
            .field("has_passkey", &self.passkey.is_some())
            .field("entered", &self.entered)
            .field("service_uuid", &self.service_uuid)
            .field("has_display_pin", &self.display_pin.is_some())
            .finish()
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum AgentSubmitResult {
    Accepted,
    Ignored,
    Invalid,
}

#[derive(Clone, Debug, DBusError, PartialEq, Eq)]
#[zbus(prefix = "org.bluez.Error")]
pub enum AgentError {
    Rejected,
    Canceled,
}

#[derive(Clone, PartialEq, Eq)]
struct AgentAuthority {
    session_generation: u64,
    bluez_generation: u64,
    owner: String,
}

#[derive(Clone, PartialEq, Eq)]
struct PairingContext {
    pairing_epoch: u64,
    session_generation: u64,
    bluez_generation: u64,
    device_path: String,
}

struct PendingInteractive {
    prompt: AgentPromptView,
    response_tx: Sender<PendingResponse>,
}

enum PendingResponse {
    Text(String),
    Passkey(u32),
    Confirmed,
    Rejected,
    Canceled,
}

#[derive(Default)]
struct BrokerState {
    authority: Option<AgentAuthority>,
    pairing: Option<PairingContext>,
    interactive: Option<PendingInteractive>,
    display: Option<AgentPromptView>,
}

struct AgentCall<'a> {
    sender: &'a str,
    session_generation: u64,
    bluez_generation: u64,
    device_path: &'a str,
}

struct PromptSpec {
    kind: AgentPromptKind,
    passkey: Option<u32>,
    entered: Option<u16>,
    service_uuid: Option<String>,
    display_pin: Option<String>,
}

#[derive(Clone)]
pub struct AgentBroker {
    state: Arc<Mutex<BrokerState>>,
    request_ids: Arc<AgentRequestIds>,
    wake_tx: Sender<()>,
    wake_rx: Receiver<()>,
    prompt_timeout: Duration,
    #[cfg(test)]
    wake_count: Arc<std::sync::atomic::AtomicUsize>,
}

#[derive(Debug, Default)]
pub(crate) struct AgentRequestIds {
    last_issued: AtomicU64,
}

impl AgentRequestIds {
    fn allocate(&self) -> Result<u64, AgentError> {
        self.last_issued
            .fetch_update(
                std::sync::atomic::Ordering::Relaxed,
                std::sync::atomic::Ordering::Relaxed,
                |last_issued| last_issued.checked_add(1),
            )
            .map(|last_issued| last_issued + 1)
            .map_err(|_| AgentError::Rejected)
    }
}

#[derive(Clone, Default)]
pub(crate) struct AgentReleaseHook {
    invalidate: Option<Arc<dyn Fn(u64, u64) + Send + Sync>>,
}

impl AgentReleaseHook {
    pub(crate) fn new(invalidate: impl Fn(u64, u64) + Send + Sync + 'static) -> Self {
        Self {
            invalidate: Some(Arc::new(invalidate)),
        }
    }

    fn invalidate(&self, session_generation: u64, bluez_generation: u64) {
        if let Some(invalidate) = &self.invalidate {
            invalidate(session_generation, bluez_generation);
        }
    }
}

impl AgentBroker {
    pub fn new() -> Self {
        Self::new_with_prompt_timeout(Duration::from_secs(60))
    }

    pub fn new_with_prompt_timeout(prompt_timeout: Duration) -> Self {
        Self::new_with_request_ids_and_prompt_timeout(
            Arc::new(AgentRequestIds::default()),
            prompt_timeout,
        )
    }

    pub(crate) fn new_with_request_ids(request_ids: Arc<AgentRequestIds>) -> Self {
        Self::new_with_request_ids_and_prompt_timeout(request_ids, Duration::from_secs(60))
    }

    fn new_with_request_ids_and_prompt_timeout(
        request_ids: Arc<AgentRequestIds>,
        prompt_timeout: Duration,
    ) -> Self {
        let (wake_tx, wake_rx) = async_channel::bounded(1);
        Self {
            state: Arc::new(Mutex::new(BrokerState::default())),
            request_ids,
            wake_tx,
            wake_rx,
            prompt_timeout,
            #[cfg(test)]
            wake_count: Arc::new(std::sync::atomic::AtomicUsize::new(0)),
        }
    }

    pub fn wake_receiver(&self) -> Receiver<()> {
        self.wake_rx.clone()
    }

    pub fn prompt(&self) -> AgentPromptView {
        let state = self.state.lock().expect("AgentBroker mutex poisoned");
        state
            .interactive
            .as_ref()
            .map(|pending| pending.prompt.clone())
            .or_else(|| state.display.clone())
            .unwrap_or_default()
    }

    pub fn set_authority(&self, session_generation: u64, bluez_generation: u64, owner: &str) {
        let mut notify = false;
        let mut state = self.state.lock().expect("AgentBroker mutex poisoned");
        let authority = AgentAuthority {
            session_generation,
            bluez_generation,
            owner: owner.to_owned(),
        };
        if state.authority.as_ref() != Some(&authority) {
            if state.authority.is_some() {
                cancel_locked(&mut state, PendingResponse::Canceled);
                state.pairing = None;
            }
            state.authority = Some(authority);
            notify = true;
        }
        drop(state);
        if notify {
            self.notify();
        }
    }

    pub fn set_pairing_context(
        &self,
        pairing_epoch: u64,
        session_generation: u64,
        bluez_generation: u64,
        device_path: &str,
    ) {
        let mut state = self.state.lock().expect("AgentBroker mutex poisoned");
        let context = PairingContext {
            pairing_epoch,
            session_generation,
            bluez_generation,
            device_path: device_path.to_owned(),
        };
        let changed = state.pairing.as_ref() != Some(&context);
        if changed {
            cancel_locked(&mut state, PendingResponse::Canceled);
            state.pairing = Some(context);
        }
        drop(state);
        if changed {
            self.notify();
        }
    }

    pub fn clear_pairing_context(&self) {
        let mut state = self.state.lock().expect("AgentBroker mutex poisoned");
        let changed =
            state.pairing.is_some() || state.interactive.is_some() || state.display.is_some();
        if changed {
            cancel_locked(&mut state, PendingResponse::Canceled);
            state.pairing = None;
        }
        drop(state);
        if changed {
            self.notify();
        }
    }

    pub fn authorize_call(
        &self,
        sender: &str,
        session_generation: u64,
        bluez_generation: u64,
    ) -> Result<(), AgentError> {
        let state = self.state.lock().expect("AgentBroker mutex poisoned");
        authorize_locked(&state, sender, session_generation, bluez_generation)
    }

    fn authority_for_sender(&self, sender: &str) -> Result<(u64, u64), AgentError> {
        let state = self.state.lock().expect("AgentBroker mutex poisoned");
        let Some(authority) = &state.authority else {
            return Err(AgentError::Rejected);
        };
        if authority.owner != sender {
            return Err(AgentError::Rejected);
        }
        Ok((authority.session_generation, authority.bluez_generation))
    }

    pub async fn request_pin_code(
        &self,
        sender: &str,
        session_generation: u64,
        bluez_generation: u64,
        device_path: &str,
    ) -> Result<String, AgentError> {
        let response = self
            .interactive(
                AgentCall {
                    sender,
                    session_generation,
                    bluez_generation,
                    device_path,
                },
                PromptSpec {
                    kind: AgentPromptKind::PinCodeInput,
                    passkey: None,
                    entered: None,
                    service_uuid: None,
                    display_pin: None,
                },
            )
            .await?;
        match response {
            PendingResponse::Text(text) => Ok(text),
            PendingResponse::Rejected => Err(AgentError::Rejected),
            PendingResponse::Canceled => Err(AgentError::Canceled),
            PendingResponse::Passkey(_) | PendingResponse::Confirmed => Err(AgentError::Rejected),
        }
    }

    pub async fn request_passkey(
        &self,
        sender: &str,
        session_generation: u64,
        bluez_generation: u64,
        device_path: &str,
    ) -> Result<u32, AgentError> {
        let response = self
            .interactive(
                AgentCall {
                    sender,
                    session_generation,
                    bluez_generation,
                    device_path,
                },
                PromptSpec {
                    kind: AgentPromptKind::PasskeyInput,
                    passkey: None,
                    entered: None,
                    service_uuid: None,
                    display_pin: None,
                },
            )
            .await?;
        match response {
            PendingResponse::Passkey(passkey) => Ok(passkey),
            PendingResponse::Rejected => Err(AgentError::Rejected),
            PendingResponse::Canceled => Err(AgentError::Canceled),
            PendingResponse::Text(_) | PendingResponse::Confirmed => Err(AgentError::Rejected),
        }
    }

    pub async fn request_confirmation(
        &self,
        sender: &str,
        session_generation: u64,
        bluez_generation: u64,
        device_path: &str,
        passkey: u32,
    ) -> Result<(), AgentError> {
        self.request_confirmation_like(
            AgentCall {
                sender,
                session_generation,
                bluez_generation,
                device_path,
            },
            PromptSpec {
                kind: AgentPromptKind::PasskeyConfirmation,
                passkey: Some(passkey),
                entered: None,
                service_uuid: None,
                display_pin: None,
            },
        )
        .await
    }

    pub async fn request_authorization(
        &self,
        sender: &str,
        session_generation: u64,
        bluez_generation: u64,
        device_path: &str,
    ) -> Result<(), AgentError> {
        self.request_confirmation_like(
            AgentCall {
                sender,
                session_generation,
                bluez_generation,
                device_path,
            },
            PromptSpec {
                kind: AgentPromptKind::Authorization,
                passkey: None,
                entered: None,
                service_uuid: None,
                display_pin: None,
            },
        )
        .await
    }

    pub async fn authorize_service(
        &self,
        sender: &str,
        session_generation: u64,
        bluez_generation: u64,
        device_path: &str,
        service_uuid: &str,
    ) -> Result<(), AgentError> {
        self.request_confirmation_like(
            AgentCall {
                sender,
                session_generation,
                bluez_generation,
                device_path,
            },
            PromptSpec {
                kind: AgentPromptKind::ServiceAuthorization,
                passkey: None,
                entered: None,
                service_uuid: Some(service_uuid.to_owned()),
                display_pin: None,
            },
        )
        .await
    }

    pub fn display_pin_code(
        &self,
        sender: &str,
        session_generation: u64,
        bluez_generation: u64,
        device_path: &str,
        display_pin: &str,
    ) -> Result<(), AgentError> {
        self.display(
            AgentCall {
                sender,
                session_generation,
                bluez_generation,
                device_path,
            },
            PromptSpec {
                kind: AgentPromptKind::DisplayPinCode,
                passkey: None,
                entered: None,
                service_uuid: None,
                display_pin: Some(display_pin.to_owned()),
            },
        )
    }

    pub fn display_passkey(
        &self,
        sender: &str,
        session_generation: u64,
        bluez_generation: u64,
        device_path: &str,
        passkey: u32,
        entered: u16,
    ) -> Result<(), AgentError> {
        self.display(
            AgentCall {
                sender,
                session_generation,
                bluez_generation,
                device_path,
            },
            PromptSpec {
                kind: AgentPromptKind::DisplayPasskey,
                passkey: Some(passkey),
                entered: Some(entered),
                service_uuid: None,
                display_pin: None,
            },
        )
    }

    pub fn submit_text(&self, request_id: u64, text: &str) -> AgentSubmitResult {
        let response = {
            let mut state = self.state.lock().expect("AgentBroker mutex poisoned");
            let Some(pending) = state.interactive.as_ref() else {
                return AgentSubmitResult::Ignored;
            };
            if pending.prompt.request_id != request_id {
                return AgentSubmitResult::Ignored;
            }
            let response = match pending.prompt.kind {
                AgentPromptKind::PinCodeInput => {
                    if text.chars().count() == 0 || text.chars().count() > 16 {
                        return AgentSubmitResult::Invalid;
                    }
                    PendingResponse::Text(text.to_owned())
                }
                AgentPromptKind::PasskeyInput => {
                    if text.is_empty() || !text.bytes().all(|byte| byte.is_ascii_digit()) {
                        return AgentSubmitResult::Invalid;
                    }
                    let Ok(passkey) = text.parse::<u32>() else {
                        return AgentSubmitResult::Invalid;
                    };
                    if passkey > 999_999 {
                        return AgentSubmitResult::Invalid;
                    }
                    PendingResponse::Passkey(passkey)
                }
                _ => return AgentSubmitResult::Invalid,
            };
            let pending = state
                .interactive
                .take()
                .expect("interactive prompt disappeared while locked");
            (pending.response_tx, response)
        };
        let _ = response.0.try_send(response.1);
        self.notify();
        AgentSubmitResult::Accepted
    }

    pub fn confirm(&self, request_id: u64, accepted: bool) -> AgentSubmitResult {
        let response = {
            let mut state = self.state.lock().expect("AgentBroker mutex poisoned");
            let Some(pending) = state.interactive.as_ref() else {
                return AgentSubmitResult::Ignored;
            };
            if pending.prompt.request_id != request_id {
                return AgentSubmitResult::Ignored;
            }
            if !matches!(
                pending.prompt.kind,
                AgentPromptKind::PasskeyConfirmation
                    | AgentPromptKind::Authorization
                    | AgentPromptKind::ServiceAuthorization
            ) {
                return AgentSubmitResult::Invalid;
            }
            let pending = state
                .interactive
                .take()
                .expect("interactive prompt disappeared while locked");
            let response = if accepted {
                PendingResponse::Confirmed
            } else {
                PendingResponse::Rejected
            };
            (pending.response_tx, response)
        };
        let _ = response.0.try_send(response.1);
        self.notify();
        AgentSubmitResult::Accepted
    }

    pub fn reject(&self, request_id: u64) -> AgentSubmitResult {
        let response = {
            let mut state = self.state.lock().expect("AgentBroker mutex poisoned");
            let Some(pending) = state.interactive.as_ref() else {
                return AgentSubmitResult::Ignored;
            };
            if pending.prompt.request_id != request_id {
                return AgentSubmitResult::Ignored;
            }
            let pending = state
                .interactive
                .take()
                .expect("interactive prompt disappeared while locked");
            (pending.response_tx, PendingResponse::Rejected)
        };
        let _ = response.0.try_send(response.1);
        self.notify();
        AgentSubmitResult::Accepted
    }

    pub fn cancel_from_bluez(
        &self,
        sender: &str,
        session_generation: u64,
        bluez_generation: u64,
    ) -> Result<(), AgentError> {
        self.authorize_call(sender, session_generation, bluez_generation)?;
        self.cancel_for_lifecycle();
        Ok(())
    }

    pub fn release_from_bluez(
        &self,
        sender: &str,
        session_generation: u64,
        bluez_generation: u64,
    ) -> Result<(), AgentError> {
        self.cancel_from_bluez(sender, session_generation, bluez_generation)
    }

    pub fn cancel_for_lifecycle(&self) {
        let mut state = self.state.lock().expect("AgentBroker mutex poisoned");
        let changed =
            state.pairing.is_some() || state.interactive.is_some() || state.display.is_some();
        if changed {
            cancel_locked(&mut state, PendingResponse::Canceled);
            state.pairing = None;
        }
        drop(state);
        if changed {
            self.notify();
        }
    }

    pub fn on_service_stop(&self) {
        self.invalidate_connection();
    }

    pub fn on_system_bus_loss(&self) {
        self.invalidate_connection();
    }

    pub fn on_bluez_owner_replaced(&self) {
        self.invalidate_connection();
    }

    fn invalidate_connection(&self) {
        let mut state = self.state.lock().expect("AgentBroker mutex poisoned");
        let changed = state.authority.is_some()
            || state.pairing.is_some()
            || state.interactive.is_some()
            || state.display.is_some();
        if changed {
            cancel_locked(&mut state, PendingResponse::Canceled);
            state.authority = None;
            state.pairing = None;
        }
        drop(state);
        if changed {
            self.notify();
        }
    }

    async fn request_confirmation_like(
        &self,
        call: AgentCall<'_>,
        prompt: PromptSpec,
    ) -> Result<(), AgentError> {
        let response = self.interactive(call, prompt).await?;
        match response {
            PendingResponse::Confirmed => Ok(()),
            PendingResponse::Rejected => Err(AgentError::Rejected),
            PendingResponse::Canceled => Err(AgentError::Canceled),
            PendingResponse::Text(_) | PendingResponse::Passkey(_) => Err(AgentError::Rejected),
        }
    }

    async fn interactive(
        &self,
        call: AgentCall<'_>,
        prompt_spec: PromptSpec,
    ) -> Result<PendingResponse, AgentError> {
        let (request_id, response_rx) = {
            let mut state = self.state.lock().expect("AgentBroker mutex poisoned");
            let pairing = context_locked(&state, &call)?;
            if state.interactive.is_some() {
                return Err(AgentError::Rejected);
            }
            let request_id = self.request_ids.allocate()?;
            let (response_tx, response_rx) = async_channel::bounded(1);
            state.display = None;
            let prompt = AgentPromptView {
                active: true,
                request_id,
                pairing_epoch: pairing.pairing_epoch,
                session_generation: pairing.session_generation,
                bluez_generation: pairing.bluez_generation,
                kind: prompt_spec.kind,
                device_path: pairing.device_path,
                passkey: prompt_spec.passkey,
                entered: prompt_spec.entered,
                service_uuid: prompt_spec.service_uuid,
                display_pin: prompt_spec.display_pin,
            };
            state.interactive = Some(PendingInteractive {
                prompt,
                response_tx,
            });
            (request_id, response_rx)
        };
        self.notify();

        let response = response_rx.recv();
        let timeout = async_io::Timer::after(self.prompt_timeout);
        pin_mut!(response, timeout);
        match select(response, timeout).await {
            Either::Left((Ok(response), _)) => Ok(response),
            Either::Left((Err(_), _)) | Either::Right((_, _)) => {
                self.clear_interactive(request_id);
                Err(AgentError::Canceled)
            }
        }
    }

    fn display(&self, call: AgentCall<'_>, prompt_spec: PromptSpec) -> Result<(), AgentError> {
        let mut state = self.state.lock().expect("AgentBroker mutex poisoned");
        let pairing = context_locked(&state, &call)?;
        let request_id = if let Some(prompt) = state.display.as_ref().filter(|prompt| {
            prompt.pairing_epoch == pairing.pairing_epoch
                && prompt.device_path == pairing.device_path
        }) {
            prompt.request_id
        } else {
            self.request_ids.allocate()?
        };
        let prompt = AgentPromptView {
            active: true,
            request_id,
            pairing_epoch: pairing.pairing_epoch,
            session_generation: pairing.session_generation,
            bluez_generation: pairing.bluez_generation,
            kind: prompt_spec.kind,
            device_path: pairing.device_path,
            passkey: prompt_spec.passkey,
            entered: prompt_spec.entered,
            service_uuid: None,
            display_pin: prompt_spec.display_pin,
        };
        let changed = state.display.as_ref() != Some(&prompt);
        state.display = Some(prompt);
        drop(state);
        if changed {
            self.notify();
        }
        Ok(())
    }

    fn clear_interactive(&self, request_id: u64) {
        let mut state = self.state.lock().expect("AgentBroker mutex poisoned");
        if state
            .interactive
            .as_ref()
            .is_some_and(|pending| pending.prompt.request_id == request_id)
        {
            state.interactive = None;
            drop(state);
            self.notify();
        }
    }

    fn notify(&self) {
        #[cfg(test)]
        self.wake_count
            .fetch_add(1, std::sync::atomic::Ordering::SeqCst);
        let _ = self.wake_tx.try_send(());
    }

    #[cfg(test)]
    pub(crate) fn notify_for_test(&self) {
        self.notify();
    }

    #[cfg(test)]
    pub(crate) fn wake_count_for_test(&self) -> usize {
        self.wake_count.load(std::sync::atomic::Ordering::SeqCst)
    }
}

impl Default for AgentBroker {
    fn default() -> Self {
        Self::new()
    }
}

fn authorize_locked(
    state: &BrokerState,
    sender: &str,
    session_generation: u64,
    bluez_generation: u64,
) -> Result<(), AgentError> {
    let Some(authority) = &state.authority else {
        return Err(AgentError::Rejected);
    };
    if authority.session_generation != session_generation
        || authority.bluez_generation != bluez_generation
        || authority.owner != sender
    {
        return Err(AgentError::Rejected);
    }
    Ok(())
}

fn context_locked(state: &BrokerState, call: &AgentCall<'_>) -> Result<PairingContext, AgentError> {
    authorize_locked(
        state,
        call.sender,
        call.session_generation,
        call.bluez_generation,
    )?;
    let Some(pairing) = &state.pairing else {
        return Err(AgentError::Rejected);
    };
    if pairing.session_generation != call.session_generation
        || pairing.bluez_generation != call.bluez_generation
        || pairing.device_path != call.device_path
    {
        return Err(AgentError::Rejected);
    }
    Ok(pairing.clone())
}

fn cancel_locked(state: &mut BrokerState, response: PendingResponse) {
    if let Some(pending) = state.interactive.take() {
        let _ = pending.response_tx.try_send(response);
    }
    state.display = None;
}

#[derive(Clone)]
pub struct Agent1 {
    broker: AgentBroker,
    release_hook: AgentReleaseHook,
}

impl Agent1 {
    pub fn new(broker: AgentBroker) -> Self {
        Self {
            broker,
            release_hook: AgentReleaseHook::default(),
        }
    }

    pub(crate) fn with_release_hook(broker: AgentBroker, release_hook: AgentReleaseHook) -> Self {
        Self {
            broker,
            release_hook,
        }
    }

    pub(crate) fn release_from_bluez(
        &self,
        sender: &str,
        session_generation: u64,
        bluez_generation: u64,
    ) -> Result<(), AgentError> {
        self.broker
            .release_from_bluez(sender, session_generation, bluez_generation)?;
        self.release_hook
            .invalidate(session_generation, bluez_generation);
        Ok(())
    }

    fn sender(header: &Header<'_>) -> Result<String, AgentError> {
        header
            .sender()
            .map(|sender| sender.as_str().to_owned())
            .ok_or(AgentError::Rejected)
    }

    fn caller(header: &Header<'_>, broker: &AgentBroker) -> Result<(String, u64, u64), AgentError> {
        let sender = Self::sender(header)?;
        let (session_generation, bluez_generation) = broker.authority_for_sender(&sender)?;
        Ok((sender, session_generation, bluez_generation))
    }
}

#[zbus::interface(name = "org.bluez.Agent1")]
impl Agent1 {
    async fn release(&self, #[zbus(header)] header: Header<'_>) -> Result<(), AgentError> {
        let (sender, session_generation, bluez_generation) = Self::caller(&header, &self.broker)?;
        self.release_from_bluez(&sender, session_generation, bluez_generation)
    }

    async fn request_pin_code(
        &self,
        device: OwnedObjectPath,
        #[zbus(header)] header: Header<'_>,
    ) -> Result<String, AgentError> {
        let (sender, session_generation, bluez_generation) = Self::caller(&header, &self.broker)?;
        self.broker
            .request_pin_code(
                &sender,
                session_generation,
                bluez_generation,
                device.as_str(),
            )
            .await
    }

    async fn display_pin_code(
        &self,
        device: OwnedObjectPath,
        pin_code: String,
        #[zbus(header)] header: Header<'_>,
    ) -> Result<(), AgentError> {
        let (sender, session_generation, bluez_generation) = Self::caller(&header, &self.broker)?;
        self.broker.display_pin_code(
            &sender,
            session_generation,
            bluez_generation,
            device.as_str(),
            &pin_code,
        )
    }

    async fn request_passkey(
        &self,
        device: OwnedObjectPath,
        #[zbus(header)] header: Header<'_>,
    ) -> Result<u32, AgentError> {
        let (sender, session_generation, bluez_generation) = Self::caller(&header, &self.broker)?;
        self.broker
            .request_passkey(
                &sender,
                session_generation,
                bluez_generation,
                device.as_str(),
            )
            .await
    }

    async fn display_passkey(
        &self,
        device: OwnedObjectPath,
        passkey: u32,
        entered: u16,
        #[zbus(header)] header: Header<'_>,
    ) -> Result<(), AgentError> {
        let (sender, session_generation, bluez_generation) = Self::caller(&header, &self.broker)?;
        self.broker.display_passkey(
            &sender,
            session_generation,
            bluez_generation,
            device.as_str(),
            passkey,
            entered,
        )
    }

    async fn request_confirmation(
        &self,
        device: OwnedObjectPath,
        passkey: u32,
        #[zbus(header)] header: Header<'_>,
    ) -> Result<(), AgentError> {
        let (sender, session_generation, bluez_generation) = Self::caller(&header, &self.broker)?;
        self.broker
            .request_confirmation(
                &sender,
                session_generation,
                bluez_generation,
                device.as_str(),
                passkey,
            )
            .await
    }

    async fn request_authorization(
        &self,
        device: OwnedObjectPath,
        #[zbus(header)] header: Header<'_>,
    ) -> Result<(), AgentError> {
        let (sender, session_generation, bluez_generation) = Self::caller(&header, &self.broker)?;
        self.broker
            .request_authorization(
                &sender,
                session_generation,
                bluez_generation,
                device.as_str(),
            )
            .await
    }

    async fn authorize_service(
        &self,
        device: OwnedObjectPath,
        service_uuid: String,
        #[zbus(header)] header: Header<'_>,
    ) -> Result<(), AgentError> {
        let (sender, session_generation, bluez_generation) = Self::caller(&header, &self.broker)?;
        self.broker
            .authorize_service(
                &sender,
                session_generation,
                bluez_generation,
                device.as_str(),
                &service_uuid,
            )
            .await
    }

    async fn cancel(&self, #[zbus(header)] header: Header<'_>) -> Result<(), AgentError> {
        let (sender, session_generation, bluez_generation) = Self::caller(&header, &self.broker)?;
        self.broker
            .cancel_from_bluez(&sender, session_generation, bluez_generation)
    }
}

#[cfg(test)]
mod tests {
    use super::{AGENT_OBJECT_PATH, Agent1, AgentBroker, AgentError, AgentReleaseHook};
    use std::sync::{
        Arc,
        atomic::{AtomicU64, AtomicUsize, Ordering},
    };
    use zbus::Message;

    const OWNER: &str = ":1.42";
    const SESSION: u64 = 7;
    const BLUEZ: u64 = 11;
    const DEVICE: &str = "/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF";

    #[test]
    fn release_interface_authenticates_sender_before_invalidating_registration() {
        let broker = AgentBroker::new();
        broker.set_authority(SESSION, BLUEZ, OWNER);
        broker.set_pairing_context(3, SESSION, BLUEZ, DEVICE);
        broker
            .display_passkey(OWNER, SESSION, BLUEZ, DEVICE, 123456, 0)
            .expect("display prompt");
        let releases = Arc::new(AtomicUsize::new(0));
        let hook_releases = Arc::clone(&releases);
        let agent = Agent1::with_release_hook(
            broker.clone(),
            AgentReleaseHook::new(move |session, bluez| {
                assert_eq!(session, SESSION);
                assert_eq!(bluez, BLUEZ);
                hook_releases.fetch_add(1, Ordering::SeqCst);
            }),
        );

        let unauthorized = release_message(":1.99");
        assert_eq!(
            smol::block_on(agent.release(unauthorized.header())),
            Err(AgentError::Rejected)
        );
        assert_eq!(releases.load(Ordering::SeqCst), 0);
        assert!(broker.prompt().active);

        let authorized = release_message(OWNER);
        smol::block_on(agent.release(authorized.header())).expect("authorized Release");
        assert_eq!(releases.load(Ordering::SeqCst), 1);
        assert!(!broker.prompt().active);
    }

    #[test]
    fn request_id_allocator_issues_max_once_then_fails_closed() {
        let ids = super::AgentRequestIds {
            last_issued: AtomicU64::new(u64::MAX - 1),
        };
        assert_eq!(ids.allocate(), Ok(u64::MAX));
        assert_eq!(ids.allocate(), Err(AgentError::Rejected));
    }

    #[test]
    fn empty_pairing_context_clear_does_not_emit_a_wake() {
        let broker = AgentBroker::new();
        let before = broker.wake_count_for_test();

        broker.clear_pairing_context();

        assert_eq!(broker.wake_count_for_test(), before);
    }

    #[test]
    fn unchanged_pairing_context_and_display_prompt_do_not_emit_wakes() {
        let broker = AgentBroker::new();
        broker.set_authority(SESSION, BLUEZ, OWNER);
        broker.set_pairing_context(3, SESSION, BLUEZ, DEVICE);
        let pairing_context_wakes = broker.wake_count_for_test();

        broker.set_pairing_context(3, SESSION, BLUEZ, DEVICE);

        assert_eq!(broker.wake_count_for_test(), pairing_context_wakes);

        broker
            .display_passkey(OWNER, SESSION, BLUEZ, DEVICE, 123456, 4)
            .expect("initial display prompt");
        let display_wakes = broker.wake_count_for_test();
        broker
            .display_passkey(OWNER, SESSION, BLUEZ, DEVICE, 123456, 4)
            .expect("identical display prompt");

        assert_eq!(broker.wake_count_for_test(), display_wakes);
    }

    fn release_message(sender: &str) -> Message {
        Message::method_call(AGENT_OBJECT_PATH, "Release")
            .expect("Release method message")
            .interface("org.bluez.Agent1")
            .expect("Agent1 interface")
            .sender(sender)
            .expect("Release sender")
            .build(&())
            .expect("Release message")
    }
}
