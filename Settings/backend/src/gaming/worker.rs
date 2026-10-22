use astrea_gaming::{config::GamingService, store::config_home};
use serde_json::Value;
use std::sync::mpsc::{self, SyncSender};
use std::thread;

#[derive(Debug)]
pub struct GamingWorkerEvent {
    pub generation: u64,
    pub snapshot: Option<Value>,
    pub error: Option<String>,
}

enum Request {
    Refresh {
        generation: u64,
    },
    SetProton {
        generation: u64,
        key: String,
        value: Value,
    },
    SetCompatibility {
        generation: u64,
        key: String,
        value: Value,
    },
}

pub struct GamingWorker {
    sender: SyncSender<Request>,
}

impl GamingWorker {
    pub fn new<F>(callback: F) -> Result<Self, String>
    where
        F: Fn(GamingWorkerEvent) + Send + 'static,
    {
        let (sender, receiver) = mpsc::sync_channel(1);
        let service = config_home().map(GamingService::new);
        thread::Builder::new()
            .name(String::from("astrea-settings-gaming"))
            .spawn(move || {
                while let Ok(request) = receiver.recv() {
                    let generation = match &request {
                        Request::Refresh { generation }
                        | Request::SetProton { generation, .. }
                        | Request::SetCompatibility { generation, .. } => *generation,
                    };
                    let result = match service.as_ref() {
                        Ok(service) => execute(service, request),
                        Err(error) => Err((error.clone(), None)),
                    };
                    let event = match result {
                        Ok(snapshot) => GamingWorkerEvent {
                            generation,
                            snapshot: Some(snapshot),
                            error: None,
                        },
                        Err((error, snapshot)) => GamingWorkerEvent {
                            generation,
                            snapshot,
                            error: Some(error),
                        },
                    };
                    callback(event);
                }
            })
            .map_err(|error| format!("failed to start Settings Gaming worker: {error}"))?;
        Ok(Self { sender })
    }

    pub fn refresh(&self, generation: u64) -> Result<(), String> {
        self.send(Request::Refresh { generation })
    }

    pub fn set_proton(&self, generation: u64, key: String, value: Value) -> Result<(), String> {
        self.send(Request::SetProton {
            generation,
            key,
            value,
        })
    }

    pub fn set_compatibility(
        &self,
        generation: u64,
        key: String,
        value: Value,
    ) -> Result<(), String> {
        self.send(Request::SetCompatibility {
            generation,
            key,
            value,
        })
    }

    fn send(&self, request: Request) -> Result<(), String> {
        self.sender
            .try_send(request)
            .map_err(|error| format!("Settings Gaming worker is unavailable: {error}"))
    }
}

fn execute(service: &GamingService, request: Request) -> Result<Value, (String, Option<Value>)> {
    match request {
        Request::Refresh { .. } => service.snapshot().map_err(|error| (error, None)),
        Request::SetProton { key, value, .. } => match service.set_proton(&key, value) {
            Ok(snapshot) => Ok(snapshot),
            Err(error) => Err((error, service.snapshot().ok())),
        },
        Request::SetCompatibility { key, value, .. } => {
            match service.set_compatibility(&key, value) {
                Ok(snapshot) => Ok(snapshot),
                Err(error) => Err((error, service.snapshot().ok())),
            }
        }
    }
}
